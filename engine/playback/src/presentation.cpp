// Portable Presentation Profile v1 payload parsing and candidate manifest construction.

#include "presentation_internal.hpp"

#include <cuexis/assets/asset_database.hpp>
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
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::playback::detail {
namespace {

constexpr std::size_t envelopeByteCount = 24;
constexpr std::uint64_t maxResourceBytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maxSessionBytes = 512ULL * 1024ULL * 1024ULL;
constexpr std::size_t maxManifestEntries = 65'536;
constexpr std::uint32_t maxMeshVertices = 1'048'576;
constexpr std::uint32_t maxMeshIndices = 3'145'728;
constexpr std::uint32_t maxTextureDimension = 8'192;
constexpr std::uint32_t maxShaderSourceBytes = 262'144;
constexpr std::uint32_t maxShaderSourceTotalBytes = 524'288;
constexpr std::uint32_t maxShaderKeywords = 4;
constexpr std::uint32_t maxShaderParameters = 32;
constexpr std::uint32_t maxShaderBindings = 16;
constexpr std::uint32_t maxShaderHostExtensions = 8;
constexpr std::uint32_t maxMaterialTextureParameters = 8;
constexpr std::uint32_t maxIdentifierBytes = 32;
constexpr std::uint32_t maxEntryBytes = 64;
constexpr std::string_view builtinRendererProfile = rendererProfileBuiltInV1;
constexpr std::array<std::byte, 8> payloadMagic{
    std::byte{'C'}, std::byte{'X'}, std::byte{'P'}, std::byte{'R'},
    std::byte{'E'}, std::byte{'S'}, std::byte{'0'}, std::byte{'1'},
};

[[nodiscard]] auto resourceTypeName(PresentationResourceType type) noexcept -> std::string_view {
    switch (type) {
    case PresentationResourceType::Mesh:
        return "mesh";
    case PresentationResourceType::Texture2D:
        return "texture2d";
    case PresentationResourceType::UnlitMaterial:
        return "unlit_material";
    case PresentationResourceType::Shader:
        return "shader";
    case PresentationResourceType::ParameterizedMaterial:
        return "parameterized_material";
    }
    return "unknown";
}

[[nodiscard]] auto indexedType(PresentationResourceType type) noexcept -> assets::AssetType {
    switch (type) {
    case PresentationResourceType::Mesh:
        return assets::AssetType::Mesh;
    case PresentationResourceType::Texture2D:
        return assets::AssetType::Texture;
    case PresentationResourceType::UnlitMaterial:
        return assets::AssetType::Material;
    case PresentationResourceType::Shader:
        return assets::AssetType::Shader;
    case PresentationResourceType::ParameterizedMaterial:
        return assets::AssetType::Material;
    }
    return assets::AssetType::Mesh;
}

[[nodiscard]] auto payloadKind(PresentationResourceType type) noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(type);
}

[[nodiscard]] auto resourceError(std::string code, std::string message, std::string_view assetId,
                                 PresentationResourceType type) -> core::Error {
    return core::Error{std::move(code), std::move(message)}
        .withContext("asset_id", std::string{assetId})
        .withContext("resource_type", std::string{resourceTypeName(type)});
}

[[nodiscard]] auto payloadError(std::string code, std::string message, std::string_view assetId,
                                PresentationResourceType type, std::size_t byteOffset)
    -> core::Error {
    return resourceError(std::move(code), std::move(message), assetId, type)
        .withContext("byte_offset", std::to_string(byteOffset));
}

[[nodiscard]] auto budgetError(std::string_view assetId, PresentationResourceType type,
                               std::uint64_t limit, std::uint64_t actual) -> core::Error {
    return resourceError("playback.presentation.resource.budget_exceeded",
                         "Portable resource byte budget was exceeded", assetId, type)
        .withContext("limit", std::to_string(limit))
        .withContext("actual", std::to_string(actual));
}

[[nodiscard]] auto checkedAdd(std::size_t left, std::size_t right, std::size_t& result) noexcept
    -> bool {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] auto checkedAddU64(std::uint64_t left, std::uint64_t right,
                                 std::uint64_t& result) noexcept -> bool {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] auto checkedMultiply(std::size_t left, std::size_t right,
                                   std::size_t& result) noexcept -> bool {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
        return false;
    }
    result = left * right;
    return true;
}

class ByteReader final {
  public:
    explicit ByteReader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] std::size_t offset() const noexcept {
        return offset_;
    }

    void seek(std::size_t offset) noexcept {
        offset_ = offset;
    }

    [[nodiscard]] auto readU32() noexcept -> std::uint32_t {
        const auto result = static_cast<std::uint32_t>(byte(offset_)) |
                            (static_cast<std::uint32_t>(byte(offset_ + 1)) << 8U) |
                            (static_cast<std::uint32_t>(byte(offset_ + 2)) << 16U) |
                            (static_cast<std::uint32_t>(byte(offset_ + 3)) << 24U);
        offset_ += 4;
        return result;
    }

    [[nodiscard]] auto readU64() noexcept -> std::uint64_t {
        std::uint64_t result = 0;
        for (std::size_t index = 0; index < 8; ++index) {
            result |= static_cast<std::uint64_t>(byte(offset_ + index)) << (index * 8U);
        }
        offset_ += 8;
        return result;
    }

    [[nodiscard]] auto readFloat() noexcept -> float {
        auto value = std::bit_cast<float>(readU32());
        if (value == 0.0F) {
            value = 0.0F;
        }
        return value;
    }

    [[nodiscard]] auto readBytes(std::size_t count) noexcept -> std::span<const std::byte> {
        const auto result = bytes_.subspan(offset_, count);
        offset_ += count;
        return result;
    }

    [[nodiscard]] auto remaining() const noexcept -> std::size_t {
        return offset_ <= bytes_.size() ? bytes_.size() - offset_ : 0;
    }

    [[nodiscard]] auto readI32() noexcept -> std::int32_t {
        return std::bit_cast<std::int32_t>(readU32());
    }

  private:
    [[nodiscard]] auto byte(std::size_t offset) const noexcept -> std::uint8_t {
        return std::to_integer<std::uint8_t>(bytes_[offset]);
    }

    std::span<const std::byte> bytes_;
    std::size_t offset_{};
};

[[nodiscard]] auto validateEnvelope(std::span<const std::byte> bytes, std::string_view assetId,
                                    PresentationResourceType type) -> core::Result<ByteReader> {
    if (bytes.size() > maxResourceBytes) {
        return core::unexpected(budgetError(assetId, type, maxResourceBytes, bytes.size()));
    }
    if (bytes.size() < envelopeByteCount) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable payload envelope is truncated", assetId,
                                             type, bytes.size()));
    }
    if (!std::equal(payloadMagic.begin(), payloadMagic.end(), bytes.begin())) {
        return core::unexpected(payloadError("playback.presentation.payload.magic_invalid",
                                             "Portable payload magic is invalid", assetId, type,
                                             0));
    }

    ByteReader reader{bytes};
    reader.seek(8);
    const auto kind = reader.readU32();
    if (kind != payloadKind(type)) {
        return core::unexpected(payloadError("playback.presentation.payload.type_mismatch",
                                             "Portable payload kind does not match the Asset Index",
                                             assetId, type, 8));
    }
    const auto version = reader.readU32();
    if (version != 1) {
        return core::unexpected(payloadError("playback.presentation.payload.version_unsupported",
                                             "Portable payload version is unsupported", assetId,
                                             type, 12));
    }
    const auto declaredByteCount = reader.readU64();
    if (declaredByteCount > bytes.size()) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable payload body is truncated", assetId, type,
                                             bytes.size()));
    }
    if (declaredByteCount != bytes.size()) {
        return core::unexpected(payloadError("playback.presentation.payload.size_mismatch",
                                             "Portable payload size does not match its envelope",
                                             assetId, type,
                                             static_cast<std::size_t>(declaredByteCount)));
    }
    return reader;
}

