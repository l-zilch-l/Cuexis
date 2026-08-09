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
    const auto& leftMaterial = std::get<PortableUnlitMaterial>(left);
    const auto& rightMaterial = std::get<PortableUnlitMaterial>(right);
    return std::equal(std::begin(leftMaterial.baseColor), std::end(leftMaterial.baseColor),
                      std::begin(rightMaterial.baseColor)) &&
           leftMaterial.alphaMode == rightMaterial.alphaMode &&
           leftMaterial.doubleSided == rightMaterial.doubleSided &&
           leftMaterial.baseColorTexture == rightMaterial.baseColorTexture;
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
    return kind >= 1 && kind <= 3 && version == 1 && byteCount == bytes.size();
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
        std::map<std::string, PresentationResourceType, std::less<>> requiredTextures;
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
            auto parsed = parseMaterial(assetId, lease.resource().bytes());
            if (!parsed) {
                return core::unexpected(std::move(parsed.error()));
            }
            const auto* record = resourceManager->database().find(assetId);
            const bool matchesNoTexture =
                !parsed->textureAssetId && record != nullptr && record->dependencies.empty();
            const bool matchesTexture =
                parsed->textureAssetId && record != nullptr && record->dependencies.size() == 1 &&
                record->dependencies.front().value == *parsed->textureAssetId;
            if (!matchesNoTexture && !matchesTexture) {
                return core::unexpected(
                    resourceError("playback.presentation.dependency.mismatch",
                                  "Portable Material payload and Asset Index dependencies differ",
                                  assetId, PresentationResourceType::UnlitMaterial));
            }
            if (parsed->textureAssetId) {
                const auto* textureRecord =
                    resourceManager->database().find(*parsed->textureAssetId);
                if (textureRecord == nullptr || textureRecord->type != assets::AssetType::Texture) {
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
        }

        if (required.size() + requiredTextures.size() > maxManifestEntries) {
            return core::unexpected(
                core::Error{"playback.presentation.session.budget_exceeded",
                            "Portable presentation manifest entry limit was exceeded"}
                    .withContext("limit", std::to_string(maxManifestEntries))
                    .withContext("actual",
                                 std::to_string(required.size() + requiredTextures.size())));
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
