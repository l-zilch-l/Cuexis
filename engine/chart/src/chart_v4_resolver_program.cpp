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
extensionSatisfied(const RequiredExtension& required,
                   const std::map<std::string, std::uint32_t, std::less<>>& supported) -> bool {
    const auto item = supported.find(required.id);
    return item != supported.end() && item->second >= required.version;
}

void validateRequiredExtensions(const std::vector<RequiredExtension>& required,
                                const std::map<std::string, std::uint32_t, std::less<>>& supported,
                                core::Diagnostics& diagnostics, std::string path,
                                std::optional<std::string_view> source,
                                std::optional<std::string_view> templateId,
                                std::optional<std::string_view> importId) {
    for (const auto& extension : required) {
        if (extensionSatisfied(extension, supported)) {
            continue;
        }
        auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error,
                                           "chart.extension.required_unsupported",
                                           "Required extension is not supported", path};
        diagnostic.withContext("extension_id", extension.id);
        diagnostic.withContext("required_version", std::to_string(extension.version));
        if (source) {
            diagnostic.withContext("source", std::string{*source});
        }
        if (templateId) {
            diagnostic.withContext("template_id", std::string{*templateId});
        }
        if (importId) {
            diagnostic.withContext("import_id", std::string{*importId});
        }
        diagnostics.add(std::move(diagnostic));
    }
}

