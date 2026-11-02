// PlaybackSession host facade: load, absolute-time update, owning headless snapshots and reload.

#include "playback_session_internal.hpp"

namespace cuexis::playback {
namespace {


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

using detail::SnapshotEntity;
using detail::SnapshotLayout;


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

using detail::presentationResourceTypeName;

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
#if defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
                std::string{capabilityBehaviorEventV2},
#endif
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

[[nodiscard]] auto candidateResourceRequirements(const chart::CanonicalResourceClosure& closure,
                                                 core::Diagnostics& diagnostics)
    -> std::optional<std::vector<chart::ChartResourceRequirement>> {
    std::map<std::string, chart::ChartResourceRequirement, std::less<>> grouped;
    for (const auto& resource : closure.resources) {
        chart::ChartResourceUse use = chart::ChartResourceUse::MainMusic;
        switch (resource.use) {
        case chart::CanonicalResourceUseKind::MainMusic:
            use = chart::ChartResourceUse::MainMusic;
            break;
        case chart::CanonicalResourceUseKind::RenderableMesh:
            use = chart::ChartResourceUse::RenderableMesh;
            break;
        case chart::CanonicalResourceUseKind::RenderableMaterial:
            use = chart::ChartResourceUse::RenderableMaterial;
            break;
        }
        auto& requirement = grouped[resource.assetId.value];
        requirement.assetId = resource.assetId;
        if (std::ranges::find(requirement.uses, use) == requirement.uses.end()) {
            requirement.uses.push_back(use);
        }
    }

    std::vector<chart::ChartResourceRequirement> requirements;
    requirements.reserve(grouped.size());
    for (auto& [unused, requirement] : grouped) {
        static_cast<void>(unused);
        const bool audio = std::ranges::any_of(requirement.uses, [](chart::ChartResourceUse use) {
            return use == chart::ChartResourceUse::MainMusic;
        });
        const bool presentation =
            std::ranges::any_of(requirement.uses, [](chart::ChartResourceUse use) {
                return use != chart::ChartResourceUse::MainMusic;
            });
        if (requirement.assetId.value.empty() || (audio && presentation)) {
            diagnostics.add(core::Diagnostic{
                core::DiagnosticSeverity::Error, "candidate.identity.resource_conflict",
                "Candidate resource closure has an empty or mixed-identity asset",
                "$/resources/" + requirement.assetId.value});
            continue;
        }
        requirements.push_back(std::move(requirement));
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return std::nullopt;
    }
    return requirements;
}

// Lowers the typed chart already validated by the explicit source factory. It does not read
// Chart JSON, expand CXT v2, or decode Packed bytes again.
[[nodiscard]] auto prepareCandidateStage(const cxc::CandidateChart& candidate, bool gameplaySource,
                                         const PlaybackPrepareOptions& options, bool replacement,
                                         const chart::ChartLimits& limits,
                                         PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::ChartRuntime> {
    if (!options.parameters.values.empty()) {
        for (std::size_t index = 0; index < options.parameters.values.size(); ++index) {
            diagnostics.add(
                core::Diagnostic{core::DiagnosticSeverity::Error, "chart.parameter.unknown",
                                 "Host parameter input is not supported by this Chart version",
                                 "$/parameterInputs/" + std::to_string(index) + "/id"});
        }
        diagnostics.sortDeterministically();
        return core::unexpected(operationError(
            replacement ? "playback.chart.reload_load_failed" : "playback.chart.load_failed",
            "Chart parameter resolution produced errors", diagnostics));
    }

    auto lowered =
        candidate.behavior ? chart::CandidateRuntimeArtifactResult{*candidate.behavior, {}}
        : gameplaySource   ? chart::lowerGameplayPresentationRuntime(candidate.semantic, limits)
                           : chart::lowerCandidateRuntime(candidate.semantic, limits);
    diagnostics.append(std::move(lowered.diagnostics));
    if (!lowered.hasValue()) {
        diagnostics.sortDeterministically();
        return core::unexpected(operationError(replacement ? "playback.chart.reload_compile_failed"
                                                           : "playback.chart.compile_failed",
                                               "Candidate lowering produced errors", diagnostics));
    }

    auto requirements =
        candidateResourceRequirements(lowered.artifact->metadata.resourceClosure, diagnostics);
    if (!requirements) {
        return core::unexpected(operationError("playback.candidate.resource_closure_invalid",
                                               "Candidate resource closure could not be prepared",
                                               diagnostics));
    }
    const bool renderable = std::ranges::any_of(lowered.artifact->runtime.objects,
                                                [](const chart::RuntimeObject& object) {
                                                    return object.components.renderable.has_value();
                                                });
    if (candidate.behavior) {
        artifact.additionalCapabilities.emplace_back(capabilityBehaviorEventV2);
        for (const auto& behavior : lowered.artifact->runtime.behaviors)
            for (const auto& track : behavior.stepTracks)
                if (track.property == chart::BehaviorStepProperty::RenderVisible)
                    artifact.additionalCapabilities.emplace_back(capabilityRenderVisibilityV1);
    }
    if (renderable) {
        artifact.additionalCapabilities.emplace_back(capabilityMaterialSnapshotV1);
    }
    artifact.resourceRequirements = std::move(*requirements);
    artifact.candidateMetadata = std::move(lowered.artifact->metadata);
    return std::move(lowered.artifact->runtime);
}

[[nodiscard]] auto loadDocumentStage(const PrepareContext& context, PrepareArtifact& artifact,
                                     core::Diagnostics& diagnostics) -> core::Result<void> {
    auto loaded = [&] {
        const internal::PrepareTraceScope trace{"load_chart"};
        return chart::detail::loadChartForPlayback(context.chartJson, context.limits);
    }();
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
    // The dispatcher canonicalizes the tree it already parsed, so this stage does not parse the
    // document a second time. A value the canonical writer rejects has no bytes to reuse and
    // travels the text entry point instead, which reports the writer error unchanged.
    const internal::PrepareTraceScope traceCanonical{"load_canonical"};
    std::string canonicalText;
    if (loaded.legacyCanonicalText.has_value()) {
        canonicalText = std::move(*loaded.legacyCanonicalText);
    } else {
        auto canonical = chart::ChartWriter::writeCanonicalJson(context.chartJson, context.limits);
        if (!canonical) {
            addErrorDiagnostic(diagnostics, canonical.error());
            return core::unexpected(std::move(canonical.error()));
        }
        canonicalText = std::move(*canonical);
    }
    artifact.chartIdentity = chart::canonicalBytesIdentity(canonicalText);
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
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
    // Product tag: the prepared snapshot layout is capped as state.candidate.
    const internal::pd::memory::TagScope tag{internal::pd::memory::ownerMain,
                                             internal::pd::memory::domainCandidate};
#endif
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
            .runtimeIndex = layout.entities.size(),
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

    auto revision = runtime::detail::PresentationAccess::state(session);
    if (!revision)
        return core::unexpected(std::move(revision.error()));
    layout.runtimeOwner = revision->stamp.owner;
    layout.runtimeLayout = revision->stamp.layout;
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

[[nodiscard]] auto
preparePresentationStage(const PrepareContext& context, PrepareArtifact& artifact,
                         const chart::ChartRuntime& chartRuntime, core::Diagnostics& diagnostics,
                         bool checkHostCapabilities = true) -> core::Result<void> {
    auto preparedPresentation = detail::preparePresentation(
        chartRuntime, artifact.resourceManager.get(), artifact.v4Artifact.has_value());
    if (!preparedPresentation) {
        addErrorDiagnostic(diagnostics, preparedPresentation.error());
        diagnostics.sortDeterministically();
        return core::unexpected(std::move(preparedPresentation.error()));
    }
    if (preparedPresentation->has_value() && checkHostCapabilities) {
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
        const bool capabilitiesOk =
            artifact.document
                ? preflightCapabilities(*artifact.document, presentationCapabilities,
                                        context.capabilities, diagnostics)
                : preflightCapabilities(chart::ChartDocument{}, presentationCapabilities,
                                        context.capabilities, diagnostics);
        if (!presentationCapabilities.empty() && !capabilitiesOk) {
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
    if (artifact.candidateMetadata) {
        auto resourceIdentities =
            assembleResourceIdentities(artifact.resourceRequirements, artifact.audioSourceLease,
                                       artifact.presentation, diagnostics);
        if (!resourceIdentities) {
            return core::unexpected(
                operationError("playback.identity.assemble_failed",
                               "Prepared semantic identity could not be assembled", diagnostics));
        }
        auto identity = chart::assembleCandidatePreparedSemanticIdentity(
            artifact.candidateMetadata->semanticIdentity, *resourceIdentities);
        if (!identity) {
            addErrorDiagnostic(diagnostics, identity.error());
            diagnostics.sortDeterministically();
            return core::unexpected(operationError(
                "playback.identity.assemble_failed",
                "Candidate prepared semantic identity could not be assembled", diagnostics));
        }
        return std::move(*identity);
    }
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


auto detail::PresentationAccess::state(const PlaybackSession& session)
    -> core::Result<CommittedPresentationState> {
    if (!session.state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("presentation_state"));
    if (session.state_->operationActive)
        return core::unexpected(reentryError("presentation_state"));
    if (!session.state_->committedPresentation)
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    return *session.state_->committedPresentation;
}

auto PlaybackSession::presentationSource() const -> core::Result<PresentationSourceInfo> {
    auto committed = detail::PresentationAccess::state(*this);
    if (!committed)
        return core::unexpected(committed.error());
    const auto& s = *committed;
    PresentationSource source{PresentationSource::Prepared};
    switch (s.source) {
    case internal::presentation::Source::Prepared:
        source = PresentationSource::Prepared;
        break;
    case internal::presentation::Source::RuntimeUpdate:
        source = PresentationSource::RuntimeUpdate;
        break;
    case internal::presentation::Source::Restore:
        source = PresentationSource::Restore;
        break;
    case internal::presentation::Source::Mutation:
        source = PresentationSource::Mutation;
        break;
    }
    return PresentationSourceInfo{s.sessionScope,
                                  s.resultVersion,
                                  s.contentRevision,
                                  s.layoutRevision,
                                  s.worldRevision,
                                  s.updateOrdinal,
                                  source,
                                  s.frame,
                                  s.presentationTick,
                                  s.workTick,
                                  s.factCursor,
                                  s.controlCursor,
                                  s.activationRevision ? std::optional{s.activationRevision}
                                                       : std::nullopt,
                                  s.hasFold};
}

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
    std::optional<chart::CandidateRuntimeMetadata> candidateMetadata;
    std::optional<assets::AudioSourceLease> audioSourceLease;
    core::Diagnostics diagnostics;
    core::Diagnostics lastOperationDiagnostics;
    std::optional<RuntimeFrame> targetFrame;
    SessionState committedState{SessionState::Ready};
    std::optional<detail::PreparedPresentation> presentation;
    std::uint64_t candidateGeneration{};
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    GameplayContent gameplayContent;
    std::unique_ptr<judgement::JudgementSession> gameplay;
    detail::PreparedProjection gameplayProjectionIndex;
#endif
};

auto PreparedPlayback::materializePresentation(const PresentationCapabilities& capabilities,
                                               const PresentationRequest& request,
                                               std::uint64_t rendererInstance,
                                               std::uint64_t rendererGeneration,
                                               std::uint64_t sequence)
    -> core::Result<PresentationCandidateInput> try {
    auto& prepared = *this;
    if (!prepared.valid() || !prepared.state_->owner || !rendererInstance || !rendererGeneration ||
        !sequence)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Invalid owning candidate context"});
    auto validation = prepared.validatePresentation(capabilities, request);
    if (!validation.hasValue())
        return core::unexpected(
            core::Error{"host.rpa.device_failed", "Candidate capabilities rejected"});
    const auto* manifest = prepared.presentationManifest();
    auto token = prepared.presentationCandidateToken();
    if (!manifest || !token)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Portable candidate missing"});
    const auto& p = *prepared.state_;
    PresentationCandidateInput value;
    value.context = {p.ownerToken,      1, 1, sequence, p.candidateGeneration, rendererGeneration,
                     p.semanticIdentity};
    value.playbackOwnerToken = p.ownerToken;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (p.owner && p.owner->state_->executionContext) {
        const auto& execution = *p.owner->state_->executionContext;
        value.context.sessionScope = execution.sessionScope;
        value.context.clockSegment = execution.clockSegment;
        value.context.runEpoch = execution.runEpoch;
    }
#endif
    value.mutationGeneration = p.expectedGeneration;
    value.candidateId = p.candidateGeneration;
    value.rendererInstance = rendererInstance;
    value.token = *token;
    value.capabilities = capabilities;
    value.manifest = *manifest;
    value.settings = *validation.settings;
    value.resources.reserve(manifest->entries.size());
    for (const auto& entry : manifest->entries) {
        auto resource = prepared.acquirePresentationResource(entry.reference);
        if (!resource)
            return core::unexpected(resource.error());
        value.resources.push_back(std::move(*resource));
    }
    if (const auto source = prepared.mainMusicSource())
        value.audio = AudioCandidateInput{std::string{source->assetId},
                                          source->timingOffsetMs,
                                          source->contentRevision,
                                          {source->bytes.begin(), source->bytes.end()}};
    return value;
} catch (...) {
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Owning candidate reservation failed"});
}

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
namespace {
struct HostMutationPermission final {
    bool& flag;
    bool previous;
    explicit HostMutationPermission(bool& value) : flag(value), previous(value) {
        flag = true;
    }
    ~HostMutationPermission() {
        flag = previous;
    }
};
auto matchingStep(const ControlReceipt& proof, Participant participant, StepPhase phase)
    -> const StepReceipt* {
    const auto& step =
        proof.steps[static_cast<unsigned>(participant)][static_cast<unsigned>(phase)];
    if (!step || step->status != StepStatus::applied ||
        step->request.target != proof.request.target ||
        step->request.key.operationId != proof.request.operationId ||
        step->request.key.participant != participant || step->request.key.phase != phase ||
        step->request.key.sessionScope != proof.request.context.sessionScope)
        return nullptr;
    return &*step;
}
} // namespace
auto computeGameplayReconciliationIdentity(const GameplayReconciliation& r)
    -> core::Result<PreparedSemanticIdentity> try {
    core::detail::Sha256 hash;
    const auto integer = [&](std::uint64_t n) {
        std::array<std::byte, 8> b{};
        for (std::size_t i = 0; i < 8; ++i)
            b[i] = std::byte((n >> (i * 8)) & 255);
        hash.update(b);
    };
    const auto text = [&](std::string_view value) {
        integer(value.size());
        hash.update(std::as_bytes(std::span{value.data(), value.size()}));
    };
    text("cuexis.host.resume.target.v1");
    integer(r.cutTick.value);
    integer(r.controlOrdinal);
    integer(r.pauseEpoch);
    integer(r.finalTraceCut);
    for (const auto& t : r.trace) {
        const auto& in = t.occurrence.input;
        integer(t.sequence);
        integer(in.observationTick.value);
        integer(t.occurrence.sourceLocalPosition);
        text(in.domain);
        text(in.sourceClass);
        text(in.channel);
        integer(static_cast<std::uint64_t>(in.action));
        integer(in.amount.has_value());
        if (in.amount) {
            integer(in.amount->numerator);
            integer(in.amount->denominator);
        }
        integer(in.sequence);
        integer(in.crossedSamplingGap);
        integer(in.reconnected);
        integer(in.droppedSamples);
        integer(in.rawTimestamps.device);
        integer(in.rawTimestamps.hostArrival);
        integer(in.rawTimestamps.audioFrame);
        integer(in.rawTimestamps.renderFrame);
    }
    return PreparedSemanticIdentity{hash.finish()};
} catch (...) {
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Resume identity reservation failed"});
}

auto PlaybackSession::enterExecutionFence(const PresentationCandidateInput& input)
    -> core::Result<ExecutionFence> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("enter_fence"));
    if (s.operationActive || s.hostFence || input.playbackOwnerToken != s.sessionToken ||
        input.mutationGeneration != s.generation || !input.candidateId ||
        !input.context.controlSequence || s.generation == UINT64_MAX)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Prepared generation changed before fence"});
    s.hostFence = ExecutionFence{input.context.sessionScope, s.generation, input.candidateId,
                                 input.context.controlSequence};
    return *s.hostFence;
}
auto PlaybackSession::commitExecutionCandidate(PreparedPlayback&& prepared, ExecutionFence fence,
                                               const ControlReceipt& ready) -> core::Result<void> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("host_commit"));
    if (!s.hostFence || *s.hostFence != fence || !prepared.valid() ||
        prepared.state_->candidateGeneration != fence.candidateId_ ||
        ready.request.context.sessionScope != fence.scope_ ||
        ready.request.context.controlSequence != fence.controlSequence_ ||
        ready.request.target.candidateId != fence.candidateId_ || !controlReadyForCommit(ready) ||
        generationFor(ready.request.target, Participant::software) != fence.generation_ ||
        ready.request.target.nextContext.value_or(ready.request.target.context).contentIdentity !=
            prepared.state_->semanticIdentity ||
        ready.request.target.nextContext.value_or(ready.request.target.context)
                .resourceGeneration != prepared.state_->candidateGeneration)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Commit fence identity mismatch"});
    struct Permission final {
        bool& flag;
        ~Permission() {
            flag = false;
        }
    } permission{s.hostCommitAuthorized};
    s.hostCommitAuthorized = true;
    return session.commit(std::move(prepared));
}
auto PlaybackSession::releaseExecutionFence(ExecutionFence fence, const ControlReceipt& confirmed)
    -> core::Result<void> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("leave_fence"));
    if (!s.hostFence || *s.hostFence != fence)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Old receipt cannot release fence"});
    if (confirmed.request.context.sessionScope != fence.scope_ ||
        confirmed.request.context.controlSequence != fence.controlSequence_ ||
        confirmed.request.target.candidateId != fence.candidateId_ ||
        !controlFullyApplied(confirmed))
        return core::unexpected(
            core::Error{"host.rpa.device_pending", "Participating owners not confirmed"});
    s.confirmedHostContext = confirmed.finalizedContext.value_or(
        confirmed.request.target.nextContext.value_or(confirmed.request.context));
    s.hostFence.reset();
    return {};
}
auto PlaybackSession::validateExecutionControlBinding(const StepRequest& request) const
    -> core::Result<void> {
    auto& session = *this;
    const auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("control_binding"));
    const bool participant = request.key.participant == Participant::software;
    const bool scope = s.acceptedHostControl &&
                       request.key.sessionScope == s.acceptedHostControl->context.sessionScope;
    const bool operation =
        s.acceptedHostControl && request.key.operationId == s.acceptedHostControl->operationId;
    const bool target = s.acceptedHostControl && request.target == s.acceptedHostControl->target;
    const bool accepted = s.acceptedHostControl.has_value();
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (auto* sink = internal::execution_diagnostics::rpa::fenceSink) {
        sink->bindingChecked = true;
        sink->bindingOwner = true;
        sink->bindingParticipant = participant;
        sink->bindingScope = scope;
        sink->bindingOperation = operation;
        sink->bindingTarget = target;
        if (!accepted) {
            sink->bindingChecked = true;
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingMissing);
        } else if (!participant)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingParticipant);
        else if (!scope)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingScope);
        else if (!operation)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingOperation);
        else if (!target)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::bindingTarget);
    }
