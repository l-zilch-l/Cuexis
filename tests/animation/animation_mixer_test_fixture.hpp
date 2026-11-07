#pragma once

// Shared clip/write fixtures for the animation mixer case units.

#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/animation/animation_diagnostics.hpp>
#include <cuexis/animation/animation_mixer.hpp>
#include <cuexis/animation/animation_system.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/world/property.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/entity.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace animation_mixer_fixture {

[[nodiscard]] inline auto beat(std::int64_t numerator, std::int64_t denominator = 1)
    -> cuexis::chart::RationalBeat {
    auto value = cuexis::chart::RationalBeat::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] inline auto entity(std::uint32_t value) -> entt::entity {
    return static_cast<entt::entity>(value);
}

[[nodiscard]] inline auto constantScalarClip(cuexis::chart::AnimationProperty property,
                                             double value) -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .tracks = {{
            .property = property,
            .segments = {{
                .startBeat = beat(0),
                .durationBeats = beat(4),
                .startValue = value,
                .endValue = value,
                .fieldPath = "$/tracks/0/segments/0",
            }},
            .fieldPath = "$/tracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] inline auto constantVec3Clip(cuexis::chart::AnimationProperty property,
                                           cuexis::core::Vec3 value)
    -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .tracks = {{
            .property = property,
            .segments = {{
                .startBeat = beat(0),
                .durationBeats = beat(4),
                .startValue = value,
                .endValue = value,
                .fieldPath = "$/tracks/0/segments/0",
            }},
            .fieldPath = "$/tracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] inline auto constantQuatClip(cuexis::core::Quat value)
    -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .tracks = {{
            .property = cuexis::chart::AnimationProperty::TransformRotation,
            .segments = {{
                .startBeat = beat(0),
                .durationBeats = beat(4),
                .startValue = value,
                .endValue = value,
                .fieldPath = "$/tracks/0/segments/0",
            }},
            .fieldPath = "$/tracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] inline auto constantVisibleClip(bool visible) -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .stepTracks = {{
            .property = cuexis::chart::AnimationStepProperty::RenderVisible,
            .steps = {{.beat = beat(0), .value = visible, .fieldPath = "$/stepTracks/0/steps/0"}},
            .fieldPath = "$/stepTracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] inline auto makeProgramClip(cuexis::chart::AnimationRecordIdentity identity,
                                          std::string clipId, cuexis::chart::AnimationClip clip)
    -> cuexis::chart::AnimationProgramClip {
    clip.id = std::move(clipId);
    return {.identity = std::move(identity), .clip = std::move(clip)};
}

[[nodiscard]] inline auto makeInstance(cuexis::chart::AnimationRecordIdentity identity,
                                       cuexis::chart::AnimationRecordIdentity clipIdentity,
                                       double weight, cuexis::chart::PropertyMask mask)
    -> cuexis::chart::ResolvedClipInstance {
    return {
        .identity = std::move(identity),
        .clipIdentity = std::move(clipIdentity),
        .startBeat = beat(0),
        .durationScale = beat(1),
        .iterations = {.infinite = false, .count = 1},
        .fillMode = cuexis::chart::AnimationFillMode::Hold,
        .weight = weight,
        .propertyMask = std::move(mask),
    };
}

[[nodiscard]] inline auto compile(cuexis::chart::AnimationProgramInput input)
    -> cuexis::animation::AnimationProgram {
    auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    REQUIRE(compiled.hasValue());
    REQUIRE(compiled.program.has_value());
    return std::move(*compiled.program);
}

[[nodiscard]] inline auto hasCode(const cuexis::core::Diagnostics& diagnostics,
                                  std::string_view code) -> bool {
    return std::ranges::any_of(diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

[[nodiscard]] inline auto findWrite(const cuexis::world::PropertyWriteBuffer& writes,
                                    cuexis::world::PropertyId property)
    -> std::optional<cuexis::world::PropertyWrite> {
    for (const auto& write : writes.writes()) {
        if (write.property == property) {
            return write;
        }
    }
    return {};
}

[[nodiscard]] inline auto scalarWrite(const cuexis::world::PropertyWriteBuffer& writes,
                                      cuexis::world::PropertyId property) -> double {
    const auto write = findWrite(writes, property);
    REQUIRE(write.has_value());
    const auto* value = std::get_if<double>(&write->value);
    REQUIRE(value != nullptr);
    return *value;
}

[[nodiscard]] inline auto vec3Write(const cuexis::world::PropertyWriteBuffer& writes,
                                    cuexis::world::PropertyId property) -> cuexis::core::Vec3 {
    const auto write = findWrite(writes, property);
    REQUIRE(write.has_value());
    const auto* value = std::get_if<cuexis::core::Vec3>(&write->value);
    REQUIRE(value != nullptr);
    return *value;
}

[[nodiscard]] inline auto quatWrite(const cuexis::world::PropertyWriteBuffer& writes)
    -> cuexis::core::Quat {
    const auto write = findWrite(writes, cuexis::world::PropertyId::TransformRotation);
    REQUIRE(write.has_value());
    const auto* value = std::get_if<cuexis::core::Quat>(&write->value);
    REQUIRE(value != nullptr);
    return *value;
}

[[nodiscard]] inline auto boolWrite(const cuexis::world::PropertyWriteBuffer& writes) -> bool {
    const auto write = findWrite(writes, cuexis::world::PropertyId::RenderVisible);
    REQUIRE(write.has_value());
    const auto* value = std::get_if<bool>(&write->value);
    REQUIRE(value != nullptr);
    return *value;
}

[[nodiscard]] inline auto objectId() -> cuexis::chart::ChartObjectId {
    return cuexis::chart::ChartObjectId{"note"};
}

[[nodiscard]] inline auto mask(std::vector<std::string> properties,
                               std::vector<std::string> prefixes = {})
    -> cuexis::chart::PropertyMask {
    return {.properties = std::move(properties), .prefixes = std::move(prefixes)};
}

[[nodiscard]] inline auto binding() -> cuexis::animation::AnimationObjectBinding {
    return {.objectId = objectId(), .entity = entity(1)};
}

[[nodiscard]] inline auto baseline(cuexis::world::PropertyId property,
                                   cuexis::world::PropertyValue value)
    -> cuexis::animation::AnimationObjectBaseline {
    return {.objectId = objectId(),
            .properties = {{.property = property, .value = std::move(value)}}};
}

[[nodiscard]] inline auto
evaluate(const cuexis::animation::AnimationProgram& program,
         std::span<const cuexis::animation::AnimationObjectBinding> bindings,
         std::span<const cuexis::animation::AnimationObjectBaseline> baselines,
         cuexis::world::PropertyWriteBuffer& writes)
    -> cuexis::core::Result<cuexis::animation::AnimationEvaluateResult> {
    return cuexis::animation::AnimationSystem::evaluate(program, beat(0), bindings, baselines,
                                                        writes);
}

} // namespace animation_mixer_fixture

using namespace animation_mixer_fixture;
