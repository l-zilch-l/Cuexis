#pragma once

//  TimingMap - bidirectional conversion between Beat and chartTimeMs
//  v1 supports offsetMs and defaultBpm; BPM changes and stops must currently be empty
//  (a non-empty list returns an error)
//  chartTimeMs = audioTimeMs - offsetMs (a positive offset means Beat 0 starts after the
//  audio begins)
//  Uses double milliseconds, but never builds the mapping by accumulating per frame (this
//  guarantees determinism)

#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/result.hpp>

namespace cuexis::chart {

class TimingMap final {
  public:
    [[nodiscard]] static auto create(double defaultBpm, double offsetMs) -> core::Result<TimingMap>;

    [[nodiscard]] auto defaultBpm() const noexcept -> double;
    [[nodiscard]] auto offsetMs() const noexcept -> double;

    // chartTimeMs is relative to Beat 0. Offset only converts audio time to chart time.
    [[nodiscard]] auto beatToChartTimeMs(const RationalBeat& beat) const noexcept -> double;
    [[nodiscard]] auto chartTimeMsToBeat(double chartTimeMs) const -> core::Result<double>;
    [[nodiscard]] auto audioTimeMsToChartTimeMs(double audioTimeMs) const -> core::Result<double>;
    [[nodiscard]] auto chartTimeMsToAudioTimeMs(double chartTimeMs) const -> core::Result<double>;

  private:
    TimingMap(double defaultBpm, double offsetMs) noexcept
        : defaultBpm_(defaultBpm), offsetMs_(offsetMs) {}

    double defaultBpm_{};
    double offsetMs_{};
};

} // namespace cuexis::chart
