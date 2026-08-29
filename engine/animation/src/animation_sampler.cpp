#include <cuexis/animation/animation_sample.hpp>

#include <cuexis/animation/animation_diagnostics.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ranges>
#include <string>
#include <utility>
#include <variant>

namespace cuexis::animation {
namespace {

[[nodiscard]] auto beatOverflow(std::string_view message) -> core::Error {
    return core::Error{std::string{sampleBeatOverflow}, std::string{message}};
}

[[nodiscard]] auto interpolate(const chart::AnimationValue& left,
                               const chart::AnimationValue& right, double t)
    -> core::Result<chart::AnimationValue> {
    if (const auto* leftScalar = std::get_if<double>(&left)) {
        const auto* rightScalar = std::get_if<double>(&right);
        if (rightScalar == nullptr) {
            return core::unexpected(core::Error{std::string{sampleValueTypeMismatch},
                                                "Animation segment value types differ"});
        }
        const double value = *leftScalar + (*rightScalar - *leftScalar) * t;
        if (!std::isfinite(value)) {
            return core::unexpected(core::Error{std::string{sampleNonFinite},
                                                "Animation scalar interpolation overflowed"});
        }
        return chart::AnimationValue{value};
    }
    if (const auto* leftVector = std::get_if<core::Vec3>(&left)) {
        const auto* rightVector = std::get_if<core::Vec3>(&right);
        if (rightVector == nullptr) {
            return core::unexpected(core::Error{std::string{sampleValueTypeMismatch},
                                                "Animation segment value types differ"});
        }
        const auto value = core::lerp(*leftVector, *rightVector, t);
        if (!core::isFinite(value)) {
            return core::unexpected(core::Error{std::string{sampleNonFinite},
                                                "Animation vector interpolation overflowed"});
        }
        return chart::AnimationValue{value};
    }
    const auto* leftRotation = std::get_if<core::Quat>(&left);
    const auto* rightRotation = std::get_if<core::Quat>(&right);
    if (leftRotation == nullptr || rightRotation == nullptr) {
        return core::unexpected(core::Error{std::string{sampleValueTypeMismatch},
                                            "Animation segment value types differ"});
    }
    auto value = core::slerp(*leftRotation, *rightRotation, t);
    if (!value) {
        return core::unexpected(core::Error{std::string{sampleQuaternionInvalid},
                                            "Quaternion interpolation produced an invalid value"}
                                    .withCause(std::move(value.error())));
    }
    return chart::AnimationValue{*value};
}

[[nodiscard]] auto segmentEndBeat(const chart::AnimationSegment& segment)
    -> core::Result<chart::RationalBeat> {
    auto end = chart::addRationalBeats(segment.startBeat, segment.durationBeats);
    if (!end) {
        return core::unexpected(beatOverflow("Animation segment end Beat overflowed")
                                    .withCause(std::move(end.error())));
    }
    return *end;
}

[[nodiscard]] auto sampleSegment(const chart::AnimationSegment& segment,
                                 chart::RationalBeat localBeat)
    -> core::Result<chart::AnimationValue> {
    if (localBeat < segment.startBeat) {
        return core::unexpected(core::Error{std::string{sampleSegmentInvalid},
                                            "Animation local Beat is before the selected segment"});
    }
    if (segment.durationBeats.numerator() == 0) {
        return segment.endValue;
    }
    auto elapsed = chart::subtractRationalBeats(localBeat, segment.startBeat);
    if (!elapsed) {
        return core::unexpected(beatOverflow("Animation segment elapsed Beat overflowed")
                                    .withCause(std::move(elapsed.error())));
    }
    auto end = segmentEndBeat(segment);
    if (!end) {
        return core::unexpected(std::move(end.error()));
    }
    if (localBeat >= *end) {
        return segment.endValue;
    }
    auto normalized = chart::divideRationalBeats(*elapsed, segment.durationBeats);
    if (!normalized) {
        return core::unexpected(beatOverflow("Animation segment progress overflowed")
                                    .withCause(std::move(normalized.error())));
    }
    return interpolate(
        segment.startValue, segment.endValue,
        core::hermiteProgress(normalized->toDouble(), segment.startSlope, segment.endSlope));
}

[[nodiscard]] auto sampleTrack(const chart::AnimationTrack& track, chart::RationalBeat localBeat)
    -> core::Result<std::optional<chart::AnimationValue>> {
    if (track.segments.empty() || localBeat < track.segments.front().startBeat) {
        return std::optional<chart::AnimationValue>{};
    }
    const auto next =
        std::upper_bound(track.segments.begin(), track.segments.end(), localBeat,
                         [](chart::RationalBeat value, const chart::AnimationSegment& segment) {
                             return value < segment.startBeat;
                         });
    const auto& segment = *(next - 1);
    auto end = segmentEndBeat(segment);
    if (!end) {
        return core::unexpected(std::move(end.error()));
    }
    if (localBeat < *end) {
        auto value = sampleSegment(segment, localBeat);
        if (!value) {
            return core::unexpected(std::move(value.error()));
        }
        return std::optional<chart::AnimationValue>{*value};
    }
    return std::optional<chart::AnimationValue>{segment.endValue};
}

[[nodiscard]] auto sampleStepTrack(const chart::AnimationStepTrack& track,
                                   chart::RationalBeat localBeat)
    -> std::optional<chart::AnimationStepValue> {
    if (track.steps.empty() || localBeat < track.steps.front().beat) {
        return {};
    }
    const auto next =
        std::upper_bound(track.steps.begin(), track.steps.end(), localBeat,
                         [](chart::RationalBeat value, const chart::AnimationStep& step) {
                             return value < step.beat;
                         });
    return (next - 1)->value;
}

} // namespace

auto AnimationSampler::resolveLocalBeat(const chart::ResolvedClipInstance& instance,
                                        const chart::AnimationClip& clip,
                                        chart::RationalBeat chartBeat)
    -> core::Result<std::optional<chart::RationalBeat>> {
    if (clip.durationBeats.numerator() <= 0) {
        return core::unexpected(
            core::Error{std::string{sampleDurationInvalid},
                        "Animation clip durationBeats must be strictly positive"});
    }
    if (instance.durationScale.numerator() <= 0) {
        return core::unexpected(
            core::Error{std::string{sampleScaleInvalid},
                        "Animation instance durationScale must be strictly positive"});
    }

    auto elapsed = chart::subtractRationalBeats(chartBeat, instance.startBeat);
    if (!elapsed) {
        return core::unexpected(beatOverflow("Animation instance elapsed Beat overflowed")
                                    .withCause(std::move(elapsed.error())));
    }
    const auto zero = chart::RationalBeat::zero();
    if (*elapsed < zero) {
        return std::optional<chart::RationalBeat>{};
    }

    auto localElapsed = chart::divideRationalBeats(*elapsed, instance.durationScale);
    if (!localElapsed) {
        return core::unexpected(beatOverflow("Animation local Beat overflowed")
                                    .withCause(std::move(localElapsed.error())));
    }

    auto quotient = chart::divideRationalBeats(*localElapsed, clip.durationBeats);
    if (!quotient) {
        return core::unexpected(beatOverflow("Animation iteration index overflowed")
                                    .withCause(std::move(quotient.error())));
    }
    auto iterationIndex = chart::floorRationalBeats(*quotient);
    if (!iterationIndex) {
        return core::unexpected(std::move(iterationIndex.error()));
    }
    auto countedIterations = chart::RationalBeat::create(*iterationIndex, 1);
    if (!countedIterations) {
        return core::unexpected(beatOverflow("Animation iteration count overflowed")
                                    .withCause(std::move(countedIterations.error())));
    }
    auto iterationStart = chart::multiplyRationalBeats(clip.durationBeats, *countedIterations);
    if (!iterationStart) {
        return core::unexpected(beatOverflow("Animation iteration start overflowed")
                                    .withCause(std::move(iterationStart.error())));
    }
    auto remainder = chart::subtractRationalBeats(*localElapsed, *iterationStart);
    if (!remainder) {
        return core::unexpected(beatOverflow("Animation local Beat remainder overflowed")
                                    .withCause(std::move(remainder.error())));
    }

    const bool atIterationBoundary = remainder->numerator() == 0 && *iterationIndex > 0;
    if (!instance.iterations.infinite &&
        (*iterationIndex > static_cast<std::int64_t>(instance.iterations.count) ||
         (*iterationIndex == static_cast<std::int64_t>(instance.iterations.count) &&
          (atIterationBoundary || remainder->numerator() != 0)))) {
        if (instance.fillMode == chart::AnimationFillMode::Hold) {
            return clip.durationBeats;
        }
        return std::optional<chart::RationalBeat>{};
    }

    if (atIterationBoundary) {
        return zero;
    }
    return *remainder;
}

auto AnimationSampler::sampleClip(const chart::AnimationClip& clip, chart::RationalBeat localBeat)
    -> core::Result<AnimationClipSample> {
    if (localBeat.numerator() < 0) {
        return core::unexpected(core::Error{std::string{sampleDurationInvalid},
                                            "Animation local Beat must not be negative"});
    }
    if (localBeat > clip.durationBeats) {
        return core::unexpected(core::Error{std::string{sampleDurationInvalid},
                                            "Animation local Beat is outside the clip duration"});
    }

    AnimationClipSample sample{.localBeat = localBeat, .tracks = {}, .steps = {}};
    sample.tracks.reserve(clip.tracks.size());
    sample.steps.reserve(clip.stepTracks.size());

    for (const auto& track : clip.tracks) {
        auto value = sampleTrack(track, localBeat);
        if (!value) {
            return core::unexpected(std::move(value.error()));
        }
        if (!value->has_value()) {
            continue;
        }
        sample.tracks.push_back(AnimationTrackSample{track.property, **value});
    }
    for (const auto& track : clip.stepTracks) {
        auto value = sampleStepTrack(track, localBeat);
        if (!value.has_value()) {
            continue;
        }
        sample.steps.push_back(AnimationStepSample{track.property, *value});
    }
    return sample;
}

auto AnimationSampler::sampleInstance(const AnimationProgram& program,
                                      const chart::ResolvedClipInstance& instance,
                                      chart::RationalBeat chartBeat)
    -> core::Result<std::optional<AnimationClipSample>> {
    const auto* clip = program.findClip(instance.clipIdentity);
    if (clip == nullptr) {
        if (const auto* clipId = std::get_if<std::string>(&instance.clipIdentity)) {
            const bool looksLikeEmbeddedId = std::ranges::any_of(
                program.clips(), [&](const chart::AnimationProgramClip& candidate) {
                    return candidate.clip.id == *clipId;
                });
            if (looksLikeEmbeddedId) {
                return core::unexpected(core::Error{
                    std::string{sampleClipIdLookupForbidden},
                    "Animation clip lookup must use AnimationRecordIdentity, not clip.id"}
                                            .withContext(std::string{contextClipId}, *clipId));
            }
        }
        return core::unexpected(core::Error{std::string{sampleClipMissing},
                                            "Clip instance refers to an unknown clip identity"});
    }

    auto localBeat = resolveLocalBeat(instance, clip->clip, chartBeat);
    if (!localBeat) {
        return core::unexpected(std::move(localBeat.error()));
    }
    if (!localBeat->has_value()) {
        return std::optional<AnimationClipSample>{};
    }
    auto sampled = sampleClip(clip->clip, **localBeat);
    if (!sampled) {
        return core::unexpected(std::move(sampled.error()));
    }
    return *sampled;
}

} // namespace cuexis::animation
