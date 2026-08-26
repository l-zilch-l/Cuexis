#include <cuexis/animation/animation_system.hpp>

#include <cuexis/animation/animation_mixer.hpp>
#include <cuexis/animation/animation_sample.hpp>

namespace cuexis::animation {

auto AnimationSystem::sample(const AnimationProgram& program, chart::RationalBeat chartBeat)
    -> core::Result<std::vector<AnimationInstanceSample>> {
    std::vector<AnimationInstanceSample> samples;
    for (const auto& object : program.objects()) {
        for (const auto& layer : object.layers) {
            for (const auto& group : layer.blendGroups) {
                for (const auto& instance : group.instances) {
                    auto clip = AnimationSampler::sampleInstance(program, instance, chartBeat);
                    if (!clip) {
                        return core::unexpected(std::move(clip.error()));
                    }
                    samples.push_back(AnimationInstanceSample{
                        .objectId = object.objectId,
                        .instanceIdentity = instance.identity,
                        .clip = std::move(*clip),
                    });
                }
            }
        }
    }
    return samples;
}

auto AnimationSystem::evaluate(const AnimationProgram& program, chart::RationalBeat chartBeat,
                               std::span<const AnimationObjectBinding> bindings,
                               std::span<const AnimationObjectBaseline> baselines,
                               world::PropertyWriteBuffer& writes, bool captureLayerContributions)
    -> core::Result<AnimationEvaluateResult> {
    return AnimationMixer::evaluate(program, chartBeat, bindings, baselines, writes,
                                    captureLayerContributions);
}

} // namespace cuexis::animation
