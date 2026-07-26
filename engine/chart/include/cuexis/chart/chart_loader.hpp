#pragma once

//  ChartLoader - chart loading entry point; routes to scheme A/B by explicit format field
//  cuexis.chart -> CanonicalChartLoader; cuexis.chart.simple -> SimpleChartImporter
//  An unknown format yields an UnsupportedFormat diagnostic; the format is never guessed
//  from the present fields

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>

#include <string_view>

namespace cuexis::chart {

class ChartLoader final {
  public:
    [[nodiscard]] static auto load(std::string_view jsonText, const ChartLimits& limits = {})
        -> ChartDocumentResult;
};

} // namespace cuexis::chart