#endif
    if (!accepted || !participant || !scope || !operation || !target)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Software step lacks accepted target"});
    return {};
}
auto PlaybackSession::validateExecutionControlProof(const StepRequest& request,
                                                    const ControlReceipt& proof) const
    -> core::Result<void> {
    if (auto valid = validateExecutionControlBinding(request); !valid)
        return valid;
    if (!state_->acceptedHostDecision || proof.decisionOrdinal != *state_->acceptedHostDecision ||
        proof.request != *state_->acceptedHostControl)
        return core::unexpected(core::Error{
            "host.rpa.context_stale", "Control proof differs from the accepted owner decision"});
    return {};
}

auto PlaybackSession::validateExecutionFrameContext(const HostContext& context) const
    -> core::Result<void> {
    auto& session = *this;
    const auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("frame_context"));
    if (s.hostFence || !s.confirmedHostContext || *s.confirmedHostContext != context)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Frame lacks a confirmed full context"});
    return {};
}
auto PlaybackSession::acceptExecutionControl(const ControlRequest& request)
    -> core::Result<std::uint64_t> try {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("host_control"));
    if (s.operationActive)
        return core::unexpected(reentryError("host_control"));
    if (s.acceptedHostControl && s.acceptedHostDecision &&
        request.context.sessionScope == s.acceptedHostControl->context.sessionScope &&
        request.operationId == s.acceptedHostControl->operationId) {
        if (request != *s.acceptedHostControl)
            return core::unexpected(
                core::Error{"host.rpa.message_conflict", "Control identity conflict"});
        return *s.acceptedHostDecision;
    }
    if (request.context != request.target.context || !request.context.sessionScope ||
        !request.operationId || request.commandOrdinal != request.target.commandOrdinal ||
        request.commandOrdinal == UINT64_MAX ||
        (s.acceptedHostControl && request.commandOrdinal <= s.acceptedHostControl->commandOrdinal))
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Control target or ordinal invalid"});
    if (s.confirmedHostContext &&
        (request.context.sessionScope != s.confirmedHostContext->sessionScope ||
         request.context.resourceGeneration != s.confirmedHostContext->resourceGeneration ||
         request.context.rendererGeneration != s.confirmedHostContext->rendererGeneration ||
         request.context.contentIdentity != s.confirmedHostContext->contentIdentity ||
         request.context.controlSequence <= s.confirmedHostContext->controlSequence))
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Confirmed owner identity mismatch"});
    // Validate a new Close against its admission generation. Legitimate post-cut delivery can
    // advance the owner before retire; a retransmitted accepted request returned above keeps
    // its original binding and decision.
    if (request.kind == ControlKind::close &&
        generationFor(request.target, Participant::software) != s.generation)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Close admission generation is stale"});
    if (request.kind == ControlKind::close && s.hostFence &&
        request.context.controlSequence <= s.hostFence->controlSequence())
        return core::unexpected(core::Error{
            "host.rpa.context_stale", "A new Close cannot reuse an older control fence identity"});
    // Reserve the complete binding before K1 changes any execution state.
    auto binding = request;
    if (s.hostFence && request.kind != ControlKind::close)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Only Close can retire an unresolved fence"});
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (s.executionScheduler && s.executionContext) {
        const auto& current = *s.executionContext;
        const bool exact = request.context.sessionScope == current.sessionScope &&
                           request.context.clockSegment == current.clockSegment &&
                           request.context.runEpoch == current.runEpoch;
        const bool abortedRoute =
            request.kind == ControlKind::close && s.acceptedHostControl &&
            s.acceptedHostControl->kind == ControlKind::resume &&
            s.acceptedHostControl->target.nextContext &&
            request.context.sessionScope ==
                s.acceptedHostControl->target.nextContext->sessionScope &&
            request.context.clockSegment ==
                s.acceptedHostControl->target.nextContext->clockSegment &&
            request.context.runEpoch == s.acceptedHostControl->target.nextContext->runEpoch;
        if (!exact && !abortedRoute)
            return core::unexpected(
                core::Error{"host.rpa.context_stale", "Control execution context mismatch"});
        if ((request.kind == ControlKind::pause &&
             (!request.target.tick || !request.target.pauseEpoch)) ||
            (request.kind == ControlKind::resume &&
             (!request.target.tick || !request.target.pauseEpoch || !request.target.nextContext ||
              (!request.target.reconciliationOrdinal &&
               (!request.target.payloadIdentity || !request.target.cut)))))
            return core::unexpected(
                core::Error{"host.rpa.context_stale", "Control target is incomplete"});
        // Close cancels outstanding work even when ordinary Gameplay is faulted or fenced.
        auto accepted = request.kind == ControlKind::close
                            ? s.executionScheduler->acceptControl(
                                  judgement::RpaContext{current.sessionScope, current.clockSegment,
                                                        current.runEpoch},
                                  request.commandOrdinal)
                            : session.acceptGameplayControl(current, request.commandOrdinal);
        if (!accepted)
            return core::unexpected(accepted.error());
        const auto decision = *s.executionScheduler->lastDecisionOrdinal();
        if (decision == UINT64_MAX)
            return core::unexpected(
                core::Error{"host.rpa.capacity_exceeded", "Host decision exhausted"});
        s.hostDecision = decision + 1;
        s.acceptedHostControl = std::move(binding);
        s.acceptedHostDecision = decision;
        return decision;
    }
