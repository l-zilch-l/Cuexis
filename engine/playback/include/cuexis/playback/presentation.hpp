#pragma once

// Portable Presentation Profile v1 public value types.
// These types own their strings and arrays and expose no provider, resource-manager, or GPU state.

#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cuexis::playback {

CUEXIS_ABI_WARNING_PUSH

enum class PresentationResourceType : std::uint8_t {
    Mesh = 1,
    Texture2D = 2,
    UnlitMaterial = 3,
};

enum class PresentationColorSpace : std::uint8_t {
    Linear = 1,
    Srgb = 2,
};

enum class PresentationAlphaMode : std::uint8_t {
    Opaque = 1,
    Blend = 2,
};

struct PresentationContentIdentity final {
    std::array<std::uint8_t, 32> sha256{};

    friend bool operator==(const PresentationContentIdentity&,
                           const PresentationContentIdentity&) = default;
};

struct PresentationResourceRef final {
    PresentationResourceType type{PresentationResourceType::Mesh};
    std::string assetId;
    PresentationContentIdentity identity;

    friend bool operator==(const PresentationResourceRef&,
                           const PresentationResourceRef&) = default;
};

struct PortableMesh final {
    std::vector<float> positions;
    std::vector<float> uv0;
    std::vector<std::uint32_t> indices;
    float boundsMin[3]{};
    float boundsMax[3]{};
};

struct PortableTexture2D final {
    std::uint32_t width{};
    std::uint32_t height{};
    PresentationColorSpace colorSpace{PresentationColorSpace::Linear};
    std::vector<std::byte> pixelsRgba8;
};

struct PortableUnlitMaterial final {
    float baseColor[4]{1.0F, 1.0F, 1.0F, 1.0F};
    PresentationAlphaMode alphaMode{PresentationAlphaMode::Opaque};
    bool doubleSided{};
    std::optional<PresentationResourceRef> baseColorTexture;
};

using PortableResourceValue = std::variant<PortableMesh, PortableTexture2D, PortableUnlitMaterial>;

struct PortableResource final {
    PresentationResourceRef reference;
    PortableResourceValue value;
};

using PortableResourcePtr = std::shared_ptr<const PortableResource>;

struct PresentationManifestEntry final {
    PresentationResourceRef reference;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
    std::vector<PresentationResourceRef> dependencies;
};

struct PresentationResourceManifest final {
    std::uint32_t version{1};
    std::vector<PresentationManifestEntry> entries;
    std::uint64_t totalEncodedBytes{};
    std::uint64_t totalDecodedBytes{};
};

struct PresentationCapabilities final {
    std::uint32_t version{1};
    std::uint32_t portableProfileVersion{1};
    bool opaquePass{};
    bool transparentPass{};
    bool linearTexture{};
    bool srgbTexture{};
    bool straightAlphaBlend{};
    bool backFaceCulling{};
    bool doubleSided{};
    bool debugPass{};
    std::uint64_t maxResourceBytes{};
    std::uint64_t maxTotalDecodedBytes{};
    std::uint32_t maxTextureDimension{};
    std::uint32_t maxMeshVertices{};
    std::uint32_t maxMeshIndices{};
};

struct PresentationRequest final {
    std::uint32_t version{1};
    std::uint32_t portableProfileVersion{1};
    bool enableDebugPass{};
};

struct EffectivePresentationSettings final {
    std::uint32_t version{1};
    std::uint32_t portableProfileVersion{1};
    bool debugPassEnabled{};
};

struct PresentationValidationResult final {
    std::optional<EffectivePresentationSettings> settings;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept {
        return settings.has_value() && !diagnostics.hasErrors();
    }
};

class PresentationCandidateToken final {
  public:
    friend bool operator==(const PresentationCandidateToken&,
                           const PresentationCandidateToken&) = default;

  private:
    friend class PreparedPlayback;
    friend class PlaybackSession;

    std::uint64_t sessionToken_{};
    std::uint64_t candidateGeneration_{};
};

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::playback
