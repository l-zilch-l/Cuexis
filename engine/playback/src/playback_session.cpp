// PlaybackSession host facade: load, absolute-time update, owning headless snapshots and reload.

#include <cuexis/playback/playback_session.hpp>

#include "playback_source_state.hpp"
#include "presentation_internal.hpp"

#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/assets/asset_database.hpp>
#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/chart_writer.hpp>
#include <cuexis/chart/detail/chart_dispatch_internal.hpp>
#include <cuexis/chart/detail/chart_v4_resolver_internal.hpp>
#include <cuexis/chart/prepared_semantic_identity.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::playback {
namespace {

std::atomic<std::uint64_t> nextPlaybackSessionToken{1};

[[nodiscard]] auto allocatePlaybackSessionToken() noexcept -> std::uint64_t {
    const auto token = nextPlaybackSessionToken.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        std::terminate();
    }
    return token;
}

void copyMatrix(const core::Mat4& source, float (&destination)[16]) noexcept {
    std::copy(source.values.begin(), source.values.end(), destination);
}

[[nodiscard]] auto runtimeFrame(const RuntimeFrame& frame) noexcept -> runtime::RuntimeFrame {
    return runtime::RuntimeFrame{.chartTimeMs = frame.chartTimeMs,
                                 .simulationDeltaTimeMs = frame.simulationDeltaTimeMs,
                                 .timeDiscontinuityId = frame.timeDiscontinuityId};
}

[[nodiscard]] auto operationError(std::string code, std::string message,
                                  const core::Diagnostics& diagnostics) -> core::Error {
    auto error = core::Error{std::move(code), std::move(message)};
    const auto firstError = std::find_if(
        diagnostics.items().begin(), diagnostics.items().end(), [](const core::Diagnostic& item) {
            return item.severity() == core::DiagnosticSeverity::Error;
        });
    if (firstError != diagnostics.items().end()) {
        error.withContext("diagnostic_code", std::string{firstError->code()});
        if (!firstError->fieldPath().empty()) {
            error.withContext("field_path", std::string{firstError->fieldPath()});
        }
    }
    return error;
}

void addErrorDiagnostic(core::Diagnostics& diagnostics, const core::Error& error) {
    std::string fieldPath;
    for (const auto& context : error.context()) {
        if (context.key == "field_path") {
            fieldPath = context.value;
            break;
        }
    }
    core::Diagnostic diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                std::string{error.message()}, std::move(fieldPath)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

// Publishes exactly one operation's diagnostics, including early returns and exceptions.
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

void publishPrepareBoundaryDiagnostic(core::Diagnostics& destination,
                                      const core::Error& error) noexcept {
    try {
        core::Diagnostics diagnostics;
        addErrorDiagnostic(diagnostics, error);
        diagnostics.sortDeterministically();
        destination = std::move(diagnostics);
    } catch (...) {
        destination.clear();
    }
}

struct SnapshotEntity final {
    chart::ChartObjectId id;
    entt::entity entity{entt::null};
    std::optional<PresentationResourceRef> mesh;
    std::map<std::string, PresentationResourceRef, std::less<>> materials;
    std::size_t maximumMeshAssetIdSize{};
    std::size_t maximumMaterialAssetIdSize{};
};

struct SnapshotLayout final {
    std::vector<SnapshotEntity> entities;
    std::optional<entt::entity> activeCamera;
};

// Prepare data stays internal and is moved into PreparedPlayback only after all stages succeed.
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

[[nodiscard]] auto ownerError(std::string_view operation) -> core::Error {
    return core::Error{"playback.session.not_owner_thread",
                       "PlaybackSession belongs to another thread"}
        .withContext("operation", std::string{operation});
}

[[nodiscard]] auto copyError(std::string_view code, const core::Error& source) -> core::Error {
    auto error = core::Error{std::string{code}, std::string{source.message()}};
    for (const auto& context : source.context()) {
        error.withContext(context.key, context.value);
    }
    return error;
}

[[nodiscard]] auto mapHostOverrideError(const core::Error& error) -> core::Error {
    const auto code = error.code();
    if (code == "runtime.override.empty") {
        return copyError("playback.override.empty", error);
    }
    if (code == "runtime.override.object_missing") {
        return copyError("playback.override.object_missing", error);
    }
    if (code == "runtime.override.token_missing") {
        return copyError("playback.override.token_missing", error);
    }
    if (code == "world.property.override_mask") {
        return copyError("playback.override.mask", error);
    }
    if (code == "runtime.session.empty") {
        return copyError("playback.session.empty", error);
    }
    if (code == "runtime.session.not_owner_thread") {
        return copyError("playback.session.not_owner_thread", error);
    }
    if (code == "runtime.session.callback_reentrant") {
        return copyError("playback.session.reentrant", error);
    }
    return error;
}

[[nodiscard]] auto toWorldProperty(HostPropertyId property) noexcept -> world::PropertyId {
    return static_cast<world::PropertyId>(static_cast<std::uint8_t>(property));
}

[[nodiscard]] auto toWorldLifetime(const HostOverrideLifetime& lifetime) noexcept
    -> world::OverrideLifetime {
    return world::OverrideLifetime{
        .kind = static_cast<world::OverrideLifetimeKind>(static_cast<std::uint8_t>(lifetime.kind)),
        .remainingFrames = lifetime.remainingFrames,
        .untilChartTimeMs = lifetime.untilChartTimeMs,
    };
}

[[nodiscard]] auto toWorldValue(const HostOverrideValue& value) -> world::PropertyValue {
    return std::visit([](const auto& item) -> world::PropertyValue { return item; }, value);
}

[[nodiscard]] auto reentryError(std::string_view operation) -> core::Error {
    return core::Error{"playback.session.reentrant",
                       "PlaybackSession cannot be re-entered while an operation is active"}
        .withContext("operation", std::string{operation});
}

[[nodiscard]] auto prepareExceptionError(std::string_view operation, bool allocationFailure,
                                         const std::exception* exception = nullptr) -> core::Error {
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

[[nodiscard]] auto presentationResourceTypeName(PresentationResourceType type) noexcept
    -> std::string_view {
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

constexpr std::size_t maxPresentationDiagnostics = 1024;

[[nodiscard]] auto presentationDiagnostics() -> core::Diagnostics {
    return core::Diagnostics{
        maxPresentationDiagnostics,
        core::Diagnostic{core::DiagnosticSeverity::Error,
                         "playback.presentation.diagnostics.limit_exceeded",
                         "Presentation diagnostics reached the configured limit", "$"}
            .withContext("max_diagnostics", std::to_string(maxPresentationDiagnostics))};
}

void addPresentationDiagnostic(core::Diagnostics& diagnostics, core::DiagnosticSeverity severity,
                               std::string code, std::string message, std::string fieldPath,
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

void addMissingPresentationCapability(core::Diagnostics& diagnostics, std::string_view capability,
                                      std::string_view fieldPath) {
    addPresentationDiagnostic(diagnostics, core::DiagnosticSeverity::Error,
                              "playback.presentation.capability.required_missing",
                              "Portable presentation requires an unsupported adapter capability",
                              std::string{fieldPath}, std::string{capability});
}

void addInsufficientPresentationLimit(core::Diagnostics& diagnostics, std::string_view capability,
                                      std::uint64_t limit, std::uint64_t actual,
                                      std::string_view fieldPath) {
    addPresentationDiagnostic(diagnostics, core::DiagnosticSeverity::Error,
                              "playback.presentation.capability.limit_insufficient",
                              "Adapter presentation limit is below the candidate requirement",
                              std::string{fieldPath}, std::string{capability},
                              std::to_string(actual), std::to_string(limit));
}

[[nodiscard]] auto snapshotResourceMismatch(std::string_view objectId, std::string_view assetId,
                                            PresentationResourceType type) -> core::Error {
    return core::Error{"playback.presentation.frame.resource_mismatch",
                       "Snapshot presentation resource could not be resolved"}
        .withContext("object_id", std::string{objectId})
        .withContext("asset_id", std::string{assetId})
        .withContext("resource_type", std::string{presentationResourceTypeName(type)});
}

void assignPresentationReference(std::optional<PresentationResourceRef>& destination,
                                 const PresentationResourceRef& source,
                                 std::size_t assetIdCapacity) {
    if (!destination) {
        destination.emplace();
    }
    if (destination->assetId.capacity() < assetIdCapacity) {
        destination->assetId.reserve(assetIdCapacity);
    }
    destination->type = source.type;
    if (destination->assetId != source.assetId) {
        destination->assetId = source.assetId;
    }
    destination->identity = source.identity;
}

class SessionOperation final {
  public:
    explicit SessionOperation(bool& active) noexcept : active_(active) {
        active_ = true;
    }

    ~SessionOperation() noexcept {
        active_ = false;
    }

    SessionOperation(const SessionOperation&) = delete;
    auto operator=(const SessionOperation&) -> SessionOperation& = delete;

  private:
    bool& active_;
};

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

[[nodiscard]] auto presentationLimitValue(const PresentationCapabilities& capabilities,
                                          const PresentationLimitAccessor& accessor) noexcept
    -> std::uint64_t {
    return std::visit(
        [&capabilities](auto member) { return static_cast<std::uint64_t>(capabilities.*member); },
        accessor);
}

[[nodiscard]] auto allCapabilities() -> PlaybackCapabilitySet {
    return PlaybackCapabilitySet{
        .version = 1,
        .ids = {std::string{capabilityAnimationClipV1}, std::string{capabilityAnimationLayersV1},
                std::string{capabilityBehaviorEventV1}, std::string{capabilityChartV3},
                std::string{capabilityChartV4}, std::string{capabilityMaterialSnapshotV1},
                std::string{capabilityRenderVisibilityV1}, std::string{capabilitySourceCxcV1},
                std::string{capabilitySourceCxtV1}, std::string{capabilityShaderAssetV1},
                std::string{capabilityMaterialParameterizedV1}},
    };
}

void normalizeCapabilities(PlaybackCapabilitySet& capabilities) {
    std::sort(capabilities.ids.begin(), capabilities.ids.end());
    capabilities.ids.erase(std::unique(capabilities.ids.begin(), capabilities.ids.end()),
                           capabilities.ids.end());
}

[[nodiscard]] auto requiredCapabilities(const chart::ChartDocument& document)
    -> std::vector<RequiredCapability> {
    std::vector<RequiredCapability> required;
    if (document.version == 3) {
        required.push_back({capabilityChartV3, "$/version"});
    }
    for (std::size_t index = 0; index < document.behaviors.size(); ++index) {
        const auto& behavior = document.behaviors[index];
        if (behavior.type == "behavior.event") {
            required.push_back({capabilityBehaviorEventV1, "$/behaviors"});
        }
        for (const auto& event : behavior.events) {
            if (event.property == chart::BehaviorProperty::MaterialOpacity ||
                event.property == chart::BehaviorProperty::MaterialTint) {
                required.push_back({capabilityMaterialSnapshotV1, "$/behaviors"});
            }
        }
        for (const auto& event : behavior.stepEvents) {
            if (event.property == chart::BehaviorStepProperty::RenderVisible) {
                required.push_back({capabilityRenderVisibilityV1, "$/behaviors"});
            } else if (event.property == chart::BehaviorStepProperty::RenderMaterial) {
                required.push_back({capabilityMaterialSnapshotV1, "$/behaviors"});
            }
        }
    }
    if (std::any_of(document.objects.begin(), document.objects.end(),
                    [](const auto& object) { return object.components.renderable.has_value(); })) {
        required.push_back({capabilityMaterialSnapshotV1, "$/objects"});
    }
    std::sort(required.begin(), required.end(), [](const auto& left, const auto& right) {
        return std::tie(left.id, left.fieldPath) < std::tie(right.id, right.fieldPath);
    });
    required.erase(
        std::unique(required.begin(), required.end(),
                    [](const auto& left, const auto& right) { return left.id == right.id; }),
        required.end());
    return required;
}

[[nodiscard]] auto capabilityFieldPath(std::string_view capability) noexcept -> std::string_view {
    for (const auto& rule : capabilityFieldRules) {
        if (rule.id == capability) {
            return rule.fieldPath;
        }
    }
    return "$";
}

[[nodiscard]] auto preflightCapabilities(const chart::ChartDocument& document,
                                         std::span<const std::string> additionalRequirements,
                                         const PlaybackCapabilitySet& supported,
                                         core::Diagnostics& diagnostics) -> bool {
    if (supported.version != 1) {
        diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "playback.capability.version_unsupported",
            "Playback capability set version is unsupported", "$/capabilities/version"}
                            .withContext("version", std::to_string(supported.version)));
        return false;
    }
    auto required = requiredCapabilities(document);
    required.reserve(required.size() + additionalRequirements.size());
    for (const auto& capability : additionalRequirements) {
        required.push_back({capability, capabilityFieldPath(capability)});
    }
    std::sort(required.begin(), required.end(), [](const auto& left, const auto& right) {
        return std::tie(left.id, left.fieldPath) < std::tie(right.id, right.fieldPath);
    });
    required.erase(
        std::unique(required.begin(), required.end(),
                    [](const auto& left, const auto& right) { return left.id == right.id; }),
        required.end());
    for (const auto& capability : required) {
        if (!std::binary_search(supported.ids.begin(), supported.ids.end(), capability.id)) {
            diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                             "playback.capability.unsupported",
                                             "Chart requires an unsupported playback capability",
                                             std::string{capability.fieldPath}}
                                .withContext("capability", std::string{capability.id}));
        }
    }
    diagnostics.sortDeterministically();
    return !diagnostics.hasErrors();
}

