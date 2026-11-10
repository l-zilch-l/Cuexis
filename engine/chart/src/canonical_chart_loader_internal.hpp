#pragma once

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/json/value.hpp>

namespace cuexis::chart::detail {

// Loads an already-owned parsed canonical chart value without reparsing serialized text.
// The value is only read, never consumed: the caller keeps ownership so it can still derive
// canonical bytes from the same tree instead of parsing the document a second time.
[[nodiscard]] auto loadCanonicalValue(const json::Value& parsed, const ChartLimits& limits = {})
    -> ChartDocumentResult;

} // namespace cuexis::chart::detail
