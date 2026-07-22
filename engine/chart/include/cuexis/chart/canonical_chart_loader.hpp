#pragma once

//  CanonicalChartLoader — 方案 A (cuexis.chart) 规范谱面 typed-reader
//  使用 Cuexis JSON Value 和 typed Reader 读取结构与语义校验
//  当前 loader 不调用 JSON Schema validator（Schema artifact 独立测试）
//  结构权威是 typed Reader + Chart 语义校验代码

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
