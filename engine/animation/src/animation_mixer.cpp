#include <cuexis/animation/animation_mixer.hpp>

#include "animation_math.hpp"

#include <cuexis/animation/animation_diagnostics.hpp>
#include <cuexis/animation/animation_sample.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::animation {
namespace {

struct MixValue final {
    world::PropertyValue value{};
    std::string_view resourceView{};
};

struct MixContribution final {
    const chart::ResolvedClipInstance* instance{};
    MixValue sample{};
    double weight{};
};

struct MixWriteKey final {
    chart::ChartObjectId objectId;
    world::PropertyId property{};

    auto operator<=>(const MixWriteKey&) const = default;
};

[[nodiscard]] auto lerpScalar(double left, double right, double t) noexcept -> double {
    return left + (right - left) * t;
}

[[nodiscard]] auto lerpVec3(const core::Vec3& left, const core::Vec3& right, double t) noexcept
    -> core::Vec3 {
    const auto blend = static_cast<float>(t);
    return core::Vec3{left.x + (right.x - left.x) * blend, left.y + (right.y - left.y) * blend,
                      left.z + (right.z - left.z) * blend};
}

[[nodiscard]] auto quaternionDot(const core::Quat& left, const core::Quat& right) noexcept
    -> double {
    return static_cast<double>(left.x) * right.x + static_cast<double>(left.y) * right.y +
           static_cast<double>(left.z) * right.z + static_cast<double>(left.w) * right.w;
}

[[nodiscard]] auto negateQuat(const core::Quat& value) noexcept -> core::Quat {
    return core::Quat{-value.x, -value.y, -value.z, -value.w};
}

[[nodiscard]] auto alignHemisphere(const core::Quat& reference, const core::Quat& value) noexcept
    -> core::Quat {
    return quaternionDot(reference, value) < 0.0 ? negateQuat(value) : value;
}

[[nodiscard]] auto mixError(std::string_view code, std::string message, world::PropertyId property)
    -> core::Error {
    return core::Error{std::string{code}, std::move(message)}.withContext(
        std::string{contextProperty}, std::string{animationPropertyName(property)});
}

void addMixDiagnostic(core::Diagnostics& diagnostics, std::string_view code, std::string message,
                      const chart::ChartObjectId& objectId, world::PropertyId property) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{code},
                                       std::move(message), std::string{fallbackFieldPath}};
    diagnostic.withContext(std::string{contextObjectId}, objectId.value);
    diagnostic.withContext(std::string{contextProperty},
                           std::string{animationPropertyName(property)});
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto isDiscrete(world::PropertyId property) noexcept -> bool {
    return property == world::PropertyId::RenderVisible ||
           property == world::PropertyId::RenderMaterial;
}

[[nodiscard]] auto isAdditiveSupported(world::PropertyId property) noexcept -> bool {
    return property == world::PropertyId::TransformPositionX ||
           property == world::PropertyId::TransformPositionY ||
           property == world::PropertyId::TransformPositionZ ||
           property == world::PropertyId::TransformRotation ||
           property == world::PropertyId::TransformScale;
}

[[nodiscard]] auto toMixValue(const chart::AnimationValue& value) -> MixValue {
    return MixValue{
        .value = std::visit([](const auto& item) -> world::PropertyValue { return item; }, value)};
}

[[nodiscard]] auto toMixValue(const chart::AnimationStepValue& value) -> MixValue {
    MixValue mix;
    std::visit(
        [&](const auto& item) {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, bool>) {
                mix.value = item;
            } else {
                mix.value = item.value;
                mix.resourceView = item.value;
            }
        },
        value);
    return mix;
}

[[nodiscard]] auto slerp(const core::Quat& left, const core::Quat& right, double t)
    -> core::Result<core::Quat> {
    auto value = shortestPathSlerp(left, right, t);
    if (!value) {
        return core::unexpected(core::Error{std::string{mixQuaternionInvalid},
                                            "Quaternion mix produced an invalid value"}
                                    .withCause(std::move(value.error())));
    }
    return *value;
}

