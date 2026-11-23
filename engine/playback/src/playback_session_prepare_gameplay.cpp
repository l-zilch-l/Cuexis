// Candidate prepare hook: candidate lowering and candidate resource-capability population.
// This translation unit belongs to the experimental source table only; the ordinary source
// table provides the same-signature rejection hook in playback_session_prepare_stages.cpp.

#include "playback_session_internal.hpp"
#include "playback_session_prepare_internal.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::playback {
namespace prepare_detail {

// Typed candidate resource closure -> session resource requirements. Called only by the
// experimental prepare hook, so this conversion stays out of the shared/default Prepare TU.
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

} // namespace prepare_detail
} // namespace cuexis::playback
