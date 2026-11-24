//  Independent pure-codec consumer.
//
//  It is configured as its own CMake project and links only Core and the Chart codec. The
//  build-only internal Chart timing root arrives through the codec's PUBLIC BUILD_INTERFACE, so
//  moving `compiled_chart_timing.hpp` back into the base Chart target, or hiding it behind the
//  tools root, breaks this consumer instead of staying hidden inside the tree.

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/compiled_chart_timing.hpp>
#include <cuexis/timing/timing_map.hpp>

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>

int main() {
    using Compiled = cuexis::chart::CompiledChartTiming;
    static_assert(
        std::is_same_v<decltype(std::declval<const Compiled&>().map), cuexis::timing::TimingMap>);
    static_assert(
        std::is_same_v<decltype(std::declval<const Compiled&>().chartOffsetUs), std::int64_t>);

    cuexis::chart::ChartTiming timing;
    timing.offsetMs = 12.5;
    auto compiled = cuexis::chart::compileChartTiming(timing, {});
    if (!compiled) {
        std::cerr << "chart timing compile failed: " << compiled.error().message() << '\n';
        return 1;
    }
    if (compiled->chartOffsetUs != 12500) {
        std::cerr << "chart offset was not quantised once: " << compiled->chartOffsetUs << '\n';
        return 2;
    }
    auto atZero = compiled->map.beatToTime(cuexis::timing::RationalBeat::zero());
    if (!atZero || atZero->value != 0) {
        std::cerr << "beat 0 did not resolve to the zero tick\n";
        return 3;
    }
    std::cout << "chart codec consumer: CompiledChartTiming compiled, offset 12500 us\n";
    return 0;
}