[[nodiscard]] auto compileAnimationProgram(std::optional<chart::AnimationProgramInput> input,
                                           const chart::ChartLimits& limits,
                                           core::Diagnostics& diagnostics)
    -> std::optional<animation::AnimationProgram> {
    if (!input.has_value()) {
        return animation::AnimationProgram{};
    }
    auto compiled = animation::AnimationCompiler::compile(std::move(*input), limits);
    diagnostics.append(std::move(compiled.diagnostics));
    if (!compiled.hasValue()) {
        diagnostics.sortDeterministically();
        return std::nullopt;
    }
    return std::move(*compiled.program);
}

[[nodiscard]] auto chartParameterInputs(const PlaybackPrepareOptions& options,
                                        core::Diagnostics& diagnostics)
    -> std::optional<std::vector<chart::ChartParameterInput>> {
    std::vector<chart::ChartParameterInput> inputs;
    inputs.reserve(options.parameters.values.size());
    for (std::size_t index = 0; index < options.parameters.values.size(); ++index) {
        const auto& parameter = options.parameters.values[index];
        chart::ChartParameterInput input;
        input.id = parameter.id;
        if (const auto* number = std::get_if<ChartParameterNumber>(&parameter.value)) {
            input.type = chart::ChartParameterType::Number;
            input.value = number->value;
        } else if (const auto* rational = std::get_if<ChartParameterRational>(&parameter.value)) {
            auto value = chart::RationalBeat::create(rational->numerator, rational->denominator);
            if (!value) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "chart.parameter.out_of_range",
                    "Rational parameter input is outside the supported range",
                    "$/parameterInputs/" + std::to_string(index) + "/value"});
                continue;
            }
            input.type = chart::ChartParameterType::Rational;
            input.value = *value;
        } else {
            const auto& weight = std::get<ChartParameterWeight>(parameter.value);
            input.type = chart::ChartParameterType::Weight;
            input.value = weight.value;
        }
        inputs.push_back(std::move(input));
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return std::nullopt;
    }
    return inputs;
}

[[nodiscard]] auto projectDocuments(std::span<const PlaybackProjectDocument> sourceDocuments)
    -> std::vector<chart::ProjectDocument> {
    std::vector<chart::ProjectDocument> documents;
    documents.reserve(sourceDocuments.size());
    for (const auto& document : sourceDocuments) {
        documents.push_back({document.path, document.utf8Text});
    }
    return documents;
}

[[nodiscard]] auto loadDocumentStage(const PrepareContext& context, PrepareArtifact& artifact,
                                     core::Diagnostics& diagnostics) -> core::Result<void> {
    auto loaded = chart::detail::loadChartForPlayback(context.chartJson, context.limits);
    artifact.isV4 = loaded.isV4;
    const bool valid = loaded.hasValue();
    diagnostics.append(std::move(loaded.diagnostics));
    if (!valid) {
        return core::unexpected(core::Error{"playback.prepare.stage.load_document_failed",
                                            artifact.isV4 ? "Chart v4 loading produced errors"
                                                          : "Chart loading produced errors"});
    }
    if (artifact.isV4) {
        artifact.v4Source = std::move(*loaded.v4Document);
        artifact.v4Input = std::move(loaded.v4Input);
        return {};
    }
    artifact.document.emplace(std::move(*loaded.legacyDocument));
    // Legacy ChartDocument is a runtime-facing projection. Identity must use the complete
    // canonical source bytes so legacy timing, template, and keyframe fields are not erased.
    auto canonical = chart::ChartWriter::writeCanonicalJson(context.chartJson, context.limits);
    if (!canonical) {
        addErrorDiagnostic(diagnostics, canonical.error());
        return core::unexpected(std::move(canonical.error()));
    }
    artifact.chartIdentity = chart::canonicalBytesIdentity(*canonical);
    artifact.parameterIdentity = chart::emptyParameterIdentity();
    return {};
}

[[nodiscard]] auto resolveParametersStage(const PrepareContext& context, PrepareArtifact& artifact,
                                          std::span<const PlaybackProjectDocument> sourceDocuments,
                                          core::Diagnostics& diagnostics) -> core::Result<void> {
    if (!artifact.v4Source) {
        for (std::size_t index = 0; index < context.options.parameters.values.size(); ++index) {
            diagnostics.add(
                core::Diagnostic{core::DiagnosticSeverity::Error, "chart.parameter.unknown",
                                 "Host parameter input is not supported by this Chart version",
                                 "$/parameterInputs/" + std::to_string(index) + "/id"});
        }
        if (diagnostics.hasErrors()) {
            diagnostics.sortDeterministically();
            return core::unexpected(core::Error{"playback.prepare.stage.resolve_parameters_failed",
                                                "Chart parameter resolution produced errors"});
        }
        return {};
    }

    auto inputs = chartParameterInputs(context.options, diagnostics);
    if (!inputs) {
        return core::unexpected(core::Error{"playback.prepare.stage.resolve_parameters_failed",
                                            "Chart parameter conversion produced errors"});
    }
    auto documents = projectDocuments(sourceDocuments);
    auto resolved = artifact.v4Input
                        ? chart::detail::resolveV4Parsed(*artifact.v4Source, *artifact.v4Input,
                                                         *inputs, documents, {}, context.limits)
                        : chart::ChartV4Resolver::resolve(*artifact.v4Source, *inputs, documents,
                                                          {}, context.limits);
    artifact.v4Input.reset();
    const bool valid = resolved.hasValue();
    diagnostics.append(std::move(resolved.diagnostics));
    if (!valid) {
        return core::unexpected(core::Error{"playback.prepare.stage.resolve_parameters_failed",
                                            "Chart v4 resolution produced errors"});
    }
    auto resolvedArtifact = std::move(*resolved.artifact);
    artifact.additionalCapabilities.insert(artifact.additionalCapabilities.end(),
                                           resolvedArtifact.capabilityRequirements.begin(),
                                           resolvedArtifact.capabilityRequirements.end());
    artifact.document.emplace(resolvedArtifact.document.chart);
    artifact.animationProgram = std::move(resolvedArtifact.animationProgram);
    artifact.chartIdentity = resolvedArtifact.chartIdentity;
    artifact.parameterIdentity = resolvedArtifact.parameterIdentity;
    artifact.cxtIdentities = resolvedArtifact.cxtIdentities;
    artifact.resourceRequirements = resolvedArtifact.resourceRequirements;
    artifact.v4Artifact = std::move(resolvedArtifact);
    return {};
}

[[nodiscard]] auto toPublicIdentity(const chart::CanonicalContentIdentity& identity) noexcept
    -> PreparedSemanticIdentity {
    return PreparedSemanticIdentity{identity.sha256};
}