[[nodiscard]] auto quatLog(const core::Quat& value) -> core::Result<core::Vec3> {
    auto aligned = value.w < 0.0F ? negateQuat(value) : value;
    auto normalized = core::normalize(aligned);
    if (!normalized) {
        return core::unexpected(core::Error{std::string{mixQuaternionInvalid},
                                            "Additive quaternion log requires a unit quaternion"}
                                    .withCause(normalized.error()));
    }
    const auto xyz = core::Vec3{normalized->x, normalized->y, normalized->z};
    const auto length =
        std::sqrt(static_cast<double>(xyz.x) * xyz.x + static_cast<double>(xyz.y) * xyz.y +
                  static_cast<double>(xyz.z) * xyz.z);
    if (length <= 1.0e-8) {
        return core::Vec3{};
    }
    const auto halfAngle = std::atan2(length, static_cast<double>(normalized->w));
    const auto scale = static_cast<float>(halfAngle / length);
    return core::Vec3{xyz.x * scale, xyz.y * scale, xyz.z * scale};
}

[[nodiscard]] auto quatExp(const core::Vec3& tangent) -> core::Result<core::Quat> {
    if (!core::isFinite(tangent)) {
        return core::unexpected(
            core::Error{std::string{mixNonFinite}, "Additive quaternion tangent overflowed"});
    }
    const auto length = std::sqrt(static_cast<double>(tangent.x) * tangent.x +
                                  static_cast<double>(tangent.y) * tangent.y +
                                  static_cast<double>(tangent.z) * tangent.z);
    if (length <= 1.0e-8) {
        return core::Quat{tangent.x, tangent.y, tangent.z, 1.0F};
    }
    const auto sine = static_cast<float>(std::sin(length) / length);
    auto result = core::Quat{tangent.x * sine, tangent.y * sine, tangent.z * sine,
                             static_cast<float>(std::cos(length))};
    auto normalized = core::normalize(result);
    if (!normalized) {
        return core::unexpected(core::Error{std::string{mixQuaternionInvalid},
                                            "Additive quaternion exp produced an invalid value"}
                                    .withCause(normalized.error()));
    }
    return *normalized;
}

