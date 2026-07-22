//  TimingMap 实现 — Beat 与 chartTimeMs 双向转换
//  chartTimeMs = audioTimeMs - offsetMs（正 offset 表示 Beat 0 在音频开始之后）
//  beatToChartTimeMs: beat * (60000.0 / BPM) - offset 的 BPM（忽略 offset 符号）
//  v1 仅 defaultBpm + offsetMs；BPM Changes/Stops 当前未启用

#include <cuexis/chart/timing_map.hpp>

#include <cuexis/core/error.hpp>

#include <cmath>
#include <string>
#include <utility>

namespace cuexis::chart {
namespace {

[[nodiscard]] auto requireFinite(double value, const char* field) -> core::Result<void> {
    if (std::isfinite(value)) {
        return {};
    }
    return core::unexpected(
        core::Error{"chart.timing.non_finite", "Timing values must be finite"}.withContext("field",
                                                                                           field));
}

} // namespace

auto TimingMap::create(double defaultBpm, double offsetMs) -> core::Result<TimingMap> {
    if (auto result = requireFinite(defaultBpm, "defaultBpm"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (defaultBpm <= 0.0) {
        return core::unexpected(
            core::Error{"chart.timing.invalid_bpm", "Default BPM must be greater than zero"}
                .withContext("defaultBpm", std::to_string(defaultBpm)));
    }
    if (auto result = requireFinite(offsetMs, "offsetMs"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    return TimingMap{defaultBpm, offsetMs};
}

auto TimingMap::defaultBpm() const noexcept -> double {
    return defaultBpm_;
}

auto TimingMap::offsetMs() const noexcept -> double {
    return offsetMs_;
}

auto TimingMap::beatToChartTimeMs(const RationalBeat& beat) const noexcept -> double {
    return beat.toDouble() * (60000.0 / defaultBpm_);
}

auto TimingMap::chartTimeMsToBeat(double chartTimeMs) const -> core::Result<double> {
    if (auto result = requireFinite(chartTimeMs, "chartTimeMs"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    const double beat = chartTimeMs * defaultBpm_ / 60000.0;
    if (!std::isfinite(beat)) {
        return core::unexpected(
            core::Error{"chart.timing.out_of_range", "Chart time conversion overflowed"});
    }
    return beat;
}

auto TimingMap::audioTimeMsToChartTimeMs(double audioTimeMs) const -> core::Result<double> {
    if (auto result = requireFinite(audioTimeMs, "audioTimeMs"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    const double chartTimeMs = audioTimeMs - offsetMs_;
    if (!std::isfinite(chartTimeMs)) {
        return core::unexpected(
            core::Error{"chart.timing.out_of_range", "Audio time conversion overflowed"});
    }
    return chartTimeMs;
}

auto TimingMap::chartTimeMsToAudioTimeMs(double chartTimeMs) const -> core::Result<double> {
    if (auto result = requireFinite(chartTimeMs, "chartTimeMs"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    const double audioTimeMs = chartTimeMs + offsetMs_;
    if (!std::isfinite(audioTimeMs)) {
        return core::unexpected(
            core::Error{"chart.timing.out_of_range", "Chart time conversion overflowed"});
    }
    return audioTimeMs;
}

} // namespace cuexis::chart
