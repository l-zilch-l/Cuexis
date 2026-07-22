#pragma once

//  TimingMap — Beat 与 chartTimeMs 之间的双向转换
//  v1 支持 offsetMs 和 defaultBpm；BPM Changes 和 Stops 当前必须为空（非空返回错误）
//  chartTimeMs = audioTimeMs - offsetMs（正 offset 表示 Beat 0 在音频开始之后）
//  使用 double 毫秒，但不通过逐帧累加建立映射（保证确定性）

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
