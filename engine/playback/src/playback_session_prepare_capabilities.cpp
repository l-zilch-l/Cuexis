// Prepare capability rules: the required capability and presentation limit preflight.

#include "playback_session_internal.hpp"
#include "playback_session_prepare_internal.hpp"

namespace cuexis::playback {
namespace prepare_detail {

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
} // namespace prepare_detail
} // namespace cuexis::playback
