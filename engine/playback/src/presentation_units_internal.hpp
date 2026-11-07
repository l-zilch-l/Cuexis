#pragma once

// The portable presentation payload is read by a byte reader and a set of per-resource parsers,
// then turned into content identities and prepared resources. Both halves share the reader and the
// parsed resource shapes, so they live here.

#include "presentation_internal.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis_internal/sha256.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cuexis::playback::detail {

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
        return PresentationContentIdentity{sha_.finish()};
    }

  private:
    core::detail::Sha256 sha_;
};

struct BuiltResource final {
    PortableResourcePtr resource;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
    std::vector<PresentationResourceRef> dependencies;
};

[[nodiscard]] auto indexedType(PresentationResourceType type) noexcept -> assets::AssetType;

[[nodiscard]] auto payloadKind(PresentationResourceType type) noexcept -> std::uint32_t;

[[nodiscard]] auto resourceError(std::string code, std::string message, std::string_view assetId,
                                 PresentationResourceType type) -> core::Error;

[[nodiscard]] auto payloadError(std::string code, std::string message, std::string_view assetId,
                                PresentationResourceType type, std::size_t byteOffset)
    -> core::Error;

[[nodiscard]] auto budgetError(std::string_view assetId, PresentationResourceType type,
                               std::uint64_t limit, std::uint64_t actual) -> core::Error;

[[nodiscard]] auto checkedAdd(std::size_t left, std::size_t right, std::size_t& result) noexcept
    -> bool;

[[nodiscard]] auto checkedAddU64(std::uint64_t left, std::uint64_t right,
                                 std::uint64_t& result) noexcept -> bool;

[[nodiscard]] auto validateEnvelope(std::span<const std::byte> bytes, std::string_view assetId,
                                    PresentationResourceType type) -> core::Result<ByteReader>;

[[nodiscard]] auto validateExactSize(std::size_t expected, std::size_t actual,
                                     std::string_view assetId, PresentationResourceType type)
    -> core::Result<void>;

[[nodiscard]] auto parseMesh(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedMesh>;

[[nodiscard]] auto parseTexture(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedTexture>;

[[nodiscard]] auto isPortableAssetId(std::string_view value) noexcept -> bool;

[[nodiscard]] auto bytesToString(std::span<const std::byte> bytes) -> std::string;

[[nodiscard]] auto isUserIdentifier(std::string_view value) noexcept -> bool;

[[nodiscard]] auto peekPayloadKind(std::span<const std::byte> bytes) noexcept
    -> std::optional<std::uint32_t>;

[[nodiscard]] auto readCountedBytes(ByteReader& reader, std::uint32_t byteCount,
                                    std::string_view assetId, PresentationResourceType type)
    -> core::Result<std::span<const std::byte>>;

[[nodiscard]] auto validateShaderSource(std::string_view source, std::string_view assetId,
                                        PresentationResourceType type, std::size_t byteOffset)
    -> core::Result<void>;

[[nodiscard]] auto unusedNumericLanesZero(ShaderParameterType type,
                                          const std::array<float, 4>& numeric) noexcept -> bool;

[[nodiscard]] auto parseMaterial(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedMaterial>;

[[nodiscard]] auto parseShader(std::string_view assetId, std::span<const std::byte> bytes)
    -> core::Result<ParsedShader>;

[[nodiscard]] auto parseParameterizedMaterial(std::string_view assetId,
                                              std::span<const std::byte> bytes)
    -> core::Result<ParsedParameterizedMaterial>;

[[nodiscard]] auto completeParameterizedMaterial(std::string_view assetId,
                                                 ParsedParameterizedMaterial& parsed,
                                                 const PortableShader& shader)
    -> core::Result<void>;

[[nodiscard]] auto meshIdentity(const PortableMesh& mesh) noexcept -> PresentationContentIdentity;

[[nodiscard]] auto textureIdentity(const PortableTexture2D& texture) noexcept
    -> PresentationContentIdentity;

[[nodiscard]] auto materialIdentity(const PortableUnlitMaterial& material) noexcept
    -> PresentationContentIdentity;

[[nodiscard]] auto shaderIdentity(const PortableShader& shader) noexcept
    -> PresentationContentIdentity;

[[nodiscard]] auto
parameterizedMaterialIdentity(const PortableParameterizedMaterial& material) noexcept
    -> PresentationContentIdentity;

[[nodiscard]] auto resourceValuesEqual(const PortableResourceValue& left,
                                       const PortableResourceValue& right) noexcept -> bool;

[[nodiscard]] auto looksPortable(std::span<const std::byte> bytes) noexcept -> bool;

[[nodiscard]] auto collectRequiredResources(const chart::ChartRuntime& chartRuntime)
    -> core::Result<std::map<std::string, PresentationResourceType, std::less<>>>;

[[nodiscard]] auto unavailableResource(std::string_view assetId, PresentationResourceType type,
                                       core::Error cause) -> core::Error;

[[nodiscard]] auto presentationRefByteCount(const PresentationResourceRef& reference) noexcept
    -> std::uint64_t;

} // namespace cuexis::playback::detail
