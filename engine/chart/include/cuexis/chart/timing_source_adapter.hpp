#pragma once

//  Chart timing source adapter.
//
//  The old Chart timing document is the authority for the v1-v4 loaders and keeps its own
//  `chart::TimingMap` with its old error codes and offset semantics. This adapter only projects
//  the same declared values into the shared `cuexis::timing` source view; it does not replace the
//  old reader path, does not apply the audio offset and does not decide a discrete tick.

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/timing/timing_map.hpp>

#include <vector>

namespace cuexis::chart {

//  The owning source view. `TimingSourceView` borrows spans, so the caller keeps this value alive
//  for the whole `TimingMap` compile call.
struct TimingSourceData final {
    double defaultBpm{120.0};
    std::vector<timing::TempoEvent> tempoEvents;
    std::vector<timing::Stop> stops;

    [[nodiscard]] auto view() const noexcept -> timing::TimingSourceView;
};

//  Projects the old Chart timing document into the shared timing source values. The old
//  `chart::RationalBeat` values are already normalized, so the conversion is a field copy through
//  `timing::RationalBeat::create`.
[[nodiscard]] auto makeTimingSource(const ChartTiming& timing) -> core::Result<TimingSourceData>;

} // namespace cuexis::chart
