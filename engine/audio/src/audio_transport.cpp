#include <cuexis/audio/audio_transport.hpp>

#include <cuexis/core/error.hpp>

#include <cmath>

namespace cuexis::audio {

IAudioClock::~IAudioClock() = default;
IAudioTransport::~IAudioTransport() = default;

auto validateSourceClockSample(const SourceClockSample& sample) -> core::Result<void> {
    if (!std::isfinite(sample.positionMs)) {
        return core::unexpected(
            core::Error{"audio.clock.position_invalid", "Source clock position must be finite"});
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
    if (initialized_ && sample.discontinuityId == discontinuityId_.load() &&
        sample.positionMs < positionMs_.load()) {
        return core::unexpected(core::Error{
            "audio.clock.segment_regressed",
            "Playing source position must not regress within one discontinuity segment"});
    }

    sequence_.fetch_add(1);
    positionMs_.store(sample.positionMs);
    state_.store(static_cast<int>(sample.state));
    discontinuityId_.store(sample.discontinuityId);
    sequence_.fetch_add(1);
    initialized_ = true;
    return {};
}

SourceClockSample HostClock::snapshot() const noexcept {
    struct SnapshotCache final {
        const HostClock* owner{};
        SourceClockSample sample{};
    };
    thread_local SnapshotCache cache;
    if (cache.owner != this) {
        cache.owner = this;
        cache.sample = {};
    }

    constexpr std::uint32_t maxAttempts = 8;
    for (std::uint32_t attempt = 0; attempt < maxAttempts; ++attempt) {
        const auto before = sequence_.load();
        if ((before & 1U) != 0) {
            continue;
        }

        SourceClockSample result;
        result.positionMs = positionMs_.load();
        result.state = static_cast<PlaybackState>(state_.load());
        result.discontinuityId = discontinuityId_.load();

        const auto after = sequence_.load();
        if (before == after && (after & 1U) == 0) {
            cache.sample = result;
            return result;
        }
    }
    return cache.sample;
}

} // namespace cuexis::audio