[[nodiscard]] auto
loadImports(const ChartV4SourceDocument& source, std::span<const ProjectDocument> projectDocuments,
            const std::map<std::string, std::uint32_t, std::less<>>& supportedExtensions,
            const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::map<std::string, ImportedTemplate, std::less<>> {
    std::map<std::string, const ProjectDocument*, std::less<>> documents;
    std::set<std::string, std::less<>> foldedPaths;
    for (std::size_t index = 0; index < projectDocuments.size(); ++index) {
        const auto& document = projectDocuments[index];
        if (!detail::isPortableProjectPath(document.path)) {
            addResolverError(diagnostics, "cxt.template.invalid",
                             "Project document path is not portable",
                             "$/projectDocuments/" + std::to_string(index) + "/path");
            continue;
        }
        if (!foldedPaths.emplace(detail::portableProjectPathCaseKey(document.path)).second ||
            !documents.emplace(document.path, &document).second) {
            addResolverError(diagnostics, "cxt.import.duplicate",
                             "Project document path is duplicated",
                             "$/projectDocuments/" + std::to_string(index) + "/path");
        }
    }

    std::map<std::string, ImportedTemplate, std::less<>> imports;
    if (source.animationTemplateImports.size() > limits.maxAnimationImports) {
        addResolverError(diagnostics, "cxt.budget.exceeded",
                         "Animation template import count exceeds the configured limit",
                         "$/animationTemplateImports");
    }
    for (const auto& item : source.animationTemplateImports) {
        const auto document = documents.find(item.source);
        if (document == documents.end()) {
            auto diagnostic = core::Diagnostic{
                core::DiagnosticSeverity::Error, "cxt.import.missing",
                "Animation template source is missing", item.fieldPath + "/source"};
            diagnostic.withContext("source", item.source);
            diagnostic.withContext("template_id", item.id);
            diagnostic.withContext("import_id", item.id);
            diagnostics.add(std::move(diagnostic));
            continue;
        }
        if (document->second->utf8Text.size() > limits.maxAnimationTemplateBytes) {
            auto diagnostic = core::Diagnostic{
                core::DiagnosticSeverity::Error, "cxt.budget.exceeded",
                "Animation template exceeds the byte limit", item.fieldPath + "/source"};
            diagnostic.withContext("source", item.source);
            diagnostic.withContext("template_id", item.id);
            diagnostic.withContext("import_id", item.id);
            diagnostics.add(std::move(diagnostic));
            continue;
        }
        auto loaded = AnimationTemplateLoader::load(document->second->utf8Text, limits);
        const bool valid = loaded.hasValue();
        appendTemplateDiagnostics(diagnostics, std::move(loaded.diagnostics), item.source, item.id);
        if (!valid || !loaded.document) {
            continue;
        }
        if (loaded.document->templateId != item.id) {
            auto diagnostic = core::Diagnostic{
                core::DiagnosticSeverity::Error, "cxt.template.id_mismatch",
                "Animation template ID does not match the Chart import", item.fieldPath + "/id"};
            diagnostic.withContext("source", item.source);
            diagnostic.withContext("import_id", item.id);
            diagnostic.withContext("template_id", loaded.document->templateId);
            diagnostics.add(std::move(diagnostic));
            continue;
        }
        validateRequiredExtensions(loaded.document->requiredExtensions, supportedExtensions,
                                   diagnostics, "$/requiredExtensions", item.source,
                                   loaded.document->templateId, item.id);
        auto canonical = ChartWriter::writeAnimationTemplate(*loaded.document, limits);
        if (!canonical) {
            addResolverError(diagnostics, canonical.error(), item.fieldPath + "/source");
            continue;
        }
        imports.emplace(item.id,
                        ImportedTemplate{std::move(*loaded.document),
                                         CanonicalContentIdentity{detail::sha256(*canonical)},
                                         item.source, item.fieldPath});
    }
    return imports;
}

[[nodiscard]] auto
resolvedWeight(const WeightSource& source,
               const std::map<std::string, ChartParameterValue, std::less<>>& parameters,
               core::Diagnostics& diagnostics, std::string path) -> std::optional<double> {
    if (const auto* literal = std::get_if<double>(&source)) {
        return *literal == 0.0 ? 0.0 : *literal;
    }
    const auto& reference = std::get<ParameterReference>(source);
    const auto item = parameters.find(reference.id);
    const auto* value = item != parameters.end() ? std::get_if<double>(&item->second) : nullptr;
    if (value == nullptr || !std::isfinite(*value) || *value < 0.0 || *value > 1.0) {
        addResolverError(diagnostics, "chart.parameter.type_mismatch",
                         "Weight parameter did not resolve to a value within [0, 1]",
                         std::move(path));
        return std::nullopt;
    }
    return *value == 0.0 ? 0.0 : *value;
}

[[nodiscard]] auto
resolvedRational(const RationalSource& source,
                 const std::map<std::string, ChartParameterValue, std::less<>>& parameters,
                 core::Diagnostics& diagnostics, std::string path) -> std::optional<RationalBeat> {
    if (const auto* literal = std::get_if<RationalBeat>(&source)) {
        return *literal;
    }
    const auto& reference = std::get<ParameterReference>(source);
    const auto item = parameters.find(reference.id);
    const auto* value =
        item != parameters.end() ? std::get_if<RationalBeat>(&item->second) : nullptr;
    if (value == nullptr || value->numerator() <= 0) {
        addResolverError(diagnostics, "chart.parameter.type_mismatch",
                         "Duration-scale parameter did not resolve to a positive Rational",
                         std::move(path));
        return std::nullopt;
    }
    return *value;
}

[[nodiscard]] auto propertyName(AnimationProperty property) -> std::string_view {
    switch (property) {
    case AnimationProperty::TransformPositionX:
        return "transform.position.x";
    case AnimationProperty::TransformPositionY:
        return "transform.position.y";
    case AnimationProperty::TransformPositionZ:
        return "transform.position.z";
    case AnimationProperty::TransformRotation:
        return "transform.rotation";
    case AnimationProperty::TransformScale:
        return "transform.scale";
    case AnimationProperty::MaterialOpacity:
        return "material.opacity";
    case AnimationProperty::MaterialTint:
        return "material.tint";
    }
    return "";
}

[[nodiscard]] auto propertyName(AnimationStepProperty property) -> std::string_view {
    switch (property) {
    case AnimationStepProperty::RenderVisible:
        return "render.visible";
    case AnimationStepProperty::RenderMaterial:
        return "render.material";
    }
    return "";
}

[[nodiscard]] auto allProperties() -> const std::array<std::string_view, 9>& {
    static constexpr std::array values{
        std::string_view{"material.opacity"},     std::string_view{"material.tint"},
        std::string_view{"render.material"},      std::string_view{"render.visible"},
        std::string_view{"transform.position.x"}, std::string_view{"transform.position.y"},
        std::string_view{"transform.position.z"}, std::string_view{"transform.rotation"},
        std::string_view{"transform.scale"},
    };
    return values;
}

[[nodiscard]] auto expandMask(const PropertyMask& mask) -> PropertySet {
    PropertySet result(mask.properties.begin(), mask.properties.end());
    for (const auto& prefix : mask.prefixes) {
        for (const auto property : allProperties()) {
            if (property.starts_with(prefix)) {
                result.emplace(property);
            }
        }
    }
    return result;
}

[[nodiscard]] auto clipProperties(const AnimationClip& clip) -> PropertySet {
    PropertySet result;
    for (const auto& track : clip.tracks) {
        result.emplace(propertyName(track.property));
    }
    for (const auto& track : clip.stepTracks) {
        result.emplace(propertyName(track.property));
    }
    return result;
}

[[nodiscard]] auto propertyMask(const AnimationClip& clip) -> PropertyMask {
    const auto properties = clipProperties(clip);
    return PropertyMask{std::vector<std::string>{properties.begin(), properties.end()}, {}};
}

[[nodiscard]] auto intersects(const PropertySet& left, const PropertySet& right) -> bool {
    auto leftItem = left.begin();
    auto rightItem = right.begin();
    while (leftItem != left.end() && rightItem != right.end()) {
        if (*leftItem == *rightItem) {
            return true;
        }
        if (*leftItem < *rightItem) {
            ++leftItem;
        } else {
            ++rightItem;
        }
    }
    return false;
}

[[nodiscard]] auto intersection(const PropertySet& left, const PropertySet& right) -> PropertySet {
    PropertySet result;
    std::ranges::set_intersection(left, right, std::inserter(result, result.end()));
    return result;
}

[[nodiscard]] auto isSubset(const PropertySet& subset, const PropertySet& superset) -> bool {
    return std::ranges::includes(superset, subset);
}

[[nodiscard]] auto isDiscrete(const PropertySet& properties) -> bool {
    return properties.contains("render.visible") || properties.contains("render.material");
}

[[nodiscard]] auto additiveAllowed(const PropertySet& properties) -> bool {
    return std::ranges::all_of(
        properties, [](const std::string& property) { return property.starts_with("transform."); });
}

[[nodiscard]] auto scaleIsPositive(const AnimationClip& clip) -> bool {
    for (const auto& track : clip.tracks) {
        if (track.property != AnimationProperty::TransformScale) {
            continue;
        }
        for (const auto& segment : track.segments) {
            const auto* start = std::get_if<core::Vec3>(&segment.startValue);
            const auto* end = std::get_if<core::Vec3>(&segment.endValue);
            if (start == nullptr || end == nullptr || start->x <= 0.0F || start->y <= 0.0F ||
                start->z <= 0.0F || end->x <= 0.0F || end->y <= 0.0F || end->z <= 0.0F) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] auto identityLess(const AnimationRecordIdentity& left,
                                const AnimationRecordIdentity& right) -> bool {
    return left < right;
}

[[nodiscard]] auto makeGeneratedIdentity(std::string_view objectId, const TemplateBinding& binding,
                                         GeneratedRecordKind kind) -> GeneratedAnimationIdentity {
    return GeneratedAnimationIdentity{std::string{objectId}, binding.bindingId, binding.templateId,
                                      kind};
}

[[nodiscard]] auto generatedRecordKindName(GeneratedRecordKind kind) -> std::string_view {
    switch (kind) {
    case GeneratedRecordKind::Clip:
        return "clip";
    case GeneratedRecordKind::Layer:
        return "layer";
    case GeneratedRecordKind::BlendGroup:
        return "blend_group";
    case GeneratedRecordKind::ClipInstance:
        return "clip_instance";
    }
    return "unknown";
}

[[nodiscard]] auto generatedScope(std::string fieldPath, const GeneratedAnimationIdentity& identity)
    -> ProgramDiagnosticScope {
    return ProgramDiagnosticScope{std::move(fieldPath), identity};
}

void addScopedError(core::Diagnostics& diagnostics, std::string code, std::string message,
                    const ProgramDiagnosticScope* scope, std::string fallbackPath) {
    auto diagnostic =
        core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code), std::move(message),
                         scope != nullptr ? scope->fieldPath : std::move(fallbackPath)};
    if (scope != nullptr && scope->generatedIdentity) {
        const auto& identity = *scope->generatedIdentity;
        diagnostic.withContext("object_id", identity.objectId);
        diagnostic.withContext("binding_id", identity.bindingId);
        diagnostic.withContext("template_id", identity.templateId);
        diagnostic.withContext("record_kind",
                               std::string{generatedRecordKindName(identity.recordKind)});
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto preferGeneratedScope(const ProgramDiagnosticScope* preferred,
                                        const ProgramDiagnosticScope* alternate)
    -> const ProgramDiagnosticScope* {
    if (preferred != nullptr && preferred->generatedIdentity) {
        return preferred;
    }
    if (alternate != nullptr && alternate->generatedIdentity) {
        return alternate;
    }
    return preferred != nullptr ? preferred : alternate;
}

[[nodiscard]] auto
buildProgram(const ChartV4SourceDocument& source,
             const std::map<std::string, AnimatorComponent, std::less<>>& animators,
             const std::map<std::string, ImportedTemplate, std::less<>>& imports,
             const std::map<std::string, ChartParameterValue, std::less<>>& parameters,
             const ChartLimits& limits, core::Diagnostics& diagnostics) -> ProgramBuild {
    ProgramBuild result;
    if (source.animationClips.size() > limits.maxAnimationClips) {
        addResolverError(diagnostics, "chart.animation.generated_limit",
                         "Animation clip count exceeds the configured limit", "$/animationClips");
    }
    result.program.clips.reserve(source.animationClips.size());
    for (const auto& clip : source.animationClips) {
        result.program.clips.push_back(AnimationProgramClip{clip.id, clip});
        result.paths.clips.emplace(AnimationRecordIdentity{clip.id},
                                   ProgramDiagnosticScope{clip.fieldPath, std::nullopt});
    }
    const auto one = RationalBeat::one();

    for (const auto& [objectId, animator] : animators) {
        ObjectAnimationProgram object{ChartObjectId{objectId}, {}};
        for (const auto& layer : animator.layers) {
            const auto layerWeight =
                resolvedWeight(layer.weight, parameters, diagnostics, layer.fieldPath + "/weight");
            if (!layerWeight) {
                continue;
            }
            ResolvedAnimationLayer resolvedLayer{
                layer.layerId, layer.priority, *layerWeight, layer.propertyMask, {}};
            result.paths.layers.emplace(LayerPathKey{objectId, resolvedLayer.identity},
                                        ProgramDiagnosticScope{layer.fieldPath, std::nullopt});
            for (const auto& group : layer.blendGroups) {
                const auto groupWeight = resolvedWeight(group.weight, parameters, diagnostics,
                                                        group.fieldPath + "/weight");
                if (!groupWeight) {
                    continue;
                }
                ResolvedBlendGroup resolvedGroup{group.groupId, group.mode, *groupWeight, {}};
                result.paths.groups.emplace(
                    GroupPathKey{objectId, resolvedLayer.identity, resolvedGroup.identity},
                    ProgramDiagnosticScope{group.fieldPath, std::nullopt});
                for (const auto& instance : group.instances) {
                    const auto instanceWeight = resolvedWeight(
                        instance.weight, parameters, diagnostics, instance.fieldPath + "/weight");
                    if (!instanceWeight) {
                        continue;
                    }
                    const AnimationRecordIdentity instanceIdentity{instance.instanceId};
                    result.paths.instances.emplace(
                        InstancePathKey{objectId, resolvedLayer.identity, resolvedGroup.identity,
                                        instanceIdentity},
                        ProgramDiagnosticScope{instance.fieldPath, std::nullopt});
                    resolvedGroup.instances.push_back(
                        ResolvedClipInstance{instanceIdentity, instance.clipId, instance.startBeat,
                                             one, instance.iterations, instance.fillMode,
                                             *instanceWeight, instance.propertyMask});
                }
                std::ranges::sort(resolvedGroup.instances, [](const auto& left, const auto& right) {
                    return identityLess(left.identity, right.identity);
                });
                resolvedLayer.blendGroups.push_back(std::move(resolvedGroup));
            }
            std::ranges::sort(resolvedLayer.blendGroups, [](const auto& left, const auto& right) {
                return identityLess(left.identity, right.identity);
            });
            object.layers.push_back(std::move(resolvedLayer));
        }

        for (const auto& binding : animator.templateBindings) {
            const auto imported = imports.find(binding.templateId);
            if (imported == imports.end()) {
                addResolverError(diagnostics, "chart.animation.template_reference_missing",
                                 "Template binding refers to an unavailable CXT import",
                                 binding.fieldPath + "/template");
                continue;
            }
            const auto durationScale =
                resolvedRational(binding.durationScale, parameters, diagnostics,
                                 binding.fieldPath + "/durationScale");
            const auto weight = resolvedWeight(binding.weight, parameters, diagnostics,
                                               binding.fieldPath + "/weight");
            if (!durationScale || !weight) {
                continue;
            }
            constexpr std::size_t recordsPerBinding = 4;
            if (recordsPerBinding > limits.maxGeneratedAnimationRecords ||
                result.generatedRecords > limits.maxGeneratedAnimationRecords - recordsPerBinding) {
                addResolverError(diagnostics, "chart.animation.generated_limit",
                                 "Generated animation record count exceeds the configured limit",
                                 binding.fieldPath);
                continue;
            }
            result.generatedRecords += recordsPerBinding;
            const auto clipIdentity =
                makeGeneratedIdentity(objectId, binding, GeneratedRecordKind::Clip);
            const auto layerIdentity =
                makeGeneratedIdentity(objectId, binding, GeneratedRecordKind::Layer);
            const auto groupIdentity =
                makeGeneratedIdentity(objectId, binding, GeneratedRecordKind::BlendGroup);
            const auto instanceIdentity =
                makeGeneratedIdentity(objectId, binding, GeneratedRecordKind::ClipInstance);
            auto clip = imported->second.document.clip;
            clip.id = binding.templateId;
            result.program.clips.push_back(AnimationProgramClip{clipIdentity, std::move(clip)});
            result.paths.clips.emplace(AnimationRecordIdentity{clipIdentity},
                                       generatedScope(binding.fieldPath, clipIdentity));
            result.paths.layers.emplace(
                LayerPathKey{objectId, AnimationRecordIdentity{layerIdentity}},
                generatedScope(binding.fieldPath, layerIdentity));
            result.paths.groups.emplace(GroupPathKey{objectId,
                                                     AnimationRecordIdentity{layerIdentity},
                                                     AnimationRecordIdentity{groupIdentity}},
                                        generatedScope(binding.fieldPath, groupIdentity));
            result.paths.instances.emplace(
                InstancePathKey{objectId, AnimationRecordIdentity{layerIdentity},
                                AnimationRecordIdentity{groupIdentity},
                                AnimationRecordIdentity{instanceIdentity}},
                generatedScope(binding.fieldPath, instanceIdentity));

            const auto mask = propertyMask(imported->second.document.clip);
            ResolvedClipInstance instance{instanceIdentity,
                                          clipIdentity,
                                          binding.startBeat,
                                          *durationScale,
                                          imported->second.document.application.iterations,
                                          imported->second.document.application.fillMode,
                                          1.0,
                                          mask};
            ResolvedBlendGroup group{groupIdentity,
                                     imported->second.document.application.blendMode,
                                     *weight,
                                     {std::move(instance)}};
            object.layers.push_back(ResolvedAnimationLayer{
                layerIdentity, binding.priority, 1.0, mask, {std::move(group)}});
        }
        std::ranges::sort(object.layers, [](const auto& left, const auto& right) {
            if (left.priority != right.priority) {
                return left.priority < right.priority;
            }
            return identityLess(left.identity, right.identity);
        });
        result.program.objects.push_back(std::move(object));
    }
    std::ranges::sort(result.program.clips, [](const auto& left, const auto& right) {
        return identityLess(left.identity, right.identity);
    });
    std::ranges::sort(result.program.objects, {},
                      [](const auto& item) { return item.objectId.value; });
    return result;
}

} // namespace cuexis::chart