#endif
    if (request.context.sessionScope != s.sessionToken || s.hostDecision == UINT64_MAX)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Software owner scope differs"});
    const auto decision = s.hostDecision++;
    s.acceptedHostControl = std::move(binding);
    s.acceptedHostDecision = decision;
    return decision;
} catch (...) {
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Host control publication failed"});
}
auto PlaybackSession::enterExecutionFence(const ControlReceipt& proof)
    -> core::Result<ExecutionFence> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("control_fence"));
    const bool operationActive = s.operationActive;
    const bool existingFence = s.hostFence.has_value();
    const bool noAcceptedControl = !s.acceptedHostControl;
    const bool requestMismatch = !noAcceptedControl && *s.acceptedHostControl != proof.request;
    const bool statusMismatch = proof.status != ControlStatus::preparing;
    const bool typeMismatch =
        proof.request.kind == ControlKind::load || proof.request.kind == ControlKind::reload;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (auto* sink = internal::execution_diagnostics::rpa::fenceSink) {
        using Predicate = internal::execution_diagnostics::rpa::FencePredicate;
        sink->identity.valid = true;
        sink->identity.scope = proof.request.context.sessionScope;
        sink->identity.candidateId = proof.request.target.candidateId;
        sink->identity.controlSequence = proof.request.context.controlSequence;
        sink->identity.operationId = proof.request.operationId;
        sink->identity.kind = static_cast<std::uint8_t>(proof.request.kind);
        sink->identity.status = static_cast<std::uint8_t>(proof.status);
        if (operationActive)
            internal::execution_diagnostics::rpa::reject(Predicate::operationActive);
        if (existingFence)
            internal::execution_diagnostics::rpa::reject(Predicate::existingFence);
        if (noAcceptedControl)
            internal::execution_diagnostics::rpa::reject(Predicate::noAcceptedControl);
        if (requestMismatch)
            internal::execution_diagnostics::rpa::reject(Predicate::requestMismatch);
        if (statusMismatch)
            internal::execution_diagnostics::rpa::reject(Predicate::statusMismatch);
        if (typeMismatch)
            internal::execution_diagnostics::rpa::reject(Predicate::typeMismatch);
        if (operationActive || existingFence || noAcceptedControl || requestMismatch ||
            statusMismatch || typeMismatch)
            internal::execution_diagnostics::rpa::noteReason(
                operationActive ? internal::execution_diagnostics::rpa::Reason::operationActive
                : existingFence ? internal::execution_diagnostics::rpa::Reason::existingFence
                : noAcceptedControl
                    ? internal::execution_diagnostics::rpa::Reason::noAcceptedControl
                : requestMismatch ? internal::execution_diagnostics::rpa::Reason::requestMismatch
                : statusMismatch  ? internal::execution_diagnostics::rpa::Reason::statusMismatch
                                  : internal::execution_diagnostics::rpa::Reason::typeMismatch);
    }
