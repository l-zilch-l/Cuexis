#pragma once

//  Build-only compiled Chart timing.
//
//  This header is owned by `cuexis_chart_codec` and only reachable through that target's
//  BUILD_INTERFACE internal root. It carries the shared timing map and the once-quantified chart
//  offset that the later prepared artifact owns; the old `chart::TimingMap` and its offset stay
//  with the base Chart target.

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/timing/timing_map.hpp>

#include <cstdint>
#include <span>

namespace cuexis::chart {

struct CompiledChartTiming final {
    timing::TimingMap map;
    std::int64_t chartOffsetUs{};
};

//  Compiles one old Chart timing document into the shared map. `requiredAnchors` are the exact
//  beats the caller collected from declarations that take part in integer comparison; the map
//  adds its own tempo/Stop/Beat-0 keys. The chart offset is quantised exactly once from
//  `offsetMs * 1000`; it is not part of the map and does not move any anchor.
[[nodiscard]] auto compileChartTiming(const ChartTiming& timing,
                                      std::span<const timing::RationalBeat> requiredAnchors)
    -> core::Result<CompiledChartTiming>;

} // namespace cuexis::chart
