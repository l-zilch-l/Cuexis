#include <cuexis/behavior/behavior_system.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cuexis::behavior {
namespace {

[[nodiscard]] auto applyEasing(Easing easing, double value) noexcept -> double {
    const double t = std::clamp(value, 0.0, 1.0);
    switch (easing) {
    case Easing::Linear:
        return t;
    case Easing::InCubic:
        return t * t * t;
    case Easing::OutCubic: {
        const double inverse = 1.0 - t;
        return 1.0 - inverse * inverse * inverse;
    }
    case Easing::InOutCubic:
        if (t < 0.5) {
            return 4.0 * t * t * t;
        }
        return 1.0 - std::pow(-2.0 * t + 2.0, 3.0) * 0.5;
    }
    return t;
}

[[nodiscard]] auto interpolate(const world::PropertyValue& left, const world::PropertyValue& right,
                               double t) -> core::Result<world::PropertyValue> {
    if (const auto* leftScalar = std::get_if<double>(&left)) {
        const auto* rightScalar = std::get_if<double>(&right);
        if (rightScalar == nullptr) {
            return core::unexpected(core::Error{"behavior.sample.value_type_mismatch",
                                                "Behavior Track key value types differ"});
        }
        const double value = *leftScalar + (*rightScalar - *leftScalar) * t;
        if (!std::isfinite(value)) {
            return core::unexpected(core::Error{"behavior.sample.non_finite",
                                                "Behavior scalar interpolation overflowed"});
        }
        return world::PropertyValue{value};
    }
    if (const auto* leftVector = std::get_if<core::Vec3>(&left)) {
        const auto* rightVector = std::get_if<core::Vec3>(&right);
        if (rightVector == nullptr) {
            return core::unexpected(core::Error{"behavior.sample.value_type_mismatch",
                                                "Behavior Track key value types differ"});
        }
        const auto value = core::lerp(*leftVector, *rightVector, t);
        if (!core::isFinite(value)) {
            return core::unexpected(core::Error{"behavior.sample.non_finite",
                                                "Behavior vector interpolation overflowed"});
        }
        return world::PropertyValue{value};
    }
    const auto* leftRotation = std::get_if<core::Quat>(&left);
    const auto* rightRotation = std::get_if<core::Quat>(&right);
    if (leftRotation == nullptr || rightRotation == nullptr) {
        return core::unexpected(core::Error{"behavior.sample.value_type_mismatch",
                                            "Behavior Track key value types differ"});
    }
    auto value = core::slerp(*leftRotation, *rightRotation, t);
    if (!value) {
        return core::unexpected(core::Error{"behavior.sample.quaternion_invalid",
                                            "Quaternion interpolation produced an invalid value"}
                                    .withCause(std::move(value.error())));
    }
    return world::PropertyValue{*value};
}

[[nodiscard]] auto sample(const BehaviorTrack& track, double chartTimeMs)
    -> core::Result<world::PropertyValue> {
    if (track.keys.empty()) {
        return core::unexpected(
            core::Error{"behavior.sample.empty_track", "Behavior Track contains no keys"});
    }
    if (chartTimeMs <= track.keys.front().chartTimeMs || track.keys.size() == 1) {
        return track.keys.front().value;
    }
    if (chartTimeMs >= track.keys.back().chartTimeMs) {
        return track.keys.back().value;
    }

    const auto right = std::lower_bound(
        track.keys.begin() + 1, track.keys.end(), chartTimeMs,
        [](const BehaviorKey& key, double time) { return key.chartTimeMs < time; });
    if (right == track.keys.end()) {
        return track.keys.back().value;
    }
    if (right->chartTimeMs == chartTimeMs) {
        return right->value;
    }
    const auto& left = *(right - 1);
    const double duration = right->chartTimeMs - left.chartTimeMs;
    if (!std::isfinite(duration) || duration <= 0.0) {
        return core::unexpected(core::Error{"behavior.sample.unsorted_keys",
                                            "Behavior Track keys must be strictly sorted"});
    }
    const double normalized = (chartTimeMs - left.chartTimeMs) / duration;
    return interpolate(left.value, right->value, applyEasing(right->easing, normalized));
}

[[nodiscard]] auto baselineFor(const BehaviorBinding& binding, world::PropertyId property)
    -> core::Result<const world::PropertyValue*> {
    const auto baseline = std::find_if(
        binding.baselines.begin(), binding.baselines.end(),
        [property](const PropertyBaseline& candidate) { return candidate.property == property; });
    if (baseline == binding.baselines.end()) {
        return core::unexpected(core::Error{"behavior.program.baseline_missing",
                                            "Behavior Event property has no captured baseline"});
    }
    return &baseline->value;
}

[[nodiscard]] auto writeValue(const world::PropertyValue& value) noexcept
    -> world::PropertyWriteValue {
    return std::visit(
        [](const auto& item) -> world::PropertyWriteValue {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::string>) {
                return std::string_view{item};
            } else {
                return item;
            }
        },
        value);
}