[[nodiscard]] auto
assembleResourceIdentities(std::span<const chart::ChartResourceRequirement> requirements,
                           const std::optional<assets::AudioSourceLease>& audioSourceLease,
                           const std::optional<detail::PreparedPresentation>& presentation,
                           core::Diagnostics& diagnostics)
    -> std::optional<std::vector<chart::PreparedResourceIdentityComponent>> {
    std::vector<chart::PreparedResourceIdentityComponent> identities;
    identities.reserve(requirements.size());
    for (const auto& requirement : requirements) {
        std::optional<chart::CanonicalContentIdentity> identity;
        const bool wantsAudio =
            std::ranges::any_of(requirement.uses, [](chart::ChartResourceUse use) {
                return use == chart::ChartResourceUse::MainMusic;
            });
        const bool wantsPresentation =
            std::ranges::any_of(requirement.uses, [](chart::ChartResourceUse use) {
                return use != chart::ChartResourceUse::MainMusic;
            });
        if (wantsAudio) {
            if (!audioSourceLease || !audioSourceLease->valid() ||
                audioSourceLease->resource().id.value != requirement.assetId.value) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "playback.identity.resource_missing",
                    "Prepared semantic identity is missing a fetched main music resource",
                    "$/resources/" + requirement.assetId.value});
                continue;
            }
            identity = chart::audioContentIdentity(audioSourceLease->resource().bytes());
        }
        if (wantsPresentation) {
            if (!presentation) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "playback.identity.resource_missing",
                    "Prepared semantic identity is missing a fetched presentation resource",
                    "$/resources/" + requirement.assetId.value});
                continue;
            }
            const auto found = std::ranges::find_if(
                presentation->manifest.entries, [&](const PresentationManifestEntry& entry) {
                    return entry.reference.assetId == requirement.assetId.value;
                });
            if (found == presentation->manifest.entries.end()) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "playback.identity.resource_missing",
                    "Prepared semantic identity is missing a fetched presentation resource",
                    "$/resources/" + requirement.assetId.value});
                continue;
            }
            chart::CanonicalContentIdentity presentationIdentity{found->reference.identity.sha256};
            if (identity && *identity != presentationIdentity) {
                diagnostics.add(core::Diagnostic{
                    core::DiagnosticSeverity::Error, "playback.identity.resource_collision",
                    "The same AssetId produced different prepared resource identities",
                    "$/resources/" + requirement.assetId.value});
                continue;
            }
            identity = presentationIdentity;
        }
        if (!identity) {
            diagnostics.add(core::Diagnostic{
                core::DiagnosticSeverity::Error, "playback.identity.resource_missing",
                "Prepared semantic identity could not resolve a required resource",
                "$/resources/" + requirement.assetId.value});
            continue;
        }
        identities.push_back({requirement.assetId, *identity});
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return std::nullopt;
    }
    return identities;
}

[[nodiscard]] auto chartInfoFor(const chart::ChartRuntime& chartRuntime, std::size_t resourceCount)
    -> ChartInfo {
    return ChartInfo{
        .objectCount = chartRuntime.objects.size(),
        .behaviorCount = chartRuntime.behaviors.size(),
        .renderableCount = static_cast<std::size_t>(
            std::count_if(chartRuntime.objects.begin(), chartRuntime.objects.end(),
                          [](const chart::RuntimeObject& object) {
                              return object.components.renderable.has_value();
                          })),
        .resourceCount = resourceCount,
    };
}

[[nodiscard]] auto buildSnapshotLayout(runtime::RuntimeSession& session,
                                       const chart::ChartRuntime& chartRuntime,
                                       const detail::PreparedPresentation* presentation)
    -> core::Result<SnapshotLayout> {
    SnapshotLayout layout;
    layout.entities.reserve(chartRuntime.objects.size());
    for (const auto& object : chartRuntime.objects) {
        auto entity = session.findEntity(object.id);
        if (!entity) {
            return core::unexpected(std::move(entity.error()));
        }
        if (!entity->has_value()) {
            return core::unexpected(core::Error{"playback.snapshot.object_missing",
                                                "Runtime object mapping is incomplete"}
                                        .withContext("object_id", object.id.value));
        }
        SnapshotEntity snapshotEntity{
            .id = object.id,
            .entity = **entity,
            .mesh = std::nullopt,
            .materials = {},
            .maximumMeshAssetIdSize = 0,
            .maximumMaterialAssetIdSize = 0,
        };
        if (object.components.renderable) {
            snapshotEntity.maximumMeshAssetIdSize = object.components.renderable->mesh.value.size();
            snapshotEntity.maximumMaterialAssetIdSize =
                object.components.renderable->material.value.size();
            if (presentation != nullptr) {
                const auto* mesh = detail::findPresentationResource(
                    *presentation, object.components.renderable->mesh.value,
                    PresentationResourceType::Mesh);
                if (mesh == nullptr) {
                    return core::unexpected(snapshotResourceMismatch(
                        object.id.value, object.components.renderable->mesh.value,
                        PresentationResourceType::Mesh));
                }
                snapshotEntity.mesh = (*mesh)->reference;

                const auto addMaterial = [&](std::string_view assetId) -> core::Result<void> {
                    const auto* unlit = detail::findPresentationResource(
                        *presentation, assetId, PresentationResourceType::UnlitMaterial);
                    const auto* parameterized = detail::findPresentationResource(
                        *presentation, assetId, PresentationResourceType::ParameterizedMaterial);
                    const auto* material = unlit != nullptr ? unlit : parameterized;
                    if (material == nullptr) {
                        return core::unexpected(snapshotResourceMismatch(
                            object.id.value, assetId, PresentationResourceType::UnlitMaterial));
                    }
                    if ((*material)->reference.type == PresentationResourceType::Shader) {
                        return core::unexpected(snapshotResourceMismatch(
                            object.id.value, assetId, PresentationResourceType::Shader));
                    }
                    snapshotEntity.maximumMaterialAssetIdSize =
                        std::max(snapshotEntity.maximumMaterialAssetIdSize, assetId.size());
                    snapshotEntity.materials.emplace(std::string{assetId}, (*material)->reference);
                    return {};
                };
                if (auto added = addMaterial(object.components.renderable->material.value);
                    !added) {
                    return core::unexpected(std::move(added.error()));
                }
            }
        }
        if (object.components.behavior) {
            const auto behavior =
                std::lower_bound(chartRuntime.behaviors.begin(), chartRuntime.behaviors.end(),
                                 object.components.behavior->behavior,
                                 [](const chart::RuntimeBehavior& candidate,
                                    const chart::BehaviorId& id) { return candidate.id < id; });
            if (behavior != chartRuntime.behaviors.end() &&
                behavior->id == object.components.behavior->behavior) {
                for (const auto& track : behavior->stepTracks) {
                    if (track.property != chart::BehaviorStepProperty::RenderMaterial) {
                        continue;
                    }
                    for (const auto& event : track.events) {
                        if (const auto* material = std::get_if<chart::AssetId>(&event.value)) {
                            snapshotEntity.maximumMaterialAssetIdSize = std::max(
                                snapshotEntity.maximumMaterialAssetIdSize, material->value.size());
                            if (presentation != nullptr && object.components.renderable) {
                                const auto* unlit = detail::findPresentationResource(
                                    *presentation, material->value,
                                    PresentationResourceType::UnlitMaterial);
                                const auto* parameterized = detail::findPresentationResource(
                                    *presentation, material->value,
                                    PresentationResourceType::ParameterizedMaterial);
                                const auto* resource = unlit != nullptr ? unlit : parameterized;
                                if (resource == nullptr) {
                                    return core::unexpected(snapshotResourceMismatch(
                                        object.id.value, material->value,
                                        PresentationResourceType::UnlitMaterial));
                                }
                                snapshotEntity.materials.emplace(material->value,
                                                                 (*resource)->reference);
                            }
                        }
                    }
                }
            }
        }
        layout.entities.push_back(std::move(snapshotEntity));
    }

    auto camera = session.withWorld(
        [&](const world::World& world) -> core::Result<std::optional<entt::entity>> {
            return world.withRegistry([&](const entt::registry& registry)
                                          -> core::Result<std::optional<entt::entity>> {
                for (std::size_t index = 0; index < chartRuntime.objects.size(); ++index) {
                    if (chartRuntime.objects[index].components.camera.has_value() &&
                        registry.all_of<render::CameraComponent>(layout.entities[index].entity)) {
                        return layout.entities[index].entity;
                    }
                }

                std::optional<entt::entity> selected;
                const auto view = registry.view<const render::CameraComponent>();
                for (const entt::entity entity : view) {
                    if (!selected.has_value() ||
                        entt::to_integral(entity) < entt::to_integral(*selected)) {
                        selected = entity;
                    }
                }
                return selected;
            });
        });
    if (!camera) {
        return core::unexpected(std::move(camera.error()));
    }
    layout.activeCamera = *camera;
    return layout;
}

[[nodiscard]] auto acquireAudioStage(PrepareArtifact& artifact,
                                     const chart::ChartRuntime& chartRuntime,
                                     core::Diagnostics& diagnostics) -> core::Result<void> {
    if (!chartRuntime.mainMusic) {
        return {};
    }
    if (!artifact.resourceManager) {
        return core::unexpected(
            core::Error{"playback.content.asset_database_missing",
                        "A chart with main music requires an AssetDatabase and ContentProvider"});
    }
    auto sourceResult = artifact.resourceManager->requestAudioSource(
        assets::AssetId{chartRuntime.mainMusic->value}, assets::ResourcePolicy::Required);
    const bool sourceValid = sourceResult.hasValue();
    diagnostics.append(std::move(sourceResult.diagnostics));
    if (!sourceValid) {
        return core::unexpected(operationError("playback.content.main_music_failed",
                                               "Required main music source could not be prepared",
                                               diagnostics));
    }
    artifact.audioSourceLease.emplace(std::move(*sourceResult.lease));
    return {};
}