#endif
    if (operationActive || existingFence || noAcceptedControl || requestMismatch ||
        statusMismatch || typeMismatch)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Control cannot enter its software fence"});
    const auto phase =
        proof.request.kind == ControlKind::resume || proof.request.kind == ControlKind::seek
            ? StepPhase::resumeRoute
            : StepPhase::cut;
    if (proof.request.participants[0] && !matchingStep(proof, Participant::capture, phase)) {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        if (internal::execution_diagnostics::rpa::fenceSink) {
            internal::execution_diagnostics::rpa::reject(
                internal::execution_diagnostics::rpa::FencePredicate::cutMissing);
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::cutMissing);
        }
#endif
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Final capture boundary is unconfirmed"});
    }
    s.hostFence =
        ExecutionFence{proof.request.context.sessionScope, s.generation,
                       proof.request.target.candidateId, proof.request.context.controlSequence};
    return *s.hostFence;
}
auto PlaybackSession::executionFence() const -> core::Result<ExecutionFence> {
    auto& session = *this;
    if (!session.state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("control_fence"));
    if (!session.state_->hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "No active software fence"});
    return *session.state_->hostFence;
}
auto PlaybackSession::completeExecutionClose(ExecutionFence fence, const ControlReceipt& proof)
    -> core::Result<void> {
    auto& session = *this;
    auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("host_close"));
    if (!s.hostFence || *s.hostFence != fence || proof.request.kind != ControlKind::close ||
        proof.request.context.sessionScope != fence.scope_ ||
        proof.request.context.controlSequence <= fence.controlSequence_ ||
        !controlFullyApplied(proof) || s.sessionState != SessionState::Empty)
        return core::unexpected(
            core::Error{"host.rpa.device_pending", "Close has not retired every fenced owner"});
    s.hostFence.reset();
    return {};
}
auto PlaybackSession::invalidateGameplayClock() -> core::Result<void> {
    auto& session = *this;
    if (!session.state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("execution_revoke"));
    auto& s = *session.state_;
    if (!s.executionScheduler)
        return core::unexpected(gameplayError("Realtime session is not started"));
    auto revoked = s.executionScheduler->clockValid(false);
    auto synced = s.syncExecutionState();
    return synced ? revoked : synced;
}
auto PlaybackSession::mapGameplayClock(const ClockObservation& clock, std::int64_t now)
    -> core::Result<ClockTicks> {
    auto& session = *this;
    if (auto guard = session.gameplayWriteGuard(); !guard)
        return core::unexpected(guard.error());
    auto& s = *session.state_;
    if (!s.realtimeClock || !s.executionScheduler)
        return core::unexpected(gameplayError("Realtime session is not started"));
    auto mapped = s.tailTransition && clock == s.tailTransition->eof
                      ? s.realtimeClock->sampleTail(now)
                      : s.realtimeClock->sample(clock, now);
    if (!mapped) {
        auto invalid = s.executionScheduler->clockValid(false);
        if (!invalid)
            return core::unexpected(invalid.error());
        auto synced = s.syncExecutionState();
        if (!synced)
            return core::unexpected(synced.error());
    }
    return mapped;
}
auto PlaybackSession::gameplayReplayCut(const GameplayReplay& replay, GameplayTick horizon) const
    -> core::Result<GameplayReplayCut> {
    auto& session = *this;
    if (!session.state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("archive_cut"));
    const auto* archive = detail::GameplayAccess::replay(replay);
    if (!archive)
        return core::unexpected(gameplayError("Owning archive is absent"));
    auto cut = judgement::replayCutAt(archive->archive, judgement::Tick{horizon.value});
    return GameplayReplayCut{
        cut.completeRecords,
        cut.partialHorizon ? std::optional<GameplayTick>{GameplayTick{cut.partialHorizon->value()}}
                           : std::nullopt};
}
auto PlaybackSession::gameplayFeedback(const std::uint64_t scope) const
    -> core::Result<std::vector<FeedbackObservation>> try {
    internal::pd::Timer measurement{internal::pd::Stage::feedback};
    auto& session = *this;
    const auto& s = *session.state_;
    if (!s.ownerThread.isCurrent())
        return core::unexpected(ownerError("feedback_membership"));
    if (!s.gameplay || !scope)
        return core::unexpected(gameplayError("Feedback scope absent"));
    std::vector<FeedbackObservation> members;
    for (const auto& scheduled : s.gameplayProjection.active) {
        const auto& b = s.gameplayProjectionIndex.rows[scheduled.row].binding;
        if (!scheduled.published || !s.gameplayProjection.lastTick ||
            b.start.value > s.gameplayProjection.lastTick->value ||
            (b.end && b.end->value <= s.gameplayProjection.lastTick->value))
            continue;
        members.push_back({{scope, scheduled.factId.commitId, scheduled.factId.localOrdinal, 0,
                            b.target, b.bindingId},
                           b.start.value,
                           b.end,
                           FeedbackStatus::created,
                           false});
    }
    return members;
} catch (...) {
    return core::unexpected(
        core::Error{"host.rpa.capacity_exceeded", "Feedback membership failed"});
}
auto PlaybackSession::sampleGameplayPresentation(GameplayPresentationTick tick,
                                                 const RuntimeFrame& frame)
    -> core::Result<void> try {
    auto& session = *this;
    if (auto guard = session.gameplayWriteGuard(); !guard)
        return guard;
    SessionOperation operation{session.state_->operationActive};
    // Obtain the immutable kernel result without advancing the scheduler.
    if (!session.state_->gameplay)
        return core::unexpected(gameplayError("Gameplay presentation is not prepared"));
    if (session.state_->gameplayState == GameplayState::Paused)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Paused presentation must stay frozen"});
    return detail::GameplayPublicationAccess::publish(session, *session.state_->gameplay, tick,
                                                      frame, true);
} catch (...) {
    return core::unexpected(gameplayError("Presentation sampling failed"));
}
auto PlaybackSession::executionOwnerState() const -> core::Result<ExecutionOwnerState> try {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("execution_owner"));
    ExecutionOwnerState value{state_->sessionToken,
                              state_->generation,
                              state_->lastFrame,
                              state_->acceptedHostControl,
                              state_->hostFence,
                              {},
                              {}};
    if (state_->executionScheduler && state_->executionScheduler->pendingEpochEnd()) {
        const auto& end = *state_->executionScheduler->pendingEpochEnd();
        value.pendingEpochEnd = GameplayEpochEnd{{end.inclusiveTick.value()}, end.pauseEpoch};
    }
    if (state_->gameplay) {
        auto query = state_->gameplay->query();
        if (!query)
            return core::unexpected(query.error());
        if (query->kernelView().rpa)
            value.reconciliationOrdinal = query->kernelView().rpa->nextControlOrdinal;
    }
    return value;
} catch (...) {
    return core::unexpected(core::Error{"host.rpa.capacity_exceeded", "Owner status copy failed"});
}
auto PlaybackSession::executionStepEffects() const -> core::Result<StepEffects> {
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("execution_effects"));
    auto query = queryGameplay();
    if (!query)
        return core::unexpected(query.error());
    const auto& p = detail::GameplayAccess::result(*query)->kernel;
    StepEffects effects;
    effects.generation = state_->generation;
    effects.sealedFactCursor = p.sealedFactCursor;
    effects.foldedFactCursor = p.fold ? p.fold->factCursor : p.sealedFactCursor;
    if (p.rpa && p.rpa->cutTick)
        effects.position = p.rpa->cutTick->value();
    return effects;
}

