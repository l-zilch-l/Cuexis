#include <cuexis/chart/compiled_chart_timing.hpp>

#include <cuexis/chart/timing_source_adapter.hpp>

#include <utility>

namespace cuexis::chart {

auto compileChartTiming(const ChartTiming& timing,
                        std::span<const timing::RationalBeat> requiredAnchors)
    -> core::Result<CompiledChartTiming> {
    auto offset = timing::roundMicroseconds(timing.offsetMs * 1000.0);
    if (!offset) {
        return core::unexpected(std::move(offset.error()));
    }
    auto source = makeTimingSource(timing);
    if (!source) {
        return core::unexpected(std::move(source.error()));
    }
    auto map = timing::TimingMap::compile(source->view(), requiredAnchors);
    if (!map) {
        return core::unexpected(std::move(map.error()));
    }
    return CompiledChartTiming{std::move(*map), offset->value};
}

} // namespace cuexis::chart