[[nodiscard]] auto prepareRuntimeStage(PrepareArtifact& artifact,
                                       const chart::ChartRuntime& chartRuntime,
                                       animation::AnimationProgram animationProgram,
                                       core::Diagnostics& diagnostics) -> core::Result<void> {
    artifact.runtimeSession =
        artifact.resourceManager
            ? std::make_unique<runtime::RuntimeSession>(*artifact.resourceManager)
            : std::make_unique<runtime::RuntimeSession>();
    auto runtimePrepared =
        artifact.runtimeSession->prepare(chartRuntime, std::move(animationProgram));
    const bool preparedValid = runtimePrepared.hasValue();
    diagnostics.append(std::move(runtimePrepared.diagnostics));
    if (!preparedValid) {
        return core::unexpected(operationError("playback.session.prepare_failed",
                                               "RuntimeSession preparation produced errors",
                                               diagnostics));
    }
    artifact.preparedRuntime = std::move(*runtimePrepared.prepared);
    return {};
}

[[nodiscard]] auto commitRuntimeStage(PrepareArtifact& artifact) -> core::Result<void> {
    if (!artifact.runtimeSession || !artifact.preparedRuntime) {
        return core::unexpected(core::Error{"playback.prepare.stage.commit_runtime_missing",
                                            "Runtime preparation artifact is unavailable"});
    }
    if (auto committed = artifact.runtimeSession->commit(std::move(*artifact.preparedRuntime));
        !committed) {
        return core::unexpected(std::move(committed.error()));
    }
    artifact.preparedRuntime.reset();
    return {};
}

[[nodiscard]] auto preparePresentationStage(const PrepareContext& context,
                                            PrepareArtifact& artifact,
                                            const chart::ChartRuntime& chartRuntime,
                                            core::Diagnostics& diagnostics) -> core::Result<void> {
    auto preparedPresentation = detail::preparePresentation(
        chartRuntime, artifact.resourceManager.get(), artifact.v4Artifact.has_value());
    if (!preparedPresentation) {
        addErrorDiagnostic(diagnostics, preparedPresentation.error());
        diagnostics.sortDeterministically();
        return core::unexpected(std::move(preparedPresentation.error()));
    }
    if (preparedPresentation->has_value()) {
        std::vector<std::string> presentationCapabilities;
        bool requiresShader = false;
        bool requiresParameterized = false;
        for (const auto& entry : preparedPresentation->value().manifest.entries) {
            if (entry.reference.type == PresentationResourceType::Shader) {
                requiresShader = true;
            } else if (entry.reference.type == PresentationResourceType::ParameterizedMaterial) {
                requiresParameterized = true;
                requiresShader = true;
            }
        }
        if (requiresShader) {
            presentationCapabilities.emplace_back(capabilityShaderAssetV1);
        }
        if (requiresParameterized) {
            presentationCapabilities.emplace_back(capabilityMaterialParameterizedV1);
        }
        if (!presentationCapabilities.empty() &&
            !preflightCapabilities(*artifact.document, presentationCapabilities,
                                   context.capabilities, diagnostics)) {
            return core::unexpected(operationError("playback.capability.preflight_failed",
                                                   "Playback capability preflight failed",
                                                   diagnostics));
        }
    }
    artifact.presentation = std::move(*preparedPresentation);
    return {};
}

[[nodiscard]] auto commitFrameStage(const PrepareContext& context, PrepareArtifact& artifact,
                                    core::Diagnostics& diagnostics) -> core::Result<void> {
    if (!context.replacement || context.targetFrame == nullptr) {
        return {};
    }
    artifact.committedFrame = *context.targetFrame;
    artifact.committedFrame->simulationDeltaTimeMs = 0.0;
    if (context.policy == ReloadPolicy::RestartAtZero) {
        artifact.committedFrame->chartTimeMs = 0.0;
    }
    if (auto updated = artifact.runtimeSession->update(runtimeFrame(*artifact.committedFrame));
        !updated) {
        addErrorDiagnostic(diagnostics, updated.error());
        diagnostics.sortDeterministically();
        return core::unexpected(operationError("playback.session.reload_sample_failed",
                                               "Reload target frame sampling failed", diagnostics));
    }
    return {};
}

[[nodiscard]] auto assembleIdentityStage(PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::CanonicalContentIdentity> {
    if (!artifact.v4Artifact) {
        if (artifact.audioSourceLease && artifact.audioSourceLease->valid()) {
            artifact.resourceRequirements.push_back(chart::ChartResourceRequirement{
                .assetId =
                    chart::AssetId{std::string{artifact.audioSourceLease->resource().id.value}},
                .uses = {chart::ChartResourceUse::MainMusic}});
        }
        if (artifact.presentation.has_value()) {
            for (const auto& entry : artifact.presentation->manifest.entries) {
                const auto existing =
                    std::ranges::find_if(artifact.resourceRequirements,
                                         [&](const chart::ChartResourceRequirement& item) {
                                             return item.assetId.value == entry.reference.assetId;
                                         });
                auto use = entry.reference.type == PresentationResourceType::Mesh
                               ? chart::ChartResourceUse::RenderableMesh
                               : chart::ChartResourceUse::RenderableMaterial;
                if (existing == artifact.resourceRequirements.end()) {
                    artifact.resourceRequirements.push_back(chart::ChartResourceRequirement{
                        .assetId = chart::AssetId{entry.reference.assetId}, .uses = {use}});
                } else if (std::ranges::find(existing->uses, use) == existing->uses.end()) {
                    existing->uses.push_back(use);
                }
            }
        }
    }
    auto resourceIdentities =
        assembleResourceIdentities(artifact.resourceRequirements, artifact.audioSourceLease,
                                   artifact.presentation, diagnostics);
    if (!resourceIdentities) {
        return core::unexpected(operationError("playback.identity.assemble_failed",
                                               "Prepared semantic identity could not be assembled",
                                               diagnostics));
    }
    return chart::assemblePreparedSemanticIdentity(*artifact.chartIdentity, artifact.cxtIdentities,
                                                   *resourceIdentities,
                                                   *artifact.parameterIdentity);
}

} // namespace

struct PlaybackSession::State final {
    State() = default;

    core::ThreadChecker ownerThread;
    // ResourceManager must outlive RuntimeSession and its ResourceScope.
    std::shared_ptr<content::IContentProvider> contentProvider;
    std::unique_ptr<assets::ResourceManager> resourceManager;
    std::string activeChartJson;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    SnapshotLayout snapshotLayout;
    std::optional<RuntimeFrame> lastFrame;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<ChartInfo> activeChartInfo;
    std::optional<PlaybackContentInfo> activeContentInfo;
    std::optional<PlaybackMode> activeMode;
    std::optional<PreparedSemanticIdentity> semanticIdentity;
    ChartParameterSet parameters;
    std::uint64_t sessionToken{allocatePlaybackSessionToken()};
    std::uint64_t generation{1};
    SessionState sessionState{SessionState::Empty};
    bool operationActive{};
    PlaybackCapabilitySet capabilities;
    std::optional<detail::PreparedPresentation> presentation;
    std::uint64_t nextCandidateGeneration{1};
};

struct PreparedPlayback::State final {
    PlaybackSession* owner{};
    std::uint64_t ownerToken{};
    std::uint64_t expectedGeneration{};
    bool replacement{};
    core::ThreadChecker ownerThread;
    std::shared_ptr<content::IContentProvider> contentProvider;
    std::unique_ptr<assets::ResourceManager> resourceManager;
    std::string chartJson;
    std::unique_ptr<runtime::RuntimeSession> runtimeSession;
    SnapshotLayout snapshotLayout;
    ChartInfo chartInfo;
    PlaybackContentInfo contentInfo;
    ChartParameterSet parameters;
    PreparedSemanticIdentity semanticIdentity;
    std::optional<assets::AudioSourceLease> audioSourceLease;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<RuntimeFrame> targetFrame;
    SessionState committedState{SessionState::Ready};
    std::optional<detail::PreparedPresentation> presentation;
    std::uint64_t candidateGeneration{};
};

static_assert(std::is_nothrow_move_assignable_v<core::Diagnostics>);
static_assert(std::is_nothrow_move_assignable_v<PlaybackContentInfo>);
static_assert(std::is_nothrow_move_assignable_v<detail::PreparedPresentation>);
static_assert(static_cast<std::uint8_t>(HostPropertyId::TransformPositionX) ==
              static_cast<std::uint8_t>(world::PropertyId::TransformPositionX));
static_assert(static_cast<std::uint8_t>(HostPropertyId::MaterialTint) ==
              static_cast<std::uint8_t>(world::PropertyId::MaterialTint));
static_assert(static_cast<std::uint8_t>(HostOverrideLifetimeKind::UntilReleased) ==
              static_cast<std::uint8_t>(world::OverrideLifetimeKind::UntilReleased));
static_assert(static_cast<std::uint8_t>(HostOverrideLifetimeKind::UntilChartTimeMs) ==
              static_cast<std::uint8_t>(world::OverrideLifetimeKind::UntilChartTimeMs));

PreparedPlayback::PreparedPlayback() noexcept = default;

PreparedPlayback::PreparedPlayback(std::unique_ptr<State> state) noexcept
    : state_(std::move(state)) {}

PreparedPlayback::~PreparedPlayback() {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
}

PreparedPlayback::PreparedPlayback(PreparedPlayback&& other) noexcept
    : state_(std::move(other.state_)) {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
}

auto PreparedPlayback::operator=(PreparedPlayback&& other) noexcept -> PreparedPlayback& {
    if ((state_ && !state_->ownerThread.isCurrent()) ||
        (other.state_ && !other.state_->ownerThread.isCurrent())) {
        std::terminate();
    }
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

bool PreparedPlayback::valid() const noexcept {
    return state_ && state_->ownerThread.isCurrent() && state_->runtimeSession != nullptr;
}

const PlaybackContentInfo* PreparedPlayback::contentInfo() const noexcept {
    return valid() ? &state_->contentInfo : nullptr;
}

std::optional<PreparedSemanticIdentity> PreparedPlayback::semanticIdentity() const noexcept {
    if (!valid()) {
        return std::nullopt;
    }
    return state_->semanticIdentity;
}

std::optional<MainMusicSourceView> PreparedPlayback::mainMusicSource() const noexcept {
    if (!valid() || !state_->audioSourceLease || !state_->audioSourceLease->valid()) {
        return std::nullopt;
    }
    const auto& source = state_->audioSourceLease->resource();
    return MainMusicSourceView{source.id.value, state_->contentInfo.timingOffsetMs,
                               source.blob->providerRevision, source.bytes()};
}

auto PreparedPlayback::presentationCandidateToken() const
    -> core::Result<PresentationCandidateToken> {
    if (!state_ || !state_->ownerThread.isCurrent()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    if (!state_->presentation) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.missing",
                        "PreparedPlayback has no portable presentation candidate"});
    }
    PresentationCandidateToken token;
    token.sessionToken_ = state_->ownerToken;
    token.candidateGeneration_ = state_->candidateGeneration;
    return token;
}

