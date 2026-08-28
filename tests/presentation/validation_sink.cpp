#include "validation_sink.hpp"

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::test_support {
namespace {

constexpr std::size_t maxDiagnostics = 1024;
constexpr std::size_t maxManifestEntries = 65'536;
constexpr std::size_t maxNormalizedRecords = 100'000;
constexpr std::uint32_t maxMeshVertices = 1'048'576;
constexpr std::uint32_t maxMeshIndices = 3'145'728;
constexpr std::uint32_t maxTextureDimension = 8'192;
constexpr std::uint64_t maxResourceBytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maxSessionBytes = 512ULL * 1024ULL * 1024ULL;
constexpr float maxPositionMagnitude = 1'000'000.0F;
constexpr double depthQuantization = 4096.0;
constexpr double signedIntegerLimit = 0x1p63;

[[nodiscard]] auto resourceTypeName(playback::PresentationResourceType type) noexcept
    -> std::string_view {
    switch (type) {
    case playback::PresentationResourceType::Mesh:
        return "mesh";
    case playback::PresentationResourceType::Texture2D:
        return "texture2d";
    case playback::PresentationResourceType::UnlitMaterial:
        return "unlit_material";
    case playback::PresentationResourceType::Shader:
        return "shader";
    case playback::PresentationResourceType::ParameterizedMaterial:
        return "parameterized_material";
    }
    return "unknown";
}

[[nodiscard]] auto diagnostics() -> core::Diagnostics {
    return core::Diagnostics{
        maxDiagnostics,
        core::Diagnostic{core::DiagnosticSeverity::Error,
                         "playback.presentation.diagnostics.limit_exceeded",
                         "Validation Sink diagnostics reached the configured limit", "$"}
            .withContext("max_diagnostics", std::to_string(maxDiagnostics))};
}

void addError(core::Diagnostics& destination, const core::Error& error) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    destination.add(std::move(diagnostic));
    destination.sortDeterministically();
}

[[nodiscard]] auto referenceError(std::string message,
                                  const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error {
    auto error = core::Error{"playback.presentation.reference.invalid", std::move(message)};
    if (reference != nullptr) {
        error.withContext("asset_id", reference->assetId)
            .withContext("resource_type", std::string{resourceTypeName(reference->type)});
    }
    return error;
}

[[nodiscard]] auto frameResourceError(std::string message, std::string_view objectId,
                                      const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error {
    auto error = core::Error{"playback.presentation.frame.resource_mismatch", std::move(message)};
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    if (reference != nullptr) {
        error.withContext("asset_id", reference->assetId)
            .withContext("resource_type", std::string{resourceTypeName(reference->type)});
    }
    return error;
}

[[nodiscard]] auto nonFiniteError(std::string_view objectId, std::string_view field)
    -> core::Error {
    auto error = core::Error{"playback.presentation.frame.non_finite",
                             "Validation Sink frame calculation contains a non-finite value"}
                     .withContext("field", std::string{field});
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

[[nodiscard]] auto frameValueError(std::string_view objectId, std::string_view field)
    -> core::Error {
    auto error = core::Error{"playback.presentation.frame.value_invalid",
                             "Validation Sink frame value is outside the Portable v1 range"}
                     .withContext("field", std::string{field});
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

[[nodiscard]] auto checkedAdd(std::uint64_t left, std::uint64_t right,
                              std::uint64_t& destination) noexcept -> bool {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    destination = left + right;
    return true;
}

[[nodiscard]] auto checkedMultiply(std::uint64_t left, std::uint64_t right,
                                   std::uint64_t& destination) noexcept -> bool {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return false;
    }
    destination = left * right;
    return true;
}

[[nodiscard]] auto referenceKey(const playback::PresentationResourceRef& reference) noexcept {
    return std::tie(reference.assetId, reference.type);
}

[[nodiscard]] auto isPortableAssetId(std::string_view value) noexcept -> bool {
    const auto alphaNumeric = [](unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9');
    };
    if (value.empty() || value.size() > 256 ||
        !alphaNumeric(static_cast<unsigned char>(value.front()))) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [&](unsigned char character) {
        return alphaNumeric(character) || character == '.' || character == '_' ||
               character == '-' || character == '/';
    });
}

[[nodiscard]] auto referenceByteCount(const playback::PresentationResourceRef& reference,
                                      std::uint64_t& destination) noexcept -> bool {
    return checkedAdd(37ULL, static_cast<std::uint64_t>(reference.assetId.size()), destination);
}

class Sha256 final {
  public:
    void update(std::span<const std::byte> bytes) noexcept {
        totalBytes_ += bytes.size();
        for (const auto value : bytes) {
            block_[blockSize_++] = std::to_integer<std::uint8_t>(value);
            if (blockSize_ == block_.size()) {
                transform(block_);
                blockSize_ = 0;
            }
        }
    }

    [[nodiscard]] auto finish() const noexcept -> playback::PresentationContentIdentity {
        auto copy = *this;
        const auto bitCount = copy.totalBytes_ * 8ULL;
        copy.block_[copy.blockSize_++] = 0x80U;
        if (copy.blockSize_ > 56) {
            std::fill(copy.block_.begin() + static_cast<std::ptrdiff_t>(copy.blockSize_),
                      copy.block_.end(), 0U);
            copy.transform(copy.block_);
            copy.blockSize_ = 0;
        }
        std::fill(copy.block_.begin() + static_cast<std::ptrdiff_t>(copy.blockSize_),
                  copy.block_.begin() + 56, 0U);
        for (std::size_t index = 0; index < 8; ++index) {
            copy.block_[56 + index] = static_cast<std::uint8_t>(bitCount >> ((7U - index) * 8U));
        }
        copy.transform(copy.block_);

        playback::PresentationContentIdentity identity;
        for (std::size_t word = 0; word < copy.state_.size(); ++word) {
            for (std::size_t byte = 0; byte < 4; ++byte) {
                identity.sha256[word * 4 + byte] =
                    static_cast<std::uint8_t>(copy.state_[word] >> ((3U - byte) * 8U));
            }
        }
        return identity;
    }

  private:
    [[nodiscard]] static auto choose(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
        -> std::uint32_t {
        return (x & y) ^ (~x & z);
    }

    [[nodiscard]] static auto majority(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
        -> std::uint32_t {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    void transform(const std::array<std::uint8_t, 64>& block) noexcept {
        static constexpr std::array<std::uint32_t, 64> constants{
            0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U,
            0x923F82A4U, 0xAB1C5ED5U, 0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
            0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U, 0xE49B69C1U, 0xEFBE4786U,
            0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
            0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U, 0xC6E00BF3U, 0xD5A79147U,
            0x06CA6351U, 0x14292967U, 0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
            0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U, 0xA2BFE8A1U, 0xA81A664BU,
            0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
            0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU,
            0x5B9CCA4FU, 0x682E6FF3U, 0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
            0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
        };

        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            words[index] = (static_cast<std::uint32_t>(block[index * 4]) << 24U) |
                           (static_cast<std::uint32_t>(block[index * 4 + 1]) << 16U) |
                           (static_cast<std::uint32_t>(block[index * 4 + 2]) << 8U) |
                           static_cast<std::uint32_t>(block[index * 4 + 3]);
        }
        for (std::size_t index = 16; index < words.size(); ++index) {
            const auto small0 = std::rotr(words[index - 15], 7) ^ std::rotr(words[index - 15], 18) ^
                                (words[index - 15] >> 3U);
            const auto small1 = std::rotr(words[index - 2], 17) ^ std::rotr(words[index - 2], 19) ^
                                (words[index - 2] >> 10U);
            words[index] = words[index - 16] + small0 + words[index - 7] + small1;
        }

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        auto f = state_[5];
        auto g = state_[6];
        auto h = state_[7];
        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto large1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto temporary1 = h + large1 + choose(e, f, g) + constants[index] + words[index];
            const auto large0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto temporary2 = large0 + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
                                        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
    std::array<std::uint8_t, 64> block_{};
    std::size_t blockSize_{};
    std::uint64_t totalBytes_{};
};

class CanonicalHash final {
  public:
    explicit CanonicalHash(playback::PresentationResourceType type) noexcept {
        static constexpr char domain[] = "cuexis.portable.presentation.v1";
        sha_.update(std::as_bytes(std::span{domain, sizeof(domain)}));
        writeU32(static_cast<std::uint32_t>(type));
    }

    void writeU32(std::uint32_t value) noexcept {
        const std::array bytes{static_cast<std::byte>(value & 0xFFU),
                               static_cast<std::byte>((value >> 8U) & 0xFFU),
                               static_cast<std::byte>((value >> 16U) & 0xFFU),
                               static_cast<std::byte>((value >> 24U) & 0xFFU)};
        sha_.update(bytes);
    }

    void writeFloat(float value) noexcept {
        if (value == 0.0F) {
            value = 0.0F;
        }
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeString(std::string_view value) noexcept {
        writeU32(static_cast<std::uint32_t>(value.size()));
        sha_.update(std::as_bytes(std::span{value.data(), value.size()}));
    }

    void writeIdentity(const playback::PresentationContentIdentity& identity) noexcept {
        sha_.update(std::as_bytes(std::span{identity.sha256}));
    }

    void writeBytes(std::span<const std::byte> bytes) noexcept {
        sha_.update(bytes);
    }

    [[nodiscard]] auto finish() const noexcept -> playback::PresentationContentIdentity {
        return sha_.finish();
    }

  private:
    Sha256 sha_;
};

[[nodiscard]] auto meshIdentity(const playback::PortableMesh& mesh) noexcept
    -> playback::PresentationContentIdentity {
    CanonicalHash hash{playback::PresentationResourceType::Mesh};
    hash.writeU32(static_cast<std::uint32_t>(mesh.positions.size() / 3U));
    hash.writeU32(mesh.uv0.empty() ? 0U : 1U);
    for (const auto value : mesh.positions) {
        hash.writeFloat(value);
    }
    for (const auto value : mesh.uv0) {
        hash.writeFloat(value);
    }
    hash.writeU32(static_cast<std::uint32_t>(mesh.indices.size()));
    for (const auto value : mesh.indices) {
        hash.writeU32(value);
    }
    return hash.finish();
}

[[nodiscard]] auto textureIdentity(const playback::PortableTexture2D& texture) noexcept
    -> playback::PresentationContentIdentity {
    CanonicalHash hash{playback::PresentationResourceType::Texture2D};
    hash.writeU32(texture.width);
    hash.writeU32(texture.height);
    hash.writeU32(static_cast<std::uint32_t>(texture.colorSpace));
    hash.writeBytes(texture.pixelsRgba8);
    return hash.finish();
}

[[nodiscard]] auto materialIdentity(const playback::PortableUnlitMaterial& material) noexcept
    -> playback::PresentationContentIdentity {
    CanonicalHash hash{playback::PresentationResourceType::UnlitMaterial};
    hash.writeU32(static_cast<std::uint32_t>(material.alphaMode));
    hash.writeU32(material.doubleSided ? 1U : 0U);
    for (const auto value : material.baseColor) {
        hash.writeFloat(value);
    }
    hash.writeU32(material.baseColorTexture ? 1U : 0U);
    if (material.baseColorTexture) {
        hash.writeString(material.baseColorTexture->assetId);
        hash.writeIdentity(material.baseColorTexture->identity);
    }
    return hash.finish();
}

[[nodiscard]] auto validateMesh(const playback::PresentationResourceRef& reference,
                                const playback::PortableMesh& mesh, std::uint64_t& decodedBytes)
    -> core::Result<void> {
    if (mesh.positions.empty() || mesh.positions.size() % 3U != 0) {
        return core::unexpected(
            referenceError("Portable Mesh position array is invalid", &reference));
    }
    const auto vertexCount = mesh.positions.size() / 3U;
    if (vertexCount > maxMeshVertices || mesh.indices.empty() || mesh.indices.size() % 3U != 0 ||
        mesh.indices.size() > maxMeshIndices ||
        (!mesh.uv0.empty() && mesh.uv0.size() != vertexCount * 2U)) {
        return core::unexpected(referenceError("Portable Mesh counts are invalid", &reference));
    }
    for (const auto value : mesh.positions) {
        if (!std::isfinite(value) || std::abs(value) > maxPositionMagnitude) {
            return core::unexpected(
                referenceError("Portable Mesh position is outside the v1 range", &reference));
        }
    }
    for (const auto value : mesh.uv0) {
        if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
            return core::unexpected(
                referenceError("Portable Mesh UV0 is outside [0, 1]", &reference));
        }
    }
    if (!std::all_of(mesh.indices.begin(), mesh.indices.end(),
                     [&](std::uint32_t index) { return index < vertexCount; })) {
        return core::unexpected(referenceError("Portable Mesh index is out of range", &reference));
    }
    for (std::size_t index = 0; index < mesh.indices.size(); index += 3U) {
        const auto index0 = mesh.indices[index];
        const auto index1 = mesh.indices[index + 1U];
        const auto index2 = mesh.indices[index + 2U];
        if (index0 == index1 || index0 == index2 || index1 == index2) {
            return core::unexpected(
                referenceError("Portable Mesh triangle repeats a vertex index", &reference));
        }
        const auto coordinate = [&](std::uint32_t vertex, std::size_t axis) {
            return static_cast<double>(mesh.positions[vertex * 3U + axis]);
        };
        const auto edge10 = coordinate(index1, 0) - coordinate(index0, 0);
        const auto edge11 = coordinate(index1, 1) - coordinate(index0, 1);
        const auto edge12 = coordinate(index1, 2) - coordinate(index0, 2);
        const auto edge20 = coordinate(index2, 0) - coordinate(index0, 0);
        const auto edge21 = coordinate(index2, 1) - coordinate(index0, 1);
        const auto edge22 = coordinate(index2, 2) - coordinate(index0, 2);
        const auto cross0 = edge11 * edge22 - edge12 * edge21;
        const auto cross1 = edge12 * edge20 - edge10 * edge22;
        const auto cross2 = edge10 * edge21 - edge11 * edge20;
        if (cross0 == 0.0 && cross1 == 0.0 && cross2 == 0.0) {
            return core::unexpected(
                referenceError("Portable Mesh triangle has zero area", &reference));
        }
    }

    std::array<float, 3> minimum{mesh.positions[0], mesh.positions[1], mesh.positions[2]};
    auto maximum = minimum;
    for (std::size_t index = 0; index < vertexCount; ++index) {
        for (std::size_t component = 0; component < 3; ++component) {
            const auto value = mesh.positions[index * 3U + component];
            minimum[component] = std::min(minimum[component], value);
            maximum[component] = std::max(maximum[component], value);
        }
    }
    for (std::size_t component = 0; component < 3; ++component) {
        if (!std::isfinite(mesh.boundsMin[component]) ||
            !std::isfinite(mesh.boundsMax[component]) ||
            mesh.boundsMin[component] != minimum[component] ||
            mesh.boundsMax[component] != maximum[component]) {
            return core::unexpected(
                referenceError("Portable Mesh derived bounds are invalid", &reference));
        }
    }

    std::uint64_t positionBytes{};
    std::uint64_t uvBytes{};
    std::uint64_t indexBytes{};
    if (!checkedMultiply(mesh.positions.size(), sizeof(float), positionBytes) ||
        !checkedMultiply(mesh.uv0.size(), sizeof(float), uvBytes) ||
        !checkedMultiply(mesh.indices.size(), sizeof(std::uint32_t), indexBytes) ||
        !checkedAdd(positionBytes, uvBytes, decodedBytes) ||
        !checkedAdd(decodedBytes, indexBytes, decodedBytes) ||
        !checkedAdd(decodedBytes, 24ULL, decodedBytes)) {
        return core::unexpected(referenceError("Portable Mesh byte count overflowed", &reference));
    }
    return {};
}

[[nodiscard]] auto validateTexture(const playback::PresentationResourceRef& reference,
                                   const playback::PortableTexture2D& texture,
                                   std::uint64_t& decodedBytes) -> core::Result<void> {
    if (texture.width == 0 || texture.height == 0 || texture.width > maxTextureDimension ||
        texture.height > maxTextureDimension ||
        (texture.colorSpace != playback::PresentationColorSpace::Linear &&
         texture.colorSpace != playback::PresentationColorSpace::Srgb)) {
        return core::unexpected(
            referenceError("Portable Texture2D metadata is invalid", &reference));
    }
    std::uint64_t pixelCount{};
    std::uint64_t pixelBytes{};
    if (!checkedMultiply(texture.width, texture.height, pixelCount) ||
        !checkedMultiply(pixelCount, 4ULL, pixelBytes) ||
        pixelBytes != texture.pixelsRgba8.size() || !checkedAdd(pixelBytes, 12ULL, decodedBytes)) {
        return core::unexpected(
            referenceError("Portable Texture2D byte count is invalid", &reference));
    }
    return {};
}

[[nodiscard]] auto validateMaterial(const playback::PresentationResourceRef& reference,
                                    const playback::PortableUnlitMaterial& material,
                                    std::uint64_t& decodedBytes) -> core::Result<void> {
    if (material.alphaMode != playback::PresentationAlphaMode::Opaque &&
        material.alphaMode != playback::PresentationAlphaMode::Blend) {
        return core::unexpected(
            referenceError("Portable Material alpha mode is invalid", &reference));
    }
    for (const auto value : material.baseColor) {
        if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
            return core::unexpected(
                referenceError("Portable Material color is invalid", &reference));
        }
    }
    if (material.alphaMode == playback::PresentationAlphaMode::Opaque &&
        material.baseColor[3] != 1.0F) {
        return core::unexpected(
            referenceError("Portable Opaque Material alpha must equal 1", &reference));
    }
    decodedBytes = 32;
    if (material.baseColorTexture) {
        if (material.baseColorTexture->type != playback::PresentationResourceType::Texture2D ||
            !isPortableAssetId(material.baseColorTexture->assetId)) {
            return core::unexpected(
                referenceError("Portable Material texture reference is invalid", &reference));
        }
        if (!checkedAdd(decodedBytes, 37ULL + material.baseColorTexture->assetId.size(),
                        decodedBytes)) {
            return core::unexpected(
                referenceError("Portable Material byte count overflowed", &reference));
        }
    }
    return {};
}

[[nodiscard]] auto validateShader(const playback::PresentationResourceRef& reference,
                                  const playback::PortableShader& shader,
                                  std::uint64_t& decodedBytes) -> core::Result<void> {
    decodedBytes = 48;
    const auto addString = [&](std::string_view value) -> bool {
        return checkedAdd(decodedBytes, 4ULL + value.size(), decodedBytes);
    };
    if (!addString(shader.vertexEntry) || !addString(shader.fragmentEntry) ||
        !addString(shader.requiredRendererProfile)) {
        return core::unexpected(
            referenceError("Portable Shader byte count overflowed", &reference));
    }
    for (const auto& keyword : shader.variantKeywords) {
        if (!addString(keyword)) {
            return core::unexpected(
                referenceError("Portable Shader byte count overflowed", &reference));
        }
    }
    for (const auto& parameter : shader.parameters) {
        if (!addString(parameter.name)) {
            return core::unexpected(
                referenceError("Portable Shader byte count overflowed", &reference));
        }
    }
    for (const auto& binding : shader.bindings) {
        if (!addString(binding.name)) {
            return core::unexpected(
                referenceError("Portable Shader byte count overflowed", &reference));
        }
    }
    for (const auto& extension : shader.requiredHostExtensions) {
        if (!addString(extension)) {
            return core::unexpected(
                referenceError("Portable Shader byte count overflowed", &reference));
        }
    }
    if (!checkedAdd(decodedBytes, shader.vertexSource.size() + shader.fragmentSource.size(),
                    decodedBytes)) {
        return core::unexpected(
            referenceError("Portable Shader byte count overflowed", &reference));
    }
    return {};
}

[[nodiscard]] auto
validateParameterizedMaterial(const playback::PresentationResourceRef& reference,
                              const playback::PortableParameterizedMaterial& material,
                              std::uint64_t& decodedBytes) -> core::Result<void> {
    if (material.alphaMode != playback::PresentationAlphaMode::Opaque &&
        material.alphaMode != playback::PresentationAlphaMode::Blend) {
        return core::unexpected(
            referenceError("Portable Material alpha mode is invalid", &reference));
    }
    if (material.shader.type != playback::PresentationResourceType::Shader ||
        !isPortableAssetId(material.shader.assetId)) {
        return core::unexpected(
            referenceError("Parameterized Material shader reference is invalid", &reference));
    }
    decodedBytes = 24ULL + 37ULL + material.shader.assetId.size();
    for (const auto& keyword : material.selectedKeywords) {
        if (!checkedAdd(decodedBytes, 4ULL + keyword.size(), decodedBytes)) {
            return core::unexpected(
                referenceError("Parameterized Material byte count overflowed", &reference));
        }
    }
    for (const auto& parameter : material.parameters) {
        if (parameter.type == playback::ShaderParameterType::Texture2D) {
            if (!parameter.texture ||
                parameter.texture->type != playback::PresentationResourceType::Texture2D ||
                !isPortableAssetId(parameter.texture->assetId)) {
                return core::unexpected(referenceError(
                    "Parameterized Material texture parameter is invalid", &reference));
            }
            if (!checkedAdd(decodedBytes,
                            8ULL + parameter.name.size() + 37ULL +
                                parameter.texture->assetId.size(),
                            decodedBytes)) {
                return core::unexpected(
                    referenceError("Parameterized Material byte count overflowed", &reference));
            }
        } else if (!checkedAdd(decodedBytes, 20ULL + parameter.name.size(), decodedBytes)) {
            return core::unexpected(
                referenceError("Parameterized Material byte count overflowed", &reference));
        }
    }
    return {};
}

[[nodiscard]] auto
expectedParameterizedDependencies(const playback::PortableParameterizedMaterial& material)
    -> std::vector<playback::PresentationResourceRef> {
    std::vector<playback::PresentationResourceRef> dependencies;
    dependencies.push_back(material.shader);
    for (const auto& parameter : material.parameters) {
        if (parameter.type == playback::ShaderParameterType::Texture2D && parameter.texture) {
            dependencies.push_back(*parameter.texture);
        }
    }
    return dependencies;
}

struct CanonicalPresentationData final {
    playback::PresentationResourceManifest manifest;
    std::vector<playback::PortableResourcePtr> resources;
};

[[nodiscard]] auto
canonicalizePresentationData(const playback::PresentationResourceManifest& manifest,
                             std::span<const playback::PortableResourcePtr> resources)
    -> core::Result<CanonicalPresentationData> {
    if (manifest.version != 1) {
        return core::unexpected(referenceError("Presentation manifest version is unsupported"));
    }
    if (manifest.entries.size() > maxManifestEntries ||
        resources.size() != manifest.entries.size()) {
        return core::unexpected(referenceError("Presentation manifest/resource count is invalid"));
    }

    CanonicalPresentationData canonical{
        manifest, std::vector<playback::PortableResourcePtr>(resources.begin(), resources.end())};
    for (std::size_t index = 0; index < canonical.manifest.entries.size(); ++index) {
        const auto& entry = canonical.manifest.entries[index];
        if (!isPortableAssetId(entry.reference.assetId)) {
            return core::unexpected(
                referenceError("Presentation AssetId is not portable", &entry.reference));
        }
        if (index != 0 && !(referenceKey(canonical.manifest.entries[index - 1].reference) <
                            referenceKey(entry.reference))) {
            return core::unexpected(referenceError(
                "Presentation manifest entries are not in canonical order", &entry.reference));
        }
        if (index != 0 &&
            canonical.manifest.entries[index - 1].reference.assetId == entry.reference.assetId) {
            return core::unexpected(referenceError(
                "Presentation manifest contains a duplicate AssetId", &entry.reference));
        }
        const auto& resource = canonical.resources[index];
        if (!resource) {
            return core::unexpected(referenceError("Presentation resource pointer is null"));
        }
        if (resource->reference != entry.reference) {
            return core::unexpected(referenceError(
                "Presentation resource order does not match the manifest", &resource->reference));
        }
    }

    std::uint64_t totalEncodedBytes{};
    std::uint64_t totalResourceDecodedBytes{};
    std::uint64_t manifestBytes = 24;
    for (std::size_t index = 0; index < canonical.manifest.entries.size(); ++index) {
        const auto& entry = canonical.manifest.entries[index];
        const auto& resource = canonical.resources[index];
        if (!resource) {
            return core::unexpected(
                referenceError("Presentation manifest resource is missing", &entry.reference));
        }

        std::uint64_t decodedBytes{};
        playback::PresentationContentIdentity computedIdentity;
        switch (entry.reference.type) {
        case playback::PresentationResourceType::Mesh: {
            const auto* value = std::get_if<playback::PortableMesh>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Mesh ref has an incompatible value", &entry.reference));
            }
            if (auto valid = validateMesh(entry.reference, *value, decodedBytes); !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = meshIdentity(*value);
            if (!entry.dependencies.empty()) {
                return core::unexpected(
                    referenceError("Portable Mesh has unexpected dependencies", &entry.reference));
            }
            break;
        }
        case playback::PresentationResourceType::Texture2D: {
            const auto* value = std::get_if<playback::PortableTexture2D>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Texture2D ref has an incompatible value", &entry.reference));
            }
            if (auto valid = validateTexture(entry.reference, *value, decodedBytes); !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = textureIdentity(*value);
            if (!entry.dependencies.empty()) {
                return core::unexpected(referenceError(
                    "Portable Texture2D has unexpected dependencies", &entry.reference));
            }
            break;
        }
        case playback::PresentationResourceType::UnlitMaterial: {
            const auto* value = std::get_if<playback::PortableUnlitMaterial>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Material ref has an incompatible value", &entry.reference));
            }
            if (auto valid = validateMaterial(entry.reference, *value, decodedBytes); !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = materialIdentity(*value);
            if (value->baseColorTexture) {
                if (entry.dependencies.size() != 1 ||
                    entry.dependencies.front() != *value->baseColorTexture) {
                    return core::unexpected(referenceError(
                        "Portable Material dependency does not match its value", &entry.reference));
                }
                const auto dependency = std::lower_bound(
                    canonical.manifest.entries.begin(), canonical.manifest.entries.end(),
                    *value->baseColorTexture, [](const auto& candidate, const auto& reference) {
                        return referenceKey(candidate.reference) < referenceKey(reference);
                    });
                if (dependency == canonical.manifest.entries.end() ||
                    dependency->reference != *value->baseColorTexture) {
                    return core::unexpected(referenceError(
                        "Portable Material texture dependency is missing", &entry.reference));
                }
            } else if (!entry.dependencies.empty()) {
                return core::unexpected(referenceError(
                    "Portable Material has unexpected dependencies", &entry.reference));
            }
            break;
        }
        case playback::PresentationResourceType::Shader: {
            const auto* value = std::get_if<playback::PortableShader>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Shader ref has an incompatible value", &entry.reference));
            }
            if (auto valid = validateShader(entry.reference, *value, decodedBytes); !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = computePresentationIdentity(resource->value);
            if (!entry.dependencies.empty()) {
                return core::unexpected(referenceError(
                    "Portable Shader has unexpected dependencies", &entry.reference));
            }
            break;
        }
        case playback::PresentationResourceType::ParameterizedMaterial: {
            const auto* value =
                std::get_if<playback::PortableParameterizedMaterial>(&resource->value);
            if (value == nullptr) {
                return core::unexpected(referenceError(
                    "Presentation Parameterized Material ref has an incompatible value",
                    &entry.reference));
            }
            if (auto valid = validateParameterizedMaterial(entry.reference, *value, decodedBytes);
                !valid) {
                return core::unexpected(std::move(valid.error()));
            }
            computedIdentity = computePresentationIdentity(resource->value);
            const auto expected = expectedParameterizedDependencies(*value);
            if (entry.dependencies != expected) {
                return core::unexpected(
                    referenceError("Parameterized Material dependency does not match its value",
                                   &entry.reference));
            }
            for (const auto& dependency : expected) {
                const auto found = std::lower_bound(
                    canonical.manifest.entries.begin(), canonical.manifest.entries.end(),
                    dependency, [](const auto& candidate, const auto& reference) {
                        return referenceKey(candidate.reference) < referenceKey(reference);
                    });
                if (found == canonical.manifest.entries.end() || found->reference != dependency) {
                    return core::unexpected(referenceError(
                        "Parameterized Material dependency is missing", &entry.reference));
                }
            }
            break;
        }
        }

        if (computedIdentity != entry.reference.identity ||
            resource->reference != entry.reference) {
            return core::unexpected(referenceError(
                "Presentation resource semantic identity does not match its reference",
                &entry.reference));
        }
        if (entry.decodedByteCount != decodedBytes || entry.encodedByteCount > maxResourceBytes ||
            decodedBytes > maxResourceBytes) {
            return core::unexpected(
                core::Error{"playback.presentation.resource.budget_exceeded",
                            "Validation Sink resource byte budget is inconsistent"}
                    .withContext("asset_id", entry.reference.assetId)
                    .withContext("resource_type",
                                 std::string{resourceTypeName(entry.reference.type)})
                    .withContext("limit", std::to_string(maxResourceBytes))
                    .withContext("actual", std::to_string(decodedBytes)));
        }
        if (!checkedAdd(totalEncodedBytes, entry.encodedByteCount, totalEncodedBytes) ||
            !checkedAdd(totalResourceDecodedBytes, decodedBytes, totalResourceDecodedBytes)) {
            return core::unexpected(core::Error{"playback.presentation.session.budget_exceeded",
                                                "Validation Sink resource totals overflowed"});
        }

        std::uint64_t entryBytes = 20;
        std::uint64_t refBytes{};
        if (!referenceByteCount(entry.reference, refBytes) ||
            !checkedAdd(entryBytes, refBytes, entryBytes)) {
            return core::unexpected(
                referenceError("Presentation manifest byte count overflowed", &entry.reference));
        }
        for (const auto& dependency : entry.dependencies) {
            if (!referenceByteCount(dependency, refBytes) ||
                !checkedAdd(entryBytes, refBytes, entryBytes)) {
                return core::unexpected(referenceError(
                    "Presentation dependency byte count overflowed", &entry.reference));
            }
        }
        if (!checkedAdd(manifestBytes, entryBytes, manifestBytes)) {
            return core::unexpected(
                referenceError("Presentation manifest total overflowed", &entry.reference));
        }
    }

    std::uint64_t totalDecodedBytes{};
    if (!checkedAdd(totalResourceDecodedBytes, manifestBytes, totalDecodedBytes) ||
        totalEncodedBytes != canonical.manifest.totalEncodedBytes ||
        totalDecodedBytes != canonical.manifest.totalDecodedBytes ||
        totalEncodedBytes > maxSessionBytes || totalDecodedBytes > maxSessionBytes) {
        return core::unexpected(core::Error{"playback.presentation.session.budget_exceeded",
                                            "Validation Sink manifest totals are inconsistent"}
                                    .withContext("limit", std::to_string(maxSessionBytes))
                                    .withContext("actual", std::to_string(totalDecodedBytes)));
    }
    return canonical;
}

struct LocatedResource final {
    const playback::PresentationManifestEntry* entry{};
    const playback::PortableResource* resource{};
};

[[nodiscard]] auto locateResource(const ValidationCandidate& candidate,
                                  const playback::PresentationResourceRef& reference) noexcept
    -> std::optional<LocatedResource> {
    const auto& manifest = candidate.manifest();
    const auto resources = candidate.resources();
    const auto found = std::lower_bound(manifest.entries.begin(), manifest.entries.end(), reference,
                                        [](const auto& entry, const auto& candidateReference) {
                                            return referenceKey(entry.reference) <
                                                   referenceKey(candidateReference);
                                        });
    if (found == manifest.entries.end() || found->reference != reference) {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(found - manifest.entries.begin());
    if (index >= resources.size() || !resources[index] ||
        resources[index]->reference != reference) {
        return std::nullopt;
    }
    return LocatedResource{&*found, resources[index].get()};
}

[[nodiscard]] auto finiteMatrix(const float (&matrix)[16]) noexcept -> bool {
    return std::all_of(std::begin(matrix), std::end(matrix),
                       [](float value) { return std::isfinite(value); });
}

struct Point3 final {
    double x{};
    double y{};
    double z{};
};

[[nodiscard]] auto transformPoint(const float (&matrix)[16], const Point3& point) noexcept
    -> Point3 {
    return Point3{
        static_cast<double>(matrix[0]) * point.x + static_cast<double>(matrix[4]) * point.y +
            static_cast<double>(matrix[8]) * point.z + static_cast<double>(matrix[12]),
        static_cast<double>(matrix[1]) * point.x + static_cast<double>(matrix[5]) * point.y +
            static_cast<double>(matrix[9]) * point.z + static_cast<double>(matrix[13]),
        static_cast<double>(matrix[2]) * point.x + static_cast<double>(matrix[6]) * point.y +
            static_cast<double>(matrix[10]) * point.z + static_cast<double>(matrix[14])};
}

[[nodiscard]] auto finitePoint(const Point3& point) noexcept -> bool {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

class SummaryHash final {
  public:
    SummaryHash() noexcept {
        static constexpr char domain[] = "cuexis.validation.summary.v1";
        writeBytes(std::as_bytes(std::span{domain, sizeof(domain)}));
    }

    void writeU8(std::uint8_t value) noexcept {
        value_ ^= value;
        value_ *= 1099511628211ULL;
    }

    void writeU32(std::uint32_t value) noexcept {
        for (std::size_t index = 0; index < 4; ++index) {
            writeU8(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
        }
    }

    void writeU64(std::uint64_t value) noexcept {
        for (std::size_t index = 0; index < 8; ++index) {
            writeU8(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
        }
    }

    void writeI64(std::int64_t value) noexcept {
        writeU64(std::bit_cast<std::uint64_t>(value));
    }

    void writeBool(bool value) noexcept {
        writeU8(value ? 1U : 0U);
    }

    void writeFloat(float value) noexcept {
        if (value == 0.0F) {
            value = 0.0F;
        }
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeDouble(double value) noexcept {
        if (value == 0.0) {
            value = 0.0;
        }
        writeU64(std::bit_cast<std::uint64_t>(value));
    }

    void writeBytes(std::span<const std::byte> bytes) noexcept {
        for (const auto value : bytes) {
            writeU8(std::to_integer<std::uint8_t>(value));
        }
    }

    void writeString(std::string_view value) noexcept {
        writeU32(static_cast<std::uint32_t>(value.size()));
        writeBytes(std::as_bytes(std::span{value.data(), value.size()}));
    }

    void writeReference(const playback::PresentationResourceRef& reference) noexcept {
        writeU32(static_cast<std::uint32_t>(reference.type));
        writeString(reference.assetId);
        writeBytes(std::as_bytes(std::span{reference.identity.sha256}));
    }

    [[nodiscard]] std::uint64_t value() const noexcept {
        return value_;
    }

  private:
    std::uint64_t value_{14695981039346656037ULL};
};

void hashCommand(SummaryHash& hash, const ValidationCommand& command) noexcept {
    hash.writeString(command.objectId);
    for (const auto value : command.worldMatrix) {
        hash.writeFloat(value);
    }
    hash.writeReference(command.mesh);
    hash.writeReference(command.material);
    for (const auto value : command.effectiveColor) {
        hash.writeDouble(value);
    }
    hash.writeU8(static_cast<std::uint8_t>(command.pass));
    hash.writeBool(command.backFaceCulling);
    hash.writeBool(command.depthTest);
    hash.writeBool(command.depthWrite);
    hash.writeBool(command.sourceOverBlend);
    hash.writeDouble(command.depthMeters);
    hash.writeI64(command.transparentDepthKey);
}

[[nodiscard]] auto summaryDigest(const ValidationSummary& summary) noexcept -> std::uint64_t {
    SummaryHash hash;
    hash.writeU32(summary.version);
    hash.writeU32(summary.viewportWidth);
    hash.writeU32(summary.viewportHeight);
    for (const auto value : summary.clearColor) {
        hash.writeFloat(value);
    }
    hash.writeBool(summary.cameraActive);
    for (const auto value : summary.viewMatrix) {
        hash.writeFloat(value);
    }
    for (const auto value : summary.projectionMatrix) {
        hash.writeFloat(value);
    }
    hash.writeBool(summary.debugPassEnabled);
    hash.writeU32(static_cast<std::uint32_t>(summary.opaque.size()));
    for (const auto& command : summary.opaque) {
        hashCommand(hash, command);
    }
    hash.writeU32(static_cast<std::uint32_t>(summary.transparent.size()));
    for (const auto& command : summary.transparent) {
        hashCommand(hash, command);
    }
    return hash.value();
}

void assignReference(playback::PresentationResourceRef& destination,
                     const playback::PresentationResourceRef& source) {
    destination.type = source.type;
    destination.assetId = source.assetId;
    destination.identity = source.identity;
}

[[nodiscard]] auto nextCommand(std::vector<ValidationCommand>& commands, std::size_t& count)
    -> ValidationCommand& {
    if (count == commands.size()) {
        commands.emplace_back();
    }
    return commands[count++];
}

} // namespace

auto computePresentationIdentity(const playback::PortableResourceValue& value) noexcept
    -> playback::PresentationContentIdentity {
    if (const auto* mesh = std::get_if<playback::PortableMesh>(&value)) {
        return meshIdentity(*mesh);
    }
    if (const auto* texture = std::get_if<playback::PortableTexture2D>(&value)) {
        return textureIdentity(*texture);
    }
    if (const auto* unlit = std::get_if<playback::PortableUnlitMaterial>(&value)) {
        return materialIdentity(*unlit);
    }
    if (const auto* shader = std::get_if<playback::PortableShader>(&value)) {
        CanonicalHash hash{playback::PresentationResourceType::Shader};
        hash.writeString(shader->vertexEntry);
        hash.writeString(shader->fragmentEntry);
        auto keywords = shader->variantKeywords;
        std::sort(keywords.begin(), keywords.end());
        hash.writeU32(static_cast<std::uint32_t>(keywords.size()));
        for (const auto& keyword : keywords) {
            hash.writeString(keyword);
        }
        hash.writeU32(static_cast<std::uint32_t>(shader->parameters.size()));
        for (const auto& parameter : shader->parameters) {
            hash.writeString(parameter.name);
            hash.writeU32(static_cast<std::uint32_t>(parameter.type));
            hash.writeU32(parameter.set);
            hash.writeU32(parameter.binding);
            for (const auto lane : parameter.defaultNumeric) {
                hash.writeFloat(lane);
            }
            hash.writeU32(std::bit_cast<std::uint32_t>(parameter.defaultInt));
            hash.writeU32(parameter.defaultBool ? 1U : 0U);
        }
        hash.writeU32(static_cast<std::uint32_t>(shader->bindings.size()));
        for (const auto& binding : shader->bindings) {
            hash.writeU32(binding.set);
            hash.writeU32(binding.binding);
            hash.writeU32(static_cast<std::uint32_t>(binding.type));
            hash.writeString(binding.name);
        }
        hash.writeU32(static_cast<std::uint32_t>(shader->defaultAlphaMode));
        hash.writeU32(shader->defaultDoubleSided ? 1U : 0U);
        hash.writeString(shader->requiredRendererProfile);
        auto extensions = shader->requiredHostExtensions;
        std::sort(extensions.begin(), extensions.end());
        hash.writeU32(static_cast<std::uint32_t>(extensions.size()));
        for (const auto& extension : extensions) {
            hash.writeString(extension);
        }
        hash.writeString(shader->vertexSource);
        hash.writeString(shader->fragmentSource);
        return hash.finish();
    }
    const auto& material = std::get<playback::PortableParameterizedMaterial>(value);
    CanonicalHash hash{playback::PresentationResourceType::ParameterizedMaterial};
    hash.writeU32(static_cast<std::uint32_t>(material.alphaMode));
    hash.writeU32(material.doubleSided ? 1U : 0U);
    hash.writeString(material.shader.assetId);
    hash.writeIdentity(material.shader.identity);
    auto keywords = material.selectedKeywords;
    std::sort(keywords.begin(), keywords.end());
    hash.writeU32(static_cast<std::uint32_t>(keywords.size()));
    for (const auto& keyword : keywords) {
        hash.writeString(keyword);
    }
    hash.writeU32(static_cast<std::uint32_t>(material.parameters.size()));
    for (const auto& parameter : material.parameters) {
        hash.writeString(parameter.name);
        hash.writeU32(static_cast<std::uint32_t>(parameter.type));
        if (parameter.type == playback::ShaderParameterType::Texture2D) {
            hash.writeU32(parameter.texture ? 1U : 0U);
            if (parameter.texture) {
                hash.writeString(parameter.texture->assetId);
                hash.writeIdentity(parameter.texture->identity);
            }
        } else if (parameter.type == playback::ShaderParameterType::Int) {
            hash.writeU32(std::bit_cast<std::uint32_t>(parameter.integer));
        } else if (parameter.type == playback::ShaderParameterType::Bool) {
            hash.writeU32(parameter.boolean ? 1U : 0U);
        } else {
            for (const auto lane : parameter.numeric) {
                hash.writeFloat(lane);
            }
        }
    }
    return hash.finish();
}

void ValidationSummary::clear() noexcept {
    version = validationSummaryVersion;
    viewportWidth = 0;
    viewportHeight = 0;
    clearColor.fill(0.0F);
    cameraActive = false;
    viewMatrix.fill(0.0F);
    projectionMatrix.fill(0.0F);
    debugPassEnabled = false;
    opaque.clear();
    transparent.clear();
    digest = 0;
}

ValidationCandidate::ValidationCandidate(playback::PresentationCandidateToken token,
                                         playback::PresentationResourceManifest manifest,
                                         std::vector<playback::PortableResourcePtr> resources,
                                         playback::EffectivePresentationSettings settings) noexcept
    : token_(std::move(token)), manifest_(std::move(manifest)), resources_(std::move(resources)),
      settings_(settings) {}

auto ValidationCandidate::token() const noexcept -> const playback::PresentationCandidateToken& {
    return token_;
}

auto ValidationCandidate::manifest() const noexcept
    -> const playback::PresentationResourceManifest& {
    return manifest_;
}

auto ValidationCandidate::resources() const noexcept
    -> std::span<const playback::PortableResourcePtr> {
    return resources_;
}

auto ValidationCandidate::settings() const noexcept
    -> const playback::EffectivePresentationSettings& {
    return settings_;
}

auto validatePresentationData(const playback::PresentationResourceManifest& manifest,
                              std::span<const playback::PortableResourcePtr> resources)
    -> core::Result<void> {
    try {
        auto canonical = canonicalizePresentationData(manifest, resources);
        if (!canonical) {
            return core::unexpected(std::move(canonical.error()));
        }
        return {};
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"playback.presentation.session.budget_exceeded",
                        "Validation Sink candidate allocation could not be satisfied"}
                .withContext("limit", std::to_string(maxSessionBytes))
                .withContext("actual", "allocation_failed"));
    } catch (const std::exception& exception) {
        return core::unexpected(referenceError("Validation Sink candidate validation failed")
                                    .withContext("exception", exception.what()));
    } catch (...) {
        return core::unexpected(referenceError("Validation Sink candidate validation failed"));
    }
}

auto prepareValidationCandidate(playback::PreparedPlayback& prepared,
                                const playback::PresentationCapabilities& capabilities,
                                const playback::PresentationRequest& request)
    -> ValidationCandidateResult {
    ValidationCandidateResult result;
    try {
        auto validation = prepared.validatePresentation(capabilities, request);
        result.diagnostics = std::move(validation.diagnostics);
        if (!validation.hasValue()) {
            return result;
        }

        const auto* manifest = prepared.presentationManifest();
        if (manifest == nullptr) {
            addError(result.diagnostics,
                     core::Error{"playback.presentation.resource.missing",
                                 "Validation Sink requires a portable presentation candidate"});
            return result;
        }
        auto token = prepared.presentationCandidateToken();
        if (!token) {
            addError(result.diagnostics, token.error());
            return result;
        }

        std::vector<playback::PortableResourcePtr> resources;
        resources.reserve(manifest->entries.size());
        for (const auto& entry : manifest->entries) {
            auto resource = prepared.acquirePresentationResource(entry.reference);
            if (!resource) {
                addError(result.diagnostics, resource.error());
                return result;
            }
            resources.push_back(std::move(*resource));
        }
        auto canonical = canonicalizePresentationData(*manifest, resources);
        if (!canonical) {
            addError(result.diagnostics, canonical.error());
            return result;
        }

        result.candidate =
            ValidationCandidate{std::move(*token), std::move(canonical->manifest),
                                std::move(canonical->resources), *validation.settings};
        return result;
    } catch (const std::bad_alloc&) {
        result.candidate.reset();
        if (result.diagnostics.empty()) {
            result.diagnostics = diagnostics();
        }
        addError(result.diagnostics,
                 core::Error{"playback.presentation.session.budget_exceeded",
                             "Validation Sink candidate allocation could not be satisfied"}
                     .withContext("limit", std::to_string(maxSessionBytes))
                     .withContext("actual", "allocation_failed"));
        return result;
    } catch (const std::exception& exception) {
        result.candidate.reset();
        if (result.diagnostics.empty()) {
            result.diagnostics = diagnostics();
        }
        addError(result.diagnostics, referenceError("Validation Sink candidate preparation failed")
                                         .withContext("exception", exception.what()));
        return result;
    } catch (...) {
        result.candidate.reset();
        if (result.diagnostics.empty()) {
            result.diagnostics = diagnostics();
        }
        addError(result.diagnostics,
                 referenceError("Validation Sink candidate preparation failed"));
        return result;
    }
}

void ValidationSink::activate(ValidationCandidate&& candidate) noexcept {
    active_.emplace(std::move(candidate));
}

bool ValidationSink::active() const noexcept {
    return active_.has_value();
}

auto ValidationSink::activeToken() const noexcept -> const playback::PresentationCandidateToken* {
    return active_ ? &active_->token() : nullptr;
}

auto ValidationSink::validateFrame(const playback::FrameSnapshot& snapshot,
                                   ValidationSummary& destination) const -> core::Result<void> {
    const auto fail = [&](core::Error error) -> core::Result<void> {
        destination.clear();
        return core::unexpected(std::move(error));
    };
    try {
        if (!active_) {
            return fail(core::Error{"playback.presentation.resource.missing",
                                    "Validation Sink has no active presentation candidate"});
        }
        if (snapshot.objects.size() > maxNormalizedRecords) {
            return fail(core::Error{"playback.presentation.frame.command_budget_exceeded",
                                    "Validation Sink command count exceeds the v1 limit"}
                            .withContext("limit", std::to_string(maxNormalizedRecords))
                            .withContext("actual", std::to_string(snapshot.objects.size())));
        }
        const std::array clearColor{snapshot.clearRed, snapshot.clearGreen, snapshot.clearBlue,
                                    snapshot.clearAlpha};
        if (!std::all_of(clearColor.begin(), clearColor.end(),
                         [](float value) { return std::isfinite(value); })) {
            return fail(nonFiniteError({}, "clear_color"));
        }

        destination.version = validationSummaryVersion;
        destination.viewportWidth = snapshot.viewportWidth;
        destination.viewportHeight = snapshot.viewportHeight;
        destination.clearColor = clearColor;
        destination.cameraActive = snapshot.camera.active;
        std::copy(std::begin(snapshot.camera.viewMatrix), std::end(snapshot.camera.viewMatrix),
                  destination.viewMatrix.begin());
        std::copy(std::begin(snapshot.camera.projectionMatrix),
                  std::end(snapshot.camera.projectionMatrix), destination.projectionMatrix.begin());
        destination.debugPassEnabled = active_->settings().debugPassEnabled;
        destination.digest = 0;
        if (destination.opaque.capacity() < snapshot.objects.size()) {
            destination.opaque.reserve(snapshot.objects.size());
        }
        if (destination.transparent.capacity() < snapshot.objects.size()) {
            destination.transparent.reserve(snapshot.objects.size());
        }

        std::size_t opaqueCount = 0;
        std::size_t transparentCount = 0;
        bool cameraValidated = false;
        for (std::size_t objectIndex = 0; objectIndex < snapshot.objects.size(); ++objectIndex) {
            const auto& object = snapshot.objects[objectIndex];
            if (object.mesh.has_value() != object.material.has_value()) {
                const auto* reference = object.mesh ? &*object.mesh : &*object.material;
                return fail(frameResourceError("Renderable Mesh and Material refs must be paired",
                                               object.id, reference));
            }
            if (!object.mesh) {
                if (!object.materialAssetId.empty()) {
                    return fail(frameResourceError(
                        "Renderable snapshot is missing portable resource refs", object.id));
                }
                continue;
            }
            if (object.mesh->type != playback::PresentationResourceType::Mesh ||
                (object.material->type != playback::PresentationResourceType::UnlitMaterial &&
                 object.material->type !=
                     playback::PresentationResourceType::ParameterizedMaterial) ||
                object.materialAssetId != object.material->assetId) {
                return fail(
                    frameResourceError("Snapshot portable refs have incompatible types or AssetIds",
                                       object.id, &*object.material));
            }

            const auto meshResource = locateResource(*active_, *object.mesh);
            const auto materialResource = locateResource(*active_, *object.material);
            if (!meshResource || !materialResource ||
                !std::holds_alternative<playback::PortableMesh>(meshResource->resource->value)) {
                return fail(frameResourceError(
                    "Snapshot ref is not backed by its active portable resource", object.id,
                    !meshResource ? &*object.mesh : &*object.material));
            }
            const auto& mesh = std::get<playback::PortableMesh>(meshResource->resource->value);
            const auto* unlit =
                std::get_if<playback::PortableUnlitMaterial>(&materialResource->resource->value);
            const auto* parameterized = std::get_if<playback::PortableParameterizedMaterial>(
                &materialResource->resource->value);
            if (unlit == nullptr && parameterized == nullptr) {
                return fail(
                    frameResourceError("Snapshot ref is not backed by its active portable resource",
                                       object.id, &*object.material));
            }
            if (unlit != nullptr) {
                if (unlit->baseColorTexture) {
                    const auto texture = locateResource(*active_, *unlit->baseColorTexture);
                    if (!texture ||
                        !std::holds_alternative<playback::PortableTexture2D>(
                            texture->resource->value) ||
                        materialResource->entry->dependencies.size() != 1 ||
                        materialResource->entry->dependencies.front() != *unlit->baseColorTexture) {
                        return fail(frameResourceError(
                            "Portable Material texture dependency is inconsistent", object.id,
                            &*unlit->baseColorTexture));
                    }
                } else if (!materialResource->entry->dependencies.empty()) {
                    return fail(frameResourceError("Portable Material has unexpected dependencies",
                                                   object.id, &*object.material));
                }
            } else {
                const auto expected = expectedParameterizedDependencies(*parameterized);
                if (materialResource->entry->dependencies != expected) {
                    return fail(
                        frameResourceError("Parameterized Material dependency is inconsistent",
                                           object.id, &*object.material));
                }
                for (const auto& dependency : expected) {
                    const auto located = locateResource(*active_, dependency);
                    if (!located) {
                        return fail(frameResourceError(
                            "Parameterized Material dependency is missing from the active cache",
                            object.id, &dependency));
                    }
                }
            }

            if (!object.visible) {
                continue;
            }
            if (!snapshot.camera.active) {
                return fail(core::Error{"playback.presentation.frame.camera_required",
                                        "Visible renderables require an active camera"}
                                .withContext("object_id", object.id));
            }
            if (!cameraValidated) {
                if (!finiteMatrix(snapshot.camera.viewMatrix) ||
                    !finiteMatrix(snapshot.camera.projectionMatrix)) {
                    return fail(nonFiniteError({}, "camera_matrix"));
                }
                cameraValidated = true;
            }
            if (!finiteMatrix(object.worldMatrix)) {
                return fail(nonFiniteError(object.id, "world_matrix"));
            }
            if (!std::isfinite(object.materialOpacity)) {
                return fail(nonFiniteError(object.id, "material_opacity"));
            }
            if (object.materialOpacity < 0.0 || object.materialOpacity > 1.0) {
                return fail(frameValueError(object.id, "material_opacity"));
            }

            const float* baseColor = unlit != nullptr ? unlit->baseColor : nullptr;
            const float parameterizedBase[4]{1.0F, 1.0F, 1.0F, 1.0F};
            if (baseColor == nullptr) {
                baseColor = parameterizedBase;
            }
            const auto alphaMode = unlit != nullptr ? unlit->alphaMode : parameterized->alphaMode;
            const bool doubleSided =
                unlit != nullptr ? unlit->doubleSided : parameterized->doubleSided;

            std::array<double, 4> effectiveColor{};
            for (std::size_t component = 0; component < 3; ++component) {
                if (!std::isfinite(object.materialTint[component])) {
                    return fail(nonFiniteError(object.id, "material_tint"));
                }
                if (object.materialTint[component] < 0.0F ||
                    object.materialTint[component] > 1.0F) {
                    return fail(frameValueError(object.id, "material_tint"));
                }
                effectiveColor[component] = static_cast<double>(baseColor[component]) *
                                            static_cast<double>(object.materialTint[component]);
                if (!std::isfinite(effectiveColor[component])) {
                    return fail(nonFiniteError(object.id, "effective_rgb"));
                }
            }
            effectiveColor[3] = static_cast<double>(baseColor[3]) * object.materialOpacity;
            if (!std::isfinite(effectiveColor[3])) {
                return fail(nonFiniteError(object.id, "effective_alpha"));
            }

            const Point3 localCenter{
                (static_cast<double>(mesh.boundsMin[0]) + static_cast<double>(mesh.boundsMax[0])) /
                    2.0,
                (static_cast<double>(mesh.boundsMin[1]) + static_cast<double>(mesh.boundsMax[1])) /
                    2.0,
                (static_cast<double>(mesh.boundsMin[2]) + static_cast<double>(mesh.boundsMax[2])) /
                    2.0};
            const auto worldCenter = transformPoint(object.worldMatrix, localCenter);
            const auto viewCenter = transformPoint(snapshot.camera.viewMatrix, worldCenter);
            if (!finitePoint(localCenter) || !finitePoint(worldCenter) ||
                !finitePoint(viewCenter)) {
                return fail(nonFiniteError(object.id, "depth_transform"));
            }
            const auto depthMeters = -viewCenter.z;
            const auto scaledDepth = depthMeters * depthQuantization;
            const auto roundedDepth = std::round(scaledDepth);
            if (!std::isfinite(depthMeters) || !std::isfinite(scaledDepth) ||
                !std::isfinite(roundedDepth) || roundedDepth < -signedIntegerLimit ||
                roundedDepth >= signedIntegerLimit) {
                return fail(nonFiniteError(object.id, "depth"));
            }
            const auto depthKey = static_cast<std::int64_t>(roundedDepth);
            const auto pass =
                alphaMode == playback::PresentationAlphaMode::Blend || effectiveColor[3] < 1.0
                    ? ValidationPass::Transparent
                    : ValidationPass::Opaque;
            auto& command = pass == ValidationPass::Opaque
                                ? nextCommand(destination.opaque, opaqueCount)
                                : nextCommand(destination.transparent, transparentCount);
            command.objectIndex = objectIndex;
            command.objectId = object.id;
            std::copy(std::begin(object.worldMatrix), std::end(object.worldMatrix),
                      command.worldMatrix.begin());
            assignReference(command.mesh, *object.mesh);
            assignReference(command.material, *object.material);
            command.effectiveColor = effectiveColor;
            command.pass = pass;
            command.backFaceCulling = !doubleSided;
            command.depthTest = true;
            command.depthWrite = pass == ValidationPass::Opaque;
            command.sourceOverBlend = pass == ValidationPass::Transparent;
            command.depthMeters = depthMeters;
            command.transparentDepthKey = depthKey;
        }

        std::sort(destination.opaque.begin(),
                  destination.opaque.begin() + static_cast<std::ptrdiff_t>(opaqueCount),
                  [](const auto& left, const auto& right) {
                      return std::tie(left.objectId, left.objectIndex) <
                             std::tie(right.objectId, right.objectIndex);
                  });
        std::sort(destination.transparent.begin(),
                  destination.transparent.begin() + static_cast<std::ptrdiff_t>(transparentCount),
                  [](const auto& left, const auto& right) {
                      if (left.transparentDepthKey != right.transparentDepthKey) {
                          return left.transparentDepthKey > right.transparentDepthKey;
                      }
                      return std::tie(left.objectId, left.objectIndex) <
                             std::tie(right.objectId, right.objectIndex);
                  });
        destination.opaque.resize(opaqueCount);
        destination.transparent.resize(transparentCount);
        destination.digest = summaryDigest(destination);
        return {};
    } catch (const std::bad_alloc&) {
        return fail(core::Error{"playback.presentation.frame.command_budget_exceeded",
                                "Validation Sink command allocation could not be satisfied"}
                        .withContext("limit", std::to_string(maxNormalizedRecords))
                        .withContext("actual", "allocation_failed"));
    } catch (const std::exception& exception) {
        return fail(frameResourceError("Validation Sink frame validation failed", {})
                        .withContext("exception", exception.what()));
    } catch (...) {
        return fail(frameResourceError("Validation Sink frame validation failed", {}));
    }
}

static_assert(std::is_nothrow_move_constructible_v<ValidationCandidate>);
static_assert(std::is_nothrow_move_assignable_v<ValidationCandidate>);

} // namespace cuexis::test_support