[[nodiscard]] auto validateExactSize(std::size_t expected, std::size_t actual,
                                     std::string_view assetId, PresentationResourceType type)
    -> core::Result<void> {
    if (expected > actual) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable payload body is truncated", assetId, type,
                                             actual));
    }
    if (expected < actual) {
        return core::unexpected(payloadError("playback.presentation.payload.size_mismatch",
                                             "Portable payload contains trailing bytes", assetId,
                                             type, expected));
    }
    return {};
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

    [[nodiscard]] auto finish() const noexcept -> PresentationContentIdentity {
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

        PresentationContentIdentity identity;
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
    explicit CanonicalHash(PresentationResourceType type) noexcept {
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

    void writeI32(std::int32_t value) noexcept {
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeIdentity(const PresentationContentIdentity& identity) noexcept {
        sha_.update(std::as_bytes(std::span{identity.sha256}));
    }

    void writeBytes(std::span<const std::byte> bytes) noexcept {
        sha_.update(bytes);
    }

    [[nodiscard]] auto finish() const noexcept -> PresentationContentIdentity {
        return sha_.finish();
    }

  private:
    Sha256 sha_;
};

struct ParsedMesh final {
    PortableMesh value;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
};

struct ParsedTexture final {
    PortableTexture2D value;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
};

struct ParsedMaterial final {
    PortableUnlitMaterial value;
    std::optional<std::string> textureAssetId;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
};

struct ParsedShader final {
    PortableShader value;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
};

struct ParsedParameterizedMaterial final {
    PortableParameterizedMaterial value;
    std::string shaderAssetId;
    std::vector<std::string> textureAssetIds;
    std::vector<std::string> selectedKeywords;
    std::uint32_t parameterCount{};
    std::vector<std::byte> parameterBytes;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
};

[[nodiscard]] auto parseMesh(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedMesh> {
    const auto type = PresentationResourceType::Mesh;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 40) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable Mesh fixed body is truncated", assetId, type,
                                             bytes.size()));
    }
    auto reader = *readerResult;
    const auto vertexCount = reader.readU32();
    const auto indexCount = reader.readU32();
    const auto flags = reader.readU32();
    const auto reserved = reader.readU32();
    if (vertexCount == 0 || vertexCount > maxMeshVertices) {
        return core::unexpected(payloadError("playback.presentation.mesh.vertex_count_invalid",
                                             "Portable Mesh vertex count is outside the v1 range",
                                             assetId, type, 24));
    }
    if (indexCount < 3 || indexCount > maxMeshIndices || indexCount % 3 != 0) {
        return core::unexpected(payloadError("playback.presentation.mesh.index_count_invalid",
                                             "Portable Mesh index count is outside the v1 range",
                                             assetId, type, 28));
    }
    if ((flags & ~1U) != 0 || reserved != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.reserved_nonzero",
                                             "Portable Mesh reserved flags or fields are non-zero",
                                             assetId, type, (flags & ~1U) != 0 ? 32 : 36));
    }

    const bool hasUv0 = (flags & 1U) != 0;
    std::size_t positionCount = 0;
    std::size_t positionBytes = 0;
    std::size_t uvCount = 0;
    std::size_t uvBytes = 0;
    std::size_t indexBytes = 0;
    std::size_t expectedBytes = 40;
    if (!checkedMultiply(vertexCount, 3, positionCount) ||
        !checkedMultiply(positionCount, sizeof(float), positionBytes) ||
        (hasUv0 && (!checkedMultiply(vertexCount, 2, uvCount) ||
                    !checkedMultiply(uvCount, sizeof(float), uvBytes))) ||
        !checkedMultiply(indexCount, sizeof(std::uint32_t), indexBytes) ||
        !checkedAdd(expectedBytes, positionBytes, expectedBytes) ||
        !checkedAdd(expectedBytes, uvBytes, expectedBytes) ||
        !checkedAdd(expectedBytes, indexBytes, expectedBytes)) {
        return core::unexpected(payloadError("playback.presentation.payload.integer_overflow",
                                             "Portable Mesh byte calculation overflowed", assetId,
                                             type, 24));
    }
    if (auto exact = validateExactSize(expectedBytes, bytes.size(), assetId, type); !exact) {
        return core::unexpected(std::move(exact.error()));
    }

    std::size_t decodedBytes = 0;
    if (!checkedAdd(positionBytes, uvBytes, decodedBytes) ||
        !checkedAdd(decodedBytes, indexBytes, decodedBytes) ||
        !checkedAdd(decodedBytes, 24, decodedBytes)) {
        return core::unexpected(payloadError("playback.presentation.payload.integer_overflow",
                                             "Portable Mesh decoded byte calculation overflowed",
                                             assetId, type, 24));
    }
    if (decodedBytes > maxResourceBytes) {
        return core::unexpected(budgetError(assetId, type, maxResourceBytes, decodedBytes));
    }

    ParsedMesh parsed;
    parsed.value.positions.resize(positionCount);
    parsed.value.uv0.resize(uvCount);
    parsed.value.indices.resize(indexCount);
    for (std::size_t index = 0; index < parsed.value.positions.size(); ++index) {
        const auto value = reader.readFloat();
        if (!std::isfinite(value) || std::abs(value) > 1'000'000.0F) {
            return core::unexpected(
                payloadError("playback.presentation.mesh.value_invalid",
                             "Portable Mesh position is non-finite or outside the v1 range",
                             assetId, type, reader.offset() - sizeof(float)));
        }
        parsed.value.positions[index] = value;
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        parsed.value.boundsMin[axis] = parsed.value.positions[axis];
        parsed.value.boundsMax[axis] = parsed.value.positions[axis];
    }
    for (std::size_t vertex = 1; vertex < vertexCount; ++vertex) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto value = parsed.value.positions[vertex * 3 + axis];
            parsed.value.boundsMin[axis] = std::min(parsed.value.boundsMin[axis], value);
            parsed.value.boundsMax[axis] = std::max(parsed.value.boundsMax[axis], value);
        }
    }
    for (std::size_t index = 0; index < parsed.value.uv0.size(); ++index) {
        const auto value = reader.readFloat();
        if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
            return core::unexpected(
                payloadError("playback.presentation.mesh.value_invalid",
                             "Portable Mesh UV0 value is non-finite or outside [0, 1]", assetId,
                             type, reader.offset() - sizeof(float)));
        }
        parsed.value.uv0[index] = value;
    }
    for (std::size_t index = 0; index < parsed.value.indices.size(); ++index) {
        const auto value = reader.readU32();
        if (value >= vertexCount) {
            return core::unexpected(payloadError("playback.presentation.mesh.index_out_of_range",
                                                 "Portable Mesh index does not name a vertex",
                                                 assetId, type,
                                                 reader.offset() - sizeof(std::uint32_t)));
        }
        parsed.value.indices[index] = value;
    }
    for (std::size_t index = 0; index < parsed.value.indices.size(); index += 3) {
        const auto index0 = parsed.value.indices[index];
        const auto index1 = parsed.value.indices[index + 1];
        const auto index2 = parsed.value.indices[index + 2];
        if (index0 == index1 || index0 == index2 || index1 == index2) {
            return core::unexpected(resourceError("playback.presentation.mesh.degenerate_triangle",
                                                  "Portable Mesh triangle repeats a vertex index",
                                                  assetId, type));
        }
        const auto coordinate = [&](std::uint32_t vertex, std::size_t axis) {
            return static_cast<double>(parsed.value.positions[vertex * 3 + axis]);
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
            return core::unexpected(resourceError("playback.presentation.mesh.degenerate_triangle",
                                                  "Portable Mesh triangle has zero area", assetId,
                                                  type));
        }
    }

    parsed.encodedByteCount = bytes.size();
    parsed.decodedByteCount = decodedBytes;
    return parsed;
}

[[nodiscard]] auto parseTexture(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedTexture> {
    const auto type = PresentationResourceType::Texture2D;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 40) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable Texture2D fixed body is truncated", assetId,
                                             type, bytes.size()));
    }
    auto reader = *readerResult;
    const auto width = reader.readU32();
    const auto height = reader.readU32();
    const auto colorSpace = reader.readU32();
    const auto reserved = reader.readU32();
    if (width == 0 || width > maxTextureDimension || height == 0 || height > maxTextureDimension) {
        return core::unexpected(payloadError(
            "playback.presentation.texture.dimension_invalid",
            "Portable Texture2D dimensions are outside the v1 range", assetId, type, 24));
    }
    if (colorSpace != static_cast<std::uint32_t>(PresentationColorSpace::Linear) &&
        colorSpace != static_cast<std::uint32_t>(PresentationColorSpace::Srgb)) {
        return core::unexpected(
            payloadError("playback.presentation.texture.color_space_unsupported",
                         "Portable Texture2D color space is unsupported", assetId, type, 32));
    }
    if (reserved != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.reserved_nonzero",
                                             "Portable Texture2D reserved field is non-zero",
                                             assetId, type, 36));
    }

    std::size_t pixelCount = 0;
    std::size_t pixelBytes = 0;
    std::size_t expectedBytes = 40;
    if (!checkedMultiply(width, height, pixelCount) ||
        !checkedMultiply(pixelCount, 4, pixelBytes) ||
        !checkedAdd(expectedBytes, pixelBytes, expectedBytes)) {
        return core::unexpected(payloadError("playback.presentation.payload.integer_overflow",
                                             "Portable Texture2D byte calculation overflowed",
                                             assetId, type, 24));
    }
    std::size_t decodedBytes = 0;
    if (!checkedAdd(pixelBytes, 12, decodedBytes)) {
        return core::unexpected(payloadError(
            "playback.presentation.payload.integer_overflow",
            "Portable Texture2D decoded byte calculation overflowed", assetId, type, 24));
    }
    if (decodedBytes > maxResourceBytes) {
        return core::unexpected(budgetError(assetId, type, maxResourceBytes, decodedBytes));
    }
    if (auto exact = validateExactSize(expectedBytes, bytes.size(), assetId, type); !exact) {
        if (expectedBytes < bytes.size()) {
            return core::unexpected(payloadError("playback.presentation.texture.pixel_size_invalid",
                                                 "Portable Texture2D pixel size is invalid",
                                                 assetId, type, 40));
        }
        return core::unexpected(std::move(exact.error()));
    }

    ParsedTexture parsed;
    parsed.value.width = width;
    parsed.value.height = height;
    parsed.value.colorSpace = static_cast<PresentationColorSpace>(colorSpace);
    const auto pixels = reader.readBytes(pixelBytes);
    parsed.value.pixelsRgba8.assign(pixels.begin(), pixels.end());
    parsed.encodedByteCount = bytes.size();
    parsed.decodedByteCount = decodedBytes;
    return parsed;
}

[[nodiscard]] auto isPortableAssetId(std::string_view value) noexcept -> bool {
    const auto isAlphaNumeric = [](unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9');
    };
    if (value.empty() || value.size() > 256 ||
        !isAlphaNumeric(static_cast<unsigned char>(value.front()))) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [&](unsigned char character) {
        return isAlphaNumeric(character) || character == '.' || character == '_' ||
               character == '-' || character == '/';
    });
}

[[nodiscard]] auto bytesToString(std::span<const std::byte> bytes) -> std::string {
    std::string text;
    text.resize(bytes.size());
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        text[index] = static_cast<char>(std::to_integer<unsigned char>(bytes[index]));
    }
    return text;
}

[[nodiscard]] auto startsWithIgnoreCase(std::string_view value, std::string_view prefix) noexcept
    -> bool {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        const auto left = static_cast<unsigned char>(value[index]);
        const auto right = static_cast<unsigned char>(prefix[index]);
        const auto lowerLeft = left >= 'A' && left <= 'Z' ? static_cast<char>(left - 'A' + 'a')
                                                          : static_cast<char>(left);
        const auto lowerRight = right >= 'A' && right <= 'Z' ? static_cast<char>(right - 'A' + 'a')
                                                             : static_cast<char>(right);
        if (lowerLeft != lowerRight) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto isShaderIdentifier(std::string_view value) noexcept -> bool {
    if (value.empty() || value.size() > maxIdentifierBytes) {
        return false;
    }
    const auto first = static_cast<unsigned char>(value.front());
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z'))) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '_';
    });
}

[[nodiscard]] auto isUserIdentifier(std::string_view value) noexcept -> bool {
    return isShaderIdentifier(value) && !startsWithIgnoreCase(value, "cuexis");
}

