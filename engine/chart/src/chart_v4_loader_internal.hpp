#pragma once

#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/json/value.hpp>

namespace cuexis::chart::detail {

struct ParsedChartInput final {
    json::Value value;
};

[[nodiscard]] auto loadV4Value(const json::Value& parsed, const ChartLimits& limits = {})
    -> ChartV4SourceResult;

} // namespace cuexis::chart::detail
