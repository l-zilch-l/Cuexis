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
#include <string_view>
#include <variant>
#include <vector>

namespace cuexis::playback {

CUEXIS_ABI_WARNING_PUSH

enum class PresentationResourceType : std::uint8_t {
    Mesh = 1,
    Texture2D = 2,
    UnlitMaterial = 3,
    Shader = 4,
    ParameterizedMaterial = 5,
};

enum class PresentationColorSpace : std::uint8_t {
    Linear = 1,
    Srgb = 2,
};

enum class PresentationAlphaMode : std::uint8_t {
    Opaque = 1,
    Blend = 2,
};

enum class ShaderStage : std::uint8_t {
    Vertex = 1,
    Fragment = 2,
};

enum class ShaderParameterType : std::uint8_t {
    Float = 1,
    Vec2 = 2,
    Vec3 = 3,
    Vec4 = 4,
    Int = 5,
    Bool = 6,
    Texture2D = 7,
};

enum class RendererProfileKind : std::uint8_t {
    Portable = 1,
    BuiltIn = 2,
    HostExtension = 3,
};

inline constexpr std::string_view importerProfileShaderV1 = "cuexis.importer.shader.v1";
inline constexpr std::string_view targetProfileSpirvV1 = "cuexis.target.spirv.v1";
inline constexpr std::string_view targetProfileGlsl330V1 = "cuexis.target.glsl330.v1";
inline constexpr std::string_view targetProfileGlslEs300V1 = "cuexis.target.glsles300.v1";
inline constexpr std::string_view rendererProfilePortableV1 = "cuexis.renderer.portable.v1";
inline constexpr std::string_view rendererProfileBuiltInV1 = "cuexis.renderer.builtin.v1";

inline constexpr std::uint64_t presentationMaxShaderSourceBytes = 262144;
inline constexpr std::uint64_t presentationMaxSpirvBytes = 1048576;
inline constexpr std::uint32_t presentationMaxVariantKeywords = 4;
inline constexpr std::uint32_t presentationMaxVariantsPerShader = 16;
inline constexpr std::uint32_t presentationMaxMaterialParameters = 32;
inline constexpr std::uint32_t presentationMaxTextureBindings = 8;

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

struct ShaderBinding final {
    std::uint32_t set{};
    std::uint32_t binding{};
    ShaderParameterType type{ShaderParameterType::Float};
    std::string name;
};

struct ShaderParameterSchemaEntry final {
    std::string name;
    ShaderParameterType type{ShaderParameterType::Float};
    std::uint32_t set{};
    std::uint32_t binding{};
    std::array<float, 4> defaultNumeric{};
    std::int32_t defaultInt{};
    bool defaultBool{};
};

struct PortableShader final {
    std::string vertexSource;
    std::string fragmentSource;
    std::string vertexEntry{"main"};
    std::string fragmentEntry{"main"};
    std::vector<std::string> variantKeywords;
    std::vector<ShaderParameterSchemaEntry> parameters;
    std::vector<ShaderBinding> bindings;
    PresentationAlphaMode defaultAlphaMode{PresentationAlphaMode::Opaque};
    bool defaultDoubleSided{};
    std::string requiredRendererProfile;
    std::vector<std::string> requiredHostExtensions;
};

struct ShaderParameterValue final {
    std::string name;
    ShaderParameterType type{ShaderParameterType::Float};
    std::array<float, 4> numeric{};
    std::int32_t integer{};
    bool boolean{};
    std::optional<PresentationResourceRef> texture;
};

struct PortableParameterizedMaterial final {
    PresentationResourceRef shader;
    PresentationAlphaMode alphaMode{PresentationAlphaMode::Opaque};
    bool doubleSided{};
    std::vector<std::string> selectedKeywords;
    std::vector<ShaderParameterValue> parameters;
};

using PortableResourceValue = std::variant<PortableMesh, PortableTexture2D, PortableUnlitMaterial,
                                           PortableShader, PortableParameterizedMaterial>;

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
    // Version 2 additive Built-in Renderer fields. Version 1 adapters leave these default.
    std::uint32_t builtInRendererProfileVersion{};
    bool parameterizedMaterial{};
    bool shaderGlsl450Source{};
    bool shaderSpirv{};
    bool shaderGlsl330{};
    bool shaderGlslEs300{};
    bool declaredVariants{};
    std::uint64_t maxShaderSourceBytes{};
    std::uint64_t maxSpirvBytes{};
    std::uint32_t maxVariantKeywords{};
    std::uint32_t maxVariantsPerShader{};
    std::uint32_t maxMaterialParameters{};
    std::uint32_t maxTextureBindings{};
    std::vector<std::string> hostExtensionIds{};
};

struct PresentationRequest final {
    std::uint32_t version{1};
    std::uint32_t portableProfileVersion{1};
    bool enableDebugPass{};
    // Version 2 additive fields. Version 1 requests ignore these values.
    bool enableShaderCompile{};
    bool enableShaderHotReload{};
};

struct EffectivePresentationSettings final {
    std::uint32_t version{1};
    std::uint32_t portableProfileVersion{1};
    bool debugPassEnabled{};
    // Version 2 additive fields. Present when request.version == 2.
    bool shaderCompileEnabled{};
    bool shaderHotReloadEnabled{};
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