[[nodiscard]] auto isWellFormedUtf8(std::span<const std::byte> bytes) noexcept -> bool {
    std::size_t index = 0;
    while (index < bytes.size()) {
        const auto lead = std::to_integer<std::uint8_t>(bytes[index]);
        std::size_t extra = 0;
        std::uint32_t codepoint = 0;
        std::uint32_t minimum = 0;
        if (lead <= 0x7FU) {
            ++index;
            continue;
        }
        if ((lead & 0xE0U) == 0xC0U) {
            extra = 1;
            codepoint = lead & 0x1FU;
            minimum = 0x80U;
        } else if ((lead & 0xF0U) == 0xE0U) {
            extra = 2;
            codepoint = lead & 0x0FU;
            minimum = 0x800U;
        } else if ((lead & 0xF8U) == 0xF0U) {
            extra = 3;
            codepoint = lead & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (index + extra >= bytes.size()) {
            return false;
        }
        for (std::size_t follow = 1; follow <= extra; ++follow) {
            const auto next = std::to_integer<std::uint8_t>(bytes[index + follow]);
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            codepoint = (codepoint << 6U) | (next & 0x3FU);
        }
        if (codepoint < minimum || codepoint > 0x10FFFFU ||
            (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
            return false;
        }
        index += 1 + extra;
    }
    return true;
}

[[nodiscard]] auto peekPayloadKind(std::span<const std::byte> bytes) noexcept
    -> std::optional<std::uint32_t> {
    if (bytes.size() < envelopeByteCount ||
        !std::equal(payloadMagic.begin(), payloadMagic.end(), bytes.begin())) {
        return std::nullopt;
    }
    ByteReader reader{bytes};
    reader.seek(8);
    return reader.readU32();
}

[[nodiscard]] auto readCountedBytes(ByteReader& reader, std::uint32_t byteCount,
                                    std::string_view assetId, PresentationResourceType type)
    -> core::Result<std::span<const std::byte>> {
    if (reader.remaining() < byteCount) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable payload body is truncated", assetId, type,
                                             reader.offset()));
    }
    return reader.readBytes(byteCount);
}

[[nodiscard]] auto validateShaderSource(std::string_view source, std::string_view assetId,
                                        PresentationResourceType type, std::size_t byteOffset)
    -> core::Result<void> {
    const auto bytes = std::as_bytes(std::span{source.data(), source.size()});
    if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        return core::unexpected(payloadError("playback.presentation.shader.source_encoding_invalid",
                                             "Shader source must be UTF-8 without a BOM", assetId,
                                             type, byteOffset));
    }
    if (source.find('\r') != std::string_view::npos) {
        return core::unexpected(payloadError("playback.presentation.shader.source_encoding_invalid",
                                             "Shader source line endings must be LF", assetId, type,
                                             byteOffset));
    }
    if (!isWellFormedUtf8(bytes)) {
        return core::unexpected(payloadError("playback.presentation.shader.source_encoding_invalid",
                                             "Shader source must be well-formed UTF-8", assetId,
                                             type, byteOffset));
    }
    const auto newline = source.find('\n');
    const auto firstLine = newline == std::string_view::npos ? source : source.substr(0, newline);
    if (firstLine != "#version 450") {
        return core::unexpected(payloadError("playback.presentation.shader.subset_invalid",
                                             "Shader source must start with #version 450", assetId,
                                             type, byteOffset));
    }
    std::size_t lineStart = 0;
    while (lineStart < source.size()) {
        const auto lineEnd = source.find('\n', lineStart);
        const auto line = source.substr(
            lineStart, (lineEnd == std::string_view::npos ? source.size() : lineEnd) - lineStart);
        std::size_t cursor = 0;
        while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
            ++cursor;
        }
        if (line.substr(cursor).starts_with("#include")) {
            return core::unexpected(payloadError("playback.presentation.shader.subset_invalid",
                                                 "Shader source must not use #include", assetId,
                                                 type, byteOffset + lineStart));
        }
        if (lineEnd == std::string_view::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return {};
}

[[nodiscard]] auto unusedNumericLanesZero(ShaderParameterType type,
                                          const std::array<float, 4>& numeric) noexcept -> bool {
    switch (type) {
    case ShaderParameterType::Float:
        return numeric[1] == 0.0F && numeric[2] == 0.0F && numeric[3] == 0.0F;
    case ShaderParameterType::Vec2:
        return numeric[2] == 0.0F && numeric[3] == 0.0F;
    case ShaderParameterType::Vec3:
        return numeric[3] == 0.0F;
    case ShaderParameterType::Vec4:
        return true;
    case ShaderParameterType::Int:
    case ShaderParameterType::Bool:
    case ShaderParameterType::Texture2D:
        return numeric[0] == 0.0F && numeric[1] == 0.0F && numeric[2] == 0.0F && numeric[3] == 0.0F;
    }
    return false;
}

[[nodiscard]] auto parseMaterial(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedMaterial> {
    const auto type = PresentationResourceType::UnlitMaterial;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 56) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable Unlit Material fixed body is truncated",
                                             assetId, type, bytes.size()));
    }
    auto reader = *readerResult;
    const auto alphaMode = reader.readU32();
    const auto doubleSided = reader.readU32();
    std::array<float, 4> baseColor{};
    for (auto& value : baseColor) {
        value = reader.readFloat();
    }
    const auto textureAssetIdByteCount = reader.readU32();
    const auto reserved = reader.readU32();
    if ((alphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Opaque) &&
         alphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Blend)) ||
        doubleSided > 1) {
        return core::unexpected(
            payloadError("playback.presentation.material.value_invalid",
                         "Portable Unlit Material alpha mode or double-sided value is invalid",
                         assetId, type, alphaMode > 2 ? 24 : 28));
    }
    for (std::size_t index = 0; index < baseColor.size(); ++index) {
        if (!std::isfinite(baseColor[index]) || baseColor[index] < 0.0F ||
            baseColor[index] > 1.0F) {
            return core::unexpected(
                payloadError("playback.presentation.material.value_invalid",
                             "Portable Unlit Material base color is outside [0, 1]", assetId, type,
                             32 + index * sizeof(float)));
        }
    }
    if (alphaMode == static_cast<std::uint32_t>(PresentationAlphaMode::Opaque) &&
        baseColor[3] != 1.0F) {
        return core::unexpected(payloadError(
            "playback.presentation.material.value_invalid",
            "Opaque Portable Unlit Material base color alpha must be one", assetId, type, 44));
    }
    if (textureAssetIdByteCount > 256) {
        return core::unexpected(payloadError(
            "playback.presentation.material.texture_reference_invalid",
            "Portable Unlit Material texture AssetId exceeds the v1 limit", assetId, type, 48));
    }
    if (reserved != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.reserved_nonzero",
                                             "Portable Unlit Material reserved field is non-zero",
                                             assetId, type, 52));
    }

    std::size_t expectedBytes = 56;
    if (!checkedAdd(expectedBytes, textureAssetIdByteCount, expectedBytes)) {
        return core::unexpected(payloadError("playback.presentation.payload.integer_overflow",
                                             "Portable Unlit Material byte calculation overflowed",
                                             assetId, type, 48));
    }
    if (auto exact = validateExactSize(expectedBytes, bytes.size(), assetId, type); !exact) {
        return core::unexpected(std::move(exact.error()));
    }

    ParsedMaterial parsed;
    parsed.value.alphaMode = static_cast<PresentationAlphaMode>(alphaMode);
    parsed.value.doubleSided = doubleSided != 0;
    std::copy(baseColor.begin(), baseColor.end(), parsed.value.baseColor);
    if (textureAssetIdByteCount != 0) {
        const auto encodedId = reader.readBytes(textureAssetIdByteCount);
        std::string textureAssetId;
        textureAssetId.reserve(textureAssetIdByteCount);
        for (const auto value : encodedId) {
            textureAssetId.push_back(static_cast<char>(std::to_integer<unsigned char>(value)));
        }
        if (!isPortableAssetId(textureAssetId)) {
            return core::unexpected(payloadError(
                "playback.presentation.material.texture_reference_invalid",
                "Portable Unlit Material texture AssetId is not portable", assetId, type, 56));
        }
        parsed.textureAssetId = std::move(textureAssetId);
    }
    parsed.encodedByteCount = bytes.size();
    parsed.decodedByteCount = 32 + (parsed.textureAssetId ? 37 + parsed.textureAssetId->size() : 0);
    if (parsed.decodedByteCount > maxResourceBytes) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    return parsed;
}

