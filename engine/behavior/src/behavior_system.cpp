#include <cuexis/behavior/behavior_system.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
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

[[nodiscard]] auto lerp(const core::Vec3& left, const core::Vec3& right, double t) noexcept
    -> core::Vec3 {
    const auto blend = static_cast<float>(t);
    return core::Vec3{left.x + (right.x - left.x) * blend, left.y + (right.y - left.y) * blend,
                      left.z + (right.z - left.z) * blend};
}

[[nodiscard]] auto slerp(const core::Quat& left, const core::Quat& right, double t)
    -> core::Result<core::Quat> {
    core::Quat target = right;
    double dot = static_cast<double>(left.x) * right.x + static_cast<double>(left.y) * right.y +
                 static_cast<double>(left.z) * right.z + static_cast<double>(left.w) * right.w;
    if (dot < 0.0) {
        dot = -dot;
        target = core::Quat{-right.x, -right.y, -right.z, -right.w};
    }
    dot = std::clamp(dot, -1.0, 1.0);

    core::Quat result;
    if (dot > 0.9995) {
        const auto blend = static_cast<float>(t);
        result =
            core::Quat{left.x + (target.x - left.x) * blend, left.y + (target.y - left.y) * blend,
                       left.z + (target.z - left.z) * blend, left.w + (target.w - left.w) * blend};
    } else {
        const double theta = std::acos(dot);
        const double sinTheta = std::sin(theta);
        const double leftWeight = std::sin((1.0 - t) * theta) / sinTheta;
        const double rightWeight = std::sin(t * theta) / sinTheta;
        result = core::Quat{
            static_cast<float>(left.x * leftWeight + target.x * rightWeight),
            static_cast<float>(left.y * leftWeight + target.y * rightWeight),
            static_cast<float>(left.z * leftWeight + target.z * rightWeight),
            static_cast<float>(left.w * leftWeight + target.w * rightWeight),
        };
    }
    auto normalized = core::normalize(result);
    if (!normalized) {
        return core::unexpected(core::Error{"behavior.sample.quaternion_invalid",
                                            "Quaternion interpolation produced an invalid value"}
                                    .withCause(normalized.error()));
    }
    return *normalized;
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
        const auto value = lerp(*leftVector, *rightVector, t);
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
    auto value = slerp(*leftRotation, *rightRotation, t);
    if (!value) {
        return core::unexpected(std::move(value.error()));
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

} // namespace

auto BehaviorSystem::evaluate(const BehaviorProgram& program, double chartTimeMs,
                              world::PropertyWriteBuffer& writes) -> core::Result<void> {
    if (!std::isfinite(chartTimeMs)) {
        return core::unexpected(
            core::Error{"behavior.frame.time_non_finite", "Behavior chartTimeMs must be finite"});
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
            auto value = sample(track, chartTimeMs);
            if (!value) {
                return core::unexpected(std::move(value.error()));
            }
            auto pushed = writes.push(world::PropertyWrite{
                .entity = binding.entity, .property = track.property, .value = std::move(*value)});
            if (!pushed) {
                return core::unexpected(std::move(pushed.error()));
            }
        }
    }
    return {};
}

} // namespace cuexis::behavior
