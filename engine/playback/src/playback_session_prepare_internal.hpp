#pragma once

// Prepare helpers shared by the capability, stage and session units: the diagnostics recorder,
// the prepare context and artifact, and the declarations of every helper another unit calls.

#include "playback_session_internal.hpp"

#include <cuexis/chart/candidate_lowering.hpp>
#include <cuexis/chart/chart_runtime.hpp>

namespace cuexis::playback {
namespace prepare_detail {

class PrepareDiagnosticsRecorder final {
  public:
    PrepareDiagnosticsRecorder(core::Diagnostics& destination,
                               core::Diagnostics& diagnostics) noexcept
        : destination_(destination), diagnostics_(diagnostics) {}

    PrepareDiagnosticsRecorder(const PrepareDiagnosticsRecorder&) = delete;
    auto operator=(const PrepareDiagnosticsRecorder&) -> PrepareDiagnosticsRecorder& = delete;

    ~PrepareDiagnosticsRecorder() noexcept {
        try {
            diagnostics_.sortDeterministically();
            destination_ = std::move(diagnostics_);
        } catch (...) {
            destination_.clear();
        }
    }

  private:
    core::Diagnostics& destination_;
    core::Diagnostics& diagnostics_;
};

struct PrepareContext final {
    PlaybackSession& owner;
    PlaybackSource& source;
    std::string_view chartJson;
    PlaybackMode mode;
    const RuntimeFrame* targetFrame{};
    ReloadPolicy policy{ReloadPolicy::KeepChartTime};
    bool replacement{};
    const PlaybackPrepareOptions& options;
    const chart::ChartLimits& limits;
    const PlaybackCapabilitySet& capabilities;
};

struct PrepareArtifact final {
    bool isV4{};
    std::optional<chart::ChartDocument> document;
    std::optional<chart::ChartV4SourceDocument> v4Source;
    std::shared_ptr<const chart::detail::ParsedChartInput> v4Input;
    std::optional<chart::AnimationProgramInput> animationProgram;
    std::optional<chart::ChartV4ResolvedArtifact> v4Artifact;
    std::optional<chart::CanonicalContentIdentity> chartIdentity;
    std::optional<chart::CanonicalContentIdentity> parameterIdentity;
    std::optional<chart::CandidateRuntimeMetadata> candidateMetadata;
    std::vector<chart::CxtIdentityComponent> cxtIdentities;
    std::vector<chart::ChartResourceRequirement> resourceRequirements;
    std::vector<std::string> additionalCapabilities;
    std::unique_ptr<assets::ResourceManager> resourceManager;
    std::optional<assets::AudioSourceLease> audioSourceLease;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    std::optional<runtime::PreparedRuntimeSession> preparedRuntime;
    std::optional<detail::PreparedPresentation> presentation;
    std::optional<RuntimeFrame> committedFrame;
    std::optional<SnapshotLayout> snapshotLayout;
};

using detail::presentationResourceTypeName;
using detail::snapshotResourceMismatch;

struct RequiredCapability final {
    std::string_view id;
    std::string_view fieldPath;
};

struct CapabilityFieldRule final {
    std::string_view id;
    std::string_view fieldPath;
};

constexpr std::array capabilityFieldRules{
    CapabilityFieldRule{capabilityChartV4, "$/version"},
    CapabilityFieldRule{capabilitySourceCxcV1, "$/source"},
    CapabilityFieldRule{capabilitySourceCxtV1, "$/animationTemplateImports"},
    CapabilityFieldRule{capabilityAnimationClipV1, "$/animationClips"},
    CapabilityFieldRule{capabilityAnimationLayersV1, "$/objects"},
    CapabilityFieldRule{capabilityShaderAssetV1, "$/objects"},
    CapabilityFieldRule{capabilityMaterialParameterizedV1, "$/objects"},
};

struct PresentationCapabilityRule final {
    bool PresentationCapabilities::* supported;
    std::string_view id;
    std::string_view fieldPath;
};

constexpr std::array presentationCapabilityRules{
    PresentationCapabilityRule{&PresentationCapabilities::opaquePass, "opaque_pass",
                               "$/capabilities/opaquePass"},
    PresentationCapabilityRule{&PresentationCapabilities::transparentPass, "transparent_pass",
                               "$/capabilities/transparentPass"},
    PresentationCapabilityRule{&PresentationCapabilities::linearTexture, "linear_texture",
                               "$/capabilities/linearTexture"},
    PresentationCapabilityRule{&PresentationCapabilities::srgbTexture, "srgb_texture",
                               "$/capabilities/srgbTexture"},
    PresentationCapabilityRule{&PresentationCapabilities::straightAlphaBlend,
                               "straight_alpha_blend", "$/capabilities/straightAlphaBlend"},
    PresentationCapabilityRule{&PresentationCapabilities::backFaceCulling, "back_face_culling",
                               "$/capabilities/backFaceCulling"},
    PresentationCapabilityRule{&PresentationCapabilities::doubleSided, "double_sided",
                               "$/capabilities/doubleSided"},
};

using PresentationLimitAccessor = std::variant<std::uint64_t PresentationCapabilities::*,
                                               std::uint32_t PresentationCapabilities::*>;

struct PresentationLimitRule final {
    std::string_view id;
    std::string_view fieldPath;
    PresentationLimitAccessor value;
};

constexpr std::array basePresentationLimitRules{
    PresentationLimitRule{"max_resource_bytes", "$/capabilities/maxResourceBytes",
                          &PresentationCapabilities::maxResourceBytes},
    PresentationLimitRule{"max_total_decoded_bytes", "$/capabilities/maxTotalDecodedBytes",
                          &PresentationCapabilities::maxTotalDecodedBytes},
    PresentationLimitRule{"max_texture_dimension", "$/capabilities/maxTextureDimension",
                          &PresentationCapabilities::maxTextureDimension},
    PresentationLimitRule{"max_mesh_vertices", "$/capabilities/maxMeshVertices",
                          &PresentationCapabilities::maxMeshVertices},
    PresentationLimitRule{"max_mesh_indices", "$/capabilities/maxMeshIndices",
                          &PresentationCapabilities::maxMeshIndices},
};

struct RequiredPresentationLimitRule final {
    std::string_view id;
    std::uint64_t required;
    std::string_view fieldPath;
    PresentationLimitAccessor value;
};

constexpr std::array parameterizedPresentationLimitRules{
    RequiredPresentationLimitRule{"max_shader_source_bytes", presentationMaxShaderSourceBytes,
                                  "$/capabilities/maxShaderSourceBytes",
                                  &PresentationCapabilities::maxShaderSourceBytes},
    RequiredPresentationLimitRule{"max_spirv_bytes", presentationMaxSpirvBytes,
                                  "$/capabilities/maxSpirvBytes",
                                  &PresentationCapabilities::maxSpirvBytes},
    RequiredPresentationLimitRule{"max_variant_keywords", presentationMaxVariantKeywords,
                                  "$/capabilities/maxVariantKeywords",
                                  &PresentationCapabilities::maxVariantKeywords},
    RequiredPresentationLimitRule{"max_variants_per_shader", presentationMaxVariantsPerShader,
                                  "$/capabilities/maxVariantsPerShader",
                                  &PresentationCapabilities::maxVariantsPerShader},
    RequiredPresentationLimitRule{"max_material_parameters", presentationMaxMaterialParameters,
                                  "$/capabilities/maxMaterialParameters",
                                  &PresentationCapabilities::maxMaterialParameters},
    RequiredPresentationLimitRule{"max_texture_bindings", presentationMaxTextureBindings,
                                  "$/capabilities/maxTextureBindings",
                                  &PresentationCapabilities::maxTextureBindings},
};

inline [[nodiscard]] auto prepareExceptionError(std::string_view operation, bool allocationFailure,
                                                const std::exception* exception = nullptr)
    -> core::Error {
    auto error = core::Error{allocationFailure ? "playback.prepare.budget_exceeded"
                                               : "playback.prepare.failed",
                             allocationFailure ? "Playback preparation could not allocate memory"
                                               : "Playback preparation failed"};
    error.withContext("operation", std::string{operation});
    if (exception != nullptr) {
        error.withContext("exception", exception->what());
    }
    return error;
}

inline void addPresentationDiagnostic(core::Diagnostics& diagnostics,
                                      core::DiagnosticSeverity severity, std::string code,
                                      std::string message, std::string fieldPath,
                                      std::string capability = {}, std::string actual = {},
                                      std::string limit = {}) {
    auto diagnostic =
        core::Diagnostic{severity, std::move(code), std::move(message), std::move(fieldPath)};
    if (!capability.empty()) {
        diagnostic.withContext("capability", std::move(capability));
    }
    if (!actual.empty()) {
        diagnostic.withContext("actual", std::move(actual));
    }
    if (!limit.empty()) {
        diagnostic.withContext("limit", std::move(limit));
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto operationError(std::string code, std::string message,
                                  const core::Diagnostics& diagnostics) -> core::Error;

void addErrorDiagnostic(core::Diagnostics& diagnostics, const core::Error& error);

void publishPrepareBoundaryDiagnostic(core::Diagnostics& destination,
                                      const core::Error& error) noexcept;

[[nodiscard]] auto mapHostOverrideError(const core::Error& error) -> core::Error;

[[nodiscard]] auto toWorldProperty(HostPropertyId property) noexcept -> world::PropertyId;

[[nodiscard]] auto toWorldLifetime(const HostOverrideLifetime& lifetime) noexcept
    -> world::OverrideLifetime;

[[nodiscard]] auto toWorldValue(const HostOverrideValue& value) -> world::PropertyValue;

[[nodiscard]] auto presentationDiagnostics() -> core::Diagnostics;

void addMissingPresentationCapability(core::Diagnostics& diagnostics, std::string_view capability,
                                      std::string_view fieldPath);

void addInsufficientPresentationLimit(core::Diagnostics& diagnostics, std::string_view capability,
                                      std::uint64_t limit, std::uint64_t actual,
                                      std::string_view fieldPath);

[[nodiscard]] auto presentationLimitValue(const PresentationCapabilities& capabilities,
                                          const PresentationLimitAccessor& accessor) noexcept
    -> std::uint64_t;

[[nodiscard]] auto allCapabilities() -> PlaybackCapabilitySet;

void normalizeCapabilities(PlaybackCapabilitySet& capabilities);

[[nodiscard]] auto preflightCapabilities(const chart::ChartDocument& document,
                                         std::span<const std::string> additionalRequirements,
                                         const PlaybackCapabilitySet& supported,
                                         core::Diagnostics& diagnostics) -> bool;

[[nodiscard]] auto compileAnimationProgram(std::optional<chart::AnimationProgramInput> input,
                                           const chart::ChartLimits& limits,
                                           core::Diagnostics& diagnostics)
    -> std::optional<animation::AnimationProgram>;

[[nodiscard]] auto chartParameterInputs(const PlaybackPrepareOptions& options,
                                        core::Diagnostics& diagnostics)
    -> std::optional<std::vector<chart::ChartParameterInput>>;

[[nodiscard]] auto projectDocuments(std::span<const PlaybackProjectDocument> sourceDocuments)
    -> std::vector<chart::ProjectDocument>;

[[nodiscard]] auto candidateResourceRequirements(const chart::CanonicalResourceClosure& closure,
                                                 core::Diagnostics& diagnostics)
    -> std::optional<std::vector<chart::ChartResourceRequirement>>;

[[nodiscard]] auto prepareCandidateStage(const cxc::CandidateChart& candidate, bool gameplaySource,
                                         const PlaybackPrepareOptions& options, bool replacement,
                                         const chart::ChartLimits& limits,
                                         PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::ChartRuntime>;

[[nodiscard]] auto loadDocumentStage(const PrepareContext& context, PrepareArtifact& artifact,
                                     core::Diagnostics& diagnostics) -> core::Result<void>;

[[nodiscard]] auto resolveParametersStage(const PrepareContext& context, PrepareArtifact& artifact,
                                          std::span<const PlaybackProjectDocument> sourceDocuments,
                                          core::Diagnostics& diagnostics) -> core::Result<void>;

[[nodiscard]] auto toPublicIdentity(const chart::CanonicalContentIdentity& identity) noexcept
    -> PreparedSemanticIdentity;

[[nodiscard]] auto chartInfoFor(const chart::ChartRuntime& chartRuntime, std::size_t resourceCount)
    -> ChartInfo;

[[nodiscard]] auto buildSnapshotLayout(runtime::RuntimeSession& session,
                                       const chart::ChartRuntime& chartRuntime,
                                       const detail::PreparedPresentation* presentation)
    -> core::Result<SnapshotLayout>;

[[nodiscard]] auto acquireAudioStage(PrepareArtifact& artifact,
                                     const chart::ChartRuntime& chartRuntime,
                                     core::Diagnostics& diagnostics) -> core::Result<void>;

[[nodiscard]] auto prepareRuntimeStage(PrepareArtifact& artifact,
                                       const chart::ChartRuntime& chartRuntime,
                                       animation::AnimationProgram animationProgram,
                                       core::Diagnostics& diagnostics) -> core::Result<void>;

[[nodiscard]] auto commitRuntimeStage(PrepareArtifact& artifact) -> core::Result<void>;

[[nodiscard]] auto
preparePresentationStage(const PrepareContext& context, PrepareArtifact& artifact,
                         const chart::ChartRuntime& chartRuntime, core::Diagnostics& diagnostics,
                         bool checkHostCapabilities = true) -> core::Result<void>;

[[nodiscard]] auto commitFrameStage(const PrepareContext& context, PrepareArtifact& artifact,
                                    core::Diagnostics& diagnostics) -> core::Result<void>;

[[nodiscard]] auto assembleIdentityStage(PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::CanonicalContentIdentity>;

} // namespace prepare_detail
} // namespace cuexis::playback
