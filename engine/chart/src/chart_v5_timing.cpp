#include <cuexis/chart/chart_v5_timing.hpp>

#include <utility>

namespace cuexis::chart {

auto makeChartTiming(const ChartTimingSource& timing) -> core::Result<ChartTiming> {
    ChartTiming document;
    document.offsetMs = timing.offsetMs;
    document.defaultBpm = timing.defaultBpm;
    document.tempoEvents.reserve(timing.tempoEvents.size());
    for (const auto& event : timing.tempoEvents) {
        document.tempoEvents.push_back(TempoEvent{event.startBeat, event.durationBeats,
                                                  event.startBpm, event.endBpm, event.startSlope,
                                                  event.endSlope});
    }
    document.stops.reserve(timing.stops.size());
    for (const auto& stop : timing.stops) {
        document.stops.push_back(TimingStop{stop.beat, stop.durationMs});
    }
    return document;
}

auto compileChartTiming(const ChartTimingSource& timing,
                        std::span<const timing::RationalBeat> requiredAnchors)
    -> core::Result<CompiledChartTiming> {
    auto document = makeChartTiming(timing);
    if (!document) {
        return core::unexpected(std::move(document.error()));
    }
    return chart::compileChartTiming(*document, requiredAnchors);
}

} // namespace cuexis::chart