auto PlaybackSession::retireExecutionStep(const StepRequest& request, const ControlReceipt& proof)
    -> core::Result<void> {
    if (auto valid = validateExecutionControlProof(request, proof); !valid)
        return valid;
    if (proof.request.kind == ControlKind::close && proof.status == ControlStatus::failed)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Failed Close cannot retire software"});
    const bool directedPhase = request.key.phase == StepPhase::retire;
    const bool directedTarget = request.target == proof.request.target;
    const bool directedRequest = *state_->acceptedHostControl == proof.request;
    const bool directedKind = proof.request.kind == ControlKind::close ||
                              proof.status == ControlStatus::suspended ||
                              proof.status == ControlStatus::failed;
    const bool captureCleared =
        !proof.request.participants[0] ||
        matchingStep(proof, Participant::capture, StepPhase::cut) != nullptr;
    const bool audioCleared = [&] {
        if (!proof.request.participants[2])
            return true;
        const auto* isolation = matchingStep(proof, Participant::audio, StepPhase::isolate);
        return matchingStep(proof, Participant::audio, StepPhase::stop) != nullptr && isolation &&
               isolation->effects.callbackIsolated;
    }();
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (auto* sink = internal::execution_diagnostics::rpa::fenceSink) {
        sink->permitAttempted = true;
        sink->permitDirected = directedPhase && directedTarget && directedRequest && directedKind;
        sink->permitRequestMatch = directedRequest;
        sink->permitTargetMatch = directedTarget;
        sink->permitOperationMatch = request.key.operationId == proof.request.operationId;
        sink->permitParticipantMatch = request.key.participant == Participant::software;
        sink->permitCaptureCut = captureCleared;
        sink->permitAudioIsolated = audioCleared;
        if (!directedPhase)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::retirePhase);
        else if (!directedTarget)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::retireTarget);
        else if (!directedRequest)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::requestMismatch);
        else if (!directedKind)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::retireKind);
        else if (!captureCleared)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::captureActive);
        else if (!audioCleared)
            internal::execution_diagnostics::rpa::noteReason(
                internal::execution_diagnostics::rpa::Reason::audioActive);
    }
#endif
    if (!directedPhase || !directedTarget || !directedRequest || !directedKind)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Retirement lacks directed cleanup"});
    if (!captureCleared)
        return core::unexpected(core::Error{"host.rpa.device_pending", "Capture is active"});
    if (!audioCleared)
        return core::unexpected(core::Error{"host.rpa.device_pending", "Audio is active"});
    HostMutationPermission permission{state_->hostCommitAuthorized};
    auto unloaded = unload();
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (auto* sink = internal::execution_diagnostics::rpa::fenceSink)
        sink->retireApplied = bool(unloaded);
