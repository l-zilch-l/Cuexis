#include <cuexis/playback/runtime_timeline.hpp>

#include <cuexis/core/error.hpp>

#include <cmath>

namespace cuexis::playback {

RuntimeTimeline::RuntimeTimeline(double timingOffsetMs) noexcept
    : timingOffsetMs_(timingOffsetMs) {}

auto RuntimeTimeline::create(double timingOffsetMs) -> core::Result<RuntimeTimeline> {
    if (!std::isfinite(timingOffsetMs)) {
        return core::unexpected(
            core::Error{"playback.timeline.offset_invalid", "Timing offset must be finite"});
    }
    return RuntimeTimeline{timingOffsetMs};
}

auto RuntimeTimeline::advance(const audio::SourceClockSample& sample)
    -> core::Result<RuntimeFrame> {
    if (auto valid = audio::validateSourceClockSample(sample); !valid) {
        return core::unexpected(std::move(valid.error()));
    }
    if (sample.state == audio::PlaybackState::Error) {
        return core::unexpected(
            core::Error{"playback.timeline.source_error", "Source clock is in Error state"});
    }

    const bool sourceDiscontinuity =
        initialized_ && sample.discontinuityId != previous_.discontinuityId;
    const bool discontinuity = pendingDiscontinuity_ || sourceDiscontinuity;
    if (discontinuity) {
        ++discontinuityId_;
        pendingDiscontinuity_ = false;
    }
    if (initialized_ && !discontinuity && sample.positionMs < previous_.positionMs) {
        return core::unexpected(core::Error{
            "playback.timeline.segment_regressed",
            "Playing source position must not regress within one discontinuity segment"});
    }

    const double chartTimeMs = sample.positionMs - timingOffsetMs_;
    if (!std::isfinite(chartTimeMs)) {
        return core::unexpected(core::Error{"playback.timeline.chart_time_invalid",
                                            "Timeline chart time is not finite"});
    }
    double deltaMs = 0.0;
    if (initialized_ && !discontinuity && sample.state == audio::PlaybackState::Playing &&
        previous_.state == audio::PlaybackState::Playing) {
        deltaMs = sample.positionMs - previous_.positionMs;
    }
    previous_ = sample;
    initialized_ = true;
    return RuntimeFrame{chartTimeMs, deltaMs, discontinuityId_};
}

auto RuntimeTimeline::reset(double timingOffsetMs) -> core::Result<void> {
    if (!std::isfinite(timingOffsetMs)) {
        return core::unexpected(
            core::Error{"playback.timeline.offset_invalid", "Timing offset must be finite"});
    }
    timingOffsetMs_ = timingOffsetMs;
    pendingDiscontinuity_ = initialized_;
    return {};
}

double RuntimeTimeline::timingOffsetMs() const noexcept {
    return timingOffsetMs_;
}

std::uint64_t RuntimeTimeline::discontinuityId() const noexcept {
    return discontinuityId_;
}

ChartClock::ChartClock(double timingOffsetMs) noexcept : timingOffsetMs_(timingOffsetMs) {}

auto ChartClock::sample(double chartTimeMs, audio::PlaybackState state)
    -> core::Result<audio::SourceClockSample> {
    const double sourceTimeMs = chartTimeMs + timingOffsetMs_;
    const audio::SourceClockSample result{sourceTimeMs, state, discontinuityId_};
    if (auto valid = audio::validateSourceClockSample(result); !valid) {
        return core::unexpected(std::move(valid.error()));
    }
    return result;
}

void ChartClock::markDiscontinuity() noexcept {
    ++discontinuityId_;
}

} // namespace cuexis::playback
