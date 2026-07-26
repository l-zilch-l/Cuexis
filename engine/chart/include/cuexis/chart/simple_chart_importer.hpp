#pragma once

//  SimpleChartImporter - deterministic conversion from scheme B (cuexis.chart.simple) to
//  scheme A
//  Scheme B uses object maps, readable IDs, string beats and shorthand references
//  Conversion results never depend on JSON object key order, file paths, the system time
//  zone or randomness
//  Object UUID = UUIDv5(chartId, "object:" + readableId), generated deterministically

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>

#include <string_view>

namespace cuexis::chart {

class SimpleChartImporter final {
  public:
    [[nodiscard]] static auto import(std::string_view jsonText, const ChartLimits& limits = {})
        -> ChartDocumentResult;
};

} // namespace cuexis::chart