[[nodiscard]] auto parseShader(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedShader> {
    const auto type = PresentationResourceType::Shader;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 72) {
        return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                             "Portable Shader fixed body is truncated", assetId,
                                             type, bytes.size()));
    }
    auto reader = *readerResult;
    const auto flags = reader.readU32();
    const auto vertexEntryByteCount = reader.readU32();
    const auto fragmentEntryByteCount = reader.readU32();
    const auto keywordCount = reader.readU32();
    const auto parameterCount = reader.readU32();
    const auto bindingCount = reader.readU32();
    const auto hostExtensionCount = reader.readU32();
    const auto defaultAlphaMode = reader.readU32();
    const auto defaultDoubleSided = reader.readU32();
    const auto profileByteCount = reader.readU32();
    const auto vertexSourceByteCount = reader.readU32();
    const auto fragmentSourceByteCount = reader.readU32();
    if (flags != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.reserved_nonzero",
                                             "Portable Shader reserved flags are non-zero", assetId,
                                             type, 24));
    }
    if (vertexEntryByteCount < 1 || vertexEntryByteCount > maxEntryBytes) {
        return core::unexpected(payloadError("playback.presentation.shader.entry_invalid",
                                             "Shader vertex entry length is outside the v1 range",
                                             assetId, type, 28));
    }
    if (fragmentEntryByteCount < 1 || fragmentEntryByteCount > maxEntryBytes) {
        return core::unexpected(payloadError("playback.presentation.shader.entry_invalid",
                                             "Shader fragment entry length is outside the v1 range",
                                             assetId, type, 32));
    }
    if (keywordCount > maxShaderKeywords) {
        return core::unexpected(payloadError("playback.presentation.shader.keyword_invalid",
                                             "Shader keyword count exceeds the v1 limit", assetId,
                                             type, 36));
    }
    if (parameterCount > maxShaderParameters) {
        return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                             "Shader parameter count exceeds the v1 limit", assetId,
                                             type, 40));
    }
    if (bindingCount > maxShaderBindings) {
        return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                             "Shader binding count exceeds the v1 limit", assetId,
                                             type, 44));
    }
    if (hostExtensionCount > maxShaderHostExtensions) {
        return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                             "Shader host extension count exceeds the v1 limit",
                                             assetId, type, 48));
    }
    if ((defaultAlphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Opaque) &&
         defaultAlphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Blend)) ||
        defaultDoubleSided > 1) {
        return core::unexpected(
            payloadError("playback.presentation.shader.schema_invalid",
                         "Shader default alpha mode or double-sided value is invalid", assetId,
                         type, defaultAlphaMode > 2 ? 52 : 56));
    }
    if (profileByteCount < 1 || profileByteCount > maxEntryBytes) {
        return core::unexpected(payloadError("playback.presentation.shader.profile_unsupported",
                                             "Shader profile length is outside the v1 range",
                                             assetId, type, 60));
    }
    if (vertexSourceByteCount < 1 || vertexSourceByteCount > maxShaderSourceBytes) {
        return core::unexpected(payloadError("playback.presentation.shader.subset_invalid",
                                             "Shader vertex source length is outside the v1 range",
                                             assetId, type, 64));
    }
    if (fragmentSourceByteCount < 1 || fragmentSourceByteCount > maxShaderSourceBytes) {
        return core::unexpected(payloadError(
            "playback.presentation.shader.subset_invalid",
            "Shader fragment source length is outside the v1 range", assetId, type, 68));
    }
    if (vertexSourceByteCount > maxShaderSourceTotalBytes - fragmentSourceByteCount) {
        return core::unexpected(budgetError(assetId, type, maxShaderSourceTotalBytes,
                                            static_cast<std::uint64_t>(vertexSourceByteCount) +
                                                fragmentSourceByteCount));
    }

    auto readName = [&](std::uint32_t byteCount) -> core::Result<std::string> {
        auto encoded = readCountedBytes(reader, byteCount, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        return bytesToString(*encoded);
    };
    auto vertexEntry = readName(vertexEntryByteCount);
    if (!vertexEntry) {
        return core::unexpected(std::move(vertexEntry.error()));
    }
    auto fragmentEntry = readName(fragmentEntryByteCount);
    if (!fragmentEntry) {
        return core::unexpected(std::move(fragmentEntry.error()));
    }
    auto profile = readName(profileByteCount);
    if (!profile) {
        return core::unexpected(std::move(profile.error()));
    }
    if (!isUserIdentifier(*vertexEntry) || !isUserIdentifier(*fragmentEntry)) {
        return core::unexpected(payloadError("playback.presentation.shader.entry_invalid",
                                             "Shader entry name is not a portable identifier",
                                             assetId, type, 72));
    }
    if (*profile != builtinRendererProfile && !isUserIdentifier(*profile)) {
        return core::unexpected(payloadError("playback.presentation.shader.profile_unsupported",
                                             "Shader required renderer profile is unsupported",
                                             assetId, type, 72));
    }

    ParsedShader parsed;
    parsed.value.vertexEntry = std::move(*vertexEntry);
    parsed.value.fragmentEntry = std::move(*fragmentEntry);
    parsed.value.requiredRendererProfile = std::move(*profile);
    parsed.value.defaultAlphaMode = static_cast<PresentationAlphaMode>(defaultAlphaMode);
    parsed.value.defaultDoubleSided = defaultDoubleSided != 0;
    parsed.value.variantKeywords.reserve(keywordCount);
    std::set<std::string, std::less<>> uniqueKeywords;
    for (std::uint32_t index = 0; index < keywordCount; ++index) {
        if (reader.remaining() < 4) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader keyword list is truncated",
                                                 assetId, type, reader.offset()));
        }
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        auto name = bytesToString(*encoded);
        if (!isUserIdentifier(name)) {
            return core::unexpected(payloadError("playback.presentation.shader.keyword_invalid",
                                                 "Shader keyword is not a portable identifier",
                                                 assetId, type, reader.offset() - nameBytes));
        }
        if (!uniqueKeywords.insert(name).second) {
            return core::unexpected(payloadError("playback.presentation.shader.keyword_invalid",
                                                 "Shader keywords must be unique", assetId, type,
                                                 reader.offset() - nameBytes));
        }
        parsed.value.variantKeywords.push_back(std::move(name));
    }

    parsed.value.parameters.reserve(parameterCount);
    std::set<std::string, std::less<>> uniqueParameterNames;
    for (std::uint32_t index = 0; index < parameterCount; ++index) {
        if (reader.remaining() < 4) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader parameter list is truncated",
                                                 assetId, type, reader.offset()));
        }
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        ShaderParameterSchemaEntry entry;
        entry.name = bytesToString(*encoded);
        if (reader.remaining() < 36) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader parameter record is truncated",
                                                 assetId, type, reader.offset()));
        }
        entry.type = static_cast<ShaderParameterType>(reader.readU32());
        entry.set = reader.readU32();
        entry.binding = reader.readU32();
        for (auto& lane : entry.defaultNumeric) {
            lane = reader.readFloat();
        }
        entry.defaultInt = reader.readI32();
        const auto defaultBool = reader.readU32();
        if (!isUserIdentifier(entry.name) || !uniqueParameterNames.insert(entry.name).second) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader parameter name is invalid or duplicated",
                                                 assetId, type, reader.offset()));
        }
        if (static_cast<std::uint32_t>(entry.type) < 1 ||
            static_cast<std::uint32_t>(entry.type) > 7) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader parameter type is unsupported", assetId,
                                                 type, reader.offset() - 32));
        }
        if (entry.set != 0 || entry.binding < 1 || entry.binding > maxShaderBindings) {
            return core::unexpected(
                payloadError(entry.set == 0 && entry.binding == 0
                                 ? "playback.presentation.shader.reserved_binding"
                                 : "playback.presentation.shader.schema_invalid",
                             "Shader parameter set or binding is outside the v1 range", assetId,
                             type, reader.offset() - 28));
        }
        if (!std::isfinite(entry.defaultNumeric[0]) || !std::isfinite(entry.defaultNumeric[1]) ||
            !std::isfinite(entry.defaultNumeric[2]) || !std::isfinite(entry.defaultNumeric[3]) ||
            !unusedNumericLanesZero(entry.type, entry.defaultNumeric) ||
            (entry.type != ShaderParameterType::Int && entry.defaultInt != 0) || defaultBool > 1 ||
            (entry.type != ShaderParameterType::Bool && defaultBool != 0) ||
            (entry.type == ShaderParameterType::Texture2D && entry.defaultInt != 0)) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader parameter default values are invalid",
                                                 assetId, type, reader.offset() - 20));
        }
        entry.defaultBool = defaultBool != 0;
        parsed.value.parameters.push_back(std::move(entry));
    }

    parsed.value.bindings.reserve(bindingCount);
    std::set<std::string, std::less<>> uniqueBindingNames;
    std::set<std::pair<std::uint32_t, std::uint32_t>> uniqueSlots;
    for (std::uint32_t index = 0; index < bindingCount; ++index) {
        if (reader.remaining() < 16) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader binding list is truncated",
                                                 assetId, type, reader.offset()));
        }
        ShaderBinding binding;
        binding.set = reader.readU32();
        binding.binding = reader.readU32();
        binding.type = static_cast<ShaderParameterType>(reader.readU32());
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        binding.name = bytesToString(*encoded);
        if (!isUserIdentifier(binding.name) || !uniqueBindingNames.insert(binding.name).second) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader binding name is invalid or duplicated",
                                                 assetId, type, reader.offset() - nameBytes));
        }
        if (static_cast<std::uint32_t>(binding.type) < 1 ||
            static_cast<std::uint32_t>(binding.type) > 7) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader binding type is unsupported", assetId,
                                                 type, reader.offset() - nameBytes - 12));
        }
        if (binding.set == 0 && binding.binding == 0) {
            return core::unexpected(
                payloadError("playback.presentation.shader.reserved_binding",
                             "User shader bindings must not occupy set 0 binding 0", assetId, type,
                             reader.offset() - nameBytes - 12));
        }
        if (binding.set != 0 || binding.binding < 1 || binding.binding > maxShaderBindings ||
            !uniqueSlots.insert({binding.set, binding.binding}).second) {
            return core::unexpected(payloadError("playback.presentation.shader.schema_invalid",
                                                 "Shader binding slot is invalid or duplicated",
                                                 assetId, type, reader.offset() - nameBytes - 12));
        }
        parsed.value.bindings.push_back(std::move(binding));
    }

    parsed.value.requiredHostExtensions.reserve(hostExtensionCount);
    std::set<std::string, std::less<>> uniqueExtensions;
    for (std::uint32_t index = 0; index < hostExtensionCount; ++index) {
        if (reader.remaining() < 4) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Portable Shader host extension list is truncated",
                                                 assetId, type, reader.offset()));
        }
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        auto name = bytesToString(*encoded);
        if (!isUserIdentifier(name) || !uniqueExtensions.insert(name).second) {
            return core::unexpected(
                payloadError("playback.presentation.shader.schema_invalid",
                             "Shader host extension ID is invalid or duplicated", assetId, type,
                             reader.offset() - nameBytes));
        }
        parsed.value.requiredHostExtensions.push_back(std::move(name));
    }
    if (parsed.value.requiredRendererProfile != builtinRendererProfile) {
        const auto found = std::find(parsed.value.requiredHostExtensions.begin(),
                                     parsed.value.requiredHostExtensions.end(),
                                     parsed.value.requiredRendererProfile);
        if (found == parsed.value.requiredHostExtensions.end()) {
            return core::unexpected(payloadError(
                "playback.presentation.shader.profile_unsupported",
                "Host-extension renderer profile must be listed in requiredHostExtensions", assetId,
                type, 60));
        }
    }

    auto vertexSourceBytes = readCountedBytes(reader, vertexSourceByteCount, assetId, type);
    if (!vertexSourceBytes) {
        return core::unexpected(std::move(vertexSourceBytes.error()));
    }
    auto fragmentSourceBytes = readCountedBytes(reader, fragmentSourceByteCount, assetId, type);
    if (!fragmentSourceBytes) {
        return core::unexpected(std::move(fragmentSourceBytes.error()));
    }
    parsed.value.vertexSource = bytesToString(*vertexSourceBytes);
    parsed.value.fragmentSource = bytesToString(*fragmentSourceBytes);
    if (auto validated =
            validateShaderSource(parsed.value.vertexSource, assetId, type,
                                 reader.offset() - vertexSourceByteCount - fragmentSourceByteCount);
        !validated) {
        return core::unexpected(std::move(validated.error()));
    }
    if (auto validated = validateShaderSource(parsed.value.fragmentSource, assetId, type,
                                              reader.offset() - fragmentSourceByteCount);
        !validated) {
        return core::unexpected(std::move(validated.error()));
    }
    if (reader.remaining() != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.size_mismatch",
                                             "Portable payload contains trailing bytes", assetId,
                                             type, reader.offset()));
    }

    for (const auto& parameter : parsed.value.parameters) {
        const auto binding = std::find_if(
            parsed.value.bindings.begin(), parsed.value.bindings.end(),
            [&](const ShaderBinding& candidate) {
                return candidate.set == parameter.set && candidate.binding == parameter.binding;
            });
        if (binding == parsed.value.bindings.end() || binding->type != parameter.type) {
            return core::unexpected(payloadError(
                "playback.presentation.shader.schema_invalid",
                "Shader parameter set/binding must match the binding table", assetId, type, 40));
        }
    }

    parsed.encodedByteCount = bytes.size();
    parsed.decodedByteCount = 48;
    const auto addString = [&](std::string_view value) -> bool {
        const auto extra = 4ULL + static_cast<std::uint64_t>(value.size());
        return checkedAddU64(parsed.decodedByteCount, extra, parsed.decodedByteCount);
    };
    if (!addString(parsed.value.vertexEntry) || !addString(parsed.value.fragmentEntry) ||
        !addString(parsed.value.requiredRendererProfile)) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    for (const auto& keyword : parsed.value.variantKeywords) {
        if (!addString(keyword)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    for (const auto& parameter : parsed.value.parameters) {
        if (!addString(parameter.name)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    for (const auto& binding : parsed.value.bindings) {
        if (!addString(binding.name)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    for (const auto& extension : parsed.value.requiredHostExtensions) {
        if (!addString(extension)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    if (!checkedAddU64(parsed.decodedByteCount, parsed.value.vertexSource.size(),
                       parsed.decodedByteCount) ||
        !checkedAddU64(parsed.decodedByteCount, parsed.value.fragmentSource.size(),
                       parsed.decodedByteCount) ||
        parsed.decodedByteCount > maxResourceBytes) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    return parsed;
}

[[nodiscard]] auto parseParameterizedMaterial(std::string_view assetId,
                                              std::span<const std::byte> bytes)
    -> core::Result<ParsedParameterizedMaterial> {
    const auto type = PresentationResourceType::ParameterizedMaterial;
    auto readerResult = validateEnvelope(bytes, assetId, type);
    if (!readerResult) {
        return core::unexpected(std::move(readerResult.error()));
    }
    if (bytes.size() < 48) {
        return core::unexpected(
            payloadError("playback.presentation.payload.truncated",
                         "Portable Parameterized Material fixed body is truncated", assetId, type,
                         bytes.size()));
    }
    auto reader = *readerResult;
    const auto alphaMode = reader.readU32();
    const auto doubleSided = reader.readU32();
    const auto keywordCount = reader.readU32();
    const auto parameterCount = reader.readU32();
    const auto shaderAssetIdByteCount = reader.readU32();
    const auto reserved = reader.readU32();
    if ((alphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Opaque) &&
         alphaMode != static_cast<std::uint32_t>(PresentationAlphaMode::Blend)) ||
        doubleSided > 1) {
        return core::unexpected(payloadError(
            "playback.presentation.material.value_invalid",
            "Portable Parameterized Material alpha mode or double-sided value is invalid", assetId,
            type, alphaMode > 2 ? 24 : 28));
    }
    if (keywordCount > maxShaderKeywords) {
        return core::unexpected(payloadError(
            "playback.presentation.material.keyword_undeclared",
            "Parameterized Material keyword count exceeds the v1 limit", assetId, type, 32));
    }
    if (parameterCount > maxShaderParameters) {
        return core::unexpected(payloadError(
            "playback.presentation.material.parameter_mismatch",
            "Parameterized Material parameter count exceeds the v1 limit", assetId, type, 36));
    }
    if (shaderAssetIdByteCount < 1 || shaderAssetIdByteCount > 256) {
        return core::unexpected(
            payloadError("playback.presentation.material.shader_reference_invalid",
                         "Parameterized Material shader AssetId length is outside the v1 range",
                         assetId, type, 40));
    }
    if (reserved != 0) {
        return core::unexpected(payloadError(
            "playback.presentation.payload.reserved_nonzero",
            "Portable Parameterized Material reserved field is non-zero", assetId, type, 44));
    }

    auto shaderIdBytes = readCountedBytes(reader, shaderAssetIdByteCount, assetId, type);
    if (!shaderIdBytes) {
        return core::unexpected(std::move(shaderIdBytes.error()));
    }
    auto shaderAssetId = bytesToString(*shaderIdBytes);
    if (!isPortableAssetId(shaderAssetId)) {
        return core::unexpected(payloadError(
            "playback.presentation.material.shader_reference_invalid",
            "Parameterized Material shader AssetId is not portable", assetId, type, 48));
    }

    ParsedParameterizedMaterial parsed;
    parsed.value.alphaMode = static_cast<PresentationAlphaMode>(alphaMode);
    parsed.value.doubleSided = doubleSided != 0;
    parsed.shaderAssetId = std::move(shaderAssetId);
    parsed.parameterCount = parameterCount;
    parsed.selectedKeywords.reserve(keywordCount);
    std::set<std::string, std::less<>> uniqueKeywords;
    for (std::uint32_t index = 0; index < keywordCount; ++index) {
        if (reader.remaining() < 4) {
            return core::unexpected(payloadError("playback.presentation.payload.truncated",
                                                 "Parameterized Material keyword list is truncated",
                                                 assetId, type, reader.offset()));
        }
        const auto nameBytes = reader.readU32();
        auto encoded = readCountedBytes(reader, nameBytes, assetId, type);
        if (!encoded) {
            return core::unexpected(std::move(encoded.error()));
        }
        auto name = bytesToString(*encoded);
        if (!isUserIdentifier(name) || !uniqueKeywords.insert(name).second) {
            return core::unexpected(
                payloadError("playback.presentation.material.keyword_undeclared",
                             "Parameterized Material keyword is invalid or duplicated", assetId,
                             type, reader.offset() - nameBytes));
        }
        parsed.selectedKeywords.push_back(std::move(name));
    }
    parsed.parameterBytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(reader.offset()),
                                 bytes.end());
    parsed.encodedByteCount = bytes.size();
    return parsed;
}

[[nodiscard]] auto completeParameterizedMaterial(std::string_view assetId,
                                                 ParsedParameterizedMaterial& parsed,
                                                 const PortableShader& shader)
    -> core::Result<void> {
    const auto type = PresentationResourceType::ParameterizedMaterial;
    if (parsed.parameterCount != shader.parameters.size()) {
        return core::unexpected(
            resourceError("playback.presentation.material.parameter_mismatch",
                          "Parameterized Material parameter count does not match the shader schema",
                          assetId, type));
    }
    for (const auto& keyword : parsed.selectedKeywords) {
        if (std::find(shader.variantKeywords.begin(), shader.variantKeywords.end(), keyword) ==
            shader.variantKeywords.end()) {
            return core::unexpected(
                resourceError("playback.presentation.material.keyword_undeclared",
                              "Selected keyword is not declared by the shader", assetId, type)
                    .withContext("keyword", keyword));
        }
    }
    ByteReader reader{parsed.parameterBytes};
    parsed.value.selectedKeywords = parsed.selectedKeywords;
    parsed.value.parameters.reserve(shader.parameters.size());
    std::uint32_t textureParameters = 0;
    for (const auto& schema : shader.parameters) {
        ShaderParameterValue value;
        value.name = schema.name;
        value.type = schema.type;
        if (schema.type == ShaderParameterType::Texture2D) {
            if (reader.remaining() < 4) {
                return core::unexpected(payloadError(
                    "playback.presentation.payload.truncated",
                    "Parameterized Material texture parameter is truncated", assetId, type,
                    envelopeByteCount + 24 + parsed.shaderAssetId.size() +
                        (parsed.parameterBytes.size() - reader.remaining())));
            }
            const auto textureBytes = reader.readU32();
            if (textureBytes < 1 || textureBytes > 256 || reader.remaining() < textureBytes) {
                return core::unexpected(payloadError(
                    "playback.presentation.material.shader_reference_invalid",
                    "Parameterized Material texture AssetId length is outside the v1 range",
                    assetId, type, reader.offset()));
            }
            auto encoded = reader.readBytes(textureBytes);
            auto textureAssetId = bytesToString(encoded);
            if (!isPortableAssetId(textureAssetId)) {
                return core::unexpected(
                    payloadError("playback.presentation.material.shader_reference_invalid",
                                 "Parameterized Material texture AssetId is not portable", assetId,
                                 type, reader.offset() - textureBytes));
            }
            ++textureParameters;
            if (textureParameters > maxMaterialTextureParameters) {
                return core::unexpected(
                    budgetError(assetId, type, maxMaterialTextureParameters, textureParameters));
            }
            parsed.textureAssetIds.push_back(textureAssetId);
            value.texture = PresentationResourceRef{
                PresentationResourceType::Texture2D, std::move(textureAssetId), {}};
        } else if (schema.type == ShaderParameterType::Int) {
            if (reader.remaining() < 16) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.truncated",
                                 "Parameterized Material int parameter is truncated", assetId, type,
                                 reader.offset()));
            }
            value.integer = reader.readI32();
            const auto padding0 = reader.readU32();
            const auto padding1 = reader.readU32();
            const auto padding2 = reader.readU32();
            if (padding0 != 0 || padding1 != 0 || padding2 != 0) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.reserved_nonzero",
                                 "Parameterized Material int parameter padding must be zero",
                                 assetId, type, reader.offset() - 12));
            }
        } else if (schema.type == ShaderParameterType::Bool) {
            if (reader.remaining() < 16) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.truncated",
                                 "Parameterized Material bool parameter is truncated", assetId,
                                 type, reader.offset()));
            }
            const auto boolean = reader.readU32();
            const auto padding0 = reader.readU32();
            const auto padding1 = reader.readU32();
            const auto padding2 = reader.readU32();
            if (boolean > 1 || padding0 != 0 || padding1 != 0 || padding2 != 0) {
                return core::unexpected(
                    payloadError("playback.presentation.material.parameter_mismatch",
                                 "Parameterized Material bool parameter is invalid", assetId, type,
                                 reader.offset() - 16));
            }
            value.boolean = boolean != 0;
        } else {
            if (reader.remaining() < 16) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.truncated",
                                 "Parameterized Material numeric parameter is truncated", assetId,
                                 type, reader.offset()));
            }
            for (auto& lane : value.numeric) {
                lane = reader.readFloat();
            }
            if (!std::isfinite(value.numeric[0]) || !std::isfinite(value.numeric[1]) ||
                !std::isfinite(value.numeric[2]) || !std::isfinite(value.numeric[3]) ||
                !unusedNumericLanesZero(schema.type, value.numeric)) {
                return core::unexpected(
                    payloadError("playback.presentation.material.parameter_mismatch",
                                 "Parameterized Material numeric parameter is invalid", assetId,
                                 type, reader.offset() - 16));
            }
        }
        parsed.value.parameters.push_back(std::move(value));
    }
    if (reader.remaining() != 0) {
        return core::unexpected(payloadError("playback.presentation.payload.size_mismatch",
                                             "Portable payload contains trailing bytes", assetId,
                                             type, reader.offset()));
    }

    parsed.decodedByteCount = 24;
    if (!checkedAddU64(parsed.decodedByteCount, 37ULL + parsed.shaderAssetId.size(),
                       parsed.decodedByteCount)) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    for (const auto& keyword : parsed.selectedKeywords) {
        if (!checkedAddU64(parsed.decodedByteCount, 4ULL + keyword.size(),
                           parsed.decodedByteCount)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    for (const auto& parameter : parsed.value.parameters) {
        std::uint64_t extra = 20ULL + parameter.name.size();
        if (parameter.type == ShaderParameterType::Texture2D && parameter.texture) {
            extra = 8ULL + parameter.name.size() + 37ULL + parameter.texture->assetId.size();
        }
        if (!checkedAddU64(parsed.decodedByteCount, extra, parsed.decodedByteCount)) {
            return core::unexpected(
                budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
        }
    }
    if (parsed.decodedByteCount > maxResourceBytes) {
        return core::unexpected(
            budgetError(assetId, type, maxResourceBytes, parsed.decodedByteCount));
    }
    return {};
}

[[nodiscard]] auto meshIdentity(const PortableMesh& mesh) noexcept -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::Mesh};
    hash.writeU32(static_cast<std::uint32_t>(mesh.positions.size() / 3));
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

[[nodiscard]] auto textureIdentity(const PortableTexture2D& texture) noexcept
    -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::Texture2D};
    hash.writeU32(texture.width);
    hash.writeU32(texture.height);
    hash.writeU32(static_cast<std::uint32_t>(texture.colorSpace));
    hash.writeBytes(texture.pixelsRgba8);
    return hash.finish();
}