#endif
    return unloaded;
}
auto PlaybackSession::pauseExecutionStep(const StepRequest& request, std::int64_t now)
    -> core::Result<void> {
    if (auto valid = validateExecutionControlBinding(request); !valid)
        return valid;
    if (state_->acceptedHostControl->kind != ControlKind::pause ||
        request.key.phase != StepPhase::softwareCommit || !state_->executionScheduler ||
        !state_->executionScheduler->pendingEpochEnd() ||
        request.target.tick !=
            state_->executionScheduler->pendingEpochEnd()->inclusiveTick.value() ||
        request.target.pauseEpoch != state_->executionScheduler->pendingEpochEnd()->pauseEpoch ||
        !state_->hostFence || state_->hostFence->scope_ != request.key.sessionScope ||
        state_->hostFence->candidateId_ != request.target.candidateId ||
        state_->hostFence->controlSequence_ != request.target.context.controlSequence ||
        (request.target.tailPauseAt && *request.target.tailPauseAt > now))
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Pause lacks accepted cut and fence"});
    HostMutationPermission permission{state_->hostCommitAuthorized};
    return pauseRealtimeGameplay(now);
}
auto PlaybackSession::resumeExecutionStep(const StepRequest& request,
                                          const GameplayReconciliation& reconciliation,
                                          GameplayExecutionContext context,
                                          const ClockObservation& clock, std::int64_t now,
                                          const StepReceipt* routeReceipt) -> core::Result<void> {
    if (auto valid = validateExecutionControlBinding(request); !valid)
        return valid;
    if (state_->acceptedHostControl->kind != ControlKind::resume ||
        request.key.phase != StepPhase::softwareCommit || !state_->hostFence ||
        !request.target.nextContext || state_->hostFence->scope_ != request.key.sessionScope ||
        state_->hostFence->candidateId_ != request.target.candidateId ||
        state_->hostFence->controlSequence_ != request.target.context.controlSequence ||
        request.target.tick != reconciliation.cutTick.value ||
        request.target.pauseEpoch != reconciliation.pauseEpoch ||
        (request.target.cut && request.target.cut != reconciliation.finalTraceCut) ||
        (request.target.reconciliationOrdinal &&
         request.target.reconciliationOrdinal != reconciliation.controlOrdinal) ||
        request.target.nextContext->sessionScope != context.sessionScope ||
        request.target.nextContext->clockSegment != context.clockSegment ||
        request.target.nextContext->runEpoch != context.runEpoch)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Resume lacks accepted route and fence"});
    if (!routeReceipt || routeReceipt->status != StepStatus::applied ||
        routeReceipt->request.key.participant != Participant::capture ||
        routeReceipt->request.key.phase != StepPhase::resumeRoute ||
        routeReceipt->request.key.operationId != request.key.operationId ||
        routeReceipt->request.key.sessionScope != request.key.sessionScope ||
        routeReceipt->request.target != request.target ||
        routeReceipt->effects.cut != reconciliation.finalTraceCut ||
        routeReceipt->effects.pauseEpoch != reconciliation.pauseEpoch ||
        reconciliation.trace.size() < reconciliation.finalTraceCut)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Final route is unconfirmed"});
    auto identity = computeGameplayReconciliationIdentity(reconciliation);
    if (!identity)
        return core::unexpected(identity.error());
    if ((request.resumeTrace ? std::optional{request.resumeTrace->identity}
                             : request.target.payloadIdentity) != *identity ||
        (request.resumeTrace &&
         (request.resumeTrace->finalCut != reconciliation.finalTraceCut ||
          routeReceipt->effects.payloadIdentity != request.resumeTrace->identity)))
        return core::unexpected(
            core::Error{"host.rpa.message_conflict", "Resume trace identity differs"});
    HostMutationPermission permission{state_->hostCommitAuthorized};
    return resumeRealtimeGameplay(reconciliation, context, clock, now, true, request.tailResumeAt);
}
auto PlaybackSession::seekExecutionStep(const StepRequest& request, const ControlReceipt& proof,
                                        const GameplayReplay& archive, GameplayTick horizon,
                                        const GameplayReplayCut& cut,
                                        std::span<const GameplayCheckpoint> checkpoints,
                                        GameplayPresentationTick tick, const RuntimeFrame& frame,
                                        SchedulingProfile profile,
                                        ClockMappingConfiguration mapping,
                                        const std::string& domain) -> core::Result<void> try {
    if (auto valid = validateExecutionControlBinding(request); !valid)
        return valid;
    const auto binding = detail::replayBindingMatches(archive, request.target.archiveIdentity,
                                                      request.target.replaySourceIdentity);
    if (!binding)
        return core::unexpected(binding.error());
    if (*state_->acceptedHostControl != proof.request || proof.request.kind != ControlKind::seek ||
        request.key.phase != StepPhase::softwareCommit || !request.target.nextContext ||
        !state_->hostFence || !controlReadyForCommit(proof) || !*binding ||
        request.target.seekHorizon != horizon.value ||
        request.target.seekCompleteRecords != cut.completeRecords ||
        request.target.seekPartialHorizon !=
            (cut.partialHorizon ? std::optional{cut.partialHorizon->value} : std::nullopt))
        return core::unexpected(core::Error{"host.rpa.context_stale", "Seek proof differs"});
    HostMutationPermission permission{state_->hostCommitAuthorized};
    auto result = seekGameplay(archive, horizon, cut, checkpoints, tick, frame);
    if (!result)
        return result;
    const auto& next = *request.target.nextContext;
    return beginRealtimeGameplay(profile, mapping,
                                 {next.sessionScope, next.clockSegment, next.runEpoch}, domain);
} catch (...) {
    return core::unexpected(core::Error{"host.rpa.capacity_exceeded", "Seek reservation failed"});
}

#endif

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

auto detail::CandidateMetadataAccess::prepared(const PreparedPlayback& playback)
    -> const chart::CandidateRuntimeMetadata* {
    if (playback.state_ == nullptr || !playback.state_->candidateMetadata) {
        return nullptr;
    }
    return &*playback.state_->candidateMetadata;
}

