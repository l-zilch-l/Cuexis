#pragma once

//  CanonicalChartLoader - typed reader for scheme A (cuexis.chart) canonical charts
//  Uses the Cuexis JSON Value and typed Reader for structural and semantic validation
//  The loader does not currently invoke the JSON Schema validator (the Schema artifact is
//  tested separately)
//  The authority on structure is the typed Reader plus the Chart semantic validation code

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>

#include <string_view>

namespace cuexis::chart {

class CanonicalChartLoader final {
  public:
    [[nodiscard]] static auto load(std::string_view jsonText, const ChartLimits& limits = {})
        -> ChartDocumentResult;
};

} // namespace cuexis::chart
