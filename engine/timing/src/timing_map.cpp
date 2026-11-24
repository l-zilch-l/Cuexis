#include <cuexis/timing/timing_map.hpp>

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace cuexis::timing {
namespace {

constexpr double minimumBpm = 1.0;
constexpr double maximumBpm = 65536.0;
constexpr std::size_t inverseIterations = 64;
constexpr std::size_t maximumTempoEvents = 4096;
constexpr std::size_t maximumStops = 4096;
constexpr std::size_t maximumIntegrationSegments = 16;
constexpr double microsecondsPerMinute = 60000000.0;

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

//  The relation failure the shared timing contract names: one invalid relation, with the stable
//  `reason` token and the source field path carried as context.
[[nodiscard]] auto relationError(std::string reason, std::string path, std::string message)
    -> core::Error {
    return core::Error{"judgement.execution.relation_invalid", std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason))
        .withContext("fieldPath", std::move(path));
}

[[nodiscard]] auto sourceError(std::string reason, std::string path, std::string message)
    -> core::Error {
    return core::Error{"timing.source_invalid", std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason))
        .withContext("fieldPath", std::move(path));
}

[[nodiscard]] auto rangeError(std::string message) -> core::Error {
    return core::Error{"timing.time_range", std::move(message)}
        .withContext("category", "budget_exceeded")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}

[[nodiscard]] auto validBpm(double value) noexcept -> bool {
    return std::isfinite(value) && value >= minimumBpm && value <= maximumBpm;
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
        sum += quadratureWeights[index] *
               (microsecondsPerMinute / leftBpm + microsecondsPerMinute / rightBpm);
    }
    return durationBeats * halfWidth * sum;
}

//  Exact rational order helpers over RationalBeat vectors.
void sortAndDeduplicate(std::vector<RationalBeat>& beats) {
    std::sort(beats.begin(), beats.end());
    beats.erase(std::unique(beats.begin(), beats.end()), beats.end());
}

//  The unsigned distance `later - earlier` of two signed ticks. The subtraction itself is done in
//  the unsigned domain so a pair that spans the whole signed range does not overflow.
[[nodiscard]] auto tickDistance(std::int64_t later, std::int64_t earlier) noexcept
    -> std::uint64_t {
    return static_cast<std::uint64_t>(later) - static_cast<std::uint64_t>(earlier);
}

} // namespace

struct detail::TimingMapStorage final {
    struct IntegrationSegment final {
        double startNormalized{};
        double endNormalized{};
        double startUs{};
        double endUs{};
    };

    struct CompiledTempo final {
        double startBeat{};
        double endBeat{};
        double startBpm{};
        double endBpm{};
        double startSlope{};
        double endSlope{};
        double startUs{};
        double endUs{};
        std::array<IntegrationSegment, maximumIntegrationSegments> segments{};
        std::size_t segmentCount{};
    };

    double defaultBpm{120.0};
    std::vector<TempoEvent> tempos;
    std::vector<RationalBeat> tempoEndBeats;
    std::vector<Stop> stops;
    std::vector<TempoBoundary> tempoBoundaries;
    std::vector<StopBoundary> stopBoundaries;
    std::vector<AnchoredBeat> anchors;

    //  Continuous integration cache. It is derived from the source snapshot only; the saved
    //  boundary ticks above stay the discrete authority.
    std::vector<CompiledTempo> compiledTempos;
    double tempoUsAtZero{};
    double stopUsBeforeZero{};
    std::vector<double> stopCumulativeUs;
};

