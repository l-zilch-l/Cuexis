#include <cuexis/chart/animation_template_loader.hpp>
#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/chart_writer.hpp>
#include <cuexis/chart/detail/chart_v4_resolver_internal.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>
#include <cuexis/json/value.hpp>

#include "canonical_chart_loader_internal.hpp"
#include "chart_parameter_resolver_internal.hpp"
#include "chart_project_path_internal.hpp"
#include "chart_v4_animation_reader_internal.hpp"
#include "chart_v4_common_reader_internal.hpp"
#include "chart_v4_loader_internal.hpp"
#include "chart_v4_resolver_units_internal.hpp"
#include "chart_writer_internal.hpp"
#include "diagnostic_limit.hpp"
#include "sha256_internal.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart {

[[nodiscard]] auto
findClip(const std::map<AnimationRecordIdentity, const AnimationClip*, std::less<>>& clips,
         const AnimationRecordIdentity& identity) -> const AnimationClip* {
    const auto item = clips.find(identity);
    return item != clips.end() ? item->second : nullptr;
}

[[nodiscard]] auto checkedSum(std::size_t left, std::size_t right) -> std::optional<std::size_t> {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

[[nodiscard]] auto checkedAccumulate(std::size_t& total, std::size_t count, std::size_t limit)
    -> bool {
    if (count > limit || total > limit - count) {
        return false;
    }
    total += count;
    return true;
}

void validateClipBudget(const AnimationClip& clip, std::string_view diagnosticPath,
                        const ProgramDiagnosticScope* scope, const ChartLimits& limits,
                        std::size_t& totalTracks, std::size_t& totalSegmentsAndSteps,
                        core::Diagnostics& diagnostics) {
    const auto trackCount = checkedSum(clip.tracks.size(), clip.stepTracks.size());
    if (!trackCount || *trackCount > limits.maxAnimationTracksPerClip) {
        addScopedError(diagnostics, "chart.animation.generated_limit",
                       "Animation track count exceeds the configured per-Clip limit", scope,
                       std::string{diagnosticPath});
    }
    if (!trackCount || !checkedAccumulate(totalTracks, *trackCount, limits.maxAnimationTracks)) {
        addScopedError(diagnostics, "chart.animation.generated_limit",
                       "Total animation track count exceeds the configured limit", scope,
                       std::string{diagnosticPath});
    }
    for (const auto& track : clip.tracks) {
        if (track.segments.size() > limits.maxAnimationSegmentsOrStepsPerTrack) {
            addScopedError(diagnostics, "chart.animation.generated_limit",
                           "Animation segment count exceeds the configured per-Track limit", scope,
                           track.fieldPath);
        }
        if (!checkedAccumulate(totalSegmentsAndSteps, track.segments.size(),
                               limits.maxAnimationSegmentsAndSteps)) {
            addScopedError(diagnostics, "chart.animation.generated_limit",
                           "Total animation segment and step count exceeds the configured limit",
                           scope, track.fieldPath);
        }
    }
    for (const auto& track : clip.stepTracks) {
        if (track.steps.size() > limits.maxAnimationSegmentsOrStepsPerTrack) {
            addScopedError(diagnostics, "chart.animation.generated_limit",
                           "Animation step count exceeds the configured per-Track limit", scope,
                           track.fieldPath);
        }
        if (!checkedAccumulate(totalSegmentsAndSteps, track.steps.size(),
                               limits.maxAnimationSegmentsAndSteps)) {
            addScopedError(diagnostics, "chart.animation.generated_limit",
                           "Total animation segment and step count exceeds the configured limit",
                           scope, track.fieldPath);
        }
    }
}

void validateProgram(const AnimationProgramInput& program, const ChartDocument& chart,
                     const ProgramPaths& paths,
                     const std::map<std::string, ImportedTemplate, std::less<>>& imports,
                     const ChartLimits& limits, core::Diagnostics& diagnostics) {
    std::map<AnimationRecordIdentity, const AnimationClip*, std::less<>> clips;
    std::size_t totalTracks{};
    std::size_t totalSegmentsAndSteps{};
    for (const auto& [id, imported] : imports) {
        static_cast<void>(id);
        validateClipBudget(imported.document.clip, imported.importFieldPath + "/source", nullptr,
                           limits, totalTracks, totalSegmentsAndSteps, diagnostics);
    }
    for (const auto& item : program.clips) {
        clips.emplace(item.identity, &item.clip);
        const auto clipPath = paths.clips.find(item.identity);
        const auto* clipScope = clipPath != paths.clips.end() ? &clipPath->second : nullptr;
        validateClipBudget(item.clip, item.clip.fieldPath, clipScope, limits, totalTracks,
                           totalSegmentsAndSteps, diagnostics);
    }

    std::map<std::string, const ObjectComponents*, std::less<>> componentsByObject;
    for (const auto& object : chart.objects) {
        componentsByObject.emplace(object.id.value, &object.components);
    }
    for (const auto& object : program.objects) {
        const auto components = componentsByObject.find(object.objectId.value);
        const ObjectComponents* objectComponents =
            components != componentsByObject.end() ? components->second : nullptr;
        for (std::size_t left = 0; left < object.layers.size(); ++left) {
            const auto leftMask = expandMask(object.layers[left].propertyMask);
            for (std::size_t right = left + 1; right < object.layers.size(); ++right) {
                if (object.layers[left].priority == object.layers[right].priority &&
                    intersects(leftMask, expandMask(object.layers[right].propertyMask))) {
                    const auto path = paths.layers.find(
                        LayerPathKey{object.objectId.value, object.layers[right].identity});
                    const auto alternatePath = paths.layers.find(
                        LayerPathKey{object.objectId.value, object.layers[left].identity});
                    const auto* scope = preferGeneratedScope(
                        path != paths.layers.end() ? &path->second : nullptr,
                        alternatePath != paths.layers.end() ? &alternatePath->second : nullptr);
                    addScopedError(diagnostics, "chart.animation.mask_conflict",
                                   "Layers with the same priority have overlapping masks", scope,
                                   "$/animationProgram");
                }
            }
            std::vector<PropertySet> groupProperties;
            for (const auto& group : object.layers[left].blendGroups) {
                PropertySet effectiveGroup;
                for (const auto& instance : group.instances) {
                    const auto instanceMask = expandMask(instance.propertyMask);
                    const auto instancePath = paths.instances.find(
                        InstancePathKey{object.objectId.value, object.layers[left].identity,
                                        group.identity, instance.identity});
                    if (!isSubset(instanceMask, leftMask)) {
                        addScopedError(diagnostics, "chart.animation.mask_conflict",
                                       "Clip instance mask is not a subset of its Layer mask",
                                       instancePath != paths.instances.end() ? &instancePath->second
                                                                             : nullptr,
                                       "$/animationProgram");
                    }
                    const auto* clip = findClip(clips, instance.clipIdentity);
                    if (clip == nullptr) {
                        addScopedError(diagnostics, "chart.animation.reference_missing",
                                       "Clip instance refers to a missing concrete Clip",
                                       instancePath != paths.instances.end() ? &instancePath->second
                                                                             : nullptr,
                                       "$/animationProgram");
                        continue;
                    }
                    auto effective = intersection(clipProperties(*clip), instanceMask);
                    effective = intersection(effective, leftMask);
                    effectiveGroup.insert(effective.begin(), effective.end());
                    if (group.mode == AnimationBlendMode::Additive &&
                        effective.contains("transform.scale") && !scaleIsPositive(*clip)) {
                        addScopedError(diagnostics, "chart.animation.additive_unsupported",
                                       "Additive transform.scale requires positive factors",
                                       instancePath != paths.instances.end() ? &instancePath->second
                                                                             : nullptr,
                                       "$/animationProgram");
                    }
                }
                const auto groupPath = paths.groups.find(GroupPathKey{
                    object.objectId.value, object.layers[left].identity, group.identity});
                if (group.mode == AnimationBlendMode::Additive &&
                    !additiveAllowed(effectiveGroup)) {
                    addScopedError(diagnostics, "chart.animation.additive_unsupported",
                                   "Additive BlendGroup targets an unsupported property",
                                   groupPath != paths.groups.end() ? &groupPath->second : nullptr,
                                   "$/animationProgram");
                }
                if (isDiscrete(effectiveGroup) &&
                    (object.layers[left].weight != 1.0 || group.weight != 1.0)) {
                    addScopedError(diagnostics, "chart.animation.discrete_weight_unsupported",
                                   "Discrete properties require resolved Layer and Group weight 1",
                                   groupPath != paths.groups.end() ? &groupPath->second : nullptr,
                                   "$/animationProgram");
                }
                for (const auto& prior : groupProperties) {
                    if (intersects(prior, effectiveGroup)) {
                        addScopedError(
                            diagnostics, "chart.animation.track_conflict",
                            "BlendGroups in one Layer have overlapping effective properties",
                            groupPath != paths.groups.end() ? &groupPath->second : nullptr,
                            "$/animationProgram");
                        break;
                    }
                }
                groupProperties.push_back(effectiveGroup);
                if (objectComponents != nullptr) {
                    for (const auto& property : effectiveGroup) {
                        if (property.starts_with("transform.") && !objectComponents->transform) {
                            addScopedError(diagnostics, "chart.animation.reference_missing",
                                           "Animation property requires cuexis.transform",
                                           groupPath != paths.groups.end() ? &groupPath->second
                                                                           : nullptr,
                                           "$/animationProgram");
                        }
                        if ((property.starts_with("material.") ||
                             property.starts_with("render.")) &&
                            !objectComponents->renderable) {
                            addScopedError(diagnostics, "chart.animation.reference_missing",
                                           "Animation property requires cuexis.renderable",
                                           groupPath != paths.groups.end() ? &groupPath->second
                                                                           : nullptr,
                                           "$/animationProgram");
                        }
                    }
                }
            }
        }
    }
}

void addResource(std::map<std::string, std::set<ChartResourceUse>, std::less<>>& resources,
                 const AssetId& assetId, ChartResourceUse use) {
    resources[assetId.value].insert(use);
}

void addRenderableResources(
    std::map<std::string, std::set<ChartResourceUse>, std::less<>>& resources,
    const ObjectComponents& components) {
    if (!components.renderable) {
        return;
    }
    addResource(resources, components.renderable->mesh, ChartResourceUse::RenderableMesh);
    addResource(resources, components.renderable->material, ChartResourceUse::RenderableMaterial);
}

void addClipResources(std::map<std::string, std::set<ChartResourceUse>, std::less<>>& resources,
                      const AnimationClip& clip) {
    for (const auto& track : clip.stepTracks) {
        if (track.property != AnimationStepProperty::RenderMaterial) {
            continue;
        }
        for (const auto& step : track.steps) {
            if (const auto* asset = std::get_if<AssetId>(&step.value)) {
                addResource(resources, *asset, ChartResourceUse::AnimationMaterial);
            }
        }
    }
}

[[nodiscard]] auto
resourceRequirements(const ChartDocument& chart, const std::vector<AnimationClip>& chartClips,
                     const std::map<std::string, ImportedTemplate, std::less<>>& imports)
    -> std::vector<ChartResourceRequirement> {
    std::map<std::string, std::set<ChartResourceUse>, std::less<>> resources;
    if (chart.audio) {
        addResource(resources, chart.audio->mainMusic, ChartResourceUse::MainMusic);
    }
    for (const auto& item : chart.templates) {
        addRenderableResources(resources, item.prototype);
    }
    for (const auto& item : chart.objects) {
        addRenderableResources(resources, item.components);
    }
    for (const auto& behavior : chart.behaviors) {
        for (const auto& event : behavior.stepEvents) {
            if (event.property == BehaviorStepProperty::RenderMaterial) {
                if (const auto* asset = std::get_if<AssetId>(&event.value)) {
                    addResource(resources, *asset, ChartResourceUse::BehaviorMaterial);
                }
            }
        }
    }
    for (const auto& clip : chartClips) {
        addClipResources(resources, clip);
    }
    for (const auto& [id, imported] : imports) {
        static_cast<void>(id);
        addClipResources(resources, imported.document.clip);
    }

    std::vector<ChartResourceRequirement> result;
    result.reserve(resources.size());
    for (const auto& [assetId, uses] : resources) {
        result.push_back(ChartResourceRequirement{
            AssetId{assetId}, std::vector<ChartResourceUse>{uses.begin(), uses.end()}});
    }
    return result;
}

[[nodiscard]] auto
capabilityRequirements(const ChartV4SourceDocument& source,
                       const std::map<std::string, AnimatorComponent, std::less<>>& animators)
    -> std::vector<std::string> {
    std::set<std::string, std::less<>> result{"cuexis.chart.v4"};
    if (!source.animationTemplateImports.empty()) {
        result.emplace("cuexis.source.cxt.v1");
    }
    bool hasAnimation = !source.animationTemplateImports.empty() || !source.animationClips.empty();
    for (const auto& animator : source.animators) {
        hasAnimation = hasAnimation || !animator.component.templateBindings.empty() ||
                       !animator.component.layers.empty();
    }
    for (const auto& [id, animator] : animators) {
        static_cast<void>(id);
        hasAnimation =
            hasAnimation || !animator.templateBindings.empty() || !animator.layers.empty();
    }
    if (hasAnimation) {
        result.emplace("cuexis.animation.clip.v1");
        result.emplace("cuexis.animation.layers.v1");
    }
    return {result.begin(), result.end()};
}

auto CanonicalContentIdentity::hex() const -> std::string {
    return detail::sha256Hex(sha256);
}

namespace {

[[nodiscard]] auto resolveParsedV4Source(const ChartV4SourceDocument& source, json::Value parsed,
                                         detail::ResolvedParameterSet resolvedParameters,
                                         std::span<const ProjectDocument> projectDocuments,
                                         std::span<const RequiredExtension> supportedExtensions,
                                         const ChartLimits& limits, core::Diagnostics diagnostics)
    -> ChartV4ResolveResult {
    auto canonicalChart = detail::writeV4Value(parsed, limits);
    if (!canonicalChart) {
        addResolverError(diagnostics, canonicalChart.error(), "$");
    }

    for (const auto& use : source.parameterUses) {
        const auto value = resolvedParameters.byId.find(use.id);
        if (value == resolvedParameters.byId.end() ||
            !replaceAtFieldPath(parsed, use.fieldPath, parameterJsonValue(value->second))) {
            addResolverError(diagnostics, "chart.parameter.use_not_allowed",
                             "Chart parameter use could not be resolved at its source path",
                             use.fieldPath);
        }
    }

    std::map<std::string, std::uint32_t, std::less<>> supported;
    for (const auto& extension : supportedExtensions) {
        auto [item, inserted] = supported.emplace(extension.id, extension.version);
        if (!inserted) {
            item->second = std::max(item->second, extension.version);
        }
    }
    validateRequiredExtensions(source.requiredExtensions, supported, diagnostics,
                               "$/requiredExtensions");
    auto imports = loadImports(source, projectDocuments, supported, limits, diagnostics);
    auto chart = makeConcreteChart(parsed, limits, diagnostics);
    auto animators = concreteAnimators(parsed, limits, diagnostics);
    auto program =
        buildProgram(source, animators, imports, resolvedParameters.byId, limits, diagnostics);
    if (chart) {
        validateProgram(program.program, *chart, program.paths, imports, limits, diagnostics);
    }

    std::vector<CxtIdentityComponent> cxtIdentities;
    cxtIdentities.reserve(imports.size());
    for (const auto& [id, imported] : imports) {
        cxtIdentities.push_back(CxtIdentityComponent{id, imported.identity});
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors() || !chart || !canonicalChart ||
        imports.size() != source.animationTemplateImports.size()) {
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    auto resources = resourceRequirements(*chart, source.animationClips, imports);
    auto capabilities = capabilityRequirements(source, animators);
    return ChartV4ResolveResult{
        ChartV4ResolvedArtifact{
            ResolvedChartDocument{std::move(*chart), std::move(resolvedParameters.values)},
            std::move(program.program), CanonicalContentIdentity{detail::sha256(*canonicalChart)},
            std::move(cxtIdentities), resolvedParameters.identity, std::move(resources),
            std::move(capabilities)},
        std::move(diagnostics)};
}

} // namespace

auto ChartV4Resolver::resolve(const ChartV4SourceDocument& source,
                              std::span<const ChartParameterInput> parameters,
                              std::span<const ProjectDocument> projectDocuments,
                              std::span<const RequiredExtension> supportedExtensions,
                              const ChartLimits& limits) -> ChartV4ResolveResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    auto resolvedParameters = detail::resolveChartParameters(
        source.parameters, source.parameterUses, parameters, limits, diagnostics);
    auto parsed =
        json::parse(source.canonicalSource.canonicalText,
                    {limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes});
    if (!parsed) {
        addResolverError(diagnostics, parsed.error(), "$");
    }
    if (!resolvedParameters || !parsed) {
        diagnostics.sortDeterministically();
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    return resolveParsedV4Source(source, std::move(*parsed), std::move(*resolvedParameters),
                                 projectDocuments, supportedExtensions, limits,
                                 std::move(diagnostics));
}

auto detail::resolveV4Parsed(const ChartV4SourceDocument& source,
                             const ParsedChartInput& parsedInput,
                             std::span<const ChartParameterInput> parameters,
                             std::span<const ProjectDocument> projectDocuments,
                             std::span<const RequiredExtension> supportedExtensions,
                             const ChartLimits& limits) -> ChartV4ResolveResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    auto resolvedParameters = detail::resolveChartParameters(
        source.parameters, source.parameterUses, parameters, limits, diagnostics);
    if (!resolvedParameters) {
        diagnostics.sortDeterministically();
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    return resolveParsedV4Source(source, parsedInput.value, std::move(*resolvedParameters),
                                 projectDocuments, supportedExtensions, limits,
                                 std::move(diagnostics));
}

} // namespace cuexis::chart
