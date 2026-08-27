#pragma once

// Absolute local-Beat sampling for a compiled AnimationProgram.
// Runtime supplies an explicit chart Beat; this module does not read Chart, CXT, or TimingMap.

#include <cuexis/animation/animation_program.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/result.hpp>

#include <optional>
#include <vector>

namespace cuexis::animation {

struct AnimationTrackSample final {
    chart::AnimationProperty property{chart::AnimationProperty::TransformPositionX};
    chart::AnimationValue value{};
};

struct AnimationStepSample final {
    chart::AnimationStepProperty property{chart::AnimationStepProperty::RenderVisible};
    chart::AnimationStepValue value{};
};

struct AnimationClipSample final {
    chart::RationalBeat localBeat;
    std::vector<AnimationTrackSample> tracks;
    std::vector<AnimationStepSample> steps;
};

struct AnimationInstanceSample final {
    chart::ChartObjectId objectId;
    chart::AnimationRecordIdentity instanceIdentity;
    std::optional<AnimationClipSample> clip;
};

class AnimationSampler final {
  public:
    AnimationSampler() = delete;

    [[nodiscard]] static auto resolveLocalBeat(const chart::ResolvedClipInstance& instance,
                                               const chart::AnimationClip& clip,
                                               chart::RationalBeat chartBeat)
        -> core::Result<std::optional<chart::RationalBeat>>;

    [[nodiscard]] static auto sampleClip(const chart::AnimationClip& clip,
                                         chart::RationalBeat localBeat)
        -> core::Result<AnimationClipSample>;

    [[nodiscard]] static auto sampleInstance(const AnimationProgram& program,
                                             const chart::ResolvedClipInstance& instance,
                                             chart::RationalBeat chartBeat)
        -> core::Result<std::optional<AnimationClipSample>>;
};

} // namespace cuexis::animation
