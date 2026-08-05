#include <cuexis/chart/timing_map.hpp>

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace cuexis::chart {
namespace {

constexpr double minimumBpm = 1.0;
constexpr double maximumBpm = 65536.0;
constexpr std::size_t inverseIterations = 64;
constexpr std::size_t maximumTempoEvents = 4096;
constexpr std::size_t maximumStops = 4096;

constexpr std::array<double, 8> quadratureNodes{
    0.095012509837637440185, 0.281603550779258913230, 0.458016777657227386342,
    0.617876244402643748447, 0.755404408355003033895, 0.865631202387831743880,
    0.944575023073232576078, 0.989400934991649932596,
};
constexpr std::array<double, 8> quadratureWeights{
    0.189450610455068496285, 0.182603415044923588867, 0.169156519395002538189,
    0.149595988816576732081, 0.124628971255533872052, 0.095158511682492784810,
    0.062253523938647892863, 0.027152459411754094852,
};

[[nodiscard]] auto requireFinite(double value, const char* field) -> core::Result<void> {
    if (std::isfinite(value)) {
        return {};
    }
    return core::unexpected(
        core::Error{"chart.timing.non_finite", "Timing values must be finite"}.withContext("field",
                                                                                           field));
}

[[nodiscard]] auto hermiteProgress(double value, double startSlope, double endSlope) noexcept
    -> double {
    const double squared = value * value;
    const double cubed = squared * value;
    return (-2.0 + startSlope + endSlope) * cubed + (3.0 - 2.0 * startSlope - endSlope) * squared +
           startSlope * value;
}

[[nodiscard]] auto eventBpm(double normalized, double startBpm, double endBpm, double startSlope,
                            double endSlope) noexcept -> double {
    return startBpm + (endBpm - startBpm) * hermiteProgress(normalized, startSlope, endSlope);
}

[[nodiscard]] auto integrateEvent(double fromNormalized, double toNormalized, double durationBeats,
                                  double startBpm, double endBpm, double startSlope,
                                  double endSlope) noexcept -> double {
    if (fromNormalized == toNormalized || durationBeats == 0.0) {
        return 0.0;
    }
    const double midpoint = (fromNormalized + toNormalized) * 0.5;
    const double halfWidth = (toNormalized - fromNormalized) * 0.5;
    double sum = 0.0;
    for (std::size_t index = 0; index < quadratureNodes.size(); ++index) {
        const double offset = halfWidth * quadratureNodes[index];
        const double leftBpm = eventBpm(midpoint - offset, startBpm, endBpm, startSlope, endSlope);
        const double rightBpm = eventBpm(midpoint + offset, startBpm, endBpm, startSlope, endSlope);
        sum += quadratureWeights[index] * (60000.0 / leftBpm + 60000.0 / rightBpm);
    }
    return durationBeats * halfWidth * sum;
}

[[nodiscard]] auto validBpm(double value) noexcept -> bool {
    return std::isfinite(value) && value >= minimumBpm && value <= maximumBpm;
}

} // namespace

TimingMap::TimingMap(double defaultBpm, double offsetMs,
                     std::vector<CompiledTempoEvent> tempoEvents, std::vector<CompiledStop> stops,
                     double tempoTimeAtZero, double stopDurationBeforeZero) noexcept
    : defaultBpm_(defaultBpm), offsetMs_(offsetMs), tempoEvents_(std::move(tempoEvents)),
      stops_(std::move(stops)), tempoTimeAtZero_(tempoTimeAtZero),
      stopDurationBeforeZero_(stopDurationBeforeZero) {}

auto TimingMap::create(double defaultBpm, double offsetMs) -> core::Result<TimingMap> {
    return create(defaultBpm, offsetMs, {}, {});
}

auto TimingMap::create(double defaultBpm, double offsetMs, std::span<const TempoEvent> tempoEvents,
                       std::span<const TimingStop> stops) -> core::Result<TimingMap> {
    if (auto result = requireFinite(defaultBpm, "defaultBpm"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (!validBpm(defaultBpm)) {
        return core::unexpected(
            core::Error{"chart.timing.invalid_bpm", "Default BPM must be in [1, 65536]"}
                .withContext("defaultBpm", std::to_string(defaultBpm)));
    }
    if (auto result = requireFinite(offsetMs, "offsetMs"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (tempoEvents.size() > maximumTempoEvents) {
        return core::unexpected(
            core::Error{"chart.limit.tempo_events", "Tempo Event count exceeds the fixed limit"});
    }
    if (stops.size() > maximumStops) {
        return core::unexpected(
            core::Error{"chart.limit.stops", "Stop count exceeds the fixed limit"});
    }

    std::vector<const TempoEvent*> sortedEvents;
    sortedEvents.reserve(tempoEvents.size());
    for (const auto& event : tempoEvents) {
        sortedEvents.push_back(&event);
    }
    std::sort(sortedEvents.begin(), sortedEvents.end(), [](const auto* left, const auto* right) {
        return left->startBeat < right->startBeat;
    });

    std::vector<CompiledTempoEvent> compiledEvents;
    compiledEvents.reserve(sortedEvents.size());
    double currentBeat = sortedEvents.empty() ? 0.0 : sortedEvents.front()->startBeat.toDouble();
    double currentTime = 0.0;
    double currentBpm = defaultBpm;
    double previousEndBeat = 0.0;
    bool hasPrevious = false;
    for (const auto* source : sortedEvents) {
        if (source->durationBeats.numerator() < 0) {
            return core::unexpected(core::Error{"chart.timing.duration_negative",
                                                "Tempo Event duration must be non-negative"});
        }
        if (!validBpm(source->startBpm) || !validBpm(source->endBpm)) {
            return core::unexpected(
                core::Error{"chart.timing.invalid_bpm", "Tempo Event BPM must be in [1, 65536]"});
        }
        if (!std::isfinite(source->startSlope) || !std::isfinite(source->endSlope) ||
            source->startSlope < 0.0 || source->endSlope < 0.0 ||
            source->startSlope + source->endSlope > 3.0) {
            return core::unexpected(core::Error{
                "chart.timing.invalid_slope",
                "Tempo Event slopes must be finite, non-negative, and sum to at most 3"});
        }
        const double startBeat = source->startBeat.toDouble();
        const double duration = source->durationBeats.toDouble();
        const double endBeat = startBeat + duration;
        if (!std::isfinite(startBeat) || !std::isfinite(endBeat)) {
            return core::unexpected(
                core::Error{"chart.timing.out_of_range", "Tempo Event Beat conversion overflowed"});
        }
        if (hasPrevious && startBeat < previousEndBeat) {
            return core::unexpected(
                core::Error{"chart.timing.tempo_overlap", "Tempo Events must not overlap"});
        }
        if (hasPrevious && startBeat == compiledEvents.back().startBeat) {
            return core::unexpected(core::Error{"chart.timing.tempo_start_duplicate",
                                                "Tempo Events must have unique start Beats"});
        }
        if (duration == 0.0 && (source->startBpm != source->endBpm || source->startSlope != 0.0 ||
                                source->endSlope != 0.0)) {
            return core::unexpected(
                core::Error{"chart.timing.zero_duration_invalid",
                            "Zero-duration Tempo Events require equal BPM and zero slopes"});
        }

        currentTime += (startBeat - currentBeat) * (60000.0 / currentBpm);
        CompiledTempoEvent compiled{
            .startBeat = startBeat,
            .endBeat = endBeat,
            .startBpm = source->startBpm,
            .endBpm = source->endBpm,
            .startSlope = source->startSlope,
            .endSlope = source->endSlope,
            .startTimeMs = currentTime,
        };
        const double bpmRatio =
            std::max(source->startBpm, source->endBpm) / std::min(source->startBpm, source->endBpm);
        const auto segmentCount = static_cast<std::size_t>(std::clamp(
            std::ceil(std::log2(bpmRatio)), 1.0, static_cast<double>(maxIntegrationSegments)));
        compiled.integrationSegmentCount = segmentCount;
        double previousNormalized = 0.0;
        double durationTime = 0.0;
        for (std::size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
            double nextNormalized = 1.0;
            if (segmentIndex + 1 < segmentCount && source->startBpm != source->endBpm) {
                const double fraction =
                    static_cast<double>(segmentIndex + 1) / static_cast<double>(segmentCount);
                const double targetBpm =
                    source->startBpm * std::pow(source->endBpm / source->startBpm, fraction);
                const double targetProgress =
                    (targetBpm - source->startBpm) / (source->endBpm - source->startBpm);
                double lower = previousNormalized;
                double upper = 1.0;
                for (std::size_t iteration = 0; iteration < inverseIterations; ++iteration) {
                    const double midpoint = (lower + upper) * 0.5;
                    if (hermiteProgress(midpoint, source->startSlope, source->endSlope) <
                        targetProgress) {
                        lower = midpoint;
                    } else {
                        upper = midpoint;
                    }
                }
                nextNormalized = (lower + upper) * 0.5;
            }
            const double segmentTime =
                integrateEvent(previousNormalized, nextNormalized, duration, source->startBpm,
                               source->endBpm, source->startSlope, source->endSlope);
            compiled.integrationSegments[segmentIndex] = CompiledTempoSegment{
                previousNormalized, nextNormalized, durationTime, durationTime + segmentTime};
            durationTime += segmentTime;
            previousNormalized = nextNormalized;
        }
        compiled.endTimeMs = currentTime + durationTime;
        if (!std::isfinite(currentTime) || !std::isfinite(durationTime)) {
            return core::unexpected(
                core::Error{"chart.timing.out_of_range", "Tempo Event integration overflowed"});
        }
        compiledEvents.push_back(std::move(compiled));
        currentBeat = endBeat;
        currentTime += durationTime;
        currentBpm = source->endBpm;
        previousEndBeat = endBeat;
        hasPrevious = true;
    }

    TimingMap partial{defaultBpm, offsetMs, std::move(compiledEvents), {}, 0.0, 0.0};
    partial.tempoTimeAtZero_ = partial.tempoTime(0.0);

    std::vector<const TimingStop*> sortedStops;
    sortedStops.reserve(stops.size());
    for (const auto& stop : stops) {
        sortedStops.push_back(&stop);
    }
    std::sort(sortedStops.begin(), sortedStops.end(),
              [](const auto* left, const auto* right) { return left->beat < right->beat; });
    double cumulative = 0.0;
    double beforeZero = 0.0;
    double previousStopBeat = 0.0;
    bool hasPreviousStop = false;
    partial.stops_.reserve(sortedStops.size());
    for (const auto* source : sortedStops) {
        const double beat = source->beat.toDouble();
        if (!std::isfinite(source->durationMs) || source->durationMs <= 0.0) {
            return core::unexpected(core::Error{"chart.timing.stop_duration_invalid",
                                                "Stop duration must be finite and positive"});
        }
        if (hasPreviousStop && beat == previousStopBeat) {
            return core::unexpected(
                core::Error{"chart.timing.stop_beat_duplicate", "Stops must have unique Beats"});
        }
        const double startTime = partial.tempoTime(beat) - partial.tempoTimeAtZero_ + cumulative;
        cumulative += source->durationMs;
        if (!std::isfinite(startTime) || !std::isfinite(cumulative)) {
            return core::unexpected(
                core::Error{"chart.timing.out_of_range", "Stop timing overflowed"});
        }
        partial.stops_.push_back(
            CompiledStop{beat, startTime, startTime + source->durationMs, cumulative});
        if (beat < 0.0) {
            beforeZero = cumulative;
        }
        previousStopBeat = beat;
        hasPreviousStop = true;
    }
    partial.stopDurationBeforeZero_ = beforeZero;
    for (auto& stop : partial.stops_) {
        stop.startTimeMs -= beforeZero;
        stop.endTimeMs -= beforeZero;
    }
    return partial;
}

auto TimingMap::defaultBpm() const noexcept -> double {
    return defaultBpm_;
}

auto TimingMap::offsetMs() const noexcept -> double {
    return offsetMs_;
}

auto TimingMap::tempoTime(double beat) const noexcept -> double {
    if (tempoEvents_.empty()) {
        return beat * (60000.0 / defaultBpm_);
    }
    const auto next = std::upper_bound(
        tempoEvents_.begin(), tempoEvents_.end(), beat,
        [](double value, const CompiledTempoEvent& event) { return value < event.startBeat; });
    if (next == tempoEvents_.begin()) {
        const auto& first = tempoEvents_.front();
        return first.startTimeMs + (beat - first.startBeat) * (60000.0 / defaultBpm_);
    }
    const auto& event = *(next - 1);
    if (event.endBeat > event.startBeat && beat < event.endBeat) {
        const double normalized = (beat - event.startBeat) / (event.endBeat - event.startBeat);
        const auto segments =
            std::span{event.integrationSegments}.first(event.integrationSegmentCount);
        const auto segment =
            std::lower_bound(segments.begin(), segments.end(), normalized,
                             [](const CompiledTempoSegment& candidate, double value) {
                                 return candidate.endNormalized < value;
                             });
        if (segment == segments.end()) {
            return event.endTimeMs;
        }
        return event.startTimeMs + segment->startTimeMs +
               integrateEvent(segment->startNormalized, normalized, event.endBeat - event.startBeat,
                              event.startBpm, event.endBpm, event.startSlope, event.endSlope);
    }
    return event.endTimeMs + (beat - event.endBeat) * (60000.0 / event.endBpm);
}

auto TimingMap::stopAdjustment(double beat) const noexcept -> double {
    const auto next =
        std::lower_bound(stops_.begin(), stops_.end(), beat,
                         [](const CompiledStop& stop, double value) { return stop.beat < value; });
    const double cumulative = next == stops_.begin() ? 0.0 : (next - 1)->cumulativeDurationMs;
    return cumulative - stopDurationBeforeZero_;
}

auto TimingMap::beatToChartTimeMs(const RationalBeat& beat) const noexcept -> double {
    const double value = beat.toDouble();
    return tempoTime(value) - tempoTimeAtZero_ + stopAdjustment(value);
}

auto TimingMap::beatToChartTimeMs(double beat) const -> core::Result<double> {
    if (auto result = requireFinite(beat, "beat"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    const double value = tempoTime(beat) - tempoTimeAtZero_ + stopAdjustment(beat);
    if (!std::isfinite(value)) {
        return core::unexpected(
            core::Error{"chart.timing.out_of_range", "Beat conversion overflowed"});
    }
    return value;
}

auto TimingMap::tempoBeat(double chartTimeMs) const noexcept -> double {
    if (tempoEvents_.empty()) {
        return chartTimeMs * defaultBpm_ / 60000.0;
    }
    const double absoluteTime = chartTimeMs + tempoTimeAtZero_;
    const auto next = std::upper_bound(
        tempoEvents_.begin(), tempoEvents_.end(), absoluteTime,
        [](double value, const CompiledTempoEvent& event) { return value < event.startTimeMs; });
    if (next == tempoEvents_.begin()) {
        const auto& first = tempoEvents_.front();
        return first.startBeat + (absoluteTime - first.startTimeMs) * defaultBpm_ / 60000.0;
    }
    const auto& event = *(next - 1);
    if (event.endBeat > event.startBeat && absoluteTime < event.endTimeMs) {
        double lower = event.startBeat;
        double upper = event.endBeat;
        for (std::size_t iteration = 0; iteration < inverseIterations; ++iteration) {
            const double midpoint = (lower + upper) * 0.5;
            if (tempoTime(midpoint) < absoluteTime) {
                lower = midpoint;
            } else {
                upper = midpoint;
            }
        }
        return (lower + upper) * 0.5;
    }
    return event.endBeat + (absoluteTime - event.endTimeMs) * event.endBpm / 60000.0;
}

auto TimingMap::sampleChartTimeMs(double chartTimeMs) const -> core::Result<BeatSample> {
    if (auto result = requireFinite(chartTimeMs, "chartTimeMs"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    const auto stop = std::upper_bound(
        stops_.begin(), stops_.end(), chartTimeMs,
        [](double value, const CompiledStop& candidate) { return value < candidate.startTimeMs; });
    if (stop != stops_.begin()) {
        const auto& candidate = *(stop - 1);
        if (chartTimeMs >= candidate.startTimeMs && chartTimeMs < candidate.endTimeMs) {
            return BeatSample{candidate.beat, true,
                              (chartTimeMs - candidate.startTimeMs) /
                                  (candidate.endTimeMs - candidate.startTimeMs)};
        }
    }
    const auto completed = std::upper_bound(
        stops_.begin(), stops_.end(), chartTimeMs,
        [](double value, const CompiledStop& candidate) { return value < candidate.endTimeMs; });
    const double cumulative =
        completed == stops_.begin() ? 0.0 : (completed - 1)->cumulativeDurationMs;
    const double withoutStops = chartTimeMs - cumulative + stopDurationBeforeZero_;
    const double beat = tempoBeat(withoutStops);
    if (!std::isfinite(beat)) {
        return core::unexpected(
            core::Error{"chart.timing.out_of_range", "Chart time conversion overflowed"});
    }
    return BeatSample{beat, false, 0.0};
}

auto TimingMap::chartTimeMsToBeat(double chartTimeMs) const -> core::Result<double> {
    auto sample = sampleChartTimeMs(chartTimeMs);
    if (!sample) {
        return core::unexpected(std::move(sample.error()));
    }
    return sample->beat;
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
