#pragma once

//  Build-only v5 author timing adapter.
//
//  The v5 author document keeps the single Chart timing authority in `ChartTimingSource` (the old
//  v4 timing shape). This adapter projects those typed source values into the old Chart timing
//  document and then compiles the shared `CompiledChartTiming`; the source values, the authored
//  offset and the discrete anchors are interpreted once, in one place. The v5 loader and the
//  candidate Playback source path stay outside the base Chart include root.

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/chart_v5_document.hpp>
#include <cuexis/chart/compiled_chart_timing.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/timing/timing_map.hpp>

#include <span>

namespace cuexis::chart {

//  Projects the typed v5 timing block into the old Chart timing document. Beats are carried as
//  exact rationals; the projection rejects nothing and leaves every range check to the shared
//  timing compile.
[[nodiscard]] auto makeChartTiming(const ChartTimingSource& timing) -> core::Result<ChartTiming>;

//  Compiles the shared timing map and the once-quantised chart offset from the typed v5 source.
[[nodiscard]] auto compileChartTiming(const ChartTimingSource& timing,
                                      std::span<const timing::RationalBeat> requiredAnchors)
    -> core::Result<CompiledChartTiming>;

} // namespace cuexis::chart