[[nodiscard]] auto multiplyQuat(const core::Quat& left, const core::Quat& right) noexcept
    -> core::Quat {
    return core::Quat{left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
                      left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
                      left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
                      left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
}

[[nodiscard]] auto lerpMix(const MixValue& left, const MixValue& right, double t,
                           world::PropertyId property) -> core::Result<MixValue> {
    if (const auto* leftScalar = std::get_if<double>(&left.value)) {
        const auto* rightScalar = std::get_if<double>(&right.value);
        if (rightScalar == nullptr) {
            return core::unexpected(
                mixError(mixValueTypeMismatch, "Animation mix value types differ", property));
        }
        const auto value = lerpScalar(*leftScalar, *rightScalar, t);
        if (!std::isfinite(value)) {
            return core::unexpected(mixError(mixNonFinite, "Animation mix overflowed", property));
        }
        return MixValue{.value = value};
    }
    if (const auto* leftVector = std::get_if<core::Vec3>(&left.value)) {
        const auto* rightVector = std::get_if<core::Vec3>(&right.value);
        if (rightVector == nullptr) {
            return core::unexpected(
                mixError(mixValueTypeMismatch, "Animation mix value types differ", property));
        }
        const auto value = lerpVec3(*leftVector, *rightVector, t);
        if (!core::isFinite(value)) {
            return core::unexpected(mixError(mixNonFinite, "Animation mix overflowed", property));
        }
        return MixValue{.value = value};
    }
    if (const auto* leftRotation = std::get_if<core::Quat>(&left.value)) {
        const auto* rightRotation = std::get_if<core::Quat>(&right.value);
        if (rightRotation == nullptr) {
            return core::unexpected(
                mixError(mixValueTypeMismatch, "Animation mix value types differ", property));
        }
        auto value = slerp(*leftRotation, *rightRotation, t);
        if (!value) {
            return core::unexpected(std::move(value.error()));
        }
        return MixValue{.value = *value};
    }
    return core::unexpected(
        mixError(mixValueTypeMismatch, "Animation mix cannot lerp this property", property));
}

[[nodiscard]] auto weightedOverride(std::vector<MixContribution> contributions,
                                    world::PropertyId property) -> core::Result<MixValue> {
    std::ranges::sort(contributions, [](const MixContribution& left, const MixContribution& right) {
        return left.instance->identity < right.instance->identity;
    });
    if (isDiscrete(property)) {
        const auto winner =
            std::min_element(contributions.begin(), contributions.end(),
                             [](const MixContribution& left, const MixContribution& right) {
                                 if (left.weight != right.weight) {
                                     return left.weight > right.weight;
                                 }
                                 return left.instance->identity < right.instance->identity;
                             });
        return winner->sample;
    }

    double totalWeight = 0.0;
    for (const auto& contribution : contributions) {
        totalWeight += contribution.weight;
    }
    if (!(totalWeight > 0.0) || !std::isfinite(totalWeight)) {
        return core::unexpected(
            mixError(mixNonFinite, "Animation override weights are invalid", property));
    }

    if (const auto* firstRotation = std::get_if<core::Quat>(&contributions.front().sample.value)) {
        core::Quat reference = *firstRotation;
        core::Quat sum{};
        for (const auto& contribution : contributions) {
            const auto* rotation = std::get_if<core::Quat>(&contribution.sample.value);
            if (rotation == nullptr) {
                return core::unexpected(
                    mixError(mixValueTypeMismatch, "Animation mix value types differ", property));
            }
            const auto aligned = alignHemisphere(reference, *rotation);
            const auto weight = static_cast<float>(contribution.weight / totalWeight);
            sum.x += aligned.x * weight;
            sum.y += aligned.y * weight;
            sum.z += aligned.z * weight;
            sum.w += aligned.w * weight;
        }
        auto normalized = core::normalize(sum);
        if (!normalized) {
            return core::unexpected(core::Error{std::string{mixQuaternionInvalid},
                                                "Override quaternion mix produced an invalid value"}
                                        .withCause(normalized.error()));
        }
        return MixValue{.value = *normalized};
    }

    MixValue mixed = contributions.front().sample;
    double remaining = contributions.front().weight;
    for (std::size_t index = 1; index < contributions.size(); ++index) {
        remaining += contributions[index].weight;
        auto next = lerpMix(mixed, contributions[index].sample,
                            contributions[index].weight / remaining, property);
        if (!next) {
            return next;
        }
        mixed = *next;
    }
    return mixed;
}

[[nodiscard]] auto applyAdditive(const MixValue& base,
                                 const std::vector<MixContribution>& contributions,
                                 double groupWeight, double layerWeight, world::PropertyId property)
    -> core::Result<MixValue> {
    if (const auto* baseScalar = std::get_if<double>(&base.value)) {
        double value = *baseScalar;
        for (const auto& contribution : contributions) {
            const auto* delta = std::get_if<double>(&contribution.sample.value);
            if (delta == nullptr) {
                return core::unexpected(mixError(
                    mixValueTypeMismatch, "Animation additive value types differ", property));
            }
            value += *delta * contribution.weight * groupWeight * layerWeight;
        }
        if (!std::isfinite(value)) {
            return core::unexpected(
                mixError(mixNonFinite, "Animation additive overflowed", property));
        }
        return MixValue{.value = value};
    }
    if (const auto* baseScale = std::get_if<core::Vec3>(&base.value)) {
        core::Vec3 value = *baseScale;
        for (const auto& contribution : contributions) {
            const auto* factor = std::get_if<core::Vec3>(&contribution.sample.value);
            if (factor == nullptr) {
                return core::unexpected(mixError(
                    mixValueTypeMismatch, "Animation additive value types differ", property));
            }
            if (!(factor->x > 0.0F) || !(factor->y > 0.0F) || !(factor->z > 0.0F) ||
                !core::isFinite(*factor)) {
                return core::unexpected(
                    mixError(mixScaleNonPositive,
                             "Additive scale factors must be finite and positive", property));
            }
            const auto weight = contribution.weight * groupWeight * layerWeight;
            const auto blended = lerpVec3(core::Vec3{1.0F, 1.0F, 1.0F}, *factor, weight);
            if (!(blended.x > 0.0F) || !(blended.y > 0.0F) || !(blended.z > 0.0F) ||
                !core::isFinite(blended)) {
                return core::unexpected(
                    mixError(mixScaleNonPositive,
                             "Additive scale factors must be finite and positive", property));
            }
            value = core::Vec3{value.x * blended.x, value.y * blended.y, value.z * blended.z};
        }
        if (!core::isFinite(value)) {
            return core::unexpected(
                mixError(mixNonFinite, "Animation additive overflowed", property));
        }
        return MixValue{.value = value};
    }
    const auto* baseRotation = std::get_if<core::Quat>(&base.value);
    if (baseRotation == nullptr) {
        return core::unexpected(
            mixError(mixValueTypeMismatch, "Animation additive value types differ", property));
    }
    core::Vec3 tangent{};
    for (const auto& contribution : contributions) {
        const auto* delta = std::get_if<core::Quat>(&contribution.sample.value);
        if (delta == nullptr) {
            return core::unexpected(
                mixError(mixValueTypeMismatch, "Animation additive value types differ", property));
        }
        auto logged = quatLog(*delta);
        if (!logged) {
            return core::unexpected(std::move(logged.error()));
        }
        const auto weight = static_cast<float>(contribution.weight * groupWeight * layerWeight);
        tangent.x += logged->x * weight;
        tangent.y += logged->y * weight;
        tangent.z += logged->z * weight;
    }
    auto delta = quatExp(tangent);
    if (!delta) {
        return core::unexpected(std::move(delta.error()));
    }
    auto normalized = core::normalize(multiplyQuat(*baseRotation, *delta));
    if (!normalized) {
        return core::unexpected(core::Error{std::string{mixQuaternionInvalid},
                                            "Additive quaternion mix produced an invalid value"}
                                    .withCause(normalized.error()));
    }
    return MixValue{.value = *normalized};
}

[[nodiscard]] auto toWriteValue(const MixValue& value) -> world::PropertyWriteValue {
    return std::visit(
        [&](const auto& item) -> world::PropertyWriteValue {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::string>) {
                return value.resourceView.empty() ? std::string_view{item} : value.resourceView;
            } else {
                return item;
            }
        },
        value.value);
}

} // namespace

