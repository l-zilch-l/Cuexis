#pragma once

#include <cuexis/chart/limits.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/json/value.hpp>

#include <string>

namespace cuexis::chart::detail {

// Canonicalizes an already-parsed v4 source value without reparsing its serialized text.
[[nodiscard]] auto writeV4Value(json::Value value, const ChartLimits& limits = {})
    -> core::Result<std::string>;

// Canonicalizes an already-parsed source value in the legacy normal form: the same bytes
// ChartWriter::writeCanonicalJson produces for the text this value was parsed from.
[[nodiscard]] auto writeCanonicalValue(json::Value value, const ChartLimits& limits = {})
    -> core::Result<std::string>;

} // namespace cuexis::chart::detail