const PresentationResourceManifest* PreparedPlayback::presentationManifest() const noexcept {
    if (!valid() || !state_->presentation) {
        return nullptr;
    }
    return &state_->presentation->manifest;
}

auto PreparedPlayback::validatePresentation(const PresentationCapabilities& capabilities,
                                            const PresentationRequest& request) const
    -> PresentationValidationResult {
    PresentationValidationResult result;
    try {
        result.diagnostics = presentationDiagnostics();
        if (!state_ || !state_->ownerThread.isCurrent() || state_->runtimeSession == nullptr) {
            addPresentationDiagnostic(
                result.diagnostics, core::DiagnosticSeverity::Error, "playback.prepared.invalid",
                "PreparedPlayback is empty or belongs to another thread", "$/prepared");
            result.diagnostics.sortDeterministically();
            return result;
        }

        if (capabilities.version != 1 && capabilities.version != 2) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.version_unsupported",
                                      "Presentation capability set version is unsupported",
                                      "$/capabilities/version", "capabilities.version",
                                      std::to_string(capabilities.version), "1 or 2");
        }
        if (request.version != 1 && request.version != 2) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.version_unsupported",
                                      "Presentation request version is unsupported",
                                      "$/request/version", "request.version",
                                      std::to_string(request.version), "1 or 2");
        }
        if (capabilities.portableProfileVersion != 1) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.profile_unsupported",
                                      "Adapter portable presentation profile is unsupported",
                                      "$/capabilities/portableProfileVersion",
                                      "capabilities.portable_profile",
                                      std::to_string(capabilities.portableProfileVersion), "1");
        }
        if (request.portableProfileVersion != 1) {
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Error,
                                      "playback.presentation.capability.profile_unsupported",
                                      "Requested portable presentation profile is unsupported",
                                      "$/request/portableProfileVersion",
                                      "request.portable_profile",
                                      std::to_string(request.portableProfileVersion), "1");
        }

        if (state_->presentation) {
            for (const auto& rule : presentationCapabilityRules) {
                if (!(capabilities.*rule.supported)) {
                    addMissingPresentationCapability(result.diagnostics, rule.id, rule.fieldPath);
                }
            }

            std::uint64_t maxResourceBytes = 0;
            std::uint32_t maxTextureDimension = 0;
            std::uint32_t maxMeshVertices = 0;
            std::uint32_t maxMeshIndices = 0;
            for (const auto& entry : state_->presentation->manifest.entries) {
                maxResourceBytes = std::max(
                    maxResourceBytes, std::max(entry.encodedByteCount, entry.decodedByteCount));
            }
            for (const auto& resource : state_->presentation->orderedResources) {
                if (!resource) {
                    continue;
                }
                if (const auto* texture = std::get_if<PortableTexture2D>(&resource->value)) {
                    maxTextureDimension =
                        std::max(maxTextureDimension, std::max(texture->width, texture->height));
                } else if (const auto* mesh = std::get_if<PortableMesh>(&resource->value)) {
                    maxMeshVertices = std::max(
                        maxMeshVertices, static_cast<std::uint32_t>(mesh->positions.size() / 3U));
                    maxMeshIndices =
                        std::max(maxMeshIndices, static_cast<std::uint32_t>(mesh->indices.size()));
                }
            }

            const std::array<std::uint64_t, basePresentationLimitRules.size()> baseRequirements{
                maxResourceBytes, state_->presentation->manifest.totalDecodedBytes,
                maxTextureDimension, maxMeshVertices, maxMeshIndices};
            for (std::size_t index = 0; index < basePresentationLimitRules.size(); ++index) {
                const auto& rule = basePresentationLimitRules[index];
                const auto actual = presentationLimitValue(capabilities, rule.value);
                if (actual < baseRequirements[index]) {
                    addInsufficientPresentationLimit(result.diagnostics, rule.id, actual,
                                                     baseRequirements[index], rule.fieldPath);
                }
            }

            bool hasParameterizedMaterial = false;
            bool hasShader = false;
            bool hasDeclaredVariants = false;
            std::set<std::string> requiredHostExtensions;
            for (const auto& resource : state_->presentation->orderedResources) {
                if (!resource) {
                    continue;
                }
                if (const auto* shader = std::get_if<PortableShader>(&resource->value)) {
                    hasShader = true;
                    if (!shader->variantKeywords.empty()) {
                        hasDeclaredVariants = true;
                    }
                    for (const auto& extension : shader->requiredHostExtensions) {
                        requiredHostExtensions.insert(extension);
                    }
                } else if (std::holds_alternative<PortableParameterizedMaterial>(resource->value)) {
                    hasParameterizedMaterial = true;
                }
            }

            if (hasParameterizedMaterial &&
                (capabilities.version < 2 || !capabilities.parameterizedMaterial)) {
                addMissingPresentationCapability(result.diagnostics, "parameterized_material",
                                                 "$/capabilities/parameterizedMaterial");
            }

            if (capabilities.version == 2) {
                for (const auto& extension : requiredHostExtensions) {
                    if (std::find(capabilities.hostExtensionIds.begin(),
                                  capabilities.hostExtensionIds.end(),
                                  extension) == capabilities.hostExtensionIds.end()) {
                        addMissingPresentationCapability(result.diagnostics, extension,
                                                         "$/capabilities/hostExtensionIds");
                    }
                }
                if (hasDeclaredVariants && capabilities.parameterizedMaterial &&
                    !capabilities.declaredVariants) {
                    addMissingPresentationCapability(result.diagnostics, "declared_variants",
                                                     "$/capabilities/declaredVariants");
                }
                if (capabilities.parameterizedMaterial && (hasShader || hasParameterizedMaterial)) {
                    for (const auto& rule : parameterizedPresentationLimitRules) {
                        const auto actual = presentationLimitValue(capabilities, rule.value);
                        if (actual < rule.required) {
                            addInsufficientPresentationLimit(result.diagnostics, rule.id, actual,
                                                             rule.required, rule.fieldPath);
                        }
                    }
                }
            }
        }

        bool debugPassEnabled = request.enableDebugPass;
        if (request.enableDebugPass && !capabilities.debugPass) {
            debugPassEnabled = false;
            addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Warning,
                                      "playback.presentation.debug_unavailable",
                                      "Requested optional Debug presentation pass is unavailable",
                                      "$/request/enableDebugPass", "debug_pass", "unsupported");
        }

        bool shaderCompileEnabled = false;
        bool shaderHotReloadEnabled = false;
        if (request.version == 2) {
            const bool compileSupported = capabilities.version == 2 &&
                                          capabilities.shaderGlsl450Source &&
                                          capabilities.shaderSpirv && capabilities.shaderGlsl330;
            if (request.enableShaderCompile) {
                if (!compileSupported) {
                    addMissingPresentationCapability(result.diagnostics, "shader_compile",
                                                     "$/request/enableShaderCompile");
                } else {
                    shaderCompileEnabled = true;
                }
            }
            if (request.enableShaderHotReload) {
                if (!compileSupported) {
                    addPresentationDiagnostic(result.diagnostics, core::DiagnosticSeverity::Warning,
                                              "playback.presentation.shader_hot_reload_unavailable",
                                              "Requested shader hot reload is unavailable",
                                              "$/request/enableShaderHotReload",
                                              "shader_hot_reload", "unsupported");
                } else {
                    shaderHotReloadEnabled = true;
                }
            }
        }

        result.diagnostics.sortDeterministically();
        if (!result.diagnostics.hasErrors()) {
            if (request.version == 2) {
                result.settings = EffectivePresentationSettings{
                    .version = 2,
                    .portableProfileVersion = 1,
                    .debugPassEnabled = debugPassEnabled,
                    .shaderCompileEnabled = shaderCompileEnabled,
                    .shaderHotReloadEnabled = shaderHotReloadEnabled,
                };
            } else {
                result.settings = EffectivePresentationSettings{
                    .version = 1,
                    .portableProfileVersion = 1,
                    .debugPassEnabled = debugPassEnabled,
                };
            }
        }
        return result;
    } catch (const std::bad_alloc&) {
        result.settings.reset();
        result.diagnostics.clear();
        result.diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "playback.presentation.diagnostics.limit_exceeded",
            "Presentation validation could not allocate its bounded diagnostics", "$/prepared"});
        return result;
    } catch (const std::exception& exception) {
        result.settings.reset();
        result.diagnostics.clear();
        result.diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                                "playback.presentation.prepare_failed",
                                                "Presentation validation failed", "$/prepared"}
                                   .withContext("exception", exception.what()));
        return result;
    } catch (...) {
        result.settings.reset();
        result.diagnostics.clear();
        result.diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                                "playback.presentation.prepare_failed",
                                                "Presentation validation failed", "$/prepared"});
        return result;
    }
}

auto PreparedPlayback::acquirePresentationResource(const PresentationResourceRef& reference) const
    -> core::Result<PortableResourcePtr> {
    if (!state_ || !state_->ownerThread.isCurrent()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    if (!state_->presentation) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.missing",
                        "PreparedPlayback has no portable presentation candidate"});
    }
    const auto* resource = detail::findPresentationResource(*state_->presentation, reference);
    if (resource == nullptr) {
        return core::unexpected(
            core::Error{"playback.presentation.reference.invalid",
                        "Portable presentation resource reference is not in the candidate manifest"}
                .withContext("asset_id", reference.assetId)
                .withContext("resource_type",
                             std::string{presentationResourceTypeName(reference.type)}));
    }
    return *resource;
}