auto animationPropertyId(chart::AnimationProperty property) noexcept -> world::PropertyId {
    switch (property) {
    case chart::AnimationProperty::TransformPositionX:
        return world::PropertyId::TransformPositionX;
    case chart::AnimationProperty::TransformPositionY:
        return world::PropertyId::TransformPositionY;
    case chart::AnimationProperty::TransformPositionZ:
        return world::PropertyId::TransformPositionZ;
    case chart::AnimationProperty::TransformRotation:
        return world::PropertyId::TransformRotation;
    case chart::AnimationProperty::TransformScale:
        return world::PropertyId::TransformScale;
    case chart::AnimationProperty::MaterialOpacity:
        return world::PropertyId::MaterialOpacity;
    case chart::AnimationProperty::MaterialTint:
        return world::PropertyId::MaterialTint;
    }
    return world::PropertyId::TransformPositionX;
}

auto animationStepPropertyId(chart::AnimationStepProperty property) noexcept -> world::PropertyId {
    switch (property) {
    case chart::AnimationStepProperty::RenderVisible:
        return world::PropertyId::RenderVisible;
    case chart::AnimationStepProperty::RenderMaterial:
        return world::PropertyId::RenderMaterial;
    }
    return world::PropertyId::RenderVisible;
}

auto animationPropertyName(world::PropertyId property) noexcept -> std::string_view {
    switch (property) {
    case world::PropertyId::TransformPositionX:
        return "transform.position.x";
    case world::PropertyId::TransformPositionY:
        return "transform.position.y";
    case world::PropertyId::TransformPositionZ:
        return "transform.position.z";
    case world::PropertyId::TransformRotation:
        return "transform.rotation";
    case world::PropertyId::TransformScale:
        return "transform.scale";
    case world::PropertyId::CameraFovY:
        return "camera.fovY";
    case world::PropertyId::RenderVisible:
        return "render.visible";
    case world::PropertyId::RenderMaterial:
        return "render.material";
    case world::PropertyId::MaterialOpacity:
        return "material.opacity";
    case world::PropertyId::MaterialTint:
        return "material.tint";
    }
    return "unknown";
}

auto maskAllows(const chart::PropertyMask& mask, world::PropertyId property) noexcept -> bool {
    const auto name = animationPropertyName(property);
    for (const auto& item : mask.properties) {
        if (item == name) {
            return true;
        }
    }
    for (const auto& prefix : mask.prefixes) {
        if (name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix) {
            return true;
        }
    }
    return false;
}

AnimationEvaluateResult::AnimationEvaluateResult()
    : diagnostics(maxDiagnostics, core::Diagnostic{core::DiagnosticSeverity::Error,
                                                   std::string{diagnosticLimitExceeded},
                                                   "Animation diagnostic limit was reached",
                                                   std::string{fallbackFieldPath}}) {}

