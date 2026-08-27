#pragma once

// Layer mixing for sampled animation clips. Writes go to PropertyWriteBuffer; Runtime/EnTT
// binding is supplied by the caller. HostOverride and PropertyResolver belong to Runtime.

#include <cuexis/animation/animation_program.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/world/property.hpp>

#include <entt/entity/entity.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace cuexis::animation {

struct AnimationObjectBinding final {
    chart::ChartObjectId objectId;
    entt::entity entity{entt::null};
};

struct AnimationPropertyBaseline final {
    world::PropertyId property{};
    world::PropertyValue value{};
};

struct AnimationObjectBaseline final {
    chart::ChartObjectId objectId;
    std::vector<AnimationPropertyBaseline> properties;
};

struct AnimationLayerContribution final {
    chart::ChartObjectId objectId;
    chart::AnimationRecordIdentity layerIdentity;
    std::int64_t priority{};
    double weight{};
    world::PropertyId property{};
    chart::PropertyMask propertyMask;
    world::PropertyValue value{};
};

struct AnimationEvaluateResult final {
    AnimationEvaluateResult();

    core::Diagnostics diagnostics;
    std::vector<AnimationLayerContribution> layerContributions;

    [[nodiscard]] auto hasErrors() const noexcept -> bool {
        return diagnostics.hasErrors();
    }
};

[[nodiscard]] auto animationPropertyId(chart::AnimationProperty property) noexcept
    -> world::PropertyId;
[[nodiscard]] auto animationStepPropertyId(chart::AnimationStepProperty property) noexcept
    -> world::PropertyId;
[[nodiscard]] auto animationPropertyName(world::PropertyId property) noexcept -> std::string_view;
[[nodiscard]] auto maskAllows(const chart::PropertyMask& mask, world::PropertyId property) noexcept
    -> bool;

class AnimationMixer final {
  public:
    AnimationMixer() = delete;

    [[nodiscard]] static auto
    evaluate(const AnimationProgram& program, chart::RationalBeat chartBeat,
             std::span<const AnimationObjectBinding> bindings,
             std::span<const AnimationObjectBaseline> baselines, world::PropertyWriteBuffer& writes,
             bool captureLayerContributions = false) -> core::Result<AnimationEvaluateResult>;
};

} // namespace cuexis::animation
