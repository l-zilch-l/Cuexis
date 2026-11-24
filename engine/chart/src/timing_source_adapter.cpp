#include <cuexis/chart/timing_source_adapter.hpp>

namespace cuexis::chart {

auto TimingSourceData::view() const noexcept -> timing::TimingSourceView {
    return timing::TimingSourceView{defaultBpm, tempoEvents, stops};
}

auto makeTimingSource(const ChartTiming& timing) -> core::Result<TimingSourceData> {
    TimingSourceData data;
    data.defaultBpm = timing.defaultBpm;
    data.tempoEvents.reserve(timing.tempoEvents.size());
    for (const auto& event : timing.tempoEvents) {
        auto start = timing::RationalBeat::create(event.startBeat.numerator(),
                                                  event.startBeat.denominator());
        if (!start) {
            return core::unexpected(std::move(start.error()));
        }
        auto duration = timing::RationalBeat::create(event.durationBeats.numerator(),
                                                     event.durationBeats.denominator());
        if (!duration) {
            return core::unexpected(std::move(duration.error()));
        }
        data.tempoEvents.push_back(timing::TempoEvent{
            *start, *duration, event.startBpm, event.endBpm, event.startSlope, event.endSlope});
    }
    data.stops.reserve(timing.stops.size());
    for (const auto& stop : timing.stops) {
        auto beat = timing::RationalBeat::create(stop.beat.numerator(), stop.beat.denominator());
        if (!beat) {
            return core::unexpected(std::move(beat.error()));
        }
        data.stops.push_back(timing::Stop{*beat, stop.durationMs});
    }
    return data;
}

} // namespace cuexis::chart
