#pragma once

//  SimpleChartImporter — 方案 B (cuexis.chart.simple) 到方案 A 的确定性转换
//  方案 B 使用对象映射、可读 ID、字符串 Beat 和简写引用
//  转换结果不依赖 JSON 对象键顺序、文件路径、系统时区或随机数
//  Object UUID = UUIDv5(chartId, "object:" + readableId) 确定性生成

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