auto AnimationMixer::evaluate(const AnimationProgram& program, chart::RationalBeat chartBeat,
                              std::span<const AnimationObjectBinding> bindings,
                              std::span<const AnimationObjectBaseline> baselines,
                              world::PropertyWriteBuffer& writes, bool captureLayerContributions)
    -> core::Result<AnimationEvaluateResult> {
    AnimationEvaluateResult result;
    writes.clear();

    std::map<chart::ChartObjectId, entt::entity> entities;
    for (const auto& binding : bindings) {
        entities.emplace(binding.objectId, binding.entity);
    }
    std::map<MixWriteKey, MixValue> current;
    for (const auto& object : baselines) {
        for (const auto& property : object.properties) {
            current.insert_or_assign(MixWriteKey{object.objectId, property.property},
                                     MixValue{.value = property.value});
        }
    }
    std::map<MixWriteKey, MixValue> accepted;

    for (const auto& object : program.objects()) {
        const auto entity = entities.find(object.objectId);
        if (entity == entities.end() || entity->second == entt::null) {
            return core::unexpected(
                core::Error{std::string{mixBindingMissing}, "Animation object has no bound entity"}
                    .withContext(std::string{contextObjectId}, object.objectId.value));
        }

        auto layerOrder = std::vector<std::size_t>(object.layers.size());
        for (std::size_t index = 0; index < layerOrder.size(); ++index) {
            layerOrder[index] = index;
        }
        std::ranges::sort(layerOrder, [&](std::size_t left, std::size_t right) {
            const auto& leftLayer = object.layers[left];
            const auto& rightLayer = object.layers[right];
            if (leftLayer.priority != rightLayer.priority) {
                return leftLayer.priority < rightLayer.priority;
            }
            return leftLayer.identity < rightLayer.identity;
        });

        std::size_t layerCursor = 0;
        while (layerCursor < layerOrder.size()) {
            const auto priority = object.layers[layerOrder[layerCursor]].priority;
            std::size_t groupEnd = layerCursor;
            while (groupEnd < layerOrder.size() &&
                   object.layers[layerOrder[groupEnd]].priority == priority) {
                ++groupEnd;
            }

            std::map<world::PropertyId, MixValue> proposed;
            std::map<world::PropertyId, chart::AnimationRecordIdentity> owners;
            std::map<world::PropertyId, bool> conflicts;

            for (std::size_t index = layerCursor; index < groupEnd; ++index) {
                const auto& layer = object.layers[layerOrder[index]];
                if (!(layer.weight > 0.0)) {
                    continue;
                }

                std::map<world::PropertyId, MixValue> layerValues;
                std::map<world::PropertyId, bool> discarded;

                auto collect = [&](const chart::ResolvedBlendGroup& group,
                                   bool additive) -> core::Result<void> {
                    if (!(group.weight > 0.0) ||
                        (group.mode == chart::AnimationBlendMode::Additive) != additive) {
                        return {};
                    }
                    std::map<world::PropertyId, std::vector<MixContribution>> grouped;
                    for (const auto& instance : group.instances) {
                        if (!(instance.weight > 0.0)) {
                            continue;
                        }
                        auto sampled =
                            AnimationSampler::sampleInstance(program, instance, chartBeat);
                        if (!sampled) {
                            return core::unexpected(std::move(sampled.error()));
                        }
                        if (!sampled->has_value()) {
                            continue;
                        }
                        auto consider = [&](world::PropertyId property, MixValue value) {
                            if (!maskAllows(layer.propertyMask, property) ||
                                !maskAllows(instance.propertyMask, property)) {
                                return;
                            }
                            grouped[property].push_back(MixContribution{.instance = &instance,
                                                                        .sample = std::move(value),
                                                                        .weight = instance.weight});
                        };
                        for (const auto& track : (*sampled)->tracks) {
                            consider(animationPropertyId(track.property), toMixValue(track.value));
                        }
                        for (const auto& track : (*sampled)->steps) {
                            consider(animationStepPropertyId(track.property),
                                     toMixValue(track.value));
                        }
                    }

                    for (auto& [property, contributions] : grouped) {
                        if (contributions.empty() || discarded[property]) {
                            continue;
                        }
                        if (layerValues.contains(property)) {
                            discarded[property] = true;
                            layerValues.erase(property);
                            addMixDiagnostic(result.diagnostics, mixGroupOverlap,
                                             "Blend groups in a layer wrote the same property",
                                             object.objectId, property);
                            continue;
                        }
                        if (additive && !isAdditiveSupported(property)) {
                            discarded[property] = true;
                            addMixDiagnostic(result.diagnostics, mixAdditiveUnsupported,
                                             "Additive mixing is not defined for this property",
                                             object.objectId, property);
                            continue;
                        }
                        if (isDiscrete(property) &&
                            (additive || layer.weight != 1.0 || group.weight != 1.0)) {
                            discarded[property] = true;
                            addMixDiagnostic(
                                result.diagnostics, mixDiscreteWeightUnsupported,
                                "Discrete animation writes require override weights of 1",
                                object.objectId, property);
                            continue;
                        }

                        const auto key = MixWriteKey{object.objectId, property};
                        const auto input = layerValues.contains(property)
                                               ? layerValues.find(property)->second
                                           : current.contains(key) ? current.find(key)->second
                                                                   : MixValue{};
                        if (!current.contains(key) && !layerValues.contains(property)) {
                            return core::unexpected(
                                mixError(mixBaselineMissing,
                                         "Animation mix is missing a layer input", property));
                        }

                        MixValue mixed;
                        if (additive) {
                            auto next = applyAdditive(input, contributions, group.weight,
                                                      layer.weight, property);
                            if (!next) {
                                return core::unexpected(std::move(next.error()));
                            }
                            mixed = *next;
                        } else {
                            auto groupValue = weightedOverride(std::move(contributions), property);
                            if (!groupValue) {
                                return core::unexpected(std::move(groupValue.error()));
                            }
                            if (isDiscrete(property)) {
                                mixed = *groupValue;
                            } else {
                                auto groupOutput =
                                    lerpMix(input, *groupValue, group.weight, property);
                                if (!groupOutput) {
                                    return core::unexpected(std::move(groupOutput.error()));
                                }
                                auto layerOutput =
                                    lerpMix(input, *groupOutput, layer.weight, property);
                                if (!layerOutput) {
                                    return core::unexpected(std::move(layerOutput.error()));
                                }
                                mixed = *layerOutput;
                            }
                        }
                        layerValues.insert_or_assign(property, mixed);
                    }
                    return {};
                };

                for (const auto& group : layer.blendGroups) {
                    auto applied = collect(group, false);
                    if (!applied) {
                        return core::unexpected(std::move(applied.error()));
                    }
                }
                for (const auto& group : layer.blendGroups) {
                    auto applied = collect(group, true);
                    if (!applied) {
                        return core::unexpected(std::move(applied.error()));
                    }
                }

                for (const auto& [property, value] : layerValues) {
                    if (discarded[property]) {
                        continue;
                    }
                    if (captureLayerContributions) {
                        auto& contribution = result.layerContributions.emplace_back();
                        contribution.objectId = object.objectId;
                        contribution.layerIdentity = layer.identity;
                        contribution.priority = layer.priority;
                        contribution.weight = layer.weight;
                        contribution.property = property;
                        contribution.propertyMask = layer.propertyMask;
                        contribution.value = value.value;
                    }
                    if (owners.contains(property) && owners[property] != layer.identity) {
                        conflicts[property] = true;
                        proposed.erase(property);
                        addMixDiagnostic(result.diagnostics, mixPriorityOverlap,
                                         "Same-priority layers wrote overlapping properties",
                                         object.objectId, property);
                        continue;
                    }
                    owners.insert_or_assign(property, layer.identity);
                    if (!conflicts[property]) {
                        proposed.insert_or_assign(property, value);
                    }
                }
            }

            for (const auto& [property, value] : proposed) {
                if (conflicts[property]) {
                    continue;
                }
                const auto key = MixWriteKey{object.objectId, property};
                current.insert_or_assign(key, value);
                accepted.insert_or_assign(key, value);
            }
            layerCursor = groupEnd;
        }
    }

    for (const auto& [key, value] : accepted) {
        auto pushed = writes.push(world::PropertyWrite{.entity = entities.at(key.objectId),
                                                       .property = key.property,
                                                       .value = toWriteValue(value)});
        if (!pushed) {
            return core::unexpected(std::move(pushed.error()));
        }
    }
    result.diagnostics.sortDeterministically();
    return result;
}

} // namespace cuexis::animation