auto detail::CandidateMetadataAccess::active(const PlaybackSession& session)
    -> const chart::CandidateRuntimeMetadata* {
    if (session.state_ == nullptr || !session.state_->candidateMetadata) {
        return nullptr;
    }
    return &*session.state_->candidateMetadata;
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
        auto source = [&] {
            const internal::PrepareTraceScope trace{"source_parse"};
            return PlaybackSource::fromChartText(std::string{jsonText});
        }();
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
        const bool candidateSource = sourceState.candidate.has_value();
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        const bool gameplayOnly = sourceState.gameplayContent.valid() &&
                                  sourceState.gameplayIntent == GameplayPrepareIntent::GameplayOnly;
#else
        const bool gameplayOnly = false;
#endif

        if (!candidateSource && entryChart == nullptr) {
            auto error =
                core::Error{"playback.source.invalid", "PlaybackSource entry Chart is unavailable"};
            addErrorDiagnostic(diagnostics, error);
            return core::unexpected(std::move(error));
        }
        const std::string_view jsonText =
            entryChart != nullptr ? entryChart->utf8Text : std::string_view{};
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

        std::optional<chart::ChartRuntime> chartRuntimeStorage;
        std::optional<animation::AnimationProgram> compiledAnimation;
        if (candidateSource) {
            auto candidateRuntime =
                prepareCandidateStage(*sourceState.candidate,
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
                                      sourceState.gameplayContent.valid(),
#else
                                      false,
#endif
                                      options, replacement, limits, artifact, diagnostics);
            if (!candidateRuntime) {
                return core::unexpected(std::move(candidateRuntime.error()));
            }
            if (!gameplayOnly &&
                !preflightCapabilities(chart::ChartDocument{}, additionalCapabilities,
                                       context.capabilities, diagnostics)) {
                return core::unexpected(operationError("playback.capability.preflight_failed",
                                                       "Playback capability preflight failed",
                                                       diagnostics));
            }
            compiledAnimation = animation::AnimationProgram{};
            chartRuntimeStorage = std::move(*candidateRuntime);
        } else {
            auto loaded = [&] {
                const internal::PrepareTraceScope trace{"load_document"};
                return loadDocumentStage(context, artifact, diagnostics);
            }();
            if (!loaded) {
                const auto message = artifact.isV4 ? "Chart v4 loading produced errors"
                                                   : "Chart loading produced errors";
                return core::unexpected(operationError(context.replacement
                                                           ? "playback.chart.reload_load_failed"
                                                           : "playback.chart.load_failed",
                                                       message, diagnostics));
            }
            auto resolved = [&] {
                const internal::PrepareTraceScope trace{"resolve_parameters"};
                return resolveParametersStage(context, artifact, sourceState.projectDocuments,
                                              diagnostics);
            }();
            if (!resolved) {
                return core::unexpected(
                    operationError(context.replacement ? "playback.chart.reload_load_failed"
                                                       : "playback.chart.load_failed",
                                   artifact.v4Source ? "Chart v4 resolution produced errors"
                                                     : "Chart parameter resolution produced errors",
                                   diagnostics));
            }

            const bool preflightOk = [&] {
                const internal::PrepareTraceScope trace{"preflight"};
                return preflightCapabilities(*document, additionalCapabilities,
                                             context.capabilities, diagnostics);
            }();
            if (!preflightOk) {
                return core::unexpected(operationError("playback.capability.preflight_failed",
                                                       "Playback capability preflight failed",
                                                       diagnostics));
            }

            compiledAnimation = [&] {
                const internal::PrepareTraceScope trace{"animation_compile"};
                return compileAnimationProgram(std::move(animationProgram), limits, diagnostics);
            }();
            if (!compiledAnimation) {
                return core::unexpected(
                    operationError(replacement ? "playback.animation.reload_compile_failed"
                                               : "playback.animation.compile_failed",
                                   "Animation program compilation produced errors", diagnostics));
            }

            auto runtimeResult = [&] {
                const internal::PrepareTraceScope trace{"chart_compile"};
                return chart::ChartCompiler::compile(*document, limits);
            }();
            const bool runtimeValid = runtimeResult.hasValue();
            diagnostics.append(std::move(runtimeResult.diagnostics));
            if (!runtimeValid) {
                return core::unexpected(
                    operationError(replacement ? "playback.chart.reload_compile_failed"
                                               : "playback.chart.compile_failed",
                                   "Chart compilation produced errors", diagnostics));
            }
            chartRuntimeStorage = std::move(*runtimeResult.runtime);
        }
        auto& chartRuntime = *chartRuntimeStorage;
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

        {
            const internal::PrepareTraceScope trace{"resources"};
            if (sourceState.database) {
                resourceManager = std::make_unique<assets::ResourceManager>(
                    std::move(*sourceState.database), sourceState.provider);
            }
        }

        auto acquired = [&] {
            const internal::PrepareTraceScope trace{"audio"};
            return acquireAudioStage(artifact, chartRuntime, diagnostics);
        }();
        if (!acquired) {
            if (diagnostics.empty()) {
                addErrorDiagnostic(diagnostics, acquired.error());
            }
            return core::unexpected(std::move(acquired.error()));
        }
        if (gameplayOnly) {
            if (auto presentation =
                    preparePresentationStage(context, artifact, chartRuntime, diagnostics, false);
                !presentation)
                return core::unexpected(std::move(presentation.error()));
            artifact.snapshotLayout.emplace();
        } else {
            auto runtime = [&] {
                const internal::PrepareTraceScope trace{"runtime"};
                return prepareRuntimeStage(artifact, chartRuntime, std::move(*compiledAnimation),
                                           diagnostics);
            }();
            if (!runtime) {
                return core::unexpected(std::move(runtime.error()));
            }
            auto presentation = [&] {
                const internal::PrepareTraceScope trace{"presentation"};
                return preparePresentationStage(context, artifact, chartRuntime, diagnostics);
            }();
            if (!presentation) {
                return core::unexpected(std::move(presentation.error()));
            }
            auto committed = [&] {
                const internal::PrepareTraceScope trace{"commit_runtime"};
                return commitRuntimeStage(artifact);
            }();
            if (!committed) {
                addErrorDiagnostic(diagnostics, committed.error());
                return core::unexpected(std::move(committed.error()));
            }
            auto sampled = [&] {
                const internal::PrepareTraceScope trace{"commit_frame"};
                return commitFrameStage(context, artifact, diagnostics);
            }();
            if (!sampled) {
                return core::unexpected(std::move(sampled.error()));
            }

            const auto* presentationCandidate =
                artifact.presentation ? &*artifact.presentation : nullptr;
            auto layout = [&] {
                const internal::PrepareTraceScope trace{"snapshot_layout"};
                return buildSnapshotLayout(*artifact.runtimeSession, chartRuntime,
                                           presentationCandidate);
            }();
            if (!layout) {
                addErrorDiagnostic(diagnostics, layout.error());
                return core::unexpected(std::move(layout.error()));
            }
            artifact.snapshotLayout = std::move(*layout);
        }
        auto assembledIdentity = [&] {
            const internal::PrepareTraceScope trace{"identity"};
            return assembleIdentityStage(artifact, diagnostics);
        }();
        if (!assembledIdentity) {
            return core::unexpected(std::move(assembledIdentity.error()));
        }

        const internal::PrepareTraceScope traceAssemble{"assemble"};
        diagnostics.sortDeterministically();
        auto prepared = std::make_unique<PreparedPlayback::State>();
        prepared->owner = &context.owner;
        prepared->ownerToken = state_->sessionToken;
        prepared->expectedGeneration = state_->generation;
        prepared->replacement = context.replacement;
        prepared->contentProvider = std::move(sourceState.provider);
        prepared->resourceManager = std::move(resourceManager);
        prepared->chartJson = candidateSource ? std::string{} : std::string{jsonText};
        prepared->runtimeSession = std::move(artifact.runtimeSession);
        prepared->snapshotLayout = std::move(*artifact.snapshotLayout);
        prepared->chartInfo = chartInfoFor(
            chartRuntime, prepared->runtimeSession ? prepared->runtimeSession->resourceCount() : 0);
        prepared->contentInfo = PlaybackContentInfo{
            chartRuntime.chartId.value, chartRuntime.version, chartRuntime.timingMap.offsetMs(),
            context.mode,
            chartRuntime.mainMusic ? std::optional<std::string>{chartRuntime.mainMusic->value}
                                   : std::nullopt};
        prepared->parameters = context.options.parameters;
        prepared->semanticIdentity = toPublicIdentity(*assembledIdentity);
        prepared->candidateMetadata = std::move(artifact.candidateMetadata);
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
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (sourceState.gameplayContent.valid()) {
            const auto* content = detail::GameplayAccess::content(sourceState.gameplayContent);
            auto index = prepareProjection(*content, prepared->snapshotLayout);
            if (!index)
                return core::unexpected(std::move(index.error()));
            auto gameplay = judgement::JudgementSession::create();
            if (!gameplay)
                return core::unexpected(std::move(gameplay.error()));
            if (auto r = gameplay->configure(content->configuration); !r)
                return core::unexpected(std::move(r.error()));
            if (auto r = gameplay->prepare(content->capsule.gameplay); !r)
                return core::unexpected(std::move(r.error()));
            prepared->gameplay =
                std::make_unique<judgement::JudgementSession>(std::move(*gameplay));
            prepared->gameplayContent = sourceState.gameplayContent;
            prepared->gameplayProjectionIndex = std::move(*index);
        }
#endif

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
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("commit"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence && !state_->hostCommitAuthorized)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence owns mutation"});
#endif
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

    auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock, true);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    detail::CommittedPresentationState descriptor;
    descriptor.source = candidate.replacement ? internal::presentation::Source::Restore
                                              : internal::presentation::Source::Prepared;
    descriptor.frame = candidate.targetFrame;
    state_->contentProvider = std::move(candidate.contentProvider);
    state_->resourceManager = std::move(candidate.resourceManager);
    state_->activeChartJson = std::move(candidate.chartJson);
    state_->runtimeSession = std::move(candidate.runtimeSession);
    state_->snapshotLayout = std::move(candidate.snapshotLayout);
    state_->activeChartInfo = candidate.chartInfo;
    state_->activeContentInfo = std::move(candidate.contentInfo);
    state_->activeMode = state_->activeContentInfo->mode;
    state_->semanticIdentity = candidate.semanticIdentity;
    state_->candidateMetadata = std::move(candidate.candidateMetadata);
    state_->parameters = std::move(candidate.parameters);
    state_->lastFrame = candidate.targetFrame;
    state_->diagnostics = std::move(candidate.diagnostics);
    state_->lastOperationDiagnostics = std::move(candidate.lastOperationDiagnostics);
    state_->sessionState = candidate.committedState;
    state_->presentation = std::move(candidate.presentation);
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    state_->gameplayContent = std::move(candidate.gameplayContent);
    state_->gameplayProjectionIndex = std::move(candidate.gameplayProjectionIndex);
    state_->pendingRetiredRequestFloor.reset();
    state_->executionScheduler.reset();
    state_->realtimeClock.reset();
    state_->executionContext.reset();
    state_->realtimeWasStarted = false;
    state_->gameplay = std::move(candidate.gameplay);
    state_->gameplayState = state_->gameplay ? GameplayState::Prepared : GameplayState::Inactive;
    state_->gameplayReceipt.reset();
    state_->gameplayProjection = {};
