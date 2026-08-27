#pragma once

// AnimationSystem evaluates a compiled AnimationProgram at an explicit chart Beat.
// S4-D samples clips and mixes Override/Additive layers into PropertyWriteBuffer.

#include <cuexis/animation/animation_mixer.hpp>
#include <cuexis/animation/animation_program.hpp>
#include <cuexis/animation/animation_sample.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/world/property.hpp>

#include <span>
#include <vector>

namespace cuexis::animation {

class AnimationSystem final {
  public:
    AnimationSystem() = delete;

    [[nodiscard]] static auto sample(const AnimationProgram& program, chart::RationalBeat chartBeat)
        -> core::Result<std::vector<AnimationInstanceSample>>;

    [[nodiscard]] static auto
    evaluate(const AnimationProgram& program, chart::RationalBeat chartBeat,
             std::span<const AnimationObjectBinding> bindings,
             std::span<const AnimationObjectBaseline> baselines, world::PropertyWriteBuffer& writes,
             bool captureLayerContributions = false) -> core::Result<AnimationEvaluateResult>;
};

} // namespace cuexis::animation