namespace {

using Storage = detail::TimingMapStorage;

//  Copies and validates the source snapshot. The declaration order is preserved for
//  `matchesSource`; the ordering rules are checked here so later stages only see a valid source.
[[nodiscard]] auto normalizeSource(const TimingSourceView& source, Storage& storage)
    -> core::Result<void> {
    if (!validBpm(source.defaultBpm)) {
        return core::unexpected(
            sourceError("invalid_bpm", "/timing/defaultBpm",
                        "the default BPM must be finite and inside [1, 65536]"));
    }
    if (source.tempos.size() > maximumTempoEvents) {
        return core::unexpected(sourceError("tempo_limit", "/timing/tempoEvents",
                                            "the tempo event count exceeds the fixed limit"));
    }
    if (source.stops.size() > maximumStops) {
        return core::unexpected(
            sourceError("stop_limit", "/timing/stops", "the Stop count exceeds the fixed limit"));
    }

    storage.defaultBpm = source.defaultBpm;
    storage.tempos.assign(source.tempos.begin(), source.tempos.end());
    storage.stops.assign(source.stops.begin(), source.stops.end());
    storage.tempoEndBeats.clear();
    storage.tempoEndBeats.reserve(storage.tempos.size());

    bool hasPrevious = false;
    RationalBeat previousEnd{RationalBeat::zero()};
    RationalBeat previousStart{RationalBeat::zero()};
    for (std::size_t index = 0; index < storage.tempos.size(); ++index) {
        const auto& event = storage.tempos[index];
        const std::string path = "/timing/tempoEvents/" + std::to_string(index);
        if (event.durationBeats.numerator() < 0) {
            return core::unexpected(
                sourceError("duration_negative", path, "a tempo duration must be non-negative"));
        }
        if (!validBpm(event.startBpm) || !validBpm(event.endBpm)) {
            return core::unexpected(
                sourceError("invalid_bpm", path, "tempo BPM values must be inside [1, 65536]"));
        }
        if (!std::isfinite(event.startSlope) || !std::isfinite(event.endSlope) ||
            event.startSlope < 0.0 || event.endSlope < 0.0 ||
            event.startSlope + event.endSlope > 3.0) {
            return core::unexpected(
                sourceError("invalid_slope", path,
                            "tempo slopes must be finite, non-negative and sum to at most 3"));
        }
        auto end = addRationalBeats(event.startBeat, event.durationBeats);
        if (!end) {
            return core::unexpected(std::move(end.error()));
        }
        if (!std::isfinite(event.startBeat.toDouble()) || !std::isfinite(end->toDouble())) {
            return core::unexpected(
                sourceError("out_of_range", path, "a tempo beat is outside the continuous range"));
        }
        if (event.durationBeats == RationalBeat::zero() &&
            (event.startBpm != event.endBpm || event.startSlope != 0.0 || event.endSlope != 0.0)) {
            return core::unexpected(
                sourceError("zero_duration_invalid", path,
                            "a zero-duration tempo event requires equal BPM and zero slopes"));
        }
        if (hasPrevious && event.startBeat < previousEnd) {
            return core::unexpected(
                sourceError("tempo_overlap", path, "tempo event sections must not overlap"));
        }
        if (hasPrevious && event.startBeat == previousStart) {
            return core::unexpected(
                sourceError("tempo_duplicate", path, "tempo events must have unique start beats"));
        }
        previousEnd = *end;
        previousStart = event.startBeat;
        hasPrevious = true;
        storage.tempoEndBeats.push_back(*end);
    }

    bool hasPreviousStop = false;
    RationalBeat previousStopBeat{RationalBeat::zero()};
    for (std::size_t index = 0; index < storage.stops.size(); ++index) {
        const auto& stop = storage.stops[index];
        const std::string path = "/timing/stops/" + std::to_string(index);
        if (!std::isfinite(stop.durationMs) || stop.durationMs <= 0.0) {
            return core::unexpected(sourceError("stop_duration_invalid", path,
                                                "a Stop duration must be finite and positive"));
        }
        if (!std::isfinite(stop.beat.toDouble())) {
            return core::unexpected(
                sourceError("out_of_range", path, "a Stop beat is outside the continuous range"));
        }
        if (hasPreviousStop && stop.beat == previousStopBeat) {
            return core::unexpected(
                sourceError("stop_duplicate", path, "Stops must have unique beats"));
        }
        if (hasPreviousStop && stop.beat < previousStopBeat) {
            return core::unexpected(
                sourceError("stop_order", path, "Stop beats must be declared in ascending order"));
        }
        previousStopBeat = stop.beat;
        hasPreviousStop = true;
    }
    return {};
}

//  The bounded continuous tempo integral `F(beat)` in absolute microseconds (origin as the source
//  declares it, not yet rebased to Beat 0).
[[nodiscard]] auto tempoTimeUs(const Storage& storage, double beat) noexcept -> double {
    if (storage.compiledTempos.empty()) {
        return beat * (microsecondsPerMinute / storage.defaultBpm);
    }
    const auto next = std::upper_bound(
        storage.compiledTempos.begin(), storage.compiledTempos.end(), beat,
        [](double value, const Storage::CompiledTempo& event) { return value < event.startBeat; });
    if (next == storage.compiledTempos.begin()) {
        const auto& first = storage.compiledTempos.front();
        return first.startUs +
               (beat - first.startBeat) * (microsecondsPerMinute / storage.defaultBpm);
    }
    const auto& event = *(next - 1);
    if (event.endBeat > event.startBeat && beat < event.endBeat) {
        const double normalized = (beat - event.startBeat) / (event.endBeat - event.startBeat);
        const auto segments = std::span{event.segments}.first(event.segmentCount);
        const auto segment =
            std::lower_bound(segments.begin(), segments.end(), normalized,
                             [](const Storage::IntegrationSegment& candidate, double value) {
                                 return candidate.endNormalized < value;
                             });
        if (segment == segments.end()) {
            return event.endUs;
        }
        return event.startUs + segment->startUs +
               integrateEvent(segment->startNormalized, normalized, event.endBeat - event.startBeat,
                              event.startBpm, event.endBpm, event.startSlope, event.endSlope);
    }
    return event.endUs + (beat - event.endBeat) * (microsecondsPerMinute / event.endBpm);
}

//  Builds the continuous coefficient cache. This is the only place the tempo curve is integrated;
//  saved discrete ticks are never derived from it once a boundary table exists.
[[nodiscard]] auto buildIntegration(Storage& storage) -> core::Result<void> {
    storage.compiledTempos.clear();
    storage.compiledTempos.reserve(storage.tempos.size());
    double currentBeat = storage.tempos.empty() ? 0.0 : storage.tempos.front().startBeat.toDouble();
    double currentUs = 0.0;
    double currentBpm = storage.defaultBpm;
    for (const auto& source : storage.tempos) {
        const double startBeat = source.startBeat.toDouble();
        const double duration = source.durationBeats.toDouble();
        const double endBeat = startBeat + duration;
        if (!std::isfinite(startBeat) || !std::isfinite(endBeat)) {
            return core::unexpected(sourceError("out_of_range", "/timing/tempoEvents",
                                                "a tempo beat is outside the continuous range"));
        }
        currentUs += (startBeat - currentBeat) * (microsecondsPerMinute / currentBpm);
        Storage::CompiledTempo compiled{
            .startBeat = startBeat,
            .endBeat = endBeat,
            .startBpm = source.startBpm,
            .endBpm = source.endBpm,
            .startSlope = source.startSlope,
            .endSlope = source.endSlope,
            .startUs = currentUs,
        };
        const double bpmRatio =
            std::max(source.startBpm, source.endBpm) / std::min(source.startBpm, source.endBpm);
        const auto segmentCount = static_cast<std::size_t>(std::clamp(
            std::ceil(std::log2(bpmRatio)), 1.0, static_cast<double>(maximumIntegrationSegments)));
        compiled.segmentCount = segmentCount;
        double previousNormalized = 0.0;
        double durationUs = 0.0;
        for (std::size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
            double nextNormalized = 1.0;
            if (segmentIndex + 1 < segmentCount && source.startBpm != source.endBpm) {
                const double fraction =
                    static_cast<double>(segmentIndex + 1) / static_cast<double>(segmentCount);
                const double targetBpm =
                    source.startBpm * std::pow(source.endBpm / source.startBpm, fraction);
                const double targetProgress =
                    (targetBpm - source.startBpm) / (source.endBpm - source.startBpm);
                double lower = previousNormalized;
                double upper = 1.0;
                for (std::size_t iteration = 0; iteration < inverseIterations; ++iteration) {
                    const double midpoint = (lower + upper) * 0.5;
                    if (hermiteProgress(midpoint, source.startSlope, source.endSlope) <
                        targetProgress) {
                        lower = midpoint;
                    } else {
                        upper = midpoint;
                    }
                }
                nextNormalized = (lower + upper) * 0.5;
            }
            const double segmentUs =
                integrateEvent(previousNormalized, nextNormalized, duration, source.startBpm,
                               source.endBpm, source.startSlope, source.endSlope);
            compiled.segments[segmentIndex] = Storage::IntegrationSegment{
                previousNormalized, nextNormalized, durationUs, durationUs + segmentUs};
            durationUs += segmentUs;
            previousNormalized = nextNormalized;
        }
        compiled.endUs = currentUs + durationUs;
        if (!std::isfinite(currentUs) || !std::isfinite(durationUs)) {
            return core::unexpected(sourceError("out_of_range", "/timing/tempoEvents",
                                                "the tempo integral overflowed"));
        }
        storage.compiledTempos.push_back(compiled);
        currentBeat = endBeat;
        currentUs += durationUs;
        currentBpm = source.endBpm;
    }
    storage.tempoUsAtZero = tempoTimeUs(storage, 0.0);
    if (!std::isfinite(storage.tempoUsAtZero)) {
        return core::unexpected(
            sourceError("out_of_range", "/timing", "the tempo integral at Beat 0 is not finite"));
    }

    storage.stopCumulativeUs.assign(storage.stops.size(), 0.0);
    storage.stopUsBeforeZero = 0.0;
    double cumulative = 0.0;
    for (std::size_t index = 0; index < storage.stops.size(); ++index) {
        const auto& stop = storage.stops[index];
        const double durationUs = stop.durationMs * 1000.0;
        if (!std::isfinite(durationUs)) {
            return core::unexpected(sourceError("stop_duration_invalid", "/timing/stops",
                                                "a Stop duration is outside the continuous range"));
        }
        cumulative += durationUs;
        if (!std::isfinite(cumulative)) {
            return core::unexpected(rangeError("the cumulative Stop duration is not finite"));
        }
        storage.stopCumulativeUs[index] = cumulative;
        if (stop.beat < RationalBeat::zero()) {
            storage.stopUsBeforeZero = cumulative;
        }
    }
    return {};
}

//  The `L(beat)` function from the contract: tempo integral rebased to Beat 0 plus every Stop
//  strictly before the beat, rebased by the Stops strictly before Beat 0.
[[nodiscard]] auto localTimeUs(const Storage& storage, const RationalBeat& beat)
    -> core::Result<double> {
    const double beatValue = beat.toDouble();
    if (!std::isfinite(beatValue)) {
        return core::unexpected(rangeError("the queried beat is outside the continuous range"));
    }
    const auto next = std::lower_bound(
        storage.stops.begin(), storage.stops.end(), beat,
        [](const Stop& stop, const RationalBeat& value) { return stop.beat < value; });
    const double cumulative =
        next == storage.stops.begin()
            ? 0.0
            : storage.stopCumulativeUs[static_cast<std::size_t>(next - storage.stops.begin()) - 1];
    const double value = tempoTimeUs(storage, beatValue) - storage.tempoUsAtZero + cumulative -
                         storage.stopUsBeforeZero;
    if (!std::isfinite(value)) {
        return core::unexpected(rangeError("the beat conversion left the continuous range"));
    }
    return value;
}

[[nodiscard]] auto stopIndexOfBeat(const Storage& storage, const RationalBeat& beat)
    -> std::optional<std::size_t> {
    const auto it = std::lower_bound(
        storage.stops.begin(), storage.stops.end(), beat,
        [](const Stop& stop, const RationalBeat& value) { return stop.beat < value; });
    if (it == storage.stops.end() || !(it->beat == beat)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(it - storage.stops.begin());
}

//  Collects the discrete boundary key set of the source: every tempo start/end beat and every Stop
//  beat. Beat 0 is added unconditionally because the anchor table always fixes it. The end beats
//  were checked when the source snapshot was normalized.
[[nodiscard]] auto sourceBoundaryKeys(const Storage& storage) -> std::vector<RationalBeat> {
    std::vector<RationalBeat> keys;
    keys.reserve(2 * storage.tempos.size() + storage.stops.size() + 1);
    for (std::size_t index = 0; index < storage.tempos.size(); ++index) {
        keys.push_back(storage.tempos[index].startBeat);
        keys.push_back(storage.tempoEndBeats[index]);
    }
    for (const auto& stop : storage.stops) {
        keys.push_back(stop.beat);
    }
    keys.push_back(RationalBeat::zero());
    sortAndDeduplicate(keys);
    return keys;
}

//  Computes the discrete boundary tables from the source snapshot and the caller's required
//  anchors. Every beat that has a Stop takes the Stop start tick.
[[nodiscard]] auto buildBoundaries(Storage& storage, std::span<const RationalBeat> requiredAnchors)
    -> core::Result<void> {
    storage.stopBoundaries.clear();
    storage.stopBoundaries.reserve(storage.stops.size());
    for (const auto& stop : storage.stops) {
        auto at = localTimeUs(storage, stop.beat);
        if (!at) {
            return core::unexpected(std::move(at.error()));
        }
        const double durationUs = stop.durationMs * 1000.0;
        auto start = roundMicroseconds(*at);
        if (!start) {
            return core::unexpected(std::move(start.error()));
        }
        auto end = roundMicroseconds(*at + durationUs);
        if (!end) {
            return core::unexpected(std::move(end.error()));
        }
        if (end->value <= start->value) {
            return core::unexpected(relationError("stop_bounds", "/timing/stops",
                                                  "a Stop does not cover a positive tick extent"));
        }
        if (!storage.stopBoundaries.empty() &&
            start->value <= storage.stopBoundaries.back().start.value) {
            return core::unexpected(relationError("stop_overlap", "/timing/stops",
                                                  "Stop start ticks must be strictly increasing"));
        }
        if (!storage.stopBoundaries.empty() &&
            storage.stopBoundaries.back().end.value == start->value) {
            return core::unexpected(
                relationError("stop_overlap", "/timing/stops",
                              "a preceding Stop ends where the next one starts"));
        }
        storage.stopBoundaries.push_back(StopBoundary{stop.beat, *start, *end});
    }

    std::vector<RationalBeat> tempoKeys;
    tempoKeys.reserve(2 * storage.tempos.size());
    for (std::size_t index = 0; index < storage.tempos.size(); ++index) {
        tempoKeys.push_back(storage.tempos[index].startBeat);
        tempoKeys.push_back(storage.tempoEndBeats[index]);
    }
    sortAndDeduplicate(tempoKeys);
    storage.tempoBoundaries.clear();
    storage.tempoBoundaries.reserve(tempoKeys.size());
    for (const auto& beat : tempoKeys) {
        if (const auto stop = stopIndexOfBeat(storage, beat); stop.has_value()) {
            storage.tempoBoundaries.push_back(
                TempoBoundary{beat, storage.stopBoundaries[*stop].start});
            continue;
        }
        auto at = localTimeUs(storage, beat);
        if (!at) {
            return core::unexpected(std::move(at.error()));
        }
        auto quantized = roundMicroseconds(*at);
        if (!quantized) {
            return core::unexpected(std::move(quantized.error()));
        }
        storage.tempoBoundaries.push_back(TempoBoundary{beat, *quantized});
    }

    std::vector<RationalBeat> anchorKeys = sourceBoundaryKeys(storage);
    anchorKeys.insert(anchorKeys.end(), requiredAnchors.begin(), requiredAnchors.end());
    sortAndDeduplicate(anchorKeys);
    storage.anchors.clear();
    storage.anchors.reserve(anchorKeys.size());
    for (const auto& beat : anchorKeys) {
        if (const auto stop = stopIndexOfBeat(storage, beat); stop.has_value()) {
            storage.anchors.push_back(AnchoredBeat{beat, storage.stopBoundaries[*stop].start});
            continue;
        }
        auto at = localTimeUs(storage, beat);
        if (!at) {
            return core::unexpected(std::move(at.error()));
        }
        auto quantized = roundMicroseconds(*at);
        if (!quantized) {
            return core::unexpected(std::move(quantized.error()));
        }
        storage.anchors.push_back(AnchoredBeat{beat, *quantized});
    }
    return {};
}

[[nodiscard]] auto findAnchor(const Storage& storage, const RationalBeat& beat) noexcept
    -> const AnchoredBeat* {
    const auto it = std::lower_bound(
        storage.anchors.begin(), storage.anchors.end(), beat,
        [](const AnchoredBeat& anchor, const RationalBeat& value) { return anchor.beat < value; });
    if (it == storage.anchors.end() || !(it->beat == beat)) {
        return nullptr;
    }
    return &*it;
}

//  Validates the caller's saved boundary tables against the source snapshot. No boundary tick is
//  recomputed: the tables are checked for key coverage, order, Start extent and the Stop rules.
[[nodiscard]] auto validateBoundaries(const Storage& storage, TimingBoundariesView boundaries,
                                      std::span<const RationalBeat> requiredAnchors)
    -> core::Result<void> {
    std::vector<RationalBeat> tempoKeys;
    tempoKeys.reserve(2 * storage.tempos.size());
    for (std::size_t index = 0; index < storage.tempos.size(); ++index) {
        tempoKeys.push_back(storage.tempos[index].startBeat);
        tempoKeys.push_back(storage.tempoEndBeats[index]);
    }
    sortAndDeduplicate(tempoKeys);
    if (boundaries.tempos.size() != tempoKeys.size()) {
        return core::unexpected(relationError("boundary_key_mismatch", "/TIM1/tempos",
                                              "the saved tempo boundary count differs from the "
                                              "source tempo key set"));
    }
    for (std::size_t index = 0; index < tempoKeys.size(); ++index) {
        if (!(boundaries.tempos[index].beat == tempoKeys[index])) {
            return core::unexpected(relationError(
                "boundary_key_mismatch", "/TIM1/tempos",
                "the saved tempo boundary keys are not exactly the source tempo keys"));
        }
    }
    for (std::size_t index = 1; index < boundaries.tempos.size(); ++index) {
        if (boundaries.tempos[index].at.value < boundaries.tempos[index - 1].at.value) {
            return core::unexpected(
                relationError("tick_regression", "/TIM1/tempos",
                              "saved tempo boundary ticks regress at distinct beats"));
        }
    }

    if (boundaries.stops.size() != storage.stops.size()) {
        return core::unexpected(relationError("boundary_key_mismatch", "/TIM1/stops",
                                              "the saved Stop boundary count differs from the "
                                              "source Stop key set"));
    }
    for (std::size_t index = 0; index < storage.stops.size(); ++index) {
        if (!(boundaries.stops[index].beat == storage.stops[index].beat)) {
            return core::unexpected(
                relationError("boundary_key_mismatch", "/TIM1/stops",
                              "the saved Stop boundary keys are not exactly the source Stop keys"));
        }
        const auto& stop = boundaries.stops[index];
        if (stop.end.value <= stop.start.value) {
            return core::unexpected(
                relationError("stop_bounds", "/TIM1/stops",
                              "a saved Stop does not cover a positive tick extent"));
        }
        if (index > 0) {
            const auto& previous = boundaries.stops[index - 1];
            if (previous.end.value == stop.start.value) {
                return core::unexpected(
                    relationError("stop_overlap", "/TIM1/stops",
                                  "a preceding saved Stop ends where the next one starts"));
            }
            if (previous.end.value > stop.start.value) {
                return core::unexpected(
                    relationError("stop_bounds", "/TIM1/stops", "saved Stop extents overlap"));
            }
        }
    }

    std::vector<RationalBeat> anchorKeys = sourceBoundaryKeys(storage);
    anchorKeys.insert(anchorKeys.end(), requiredAnchors.begin(), requiredAnchors.end());
    sortAndDeduplicate(anchorKeys);
    if (boundaries.anchors.size() != anchorKeys.size()) {
        return core::unexpected(
            relationError("anchor_key_mismatch", "/TIM1/anchors",
                          "the saved anchor count differs from the independent key union"));
    }
    for (std::size_t index = 0; index < anchorKeys.size(); ++index) {
        if (!(boundaries.anchors[index].beat == anchorKeys[index])) {
            return core::unexpected(
                relationError("anchor_key_mismatch", "/TIM1/anchors",
                              "the saved anchor keys are not the independent key union"));
        }
        if (index > 0 &&
            boundaries.anchors[index].at.value < boundaries.anchors[index - 1].at.value) {
            return core::unexpected(relationError("tick_regression", "/TIM1/anchors",
                                                  "saved anchor ticks regress at distinct beats"));
        }
    }
    if (!anchorKeys.empty()) {
        const auto zero = std::lower_bound(
            boundaries.anchors.begin(), boundaries.anchors.end(), RationalBeat::zero(),
            [](const AnchoredBeat& anchor, const RationalBeat& value) {
                return anchor.beat < value;
            });
        if (zero == boundaries.anchors.end() || !(zero->beat == RationalBeat::zero()) ||
            zero->at.value != 0) {
            return core::unexpected(
                relationError("zero_anchor", "/TIM1/anchors", "Beat 0 must be anchored at tick 0"));
        }
    }

    for (const auto& tempo : boundaries.tempos) {
        const auto it =
            std::lower_bound(boundaries.anchors.begin(), boundaries.anchors.end(), tempo.beat,
                             [](const AnchoredBeat& anchor, const RationalBeat& value) {
                                 return anchor.beat < value;
                             });
        if (it == boundaries.anchors.end() || !(it->beat == tempo.beat)) {
            return core::unexpected(relationError("anchor_key_mismatch", "/TIM1/anchors",
                                                  "a saved tempo boundary has no matching anchor"));
        }
        if (it->at.value != tempo.at.value) {
            return core::unexpected(
                relationError("tick_mismatch", "/TIM1/tempos",
                              "a saved tempo boundary and its anchor disagree on the tick"));
        }
    }
    for (const auto& stop : boundaries.stops) {
        const auto it =
            std::lower_bound(boundaries.anchors.begin(), boundaries.anchors.end(), stop.beat,
                             [](const AnchoredBeat& anchor, const RationalBeat& value) {
                                 return anchor.beat < value;
                             });
        if (it == boundaries.anchors.end() || !(it->beat == stop.beat)) {
            return core::unexpected(relationError("anchor_key_mismatch", "/TIM1/anchors",
                                                  "a saved Stop has no matching anchor"));
        }
        if (it->at.value != stop.start.value) {
            return core::unexpected(
                relationError("tick_mismatch", "/TIM1/stops",
                              "a saved Stop start and its anchor disagree on the tick"));
        }
    }

    //  The contract's Stop relation: every earlier beat is at or before S, every later beat is at
    //  or after E.
    for (const auto& stop : boundaries.stops) {
        for (const auto& anchor : boundaries.anchors) {
            if (anchor.beat < stop.beat && anchor.at.value > stop.start.value) {
                return core::unexpected(
                    relationError("stop_bounds", "/TIM1/anchors",
                                  "an earlier anchor tick is after the Stop start"));
            }
            if (stop.beat < anchor.beat && anchor.at.value < stop.end.value) {
                return core::unexpected(relationError(
                    "stop_bounds", "/TIM1/anchors", "a later anchor tick is before the Stop end"));
            }
        }
    }
    return {};
}

} // namespace

auto roundMicroseconds(double microseconds) -> core::Result<TimeUs> {
    if (!std::isfinite(microseconds)) {
        return core::unexpected(rangeError("the microsecond value is not finite"));
    }
    //  This predicate reads the value, never the current floating-point rounding mode.
    const double floorValue = std::floor(microseconds);
    const double fraction = microseconds - floorValue;
    double rounded = floorValue;
    if (fraction > 0.5) {
        rounded = floorValue + 1.0;
    } else if (fraction == 0.5) {
        rounded = (std::fmod(floorValue, 2.0) == 0.0) ? floorValue : floorValue + 1.0;
    }
    const double limit = std::ldexp(1.0, 63);
    if (!(rounded >= -limit && rounded < limit)) {
        return core::unexpected(rangeError("the rounded microsecond value leaves signed 64-bit"));
    }
    return TimeUs{static_cast<std::int64_t>(rounded)};
}

TimingMap::TimingMap(std::shared_ptr<const detail::TimingMapStorage> storage) noexcept
    : storage_(std::move(storage)) {}

auto TimingMap::compile(TimingSourceView source, std::span<const RationalBeat> requiredAnchors)
    -> core::Result<TimingMap> {
    auto storage = std::make_shared<Storage>();
    if (auto normalized = normalizeSource(source, *storage); !normalized) {
        return core::unexpected(std::move(normalized.error()));
    }
    if (auto integrated = buildIntegration(*storage); !integrated) {
        return core::unexpected(std::move(integrated.error()));
    }
    if (auto boundaries = buildBoundaries(*storage, requiredAnchors); !boundaries) {
        return core::unexpected(std::move(boundaries.error()));
    }
    return TimingMap{std::move(storage)};
}

auto TimingMap::fromCompiled(TimingSourceView source, TimingBoundariesView boundaries,
                             std::span<const RationalBeat> requiredAnchors)
    -> core::Result<TimingMap> {
    auto storage = std::make_shared<Storage>();
    if (auto normalized = normalizeSource(source, *storage); !normalized) {
        return core::unexpected(std::move(normalized.error()));
    }
    if (auto integrated = buildIntegration(*storage); !integrated) {
        return core::unexpected(std::move(integrated.error()));
    }
    if (auto valid = validateBoundaries(*storage, boundaries, requiredAnchors); !valid) {
        return core::unexpected(std::move(valid.error()));
    }
    storage->tempoBoundaries.assign(boundaries.tempos.begin(), boundaries.tempos.end());
    storage->stopBoundaries.assign(boundaries.stops.begin(), boundaries.stops.end());
    storage->anchors.assign(boundaries.anchors.begin(), boundaries.anchors.end());
    return TimingMap{std::move(storage)};
}

auto TimingMap::boundaries() const noexcept -> TimingBoundariesView {
    return TimingBoundariesView{storage_->tempoBoundaries, storage_->stopBoundaries,
                                storage_->anchors};
}

auto TimingMap::matchesSource(TimingSourceView source) const noexcept -> bool {
    if (!(source.defaultBpm == storage_->defaultBpm)) {
        return false;
    }
    if (source.tempos.size() != storage_->tempos.size() ||
        source.stops.size() != storage_->stops.size()) {
        return false;
    }
    for (std::size_t index = 0; index < source.tempos.size(); ++index) {
        const auto& left = source.tempos[index];
        const auto& right = storage_->tempos[index];
        if (!(left.startBeat == right.startBeat) || !(left.durationBeats == right.durationBeats) ||
            !(left.startBpm == right.startBpm) || !(left.endBpm == right.endBpm) ||
            !(left.startSlope == right.startSlope) || !(left.endSlope == right.endSlope)) {
            return false;
        }
    }
    for (std::size_t index = 0; index < source.stops.size(); ++index) {
        const auto& left = source.stops[index];
        const auto& right = storage_->stops[index];
        if (!(left.beat == right.beat) || !(left.durationMs == right.durationMs)) {
            return false;
        }
    }
    return true;
}

auto TimingMap::beatToTime(RationalBeat beat) const -> core::Result<TimeUs> {
    if (const auto* anchor = findAnchor(*storage_, beat); anchor != nullptr) {
        return anchor->at;
    }
    auto local = localTimeUs(*storage_, beat);
    if (!local) {
        return core::unexpected(std::move(local.error()));
    }
    return roundMicroseconds(*local);
}

auto TimingMap::timeToBeat(TimeUs time) const -> core::Result<BeatSample> {
    //  A Stop is authoritative in the reverse direction: `[S, E)` keeps the exact Stop beat and
    //  `E` leaves the Stop at the same beat.
    for (const auto& stop : storage_->stopBoundaries) {
        if (time.value >= stop.start.value && time.value < stop.end.value) {
            const double start = static_cast<double>(stop.start.value);
            const double end = static_cast<double>(stop.end.value);
            double progress = (static_cast<double>(time.value) - start) / (end - start);
            const double largestBelowOne = std::nextafter(1.0, 0.0);
            if (!(progress >= 0.0) || progress > largestBelowOne) {
                progress = std::max(0.0, std::min(progress, largestBelowOne));
            }
            return BeatSample{stop.beat.toDouble(), stop.beat, true, progress};
        }
        if (time.value == stop.end.value) {
            return BeatSample{stop.beat.toDouble(), stop.beat, false, 0.0};
        }
    }

    if (storage_->stopBoundaries.size() > 0) {
        //  A tick before the first Stop and after the last one needs no adjustment; in between,
        //  remove the duration of every Stop that has already ended.
        double completed = 0.0;
        for (const auto& stop : storage_->stopBoundaries) {
            if (time.value >= stop.end.value) {
                completed += static_cast<double>(tickDistance(stop.end.value, stop.start.value));
            }
        }
        const double tempoUs =
            static_cast<double>(time.value) - completed + storage_->tempoUsAtZero;
        if (!std::isfinite(tempoUs)) {
            return core::unexpected(rangeError("the reverse conversion left the continuous range"));
        }
        double beat = 0.0;
        if (storage_->compiledTempos.empty()) {
            beat = tempoUs * storage_->defaultBpm / microsecondsPerMinute;
        } else {
            const auto next =
                std::upper_bound(storage_->compiledTempos.begin(), storage_->compiledTempos.end(),
                                 tempoUs, [](double value, const Storage::CompiledTempo& event) {
                                     return value < event.startUs;
                                 });
            if (next == storage_->compiledTempos.begin()) {
                const auto& first = storage_->compiledTempos.front();
                beat = first.startBeat +
                       (tempoUs - first.startUs) * storage_->defaultBpm / microsecondsPerMinute;
            } else {
                const auto& event = *(next - 1);
                if (event.endBeat > event.startBeat && tempoUs < event.endUs) {
                    double lower = event.startBeat;
                    double upper = event.endBeat;
                    for (std::size_t iteration = 0; iteration < inverseIterations; ++iteration) {
                        const double midpoint = (lower + upper) * 0.5;
                        if (tempoTimeUs(*storage_, midpoint) < tempoUs) {
                            lower = midpoint;
                        } else {
                            upper = midpoint;
                        }
                    }
                    beat = (lower + upper) * 0.5;
                } else {
                    beat = event.endBeat +
                           (tempoUs - event.endUs) * event.endBpm / microsecondsPerMinute;
                }
            }
        }
        if (!std::isfinite(beat)) {
            return core::unexpected(rangeError("the reverse conversion left the continuous range"));
        }
        return BeatSample{beat, std::nullopt, false, 0.0};
    }

    const double tempoUs = static_cast<double>(time.value) + storage_->tempoUsAtZero;
    if (!std::isfinite(tempoUs)) {
        return core::unexpected(rangeError("the reverse conversion left the continuous range"));
    }
    double beat = 0.0;
    if (storage_->compiledTempos.empty()) {
        beat = tempoUs * storage_->defaultBpm / microsecondsPerMinute;
    } else {
        const auto next =
            std::upper_bound(storage_->compiledTempos.begin(), storage_->compiledTempos.end(),
                             tempoUs, [](double value, const Storage::CompiledTempo& event) {
                                 return value < event.startUs;
                             });
        if (next == storage_->compiledTempos.begin()) {
            const auto& first = storage_->compiledTempos.front();
            beat = first.startBeat +
                   (tempoUs - first.startUs) * storage_->defaultBpm / microsecondsPerMinute;
        } else {
            const auto& event = *(next - 1);
            if (event.endBeat > event.startBeat && tempoUs < event.endUs) {
                double lower = event.startBeat;
                double upper = event.endBeat;
                for (std::size_t iteration = 0; iteration < inverseIterations; ++iteration) {
                    const double midpoint = (lower + upper) * 0.5;
                    if (tempoTimeUs(*storage_, midpoint) < tempoUs) {
                        lower = midpoint;
                    } else {
                        upper = midpoint;
                    }
                }
                beat = (lower + upper) * 0.5;
            } else {
                beat =
                    event.endBeat + (tempoUs - event.endUs) * event.endBpm / microsecondsPerMinute;
            }
        }
    }
    if (!std::isfinite(beat)) {
        return core::unexpected(rangeError("the reverse conversion left the continuous range"));
    }
    return BeatSample{beat, std::nullopt, false, 0.0};
}

} // namespace cuexis::timing