[[nodiscard]] auto sampleEvent(const BehaviorEventTrack& track, double beat,
                               const world::PropertyValue& baseline)
    -> core::Result<world::PropertyValue> {
    if (track.events.empty() || beat < track.events.front().startBeat) {
        return baseline;
    }
    const auto next = std::upper_bound(
        track.events.begin(), track.events.end(), beat,
        [](double value, const BehaviorEvent& event) { return value < event.startBeat; });
    const auto& event = *(next - 1);
    if (event.instantaneous || beat >= event.endBeat) {
        return event.endValue;
    }
    const double duration = event.endBeat - event.startBeat;
    if (!std::isfinite(duration) || duration <= 0.0) {
        return core::unexpected(core::Error{"behavior.sample.event_duration_invalid",
                                            "Behavior Event duration must be positive"});
    }
    const double normalized = (beat - event.startBeat) / duration;
    return interpolate(event.startValue, event.endValue,
                       core::hermiteProgress(normalized, event.startSlope, event.endSlope));
}

[[nodiscard]] auto sampleStep(const BehaviorStepTrack& track, double beat,
                              const world::PropertyValue& baseline) -> const world::PropertyValue& {
    if (track.events.empty() || beat < track.events.front().beat) {
        return baseline;
    }
    const auto next = std::upper_bound(
        track.events.begin(), track.events.end(), beat,
        [](double value, const BehaviorStepEvent& event) { return value < event.beat; });
    return (next - 1)->value;
}

} // namespace

auto BehaviorSystem::evaluate(const BehaviorProgram& program, double chartTimeMs,
                              world::PropertyWriteBuffer& writes) -> core::Result<void> {
    return evaluate(program, BehaviorSample{.chartTimeMs = chartTimeMs}, writes);
}

auto BehaviorSystem::evaluate(const BehaviorProgram& program, const BehaviorSample& sampleValue,
                              world::PropertyWriteBuffer& writes) -> core::Result<void> {
    if (!std::isfinite(sampleValue.chartTimeMs)) {
        return core::unexpected(
            core::Error{"behavior.frame.time_non_finite", "Behavior chartTimeMs must be finite"});
    }
    if (!std::isfinite(sampleValue.beat) || !std::isfinite(sampleValue.stopProgress) ||
        sampleValue.stopProgress < 0.0 || sampleValue.stopProgress >= 1.0) {
        return core::unexpected(
            core::Error{"behavior.frame.beat_invalid", "Behavior Beat sample is invalid"});
    }
    writes.clear();
    for (const auto& binding : program.bindings) {
        if (binding.entity == entt::null || !binding.behavior.valid() ||
            binding.behavior.value >= program.definitions.size()) {
            return core::unexpected(
                core::Error{"behavior.program.binding_invalid",
                            "Behavior binding references invalid program data"});
        }
        const auto& definition = program.definitions[binding.behavior.value];
        for (const auto& track : definition.tracks) {
            auto value = sample(track, sampleValue.chartTimeMs);
            if (!value) {
                return core::unexpected(std::move(value.error()));
            }
            auto pushed = writes.push(world::PropertyWrite{
                .entity = binding.entity, .property = track.property, .value = writeValue(*value)});
            if (!pushed) {
                return core::unexpected(std::move(pushed.error()));
            }
        }
        for (const auto& track : definition.eventTracks) {
            auto baseline = baselineFor(binding, track.property);
            if (!baseline) {
                return core::unexpected(std::move(baseline.error()));
            }
            auto value = sampleEvent(track, sampleValue.beat, **baseline);
            if (!value) {
                return core::unexpected(std::move(value.error()));
            }
            auto pushed = writes.push(world::PropertyWrite{
                .entity = binding.entity, .property = track.property, .value = writeValue(*value)});
            if (!pushed) {
                return core::unexpected(std::move(pushed.error()));
            }
        }
        for (const auto& track : definition.stepTracks) {
            auto baseline = baselineFor(binding, track.property);
            if (!baseline) {
                return core::unexpected(std::move(baseline.error()));
            }
            auto pushed = writes.push(world::PropertyWrite{
                .entity = binding.entity,
                .property = track.property,
                .value = writeValue(sampleStep(track, sampleValue.beat, **baseline)),
            });
            if (!pushed) {
                return core::unexpected(std::move(pushed.error()));
            }
        }
    }
    return {};
}

} // namespace cuexis::behavior