[[nodiscard]] auto materialIdentity(const PortableUnlitMaterial& material) noexcept
    -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::UnlitMaterial};
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

[[nodiscard]] auto shaderIdentity(const PortableShader& shader) noexcept
    -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::Shader};
    hash.writeString(shader.vertexEntry);
    hash.writeString(shader.fragmentEntry);
    auto keywords = shader.variantKeywords;
    std::sort(keywords.begin(), keywords.end());
    hash.writeU32(static_cast<std::uint32_t>(keywords.size()));
    for (const auto& keyword : keywords) {
        hash.writeString(keyword);
    }
    hash.writeU32(static_cast<std::uint32_t>(shader.parameters.size()));
    for (const auto& parameter : shader.parameters) {
        hash.writeString(parameter.name);
        hash.writeU32(static_cast<std::uint32_t>(parameter.type));
        hash.writeU32(parameter.set);
        hash.writeU32(parameter.binding);
        for (const auto lane : parameter.defaultNumeric) {
            hash.writeFloat(lane);
        }
        hash.writeI32(parameter.defaultInt);
        hash.writeU32(parameter.defaultBool ? 1U : 0U);
    }
    hash.writeU32(static_cast<std::uint32_t>(shader.bindings.size()));
    for (const auto& binding : shader.bindings) {
        hash.writeU32(binding.set);
        hash.writeU32(binding.binding);
        hash.writeU32(static_cast<std::uint32_t>(binding.type));
        hash.writeString(binding.name);
    }
    hash.writeU32(static_cast<std::uint32_t>(shader.defaultAlphaMode));
    hash.writeU32(shader.defaultDoubleSided ? 1U : 0U);
    hash.writeString(shader.requiredRendererProfile);
    auto extensions = shader.requiredHostExtensions;
    std::sort(extensions.begin(), extensions.end());
    hash.writeU32(static_cast<std::uint32_t>(extensions.size()));
    for (const auto& extension : extensions) {
        hash.writeString(extension);
    }
    hash.writeString(shader.vertexSource);
    hash.writeString(shader.fragmentSource);
    return hash.finish();
}

