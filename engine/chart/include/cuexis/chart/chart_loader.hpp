#pragma once

//  ChartLoader - canonical chart loading entry point selected by the explicit format field
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