PlaybackSession::PlaybackSession() noexcept : PlaybackSession(allCapabilities()) {}

PlaybackSession::PlaybackSession(PlaybackCapabilitySet capabilities) noexcept
    : state_(std::make_unique<State>()) {
    normalizeCapabilities(capabilities);
    state_->capabilities = std::move(capabilities);
}

PlaybackSession::~PlaybackSession() {
    if (state_ && !state_->ownerThread.isCurrent()) {
        std::terminate();
    }
}

auto PlaybackSession::state() const -> core::Result<SessionState> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("state"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("state"));
    }
    return state_->sessionState;
}

auto PlaybackSession::capabilities() const -> core::Result<PlaybackCapabilitySet> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("capabilities"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("capabilities"));
    }
    try {
        return state_->capabilities;
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("capabilities", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("capabilities", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("capabilities", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("capabilities", false));
    }
}

auto PlaybackSession::prepareLoad(std::string_view jsonText, PlaybackMode mode)
    -> core::Result<PreparedPlayback> {
    return prepareLoad(jsonText, mode, PlaybackPrepareOptions{});
}

auto PlaybackSession::prepareLoad(std::string_view jsonText, PlaybackMode mode,
                                  const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_load"));
    }
    if (state_->operationActive) {
        auto error = reentryError("prepare_load");
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    try {
        auto source = PlaybackSource::fromChartText(std::string{jsonText});
        if (!source) {
            publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, source.error());
            return core::unexpected(std::move(source.error()));
        }
        return prepare(std::move(*source), mode, nullptr, ReloadPolicy::KeepChartTime, false,
                       options);
    } catch (const std::bad_alloc&) {
        auto error = prepareExceptionError("prepare_load", true);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    } catch (const std::exception& exception) {
        auto error = prepareExceptionError("prepare_load", false, &exception);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    } catch (...) {
        auto error = prepareExceptionError("prepare_load", false);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
}

auto PlaybackSession::prepareLoad(PlaybackSource&& source, PlaybackMode mode)
    -> core::Result<PreparedPlayback> {
    return prepareLoad(std::move(source), mode, PlaybackPrepareOptions{});
}

auto PlaybackSession::prepareLoad(PlaybackSource&& source, PlaybackMode mode,
                                  const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_load"));
    }
    if (state_->operationActive) {
        auto error = reentryError("prepare_load");
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    return prepare(std::move(source), mode, nullptr, ReloadPolicy::KeepChartTime, false, options);
}

auto PlaybackSession::prepareReload(std::string_view replacementJson,
                                    const RuntimeFrame& targetFrame, ReloadPolicy policy)
    -> core::Result<PreparedPlayback> {
    return prepareReload(replacementJson, targetFrame, policy, PlaybackPrepareOptions{});
}

auto PlaybackSession::prepareReload(std::string_view replacementJson,
                                    const RuntimeFrame& targetFrame, ReloadPolicy policy,
                                    const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_reload"));
    }
    if (state_->operationActive) {
        auto error = reentryError("prepare_reload");
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    if (!state_->activeMode) {
        auto error =
            core::Error{"playback.session.not_ready", "PlaybackSession has no active mode"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    try {
        auto source = PlaybackSource::fromChartText(std::string{replacementJson});
        if (!source) {
            publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, source.error());
            return core::unexpected(std::move(source.error()));
        }
        if (state_->resourceManager) {
            auto sourceState = std::move(source->state_);
            sourceState->provider = state_->contentProvider;
            sourceState->database.emplace(state_->resourceManager->database());
            source->state_ = std::move(sourceState);
        }
        return prepare(std::move(*source), *state_->activeMode, &targetFrame, policy, true,
                       options);
    } catch (const std::bad_alloc&) {
        auto error = prepareExceptionError("prepare_reload", true);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    } catch (const std::exception& exception) {
        auto error = prepareExceptionError("prepare_reload", false, &exception);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    } catch (...) {
        auto error = prepareExceptionError("prepare_reload", false);
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
}

auto PlaybackSession::prepareReload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                                    ReloadPolicy policy) -> core::Result<PreparedPlayback> {
    return prepareReload(std::move(replacement), targetFrame, policy, PlaybackPrepareOptions{});
}

auto PlaybackSession::prepareReload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                                    ReloadPolicy policy, const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("prepare_reload"));
    }
    if (state_->operationActive) {
        auto error = reentryError("prepare_reload");
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    if (!state_->activeMode) {
        auto error =
            core::Error{"playback.session.not_ready", "PlaybackSession has no active mode"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    return prepare(std::move(replacement), *state_->activeMode, &targetFrame, policy, true,
                   options);
}

auto PlaybackSession::prepare(PlaybackSource&& source, PlaybackMode mode,
                              const RuntimeFrame* targetFrame, ReloadPolicy policy,
                              bool replacement, const PlaybackPrepareOptions& options)
    -> core::Result<PreparedPlayback> {
    core::Diagnostics diagnostics;
    std::optional<PrepareDiagnosticsRecorder> diagnosticsRecorder;
    try {
        if (!state_->ownerThread.isCurrent()) {
            return core::unexpected(ownerError(replacement ? "prepare_reload" : "prepare_load"));
        }
        diagnosticsRecorder.emplace(state_->lastOperationDiagnostics, diagnostics);
        if (state_->operationActive) {
            auto error = reentryError(replacement ? "prepare_reload" : "prepare_load");
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }
        SessionOperation operation{state_->operationActive};
        if ((!replacement && state_->sessionState != SessionState::Empty) ||
            (replacement && state_->sessionState != SessionState::Ready &&
             state_->sessionState != SessionState::Running)) {
            auto error = core::Error{
                replacement ? "playback.session.not_ready" : "playback.session.not_empty",
                replacement ? "PlaybackSession must be active before preparing a reload"
                            : "PlaybackSession must be Empty before preparing a load"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }

        if (!source.state_) {
            auto error = core::Error{"playback.source.invalid", "PlaybackSource is empty"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }
        auto& sourceState = *source.state_;
        const auto* entryChart = sourceState.entryChart();
        if (entryChart == nullptr) {
            auto error =
                core::Error{"playback.source.invalid", "PlaybackSource entry Chart is unavailable"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }
        const auto& jsonText = entryChart->utf8Text;
        const chart::ChartLimits limits;
        PrepareContext context{*this,  source,      jsonText, mode,   targetFrame,
                               policy, replacement, options,  limits, state_->capabilities};
        PrepareArtifact artifact;
        auto& document = artifact.document;
        auto& animationProgram = artifact.animationProgram;
        auto& additionalCapabilities = artifact.additionalCapabilities;
        auto& resourceManager = artifact.resourceManager;
        auto& audioSourceLease = artifact.audioSourceLease;
        auto& committedFrame = artifact.committedFrame;
        if (sourceState.cxcPackageIdentity) {
            additionalCapabilities.emplace_back(capabilitySourceCxcV1);
        }

        if (auto loaded = loadDocumentStage(context, artifact, diagnostics); !loaded) {
            const auto message = artifact.isV4 ? "Chart v4 loading produced errors"
                                               : "Chart loading produced errors";
            return core::unexpected(operationError(context.replacement
                                                       ? "playback.chart.reload_load_failed"
                                                       : "playback.chart.load_failed",
                                                   message, diagnostics));
        }
        if (auto resolved = resolveParametersStage(context, artifact, sourceState.projectDocuments,
                                                   diagnostics);
            !resolved) {
            return core::unexpected(
                operationError(context.replacement ? "playback.chart.reload_load_failed"
                                                   : "playback.chart.load_failed",
                               artifact.v4Source ? "Chart v4 resolution produced errors"
                                                 : "Chart parameter resolution produced errors",
                               diagnostics));
        }

        if (!preflightCapabilities(*document, additionalCapabilities, context.capabilities,
                                   diagnostics)) {
            return core::unexpected(operationError("playback.capability.preflight_failed",
                                                   "Playback capability preflight failed",
                                                   diagnostics));
        }

        auto compiledAnimation =
            compileAnimationProgram(std::move(animationProgram), limits, diagnostics);
        if (!compiledAnimation) {
            return core::unexpected(
                operationError(replacement ? "playback.animation.reload_compile_failed"
                                           : "playback.animation.compile_failed",
                               "Animation program compilation produced errors", diagnostics));
        }

        auto runtimeResult = chart::ChartCompiler::compile(*document, limits);
        const bool runtimeValid = runtimeResult.hasValue();
        diagnostics.append(std::move(runtimeResult.diagnostics));
        if (!runtimeValid) {
            return core::unexpected(
                operationError(replacement ? "playback.chart.reload_compile_failed"
                                           : "playback.chart.compile_failed",
                               "Chart compilation produced errors", diagnostics));
        }
        auto& chartRuntime = *runtimeResult.runtime;
        const bool hasMainMusic = chartRuntime.mainMusic.has_value();
        if ((context.mode == PlaybackMode::ChartClock && hasMainMusic) ||
            (context.mode != PlaybackMode::ChartClock && !hasMainMusic)) {
            auto error =
                core::Error{"playback.mode.content_mismatch",
                            context.mode == PlaybackMode::ChartClock
                                ? "ChartClock requires a chart without main music"
                                : "HostClock and CuexisAudio require a chart with main music"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }

        if (sourceState.database) {
            resourceManager = std::make_unique<assets::ResourceManager>(
                std::move(*sourceState.database), sourceState.provider);
        }

        if (auto acquired = acquireAudioStage(artifact, chartRuntime, diagnostics); !acquired) {
            if (diagnostics.empty()) {
                addErrorDiagnostic(diagnostics, acquired.error());
            }
            return core::unexpected(std::move(acquired.error()));
        }
        if (auto runtime = prepareRuntimeStage(artifact, chartRuntime,
                                               std::move(*compiledAnimation), diagnostics);
            !runtime) {
            return core::unexpected(std::move(runtime.error()));
        }
        if (auto presentation =
                preparePresentationStage(context, artifact, chartRuntime, diagnostics);
            !presentation) {
            return core::unexpected(std::move(presentation.error()));
        }
        if (auto committed = commitRuntimeStage(artifact); !committed) {
            addErrorDiagnostic(diagnostics, committed.error());
            return core::unexpected(std::move(committed.error()));
        }
        if (auto sampled = commitFrameStage(context, artifact, diagnostics); !sampled) {
            return core::unexpected(std::move(sampled.error()));
        }

        const auto* presentationCandidate =
            artifact.presentation ? &*artifact.presentation : nullptr;
        auto layout =
            buildSnapshotLayout(*artifact.runtimeSession, chartRuntime, presentationCandidate);
        if (!layout) {
            addErrorDiagnostic(diagnostics, layout.error());
            return core::unexpected(std::move(layout.error()));
        }
        artifact.snapshotLayout = std::move(*layout);

        auto assembledIdentity = assembleIdentityStage(artifact, diagnostics);
        if (!assembledIdentity) {
            return core::unexpected(std::move(assembledIdentity.error()));
        }

        diagnostics.sortDeterministically();
        auto prepared = std::make_unique<PreparedPlayback::State>();
        prepared->owner = &context.owner;
        prepared->ownerToken = state_->sessionToken;
        prepared->expectedGeneration = state_->generation;
        prepared->replacement = context.replacement;
        prepared->contentProvider = std::move(sourceState.provider);
        prepared->resourceManager = std::move(resourceManager);
        prepared->chartJson = jsonText;
        prepared->runtimeSession = std::move(artifact.runtimeSession);
        prepared->snapshotLayout = std::move(*artifact.snapshotLayout);
        prepared->chartInfo = chartInfoFor(chartRuntime, prepared->runtimeSession->resourceCount());
        prepared->contentInfo = PlaybackContentInfo{
            chartRuntime.chartId.value, chartRuntime.version, chartRuntime.timingMap.offsetMs(),
            context.mode,
            chartRuntime.mainMusic ? std::optional<std::string>{chartRuntime.mainMusic->value}
                                   : std::nullopt};
        prepared->parameters = context.options.parameters;
        prepared->semanticIdentity = toPublicIdentity(*assembledIdentity);
        prepared->audioSourceLease = std::move(audioSourceLease);
        prepared->targetFrame = committedFrame;
        prepared->committedState = replacement ? state_->sessionState : SessionState::Ready;
        prepared->presentation = std::move(artifact.presentation);
        prepared->candidateGeneration = state_->nextCandidateGeneration++;
        if (prepared->candidateGeneration == 0) {
            prepared->candidateGeneration = state_->nextCandidateGeneration++;
        }
        // Keep the candidate snapshot independent; the operation recorder still publishes the
        // same warnings (or an empty set) to lastOperationDiagnostics on scope exit.
        prepared->diagnostics = diagnostics;
        prepared->lastOperationDiagnostics = prepared->diagnostics;
        return PreparedPlayback{std::move(prepared)};
    } catch (const std::bad_alloc&) {
        auto error = prepareExceptionError(replacement ? "prepare_reload" : "prepare_load", true);
        try {
            addErrorDiagnostic(diagnostics, error);
        } catch (...) {
            diagnostics.clear();
        }
        return core::unexpected(std::move(error));
    } catch (const std::exception& exception) {
        auto error = prepareExceptionError(replacement ? "prepare_reload" : "prepare_load", false,
                                           &exception);
        try {
            addErrorDiagnostic(diagnostics, error);
        } catch (...) {
            diagnostics.clear();
        }
        return core::unexpected(std::move(error));
    } catch (...) {
        auto error = prepareExceptionError(replacement ? "prepare_reload" : "prepare_load", false);
        try {
            addErrorDiagnostic(diagnostics, error);
        } catch (...) {
            diagnostics.clear();
        }
        return core::unexpected(std::move(error));
    }
}

auto PlaybackSession::commit(PreparedPlayback&& prepared) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("commit"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("commit"));
    }
    SessionOperation operation{state_->operationActive};
    if (!prepared.state_ || !prepared.state_->ownerThread.isCurrent()) {
        return core::unexpected(core::Error{
            "playback.prepared.invalid", "PreparedPlayback is empty or belongs to another thread"});
    }
    auto& candidate = *prepared.state_;
    if (candidate.owner != this || candidate.ownerToken != state_->sessionToken) {
        auto error = core::Error{"playback.prepared.wrong_session",
                                 "PreparedPlayback belongs to another PlaybackSession"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    if (candidate.expectedGeneration != state_->generation) {
        auto error =
            core::Error{"playback.prepared.stale", "PlaybackSession changed after preparation"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }
    if ((!candidate.replacement && state_->sessionState != SessionState::Empty) ||
        (candidate.replacement && state_->sessionState != SessionState::Ready &&
         state_->sessionState != SessionState::Running)) {
        auto error = core::Error{"playback.prepared.lifecycle_changed",
                                 "PlaybackSession lifecycle changed after preparation"};
        publishPrepareBoundaryDiagnostic(state_->lastOperationDiagnostics, error);
        return core::unexpected(std::move(error));
    }

    state_->contentProvider = std::move(candidate.contentProvider);
    state_->resourceManager = std::move(candidate.resourceManager);
    state_->activeChartJson = std::move(candidate.chartJson);
    state_->runtimeSession = std::move(candidate.runtimeSession);
    state_->snapshotLayout = std::move(candidate.snapshotLayout);
    state_->activeChartInfo = candidate.chartInfo;
    state_->activeContentInfo = std::move(candidate.contentInfo);
    state_->activeMode = state_->activeContentInfo->mode;
    state_->semanticIdentity = candidate.semanticIdentity;
    state_->parameters = std::move(candidate.parameters);
    state_->lastFrame = candidate.targetFrame;
    state_->diagnostics = std::move(candidate.diagnostics);
    state_->lastOperationDiagnostics = std::move(candidate.lastOperationDiagnostics);
    state_->sessionState = candidate.committedState;
    state_->presentation = std::move(candidate.presentation);
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
    prepared.state_.reset();
    return {};
}

auto PlaybackSession::loadChart(std::string_view jsonText) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("load_chart"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("load_chart"));
    }
    auto prepared = prepareLoad(jsonText, PlaybackMode::ChartClock);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::load(PlaybackSource&& source, PlaybackMode mode) -> core::Result<void> {
    return load(std::move(source), mode, PlaybackPrepareOptions{});
}

auto PlaybackSession::load(PlaybackSource&& source, PlaybackMode mode,
                           const PlaybackPrepareOptions& options) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("load"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("load"));
    }
    auto prepared = prepareLoad(std::move(source), mode, options);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::update(const RuntimeFrame& frame) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("update"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("update"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running) {
        return core::unexpected(core::Error{"playback.session.not_ready",
                                            "PlaybackSession must be active to receive updates"});
    }
    auto updated = state_->runtimeSession->update(runtimeFrame(frame));
    if (!updated) {
        return core::unexpected(std::move(updated.error()));
    }
    state_->lastFrame = frame;
    state_->sessionState = SessionState::Running;
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
    return {};
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport) const
    -> core::Result<FrameSnapshot> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("extract_frame"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("extract_frame"));
    }
    FrameSnapshot snapshot;
    if (auto extracted = extractFrame(viewport, snapshot); !extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    return snapshot;
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport, FrameSnapshot& snapshot) const
    -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("extract_frame"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("extract_frame"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState == SessionState::Empty || !state_->runtimeSession ||
        !state_->activeChartInfo.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    }
    if (viewport.width == 0 || viewport.height == 0) {
        return core::unexpected(core::Error{"playback.viewport.invalid",
                                            "FrameViewport width and height must be positive"});
    }

    auto extracted = state_->runtimeSession->withWorld([&](const world::World& world)
                                                           -> core::Result<void> {
        return world.withRegistry([&](const entt::registry& registry) -> core::Result<void> {
            snapshot.camera = {};
            snapshot.clearRed = 0.055F;
            snapshot.clearGreen = 0.063F;
            snapshot.clearBlue = 0.071F;
            snapshot.clearAlpha = 1.0F;
            snapshot.viewportWidth = viewport.width;
            snapshot.viewportHeight = viewport.height;
            if (snapshot.objects.size() < state_->snapshotLayout.entities.size()) {
                snapshot.objects.resize(state_->snapshotLayout.entities.size());
            }

            const core::Mat4 identity;
            for (std::size_t index = 0; index < state_->snapshotLayout.entities.size(); ++index) {
                const auto& entry = state_->snapshotLayout.entities[index];
                auto& object = snapshot.objects[index];
                if (object.materialAssetId.capacity() < entry.maximumMaterialAssetIdSize) {
                    object.materialAssetId.reserve(entry.maximumMaterialAssetIdSize);
                }
                if (entry.mesh) {
                    assignPresentationReference(object.mesh, *entry.mesh,
                                                entry.maximumMeshAssetIdSize);
                } else {
                    object.mesh.reset();
                }
                if (object.id != entry.id.value) {
                    object.id = entry.id.value;
                }
                object.hasTransform = registry.all_of<world::WorldTransformComponent>(entry.entity);
                if (object.hasTransform) {
                    copyMatrix(registry.get<world::WorldTransformComponent>(entry.entity).matrix,
                               object.worldMatrix);
                } else {
                    copyMatrix(identity, object.worldMatrix);
                }
                if (registry.all_of<render::AppearanceComponent>(entry.entity)) {
                    const auto& appearance =
                        registry.get<render::AppearanceComponent>(entry.entity);
                    object.visible = appearance.visible;
                    object.materialAssetId.assign(appearance.materialAssetId);
                    if (entry.mesh) {
                        const auto material = entry.materials.find(appearance.materialAssetId);
                        if (material == entry.materials.end()) {
                            return core::unexpected(
                                snapshotResourceMismatch(entry.id.value, appearance.materialAssetId,
                                                         PresentationResourceType::UnlitMaterial));
                        }
                        assignPresentationReference(object.material, material->second,
                                                    entry.maximumMaterialAssetIdSize);
                    } else {
                        object.material.reset();
                    }
                    object.materialOpacity = appearance.opacity;
                    object.materialTint[0] = appearance.tint.x;
                    object.materialTint[1] = appearance.tint.y;
                    object.materialTint[2] = appearance.tint.z;
                } else {
                    if (entry.mesh) {
                        return core::unexpected(snapshotResourceMismatch(
                            entry.id.value, "", PresentationResourceType::UnlitMaterial));
                    }
                    object.visible = true;
                    object.materialAssetId.clear();
                    object.material.reset();
                    object.materialOpacity = 1.0;
                    object.materialTint[0] = 1.0F;
                    object.materialTint[1] = 1.0F;
                    object.materialTint[2] = 1.0F;
                }
            }
            snapshot.objects.resize(state_->snapshotLayout.entities.size());

            if (!state_->snapshotLayout.activeCamera.has_value()) {
                return {};
            }
            const auto activeCamera = *state_->snapshotLayout.activeCamera;
            if (!registry.all_of<render::CameraComponent>(activeCamera)) {
                return core::unexpected(core::Error{"playback.snapshot.camera_missing",
                                                    "Active camera is unavailable"});
            }
            const auto& camera = registry.get<render::CameraComponent>(activeCamera);
            const auto projection =
                core::makePerspective(camera.fovY * 3.14159265358979323846 / 180.0,
                                      static_cast<double>(viewport.width) / viewport.height,
                                      camera.nearPlane, camera.farPlane);
            if (!projection) {
                return core::unexpected(core::Error{"playback.snapshot.camera_invalid",
                                                    "Active camera parameters are invalid"}
                                            .withCause(projection.error()));
            }

            snapshot.camera.active = true;
            snapshot.camera.fovY = camera.fovY;
            snapshot.camera.nearPlane = camera.nearPlane;
            snapshot.camera.farPlane = camera.farPlane;
            snapshot.camera.pitch = camera.pitch;
            snapshot.camera.yaw = camera.yaw;
            snapshot.camera.roll = camera.roll;
            copyMatrix(*projection, snapshot.camera.projectionMatrix);

            core::Mat4 viewMatrix{};
            if (registry.all_of<world::WorldTransformComponent>(activeCamera)) {
                auto inverse = core::inverse(
                    registry.get<world::WorldTransformComponent>(activeCamera).matrix);
                if (!inverse) {
                    return core::unexpected(core::Error{"playback.snapshot.camera_not_invertible",
                                                        "Active camera transform is not invertible"}
                                                .withCause(inverse.error()));
                }
                viewMatrix = *inverse;
            }
            copyMatrix(viewMatrix, snapshot.camera.viewMatrix);
            return {};
        });
    });
    if (!extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    return {};
}

auto PlaybackSession::reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy) -> core::Result<void> {
    return reload(replacementJson, targetFrame, policy, PlaybackPrepareOptions{});
}

auto PlaybackSession::reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy, const PlaybackPrepareOptions& options)
    -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("reload"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("reload"));
    }
    auto prepared = prepareReload(replacementJson, targetFrame, policy, options);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::reload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy) -> core::Result<void> {
    return reload(std::move(replacement), targetFrame, policy, PlaybackPrepareOptions{});
}

auto PlaybackSession::reload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                             ReloadPolicy policy, const PlaybackPrepareOptions& options)
    -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("reload"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("reload"));
    }
    auto prepared = prepareReload(std::move(replacement), targetFrame, policy, options);
    if (!prepared) {
        return core::unexpected(std::move(prepared.error()));
    }
    return commit(std::move(*prepared));
}

