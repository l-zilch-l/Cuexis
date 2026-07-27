#pragma once

// Converts backend-neutral source clock samples into the PlaybackSession RuntimeFrame contract.

#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_session.hpp>

#include <cstdint>

namespace cuexis::playback {

class RuntimeTimeline final {
  public:
    [[nodiscard]] static auto create(double timingOffsetMs) -> core::Result<RuntimeTimeline>;

    [[nodiscard]] auto advance(const audio::SourceClockSample& sample)
        -> core::Result<RuntimeFrame>;
    [[nodiscard]] auto reset(double timingOffsetMs) -> core::Result<void>;
    [[nodiscard]] double timingOffsetMs() const noexcept;
    [[nodiscard]] std::uint64_t discontinuityId() const noexcept;

  private:
    explicit RuntimeTimeline(double timingOffsetMs) noexcept;

    double timingOffsetMs_{};
    audio::SourceClockSample previous_{};
    std::uint64_t discontinuityId_{};
    bool initialized_{};
    bool pendingDiscontinuity_{};
};

class ChartClock final {
  public:
    explicit ChartClock(double timingOffsetMs) noexcept;

    [[nodiscard]] auto sample(double chartTimeMs,
                              audio::PlaybackState state = audio::PlaybackState::Playing)
        -> core::Result<audio::SourceClockSample>;
    void markDiscontinuity() noexcept;

  private:
    double timingOffsetMs_{};
    std::uint64_t discontinuityId_{};
};

} // namespace cuexis::playback
