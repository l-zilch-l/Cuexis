#include <cuexis/audio/audio_transport.hpp>

#include <cuexis/core/error.hpp>

#include <cmath>

namespace cuexis::audio {

IAudioClock::~IAudioClock() = default;
IAudioTransport::~IAudioTransport() = default;

auto validateSourceClockSample(const SourceClockSample& sample) -> core::Result<void> {
    if (!std::isfinite(sample.positionMs) || sample.positionMs < 0.0) {
        return core::unexpected(
            core::Error{"audio.clock.position_invalid",
                        "Source clock position must be finite and non-negative"});
    }
    switch (sample.state) {
    case PlaybackState::Stopped:
        if (sample.positionMs != 0.0) {
            return core::unexpected(core::Error{"audio.clock.stopped_position_invalid",
                                                "Stopped source clock position must be zero"});
        }
        break;
    case PlaybackState::Playing:
    case PlaybackState::Paused:
    case PlaybackState::Ended:
    case PlaybackState::Error:
        break;
    case PlaybackState::Empty:
        return core::unexpected(core::Error{"audio.clock.state_invalid",
                                            "A source clock sample cannot use Empty state"});
    default:
        return core::unexpected(
            core::Error{"audio.clock.state_invalid", "Source clock sample has an invalid state"});
    }
    return {};
}

auto HostClock::submit(const SourceClockSample& sample) -> core::Result<void> {
    if (auto valid = validateSourceClockSample(sample); !valid) {
        return valid;
    }
    if (initialized_ && sample.discontinuityId == sample_.discontinuityId &&
        sample.positionMs < sample_.positionMs) {
        return core::unexpected(core::Error{
            "audio.clock.segment_regressed",
            "Playing source position must not regress within one discontinuity segment"});
    }
    sample_ = sample;
    initialized_ = true;
    return {};
}

SourceClockSample HostClock::snapshot() const noexcept {
    return sample_;
}

} // namespace cuexis::audio
