#pragma once

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/json/value.hpp>

namespace cuexis::chart::detail {

// Loads an already-owned parsed canonical chart value without reparsing serialized text.
// The value is consumed by the typed reader and never escapes the chart implementation.
[[nodiscard]] auto loadCanonicalValue(json::Value parsed, const ChartLimits& limits = {})
    -> ChartDocumentResult;

} // namespace cuexis::chart::detail
