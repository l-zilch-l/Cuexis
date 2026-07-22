#pragma once

//  ChartLoader — 谱面加载入口，按显式 format 字段路由方案 A/B
//  cuexis.chart -> CanonicalChartLoader；cuexis.chart.simple -> SimpleChartImporter
//  未知 format 返回 UnsupportedFormat 诊断，不根据字段猜测格式

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