#endif
    state_->bumpGeneration();
    state_->publishPresentation(*mutation, descriptor);
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
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("update"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks update"});
#endif
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
    if (state_->gameplayState == GameplayState::Paused)
        return core::unexpected(gameplayError("Paused Gameplay preserves the published frame"));
#endif
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
    if (!state_->runtimeSession)
        return core::unexpected(
            core::Error{"playback.session.empty", "PlaybackSession has no committed World"});
    auto mutation =
        internal::presentation::Mutation::reserve(state_->presentationClock, false, true);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto updated = state_->runtimeSession->update(runtimeFrame(frame));
    if (!updated) {
        return core::unexpected(std::move(updated.error()));
    }
    detail::CommittedPresentationState descriptor;
    descriptor.source = internal::presentation::Source::RuntimeUpdate;
    descriptor.frame = frame;
    descriptor.activationRevision = state_->committedPresentation->activationRevision;
    state_->publishPresentation(*mutation, descriptor);
    state_->lastFrame = frame;
    state_->sessionState = SessionState::Running;
    state_->bumpGeneration();
    return {};
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport) const
    -> core::Result<FrameSnapshot> try {
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
} catch (const std::bad_alloc&) {
    return core::unexpected(prepareExceptionError("extract_frame", true));
} catch (const std::length_error&) {
    return core::unexpected(prepareExceptionError("extract_frame", true));
} catch (const std::exception& exception) {
    return core::unexpected(prepareExceptionError("extract_frame", false, &exception));
} catch (...) {
    return core::unexpected(prepareExceptionError("extract_frame", false));
}

auto PlaybackSession::extractFrame(const FrameViewport& viewport, FrameSnapshot& snapshot) const
    -> core::Result<void> {
    return detail::PresentationAccess::materialize(*this, std::nullopt, viewport, snapshot);
}

auto detail::PresentationAccess::materialize(const PlaybackSession& session,
                                             std::optional<std::uint64_t> exactVersion,
                                             const FrameViewport& viewport, FrameSnapshot& snapshot)
    -> core::Result<void> try {
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
    // Product tag: extract-frame materialization is the per-call snapshot DTO, not session state.
    const internal::pd::memory::TagScope tag{internal::pd::memory::ownerMain,
                                             internal::pd::memory::domainSnapshot};
#endif
    auto* state_ = session.state_.get();
    internal::pd::Timer measurement{internal::pd::Stage::snapshot};
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

    if (!state_->committedPresentation ||
        (exactVersion && *exactVersion != state_->committedPresentation->resultVersion))
        return core::unexpected(core::Error{"playback.presentation.version_unavailable",
                                            "Requested presentation version is unavailable"});
    measurement.items(state_->snapshotLayout.entities.size());
    const auto& committed = *state_->committedPresentation;
    auto extracted = runtime::detail::PresentationAccess::read(
        std::as_const(*state_->runtimeSession), committed.runtime,
        [&](const runtime::detail::PresentationRead& view) {
            return detail::materializeSnapshot(view, state_->snapshotLayout, viewport, snapshot);
        });
    if (!extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    return {};
} catch (const std::bad_alloc&) {
    return core::unexpected(prepareExceptionError("extract_frame", true));
} catch (const std::length_error&) {
    return core::unexpected(prepareExceptionError("extract_frame", true));
} catch (const std::exception& exception) {
    return core::unexpected(prepareExceptionError("extract_frame", false, &exception));
} catch (...) {
    return core::unexpected(prepareExceptionError("extract_frame", false));
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
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("unload"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence && !state_->hostCommitAuthorized)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks unload"});
#endif
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
    auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
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
    state_->candidateMetadata.reset();
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    state_->pendingRetiredRequestFloor.reset();
    state_->executionScheduler.reset();
    state_->realtimeClock.reset();
    state_->executionContext.reset();
    state_->realtimeWasStarted = false;
    state_->gameplay.reset();
    state_->gameplayContent = {};
    state_->gameplayProjectionIndex = {};
    state_->gameplayState = GameplayState::Inactive;
    state_->gameplayReceipt.reset();
    state_->gameplayProjection = {};
#endif
    state_->parameters = {};
    state_->presentation.reset();
    state_->sessionState = SessionState::Empty;
    state_->publishPresentation(*mutation, {});
    state_->bumpGeneration();
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
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("acquire_host_override"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks override"});
#endif
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (auto guard = gameplayWriteGuard(); !guard)
        return core::unexpected(std::move(guard.error()));
#endif
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
        auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock);
        if (!mutation)
            return core::unexpected(std::move(mutation.error()));
        auto token = state_->runtimeSession->acquireOverride(
            world::OverrideKind::Host, std::string{ownerId}, priority, propertyMask,
            toWorldLifetime(lifetime), mapped);
        if (!token) {
            return core::unexpected(mapHostOverrideError(token.error()));
        }
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (state_->gameplay)
            state_->bumpGeneration();
#endif
        detail::CommittedPresentationState descriptor;
        descriptor.source = internal::presentation::Source::Mutation;
        descriptor.activationRevision = state_->committedPresentation->activationRevision;
        state_->publishPresentation(*mutation, descriptor);
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
    if (!state_->ownerThread.isCurrent())
        return core::unexpected(ownerError("release_host_override"));
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->hostFence)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Commit fence blocks override"});
#endif
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (auto guard = gameplayWriteGuard(); !guard)
        return guard;
#endif
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
    auto mutation = internal::presentation::Mutation::reserve(state_->presentationClock);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto released =
        state_->runtimeSession->releaseOverride(world::OverrideTokenId{.value = token.value});
    if (!released) {
        return core::unexpected(mapHostOverrideError(released.error()));
    }
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (state_->gameplay)
        state_->bumpGeneration();
#endif
    detail::CommittedPresentationState descriptor;
    descriptor.source = internal::presentation::Source::Mutation;
    descriptor.activationRevision = state_->committedPresentation->activationRevision;
    state_->publishPresentation(*mutation, descriptor);
    return {};
}


} // namespace cuexis::playback
