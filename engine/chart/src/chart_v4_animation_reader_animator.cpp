#include "chart_v4_animation_reader_internal.hpp"

#include "chart_v4_animation_reader_values_internal.hpp"
#include "chart_v4_common_reader_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::chart::detail {

auto readAnimatorComponent(const json::Reader& reader, const ChartLimits& limits,
                           core::Diagnostics& diagnostics, std::vector<ParameterUse>& parameterUses)
    -> std::optional<AnimatorComponent> {
    constexpr std::array fields{std::string_view{"version"}, std::string_view{"templateBindings"},
                                std::string_view{"layers"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto versionReader = reader.requiredField("version");
    const auto bindingsReader = reader.requiredField("templateBindings");
    const auto layersReader = reader.requiredField("layers");
    auto version = std::optional<std::int64_t>{};
    if (versionReader) {
        version = versionReader->readInt64();
    }
    if (version && *version != 1) {
        addV4Error(diagnostics, "chart.version.unsupported", "Animator version is unsupported",
                   std::string{versionReader->fieldPath()});
    }
    if (!version || *version != 1 || !bindingsReader || !layersReader) {
        return std::nullopt;
    }
    const auto* bindings = bindingsReader->readArray();
    const auto* layers = layersReader->readArray();
    if (bindings == nullptr || layers == nullptr) {
        return std::nullopt;
    }
    if (bindings->size() > limits.maxTemplateBindingsPerAnimator ||
        layers->size() > limits.maxAnimationLayersPerAnimator) {
        addV4Error(diagnostics, "chart.animation.generated_limit",
                   "Animator record count exceeds the configured limit",
                   std::string{reader.fieldPath()});
    }

    std::vector<TemplateBinding> parsedBindings;
    std::set<std::string, std::less<>> bindingIds;
    for (std::size_t index = 0; index < bindings->size(); ++index) {
        const auto item = bindingsReader->element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array bindingFields{
            std::string_view{"bindingId"}, std::string_view{"template"},
            std::string_view{"startBeat"}, std::string_view{"durationScale"},
            std::string_view{"weight"},    std::string_view{"priority"}};
        item->rejectUnknownFields(bindingFields);
        const auto idReader = item->requiredField("bindingId");
        const auto templateReader = item->requiredField("template");
        const auto startReader = item->requiredField("startBeat");
        const auto durationReader = item->requiredField("durationScale");
        const auto weightReader = item->requiredField("weight");
        const auto priorityReader = item->requiredField("priority");
        const auto id = idReader
                            ? readPortableStableId(*idReader, limits, diagnostics, "Binding ID")
                            : std::nullopt;
        std::optional<std::string> templateId;
        if (templateReader && templateReader->readObject() != nullptr) {
            constexpr std::array referenceFields{std::string_view{"domain"},
                                                 std::string_view{"id"}};
            templateReader->rejectUnknownFields(referenceFields);
            const auto domainReader = templateReader->requiredField("domain");
            const auto referenceIdReader = templateReader->requiredField("id");
            const auto domain = domainReader ? domainReader->readString() : std::nullopt;
            templateId = referenceIdReader
                             ? readPortableStableId(*referenceIdReader, limits, diagnostics,
                                                    "Animation template reference")
                             : std::nullopt;
            if (domain && *domain != "animation-template") {
                addV4Error(diagnostics, "chart.animation.template_reference_missing",
                           "Template binding must use the animation-template domain",
                           std::string{domainReader->fieldPath()});
            }
        }
        const auto start = startReader
                               ? readV4Rational(*startReader, limits, diagnostics, false, true)
                               : std::nullopt;
        const auto duration =
            durationReader ? readRationalSource(*durationReader, limits, diagnostics, parameterUses)
                           : std::nullopt;
        const auto weight =
            weightReader ? readWeightSource(*weightReader, limits, diagnostics, parameterUses)
                         : std::nullopt;
        auto priority = std::optional<std::int64_t>{};
        if (priorityReader) {
            priority = priorityReader->readInt64();
        }
        if (!id || !templateId || !start || !duration || !weight || !priority) {
            continue;
        }
        if (!bindingIds.emplace(*id).second) {
            addV4Error(diagnostics, "chart.animation.template_binding_conflict",
                       "Template binding ID is duplicated", std::string{idReader->fieldPath()});
        }
        parsedBindings.push_back(TemplateBinding{*id, *templateId, *start, *duration, *weight,
                                                 *priority, std::string{item->fieldPath()}});
    }

    std::vector<AnimationLayer> parsedLayers;
    std::set<std::string, std::less<>> layerIds;
    for (std::size_t layerIndex = 0; layerIndex < layers->size(); ++layerIndex) {
        const auto layerReader = layersReader->element(layerIndex);
        if (!layerReader || layerReader->readObject() == nullptr) {
            continue;
        }
        constexpr std::array layerFields{
            std::string_view{"layerId"}, std::string_view{"priority"}, std::string_view{"weight"},
            std::string_view{"propertyMask"}, std::string_view{"blendGroups"}};
        layerReader->rejectUnknownFields(layerFields);
        const auto idReader = layerReader->requiredField("layerId");
        const auto priorityReader = layerReader->requiredField("priority");
        const auto weightReader = layerReader->requiredField("weight");
        const auto maskReader = layerReader->requiredField("propertyMask");
        const auto groupsReader = layerReader->requiredField("blendGroups");
        const auto id = idReader ? readPortableStableId(*idReader, limits, diagnostics, "Layer ID")
                                 : std::nullopt;
        auto priority = std::optional<std::int64_t>{};
        if (priorityReader) {
            priority = priorityReader->readInt64();
        }
        const auto weight =
            weightReader ? readWeightSource(*weightReader, limits, diagnostics, parameterUses)
                         : std::nullopt;
        const auto mask =
            maskReader ? readPropertyMask(*maskReader, limits, diagnostics) : std::nullopt;
        if (!id || !priority || !weight || !mask || !groupsReader) {
            continue;
        }
        if (!layerIds.emplace(*id).second) {
            addV4Error(diagnostics, "chart.animation.mask_conflict", "Layer ID is duplicated",
                       std::string{idReader->fieldPath()});
        }
        const auto* groups = groupsReader->readArray();
        if (groups == nullptr) {
            continue;
        }
        if (groups->size() > limits.maxBlendGroupsPerLayer) {
            addV4Error(diagnostics, "chart.animation.generated_limit",
                       "Blend group count exceeds the configured limit",
                       std::string{groupsReader->fieldPath()});
        }
        std::vector<BlendGroup> parsedGroups;
        std::set<std::string, std::less<>> groupIds;
        for (std::size_t groupIndex = 0; groupIndex < groups->size(); ++groupIndex) {
            const auto groupReader = groupsReader->element(groupIndex);
            if (!groupReader || groupReader->readObject() == nullptr) {
                continue;
            }
            constexpr std::array groupFields{std::string_view{"groupId"}, std::string_view{"mode"},
                                             std::string_view{"weight"},
                                             std::string_view{"instances"}};
            groupReader->rejectUnknownFields(groupFields);
            const auto groupIdReader = groupReader->requiredField("groupId");
            const auto modeReader = groupReader->requiredField("mode");
            const auto groupWeightReader = groupReader->requiredField("weight");
            const auto instancesReader = groupReader->requiredField("instances");
            const auto groupId = groupIdReader ? readPortableStableId(*groupIdReader, limits,
                                                                      diagnostics, "Blend group ID")
                                               : std::nullopt;
            auto mode = std::optional<AnimationBlendMode>{};
            if (modeReader) {
                mode = readBlendMode(*modeReader, diagnostics);
            }
            const auto groupWeight =
                groupWeightReader
                    ? readWeightSource(*groupWeightReader, limits, diagnostics, parameterUses)
                    : std::nullopt;
            if (!groupId || !mode || !groupWeight || !instancesReader) {
                continue;
            }
            if (!groupIds.emplace(*groupId).second) {
                addV4Error(diagnostics, "chart.animation.track_conflict",
                           "Blend group ID is duplicated", std::string{groupIdReader->fieldPath()});
            }
            const auto* instances = instancesReader->readArray();
            if (instances == nullptr) {
                continue;
            }
            if (instances->size() > limits.maxClipInstancesPerBlendGroup) {
                addV4Error(diagnostics, "chart.animation.generated_limit",
                           "Clip instance count exceeds the configured limit",
                           std::string{instancesReader->fieldPath()});
            }
            std::vector<ClipInstance> parsedInstances;
            std::set<std::string, std::less<>> instanceIds;
            for (std::size_t instanceIndex = 0; instanceIndex < instances->size();
                 ++instanceIndex) {
                const auto instanceReader = instancesReader->element(instanceIndex);
                if (!instanceReader || instanceReader->readObject() == nullptr) {
                    continue;
                }
                constexpr std::array instanceFields{
                    std::string_view{"instanceId"},  std::string_view{"clip"},
                    std::string_view{"startBeat"},   std::string_view{"iterations"},
                    std::string_view{"fillMode"},    std::string_view{"weight"},
                    std::string_view{"propertyMask"}};
                instanceReader->rejectUnknownFields(instanceFields);
                const auto instanceIdReader = instanceReader->requiredField("instanceId");
                const auto clipReader = instanceReader->requiredField("clip");
                const auto instanceStartReader = instanceReader->requiredField("startBeat");
                const auto iterationsReader = instanceReader->requiredField("iterations");
                const auto fillReader = instanceReader->requiredField("fillMode");
                const auto instanceWeightReader = instanceReader->requiredField("weight");
                const auto instanceMaskReader = instanceReader->requiredField("propertyMask");
                const auto instanceId = instanceIdReader
                                            ? readPortableStableId(*instanceIdReader, limits,
                                                                   diagnostics, "Clip instance ID")
                                            : std::nullopt;
                std::optional<std::string> clipId;
                if (clipReader && clipReader->readObject() != nullptr) {
                    constexpr std::array referenceFields{std::string_view{"domain"},
                                                         std::string_view{"id"}};
                    clipReader->rejectUnknownFields(referenceFields);
                    const auto domainReader = clipReader->requiredField("domain");
                    const auto referenceIdReader = clipReader->requiredField("id");
                    const auto domain = domainReader ? domainReader->readString() : std::nullopt;
                    clipId = referenceIdReader
                                 ? readPortableStableId(*referenceIdReader, limits, diagnostics,
                                                        "Animation reference")
                                 : std::nullopt;
                    if (domain && *domain != "animation") {
                        addV4Error(diagnostics, "chart.animation.reference_missing",
                                   "Clip instance must use the animation domain",
                                   std::string{domainReader->fieldPath()});
                    }
                }
                const auto instanceStart =
                    instanceStartReader
                        ? readV4Rational(*instanceStartReader, limits, diagnostics, false, true)
                        : std::nullopt;
                auto iterations = std::optional<AnimationIterations>{};
                auto fill = std::optional<AnimationFillMode>{};
                if (iterationsReader) {
                    iterations = readIterations(*iterationsReader, diagnostics);
                }
                if (fillReader) {
                    fill = readFillMode(*fillReader, diagnostics);
                }
                const auto instanceWeight = instanceWeightReader
                                                ? readWeightSource(*instanceWeightReader, limits,
                                                                   diagnostics, parameterUses)
                                                : std::nullopt;
                const auto instanceMask =
                    instanceMaskReader ? readPropertyMask(*instanceMaskReader, limits, diagnostics)
                                       : std::nullopt;
                if (!instanceId || !clipId || !instanceStart || !iterations || !fill ||
                    !instanceWeight || !instanceMask) {
                    continue;
                }
                auto instance =
                    ClipInstance{*instanceId,    *clipId,
                                 *instanceStart, *iterations,
                                 *fill,          *instanceWeight,
                                 *instanceMask,  std::string{instanceReader->fieldPath()}};
                if (instance.iterations.infinite && instance.fillMode != AnimationFillMode::None) {
                    addV4Error(diagnostics, "chart.animation.clip_invalid",
                               "Infinite clip instance must use fillMode none",
                               std::string{fillReader->fieldPath()});
                }
                if (!instanceIds.emplace(*instanceId).second) {
                    addV4Error(diagnostics, "chart.animation.track_conflict",
                               "Clip instance ID is duplicated",
                               std::string{instanceIdReader->fieldPath()});
                }
                parsedInstances.push_back(std::move(instance));
            }
            std::ranges::sort(parsedInstances, {}, &ClipInstance::instanceId);
            parsedGroups.push_back(BlendGroup{*groupId, *mode, *groupWeight,
                                              std::move(parsedInstances),
                                              std::string{groupReader->fieldPath()}});
        }
        std::ranges::sort(parsedGroups, {}, &BlendGroup::groupId);
        parsedLayers.push_back(AnimationLayer{*id, *priority, *weight, *mask,
                                              std::move(parsedGroups),
                                              std::string{layerReader->fieldPath()}});
    }
    for (std::size_t left = 0; left < parsedLayers.size(); ++left) {
        for (std::size_t right = left + 1; right < parsedLayers.size(); ++right) {
            if (parsedLayers[left].priority == parsedLayers[right].priority &&
                masksOverlap(parsedLayers[left].propertyMask, parsedLayers[right].propertyMask)) {
                addV4Error(diagnostics, "chart.animation.mask_conflict",
                           "Layers with the same priority have overlapping masks",
                           parsedLayers[right].fieldPath);
            }
        }
    }
    std::ranges::sort(parsedBindings, {}, &TemplateBinding::bindingId);
    std::ranges::sort(parsedLayers, {}, &AnimationLayer::layerId);
    return AnimatorComponent{std::move(parsedBindings), std::move(parsedLayers)};
}

} // namespace cuexis::chart::detail