[[nodiscard]] auto
parameterizedMaterialIdentity(const PortableParameterizedMaterial& material) noexcept
    -> PresentationContentIdentity {
    CanonicalHash hash{PresentationResourceType::ParameterizedMaterial};
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
        if (parameter.type == ShaderParameterType::Texture2D) {
            hash.writeU32(parameter.texture ? 1U : 0U);
            if (parameter.texture) {
                hash.writeString(parameter.texture->assetId);
                hash.writeIdentity(parameter.texture->identity);
            }
        } else if (parameter.type == ShaderParameterType::Int) {
            hash.writeI32(parameter.integer);
        } else if (parameter.type == ShaderParameterType::Bool) {
            hash.writeU32(parameter.boolean ? 1U : 0U);
        } else {
            for (const auto lane : parameter.numeric) {
                hash.writeFloat(lane);
            }
        }
    }
    return hash.finish();
}

[[nodiscard]] auto resourceValuesEqual(const PortableResourceValue& left,
                                       const PortableResourceValue& right) noexcept -> bool {
    if (left.index() != right.index()) {
        return false;
    }
    if (const auto* leftMesh = std::get_if<PortableMesh>(&left)) {
        const auto& rightMesh = std::get<PortableMesh>(right);
        return leftMesh->positions == rightMesh.positions && leftMesh->uv0 == rightMesh.uv0 &&
               leftMesh->indices == rightMesh.indices;
    }
    if (const auto* leftTexture = std::get_if<PortableTexture2D>(&left)) {
        const auto& rightTexture = std::get<PortableTexture2D>(right);
        return leftTexture->width == rightTexture.width &&
               leftTexture->height == rightTexture.height &&
               leftTexture->colorSpace == rightTexture.colorSpace &&
               leftTexture->pixelsRgba8 == rightTexture.pixelsRgba8;
    }
    if (const auto* leftUnlit = std::get_if<PortableUnlitMaterial>(&left)) {
        const auto& rightUnlit = std::get<PortableUnlitMaterial>(right);
        return std::equal(std::begin(leftUnlit->baseColor), std::end(leftUnlit->baseColor),
                          std::begin(rightUnlit.baseColor)) &&
               leftUnlit->alphaMode == rightUnlit.alphaMode &&
               leftUnlit->doubleSided == rightUnlit.doubleSided &&
               leftUnlit->baseColorTexture == rightUnlit.baseColorTexture;
    }
    if (const auto* leftShader = std::get_if<PortableShader>(&left)) {
        const auto& rightShader = std::get<PortableShader>(right);
        return leftShader->vertexSource == rightShader.vertexSource &&
               leftShader->fragmentSource == rightShader.fragmentSource &&
               leftShader->vertexEntry == rightShader.vertexEntry &&
               leftShader->fragmentEntry == rightShader.fragmentEntry &&
               leftShader->variantKeywords == rightShader.variantKeywords &&
               leftShader->defaultAlphaMode == rightShader.defaultAlphaMode &&
               leftShader->defaultDoubleSided == rightShader.defaultDoubleSided &&
               leftShader->requiredRendererProfile == rightShader.requiredRendererProfile &&
               leftShader->requiredHostExtensions == rightShader.requiredHostExtensions &&
               leftShader->bindings.size() == rightShader.bindings.size() &&
               leftShader->parameters.size() == rightShader.parameters.size();
    }
    const auto& leftMaterial = std::get<PortableParameterizedMaterial>(left);
    const auto& rightMaterial = std::get<PortableParameterizedMaterial>(right);
    return leftMaterial.shader == rightMaterial.shader &&
           leftMaterial.alphaMode == rightMaterial.alphaMode &&
           leftMaterial.doubleSided == rightMaterial.doubleSided &&
           leftMaterial.selectedKeywords == rightMaterial.selectedKeywords &&
           leftMaterial.parameters.size() == rightMaterial.parameters.size();
}

[[nodiscard]] auto looksPortable(std::span<const std::byte> bytes) noexcept -> bool {
    if (bytes.size() >= payloadMagic.size() &&
        std::equal(payloadMagic.begin(), payloadMagic.end(), bytes.begin())) {
        return true;
    }
    static constexpr std::array<std::byte, 6> prefix{
        std::byte{'C'}, std::byte{'X'}, std::byte{'P'},
        std::byte{'R'}, std::byte{'E'}, std::byte{'S'},
    };
    if (bytes.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), bytes.begin())) {
        return true;
    }
    if (bytes.size() < envelopeByteCount) {
        return false;
    }
    ByteReader reader{bytes};
    reader.seek(8);
    const auto kind = reader.readU32();
    const auto version = reader.readU32();
    const auto byteCount = reader.readU64();
    return kind >= 1 && kind <= 5 && version == 1 && byteCount == bytes.size();
}

