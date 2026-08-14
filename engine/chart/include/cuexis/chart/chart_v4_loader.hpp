#pragma once

#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>

#include <string_view>

namespace cuexis::chart {

class ChartV4Loader final {
  public:
    [[nodiscard]] static auto isV4(std::string_view jsonText, const ChartLimits& limits = {})
        -> bool;
    [[nodiscard]] static auto load(std::string_view jsonText, const ChartLimits& limits = {})
        -> ChartV4SourceResult;
};

} // namespace cuexis::chart