auto PlaybackSession::unload() -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("unload"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("unload"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState == SessionState::Empty) {
        return {};
    }
    if (state_->runtimeSession) {
        if (auto result = state_->runtimeSession->unload(); !result) {
            return result;
        }
    }
    state_->runtimeSession.reset();
    state_->resourceManager.reset();
    state_->contentProvider.reset();
    state_->activeChartJson.clear();
    state_->snapshotLayout = {};
    state_->lastFrame.reset();
    state_->diagnostics.clear();
    state_->lastOperationDiagnostics.clear();
    state_->activeChartInfo.reset();
    state_->activeContentInfo.reset();
    state_->activeMode.reset();
    state_->semanticIdentity.reset();
    state_->parameters = {};
    state_->presentation.reset();
    state_->sessionState = SessionState::Empty;
    ++state_->generation;
    if (state_->generation == 0) {
        ++state_->generation;
    }
    return {};
}

auto PlaybackSession::chartInfo() const -> core::Result<ChartInfo> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("chart_info"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("chart_info"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->runtimeSession || state_->runtimeSession->empty() ||
        !state_->activeChartInfo.has_value()) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed data"});
    }
    auto info = *state_->activeChartInfo;
    info.resourceCount = state_->runtimeSession->resourceCount();
    return info;
}