[[nodiscard]] auto collectRequiredResources(const chart::ChartRuntime& chartRuntime)
    -> core::Result<std::map<std::string, PresentationResourceType, std::less<>>> {
    std::map<std::string, PresentationResourceType, std::less<>> required;
    const auto add = [&](std::string_view assetId,
                         PresentationResourceType type) -> core::Result<void> {
        const auto [found, inserted] = required.emplace(std::string{assetId}, type);
        if (!inserted && found->second != type) {
            return core::unexpected(
                resourceError("playback.presentation.reference.invalid",
                              "One AssetId is referenced with multiple presentation resource types",
                              assetId, type));
        }
        return {};
    };
    for (const auto& object : chartRuntime.objects) {
        if (!object.components.renderable) {
            continue;
        }
        if (auto result =
                add(object.components.renderable->mesh.value, PresentationResourceType::Mesh);
            !result) {
            return core::unexpected(std::move(result.error()));
        }
        if (auto result = add(object.components.renderable->material.value,
                              PresentationResourceType::UnlitMaterial);
            !result) {
            return core::unexpected(std::move(result.error()));
        }
        const auto& behaviorReference = object.components.behavior;
        if (!behaviorReference) {
            continue;
        }
        const auto behavior =
            std::lower_bound(chartRuntime.behaviors.begin(), chartRuntime.behaviors.end(),
                             behaviorReference->behavior,
                             [](const chart::RuntimeBehavior& candidate,
                                const chart::BehaviorId& id) { return candidate.id < id; });
        if (behavior == chartRuntime.behaviors.end() ||
            behavior->id != behaviorReference->behavior) {
            continue;
        }
        for (const auto& track : behavior->stepTracks) {
            if (track.property != chart::BehaviorStepProperty::RenderMaterial) {
                continue;
            }
            for (const auto& event : track.events) {
                if (const auto* assetId = std::get_if<chart::AssetId>(&event.value)) {
                    if (auto result = add(assetId->value, PresentationResourceType::UnlitMaterial);
                        !result) {
                        return core::unexpected(std::move(result.error()));
                    }
                }
            }
        }
    }
    return required;
}

[[nodiscard]] auto unavailableResource(std::string_view assetId, PresentationResourceType type,
                                       core::Error cause) -> core::Error {
    for (const core::Error* current = &cause; current != nullptr; current = current->cause()) {
        if (current->code() == "assets.resource.provider_too_large" ||
            current->code() == "content.provider.too_large" ||
            current->code() == "content.filesystem.too_large") {
            return resourceError("playback.presentation.resource.budget_exceeded",
                                 "Portable resource encoded byte budget was exceeded", assetId,
                                 type)
                .withContext("limit", std::to_string(maxResourceBytes))
                .withContext("actual", "greater_than_limit")
                .withCause(std::move(cause));
        }
    }
    return resourceError("playback.presentation.resource.missing",
                         "Required portable presentation resource is unavailable", assetId, type)
        .withCause(std::move(cause));
}

struct BuiltResource final {
    PortableResourcePtr resource;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
    std::vector<PresentationResourceRef> dependencies;
};

[[nodiscard]] auto presentationRefByteCount(const PresentationResourceRef& reference) noexcept
    -> std::uint64_t {
    return 37ULL + reference.assetId.size();
}

} // namespace