auto PlaybackSession::presentationManifest() const -> core::Result<PresentationResourceManifest> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("presentation_manifest"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("presentation_manifest"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->presentation) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.missing",
                        "PlaybackSession has no portable presentation manifest"});
    }
    try {
        return state_->presentation->manifest;
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.budget_exceeded",
                        "Portable presentation manifest copy could not be allocated"});
    } catch (...) {
        return core::unexpected(core::Error{"playback.presentation.prepare_failed",
                                            "Portable presentation manifest copy failed"});
    }
}

auto PlaybackSession::acquirePresentationResource(const PresentationResourceRef& reference) const
    -> core::Result<PortableResourcePtr> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("acquire_presentation_resource"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("acquire_presentation_resource"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->presentation) {
        return core::unexpected(
            core::Error{"playback.presentation.resource.missing",
                        "PlaybackSession has no portable presentation resources"});
    }
    const auto* resource = detail::findPresentationResource(*state_->presentation, reference);
    if (resource == nullptr) {
        return core::unexpected(
            core::Error{"playback.presentation.reference.invalid",
                        "Portable presentation resource reference is not in the active manifest"}
                .withContext("asset_id", reference.assetId)
                .withContext("resource_type",
                             std::string{presentationResourceTypeName(reference.type)}));
    }
    return *resource;
}

auto PlaybackSession::contentInfo() const -> core::Result<PlaybackContentInfo> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("content_info"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("content_info"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->activeContentInfo) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed content"});
    }
    try {
        return *state_->activeContentInfo;
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("content_info", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("content_info", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("content_info", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("content_info", false));
    }
}

auto PlaybackSession::semanticIdentity() const -> core::Result<PreparedSemanticIdentity> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("semantic_identity"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("semantic_identity"));
    }
    SessionOperation operation{state_->operationActive};
    if (!state_->semanticIdentity) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed content"});
    }
    return *state_->semanticIdentity;
}

auto PlaybackSession::diagnostics() const -> core::Result<core::Diagnostics> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("diagnostics"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("diagnostics"));
    }
    SessionOperation operation{state_->operationActive};
    try {
        return state_->diagnostics;
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("diagnostics", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("diagnostics", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("diagnostics", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("diagnostics", false));
    }
}

auto PlaybackSession::lastOperationDiagnostics() const -> core::Result<core::Diagnostics> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("last_operation_diagnostics"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("last_operation_diagnostics"));
    }
    SessionOperation operation{state_->operationActive};
    try {
        return state_->lastOperationDiagnostics;
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("last_operation_diagnostics", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("last_operation_diagnostics", true));
    } catch (const std::exception& exception) {
        return core::unexpected(
            prepareExceptionError("last_operation_diagnostics", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("last_operation_diagnostics", false));
    }
}

auto PlaybackSession::acquireHostOverride(std::string_view ownerId, std::int64_t priority,
                                          std::uint16_t propertyMask,
                                          const HostOverrideLifetime& lifetime,
                                          std::span<const HostOverrideWrite> writes)
    -> core::Result<HostOverrideToken> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("acquire_host_override"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("acquire_host_override"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running) {
        return core::unexpected(
            core::Error{"playback.session.not_ready",
                        "PlaybackSession must be active to acquire a host override"});
    }
    if (!state_->runtimeSession) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    }
    if (writes.empty()) {
        return core::unexpected(core::Error{"playback.override.empty",
                                            "Override tokens require at least one property write"});
    }

    try {
        std::vector<runtime::PropertyOverrideWrite> mapped;
        mapped.reserve(writes.size());
        for (const auto& write : writes) {
            mapped.push_back(runtime::PropertyOverrideWrite{
                .objectId = chart::ChartObjectId{write.objectId},
                .property = toWorldProperty(write.property),
                .value = toWorldValue(write.value),
            });
        }
        auto token = state_->runtimeSession->acquireOverride(
            world::OverrideKind::Host, std::string{ownerId}, priority, propertyMask,
            toWorldLifetime(lifetime), mapped);
        if (!token) {
            return core::unexpected(mapHostOverrideError(token.error()));
        }
        return HostOverrideToken{.value = token->value};
    } catch (const std::bad_alloc&) {
        return core::unexpected(prepareExceptionError("acquire_host_override", true));
    } catch (const std::length_error&) {
        return core::unexpected(prepareExceptionError("acquire_host_override", true));
    } catch (const std::exception& exception) {
        return core::unexpected(prepareExceptionError("acquire_host_override", false, &exception));
    } catch (...) {
        return core::unexpected(prepareExceptionError("acquire_host_override", false));
    }
}

auto PlaybackSession::releaseHostOverride(HostOverrideToken token) -> core::Result<void> {
    if (!state_->ownerThread.isCurrent()) {
        return core::unexpected(ownerError("release_host_override"));
    }
    if (state_->operationActive) {
        return core::unexpected(reentryError("release_host_override"));
    }
    SessionOperation operation{state_->operationActive};
    if (state_->sessionState != SessionState::Ready &&
        state_->sessionState != SessionState::Running) {
        return core::unexpected(
            core::Error{"playback.session.not_ready",
                        "PlaybackSession must be active to release a host override"});
    }
    if (!state_->runtimeSession) {
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    }
    auto released =
        state_->runtimeSession->releaseOverride(world::OverrideTokenId{.value = token.value});
    if (!released) {
        return core::unexpected(mapHostOverrideError(released.error()));
    }
    return {};
}

} // namespace cuexis::playback