auto preparePresentation(const chart::ChartRuntime& chartRuntime,
                         assets::ResourceManager* resourceManager)
    -> core::Result<std::optional<PreparedPresentation>> {
    try {
        auto requiredResult = collectRequiredResources(chartRuntime);
        if (!requiredResult) {
            return core::unexpected(std::move(requiredResult.error()));
        }
        auto& required = *requiredResult;
        if (required.size() > maxManifestEntries) {
            return core::unexpected(
                core::Error{"playback.presentation.session.budget_exceeded",
                            "Portable presentation manifest entry limit was exceeded"}
                    .withContext("limit", std::to_string(maxManifestEntries))
                    .withContext("actual", std::to_string(required.size())));
        }
        if (required.empty()) {
            PreparedPresentation empty;
            empty.manifest.totalDecodedBytes = 24;
            return std::optional<PreparedPresentation>{std::move(empty)};
        }
        if (resourceManager == nullptr) {
            const auto& first = *required.begin();
            return core::unexpected(
                resourceError("playback.presentation.resource.missing",
                              "Renderable chart has no AssetDatabase or ContentProvider",
                              first.first, first.second));
        }

        std::map<std::string, assets::MeshLease, std::less<>> meshLeases;
        std::map<std::string, assets::MaterialLease, std::less<>> materialLeases;
        bool portableCandidate = false;
        for (const auto& [assetId, type] : required) {
            const auto* record = resourceManager->database().find(assetId);
            if (record == nullptr || record->type != indexedType(type)) {
                auto error = resourceError(
                    "playback.presentation.resource.missing",
                    "Required presentation AssetId has a missing or incompatible Asset Index entry",
                    assetId, type);
                if (record != nullptr) {
                    error.withContext("actual", std::string{assets::assetTypeName(record->type)});
                }
                return core::unexpected(std::move(error));
            }
            if (type == PresentationResourceType::Mesh) {
                auto loaded = resourceManager->loadMesh(assets::AssetId{assetId});
                if (!loaded) {
                    return core::unexpected(
                        unavailableResource(assetId, type, std::move(loaded.error())));
                }
                portableCandidate = portableCandidate || looksPortable(loaded->resource().bytes());
                meshLeases.emplace(assetId, std::move(*loaded));
            } else {
                auto loaded = resourceManager->loadMaterial(assets::AssetId{assetId});
                if (!loaded) {
                    return core::unexpected(
                        unavailableResource(assetId, type, std::move(loaded.error())));
                }
                portableCandidate = portableCandidate || looksPortable(loaded->resource().bytes());
                materialLeases.emplace(assetId, std::move(*loaded));
            }
        }
        if (!portableCandidate) {
            return std::optional<PreparedPresentation>{};
        }

        std::map<std::string, ParsedMesh, std::less<>> parsedMeshes;
        std::map<std::string, ParsedMaterial, std::less<>> parsedMaterials;
        std::map<std::string, ParsedParameterizedMaterial, std::less<>> parsedParameterized;
        std::map<std::string, PresentationResourceType, std::less<>> requiredTextures;
        std::map<std::string, PresentationResourceType, std::less<>> requiredShaders;
        for (auto& [assetId, lease] : meshLeases) {
            const auto* record = resourceManager->database().find(assetId);
            if (record == nullptr || !record->dependencies.empty()) {
                return core::unexpected(resourceError("playback.presentation.dependency.mismatch",
                                                      "Portable Mesh dependency list must be empty",
                                                      assetId, PresentationResourceType::Mesh));
            }
            auto parsed = parseMesh(assetId, lease.resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            parsedMeshes.emplace(assetId, std::move(*parsed));
        }
        for (auto& [assetId, lease] : materialLeases) {
            const auto kind = peekPayloadKind(lease.resource().bytes());
            if (!kind || (*kind != payloadKind(PresentationResourceType::UnlitMaterial) &&
                          *kind != payloadKind(PresentationResourceType::ParameterizedMaterial))) {
                return core::unexpected(
                    payloadError("playback.presentation.payload.type_mismatch",
                                 "Portable payload kind does not match the Asset Index", assetId,
                                 PresentationResourceType::UnlitMaterial, 8));
            }
            if (*kind == payloadKind(PresentationResourceType::UnlitMaterial)) {
                auto parsed = parseMaterial(assetId, lease.resource().bytes());
                if (!parsed) {
                    return core::unexpected(std::move(parsed.error()));
                }
                const auto* record = resourceManager->database().find(assetId);
                const bool matchesNoTexture =
                    !parsed->textureAssetId && record != nullptr && record->dependencies.empty();
                const bool matchesTexture =
                    parsed->textureAssetId && record != nullptr &&
                    record->dependencies.size() == 1 &&
                    record->dependencies.front().value == *parsed->textureAssetId;
                if (!matchesNoTexture && !matchesTexture) {
                    return core::unexpected(resourceError(
                        "playback.presentation.dependency.mismatch",
                        "Portable Material payload and Asset Index dependencies differ", assetId,
                        PresentationResourceType::UnlitMaterial));
                }
                if (parsed->textureAssetId) {
                    const auto* textureRecord =
                        resourceManager->database().find(*parsed->textureAssetId);
                    if (textureRecord == nullptr ||
                        textureRecord->type != assets::AssetType::Texture) {
                        return core::unexpected(
                            resourceError("playback.presentation.dependency.mismatch",
                                          "Portable Material dependency is not a Texture2D AssetId",
                                          assetId, PresentationResourceType::UnlitMaterial)
                                .withContext("dependency", *parsed->textureAssetId));
                    }
                    requiredTextures.emplace(*parsed->textureAssetId,
                                             PresentationResourceType::Texture2D);
                }
                parsedMaterials.emplace(assetId, std::move(*parsed));
                continue;
            }

            auto parsed = parseParameterizedMaterial(assetId, lease.resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            required[assetId] = PresentationResourceType::ParameterizedMaterial;
            requiredShaders.emplace(parsed->shaderAssetId, PresentationResourceType::Shader);
            parsedParameterized.emplace(assetId, std::move(*parsed));
        }

        std::map<std::string, ParsedShader, std::less<>> parsedShaders;
        for (const auto& [assetId, type] : requiredShaders) {
            const auto* record = resourceManager->database().find(assetId);
            if (record == nullptr || record->type != assets::AssetType::Shader) {
                auto error = resourceError(
                    "playback.presentation.material.shader_reference_invalid",
                    "Parameterized Material shader AssetId is missing or not a Shader", assetId,
                    type);
                if (record != nullptr) {
                    error.withContext("actual", std::string{assets::assetTypeName(record->type)});
                }
                return core::unexpected(std::move(error));
            }
            if (!record->dependencies.empty()) {
                return core::unexpected(
                    resourceError("playback.presentation.dependency.mismatch",
                                  "Portable Shader dependency list must be empty", assetId, type));
            }
            auto loaded = resourceManager->loadShader(assets::AssetId{assetId});
            if (!loaded) {
                return core::unexpected(
                    unavailableResource(assetId, type, std::move(loaded.error())));
            }
            auto parsed = parseShader(assetId, loaded->resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            parsedShaders.emplace(assetId, std::move(*parsed));
        }

        for (auto& [assetId, parsed] : parsedParameterized) {
            const auto shader = parsedShaders.find(parsed.shaderAssetId);
            if (shader == parsedShaders.end()) {
                return core::unexpected(
                    resourceError("playback.presentation.material.shader_reference_invalid",
                                  "Parameterized Material shader was not prepared", assetId,
                                  PresentationResourceType::ParameterizedMaterial)
                        .withContext("dependency", parsed.shaderAssetId));
            }
            if (auto completed =
                    completeParameterizedMaterial(assetId, parsed, shader->second.value);
                !completed) {
                return core::unexpected(std::move(completed.error()));
            }
            const auto* record = resourceManager->database().find(assetId);
            std::vector<std::string> expected{parsed.shaderAssetId};
            expected.insert(expected.end(), parsed.textureAssetIds.begin(),
                            parsed.textureAssetIds.end());
            std::sort(expected.begin(), expected.end());
            expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
            std::vector<std::string> actual;
            if (record != nullptr) {
                actual.reserve(record->dependencies.size());
                for (const auto& dependency : record->dependencies) {
                    actual.push_back(dependency.value);
                }
            }
            if (record == nullptr || actual != expected) {
                return core::unexpected(
                    resourceError("playback.presentation.dependency.mismatch",
                                  "Portable Material payload and Asset Index dependencies differ",
                                  assetId, PresentationResourceType::ParameterizedMaterial));
            }
            for (const auto& textureAssetId : parsed.textureAssetIds) {
                const auto* textureRecord = resourceManager->database().find(textureAssetId);
                if (textureRecord == nullptr || textureRecord->type != assets::AssetType::Texture) {
                    return core::unexpected(
                        resourceError("playback.presentation.dependency.mismatch",
                                      "Portable Material dependency is not a Texture2D AssetId",
                                      assetId, PresentationResourceType::ParameterizedMaterial)
                            .withContext("dependency", textureAssetId));
                }
                requiredTextures.emplace(textureAssetId, PresentationResourceType::Texture2D);
            }
        }

        const auto totalEntries =
            required.size() + requiredTextures.size() + requiredShaders.size();
        if (totalEntries > maxManifestEntries) {
            return core::unexpected(
                core::Error{"playback.presentation.session.budget_exceeded",
                            "Portable presentation manifest entry limit was exceeded"}
                    .withContext("limit", std::to_string(maxManifestEntries))
                    .withContext("actual", std::to_string(totalEntries)));
        }

        std::map<std::string, ParsedTexture, std::less<>> parsedTextures;
        for (const auto& [assetId, type] : requiredTextures) {
            auto loaded = resourceManager->loadTexture(assets::AssetId{assetId});
            if (!loaded) {
                return core::unexpected(
                    unavailableResource(assetId, type, std::move(loaded.error())));
            }
            const auto* record = resourceManager->database().find(assetId);
            if (record == nullptr || !record->dependencies.empty()) {
                return core::unexpected(resourceError(
                    "playback.presentation.dependency.mismatch",
                    "Portable Texture2D dependency list must be empty", assetId, type));
            }
            auto parsed = parseTexture(assetId, loaded->resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            parsedTextures.emplace(assetId, std::move(*parsed));
        }

        std::map<PresentationResourceKey, BuiltResource> built;
        std::map<std::array<std::uint8_t, 32>, PortableResourcePtr> identities;
        const auto addResource =
            [&](PortableResourcePtr resource, std::uint64_t encodedByteCount,
                std::uint64_t decodedByteCount,
                std::vector<PresentationResourceRef> dependencies) -> core::Result<void> {
            const auto identity = resource->reference.identity.sha256;
            const auto collision = identities.find(identity);
            if (collision != identities.end() &&
                !resourceValuesEqual(collision->second->value, resource->value)) {
                return core::unexpected(resourceError(
                    "playback.presentation.identity_collision",
                    "Different portable resource values produced the same semantic identity",
                    resource->reference.assetId, resource->reference.type));
            }
            identities.try_emplace(identity, resource);
            const PresentationResourceKey key{resource->reference.assetId,
                                              resource->reference.type};
            const auto [unused, inserted] =
                built.emplace(key, BuiltResource{std::move(resource), encodedByteCount,
                                                 decodedByteCount, std::move(dependencies)});
            if (!inserted) {
                return core::unexpected(
                    resourceError("playback.presentation.reference.invalid",
                                  "Portable presentation contains a duplicate AssetId and type",
                                  key.assetId, key.type));
            }
            return {};
        };

        for (auto& [assetId, parsed] : parsedTextures) {
            PresentationResourceRef reference{PresentationResourceType::Texture2D, assetId,
                                              textureIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, {});
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
        for (auto& [assetId, parsed] : parsedMeshes) {
            PresentationResourceRef reference{PresentationResourceType::Mesh, assetId,
                                              meshIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, {});
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
        for (auto& [assetId, parsed] : parsedShaders) {
            PresentationResourceRef reference{PresentationResourceType::Shader, assetId,
                                              shaderIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, {});
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
        for (auto& [assetId, parsed] : parsedMaterials) {
            std::vector<PresentationResourceRef> dependencies;
            if (parsed.textureAssetId) {
                const auto found = built.find(PresentationResourceKey{
                    *parsed.textureAssetId, PresentationResourceType::Texture2D});
                if (found == built.end()) {
                    return core::unexpected(
                        resourceError("playback.presentation.resource.missing",
                                      "Portable Material texture dependency was not prepared",
                                      assetId, PresentationResourceType::UnlitMaterial));
                }
                parsed.value.baseColorTexture = found->second.resource->reference;
                dependencies.push_back(found->second.resource->reference);
            }
            PresentationResourceRef reference{PresentationResourceType::UnlitMaterial, assetId,
                                              materialIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, std::move(dependencies));
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
        for (auto& [assetId, parsed] : parsedParameterized) {
            std::vector<PresentationResourceRef> dependencies;
            const auto shader = built.find(
                PresentationResourceKey{parsed.shaderAssetId, PresentationResourceType::Shader});
            if (shader == built.end()) {
                return core::unexpected(
                    resourceError("playback.presentation.material.shader_reference_invalid",
                                  "Parameterized Material shader dependency was not prepared",
                                  assetId, PresentationResourceType::ParameterizedMaterial));
            }
            parsed.value.shader = shader->second.resource->reference;
            dependencies.push_back(shader->second.resource->reference);
            for (auto& parameter : parsed.value.parameters) {
                if (!parameter.texture) {
                    continue;
                }
                const auto found = built.find(PresentationResourceKey{
                    parameter.texture->assetId, PresentationResourceType::Texture2D});
                if (found == built.end()) {
                    return core::unexpected(
                        resourceError("playback.presentation.resource.missing",
                                      "Portable Material texture dependency was not prepared",
                                      assetId, PresentationResourceType::ParameterizedMaterial));
                }
                parameter.texture = found->second.resource->reference;
                dependencies.push_back(found->second.resource->reference);
            }
            PresentationResourceRef reference{PresentationResourceType::ParameterizedMaterial,
                                              assetId, parameterizedMaterialIdentity(parsed.value)};
            auto resource = std::make_shared<const PortableResource>(
                PortableResource{reference, std::move(parsed.value)});
            if (auto added = addResource(std::move(resource), parsed.encodedByteCount,
                                         parsed.decodedByteCount, std::move(dependencies));
                !added) {
                return core::unexpected(std::move(added.error()));
            }
        }

        PreparedPresentation presentation;
        presentation.manifest.entries.reserve(built.size());
        presentation.orderedResources.reserve(built.size());
        std::uint64_t totalEncodedBytes = 0;
        std::uint64_t totalResourceDecodedBytes = 0;
        std::uint64_t manifestBytes = 24;
        for (const auto& [key, resource] : built) {
            if (resource.encodedByteCount > maxSessionBytes - totalEncodedBytes ||
                resource.decodedByteCount > maxSessionBytes - totalResourceDecodedBytes) {
                return core::unexpected(
                    core::Error{"playback.presentation.session.budget_exceeded",
                                "Portable presentation resource byte total was exceeded"}
                        .withContext("limit", std::to_string(maxSessionBytes)));
            }
            totalEncodedBytes += resource.encodedByteCount;
            totalResourceDecodedBytes += resource.decodedByteCount;
            auto entryBytes = 20ULL + presentationRefByteCount(resource.resource->reference);
            for (const auto& dependency : resource.dependencies) {
                entryBytes += presentationRefByteCount(dependency);
            }
            if (entryBytes > maxSessionBytes - manifestBytes) {
                return core::unexpected(
                    core::Error{"playback.presentation.session.budget_exceeded",
                                "Portable presentation manifest byte total was exceeded"}
                        .withContext("limit", std::to_string(maxSessionBytes)));
            }
            manifestBytes += entryBytes;
            presentation.manifest.entries.push_back(
                PresentationManifestEntry{resource.resource->reference, resource.encodedByteCount,
                                          resource.decodedByteCount, resource.dependencies});
            presentation.resources.emplace(key, resource.resource);
            presentation.orderedResources.push_back(resource.resource);
        }
        if (totalEncodedBytes > maxSessionBytes ||
            totalResourceDecodedBytes > maxSessionBytes - manifestBytes) {
            const auto actual = totalResourceDecodedBytes + manifestBytes;
            return core::unexpected(
                core::Error{"playback.presentation.session.budget_exceeded",
                            "Portable presentation session byte budget was exceeded"}
                    .withContext("limit", std::to_string(maxSessionBytes))
                    .withContext("actual", std::to_string(actual)));
        }
        presentation.manifest.totalEncodedBytes = totalEncodedBytes;
        presentation.manifest.totalDecodedBytes = totalResourceDecodedBytes + manifestBytes;
        return std::optional<PreparedPresentation>{std::move(presentation)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.budget_exceeded",
                        "Portable presentation allocation could not be satisfied"}
                .withContext("limit", std::to_string(maxSessionBytes))
                .withContext("actual", "allocation_failed"));
    } catch (const std::exception& exception) {
        return core::unexpected(core::Error{"playback.presentation.prepare_failed",
                                            "Portable presentation preparation failed"}
                                    .withContext("exception", exception.what()));
    } catch (...) {
        return core::unexpected(core::Error{"playback.presentation.prepare_failed",
                                            "Portable presentation preparation failed"});
    }
}

auto findPresentationResource(const PreparedPresentation& presentation,
                              const PresentationResourceRef& reference) noexcept
    -> const PortableResourcePtr* {
    const auto found =
        presentation.resources.find(PresentationResourceKey{reference.assetId, reference.type});
    if (found == presentation.resources.end() || found->second->reference != reference) {
        return nullptr;
    }
    return &found->second;
}

auto findPresentationResource(const PreparedPresentation& presentation, std::string_view assetId,
                              PresentationResourceType type) noexcept
    -> const PortableResourcePtr* {
    const auto found = std::find_if(
        presentation.resources.begin(), presentation.resources.end(), [&](const auto& entry) {
            return entry.first.assetId == assetId && entry.first.type == type;
        });
    if (found == presentation.resources.end()) {
        return nullptr;
    }
    return &found->second;
}

} // namespace cuexis::playback::detail
