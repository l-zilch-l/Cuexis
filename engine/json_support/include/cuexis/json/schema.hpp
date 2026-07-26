#pragma once

//  JSON Schema validation adapter - built on nlohmann/json-schema-validator
//  An invalid schema itself is returned as an operation error; instance violations are
//  appended to diagnostics
//  No loader calls this adapter yet, but the Schema artifact and its standalone tests exist

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/json/value.hpp>

#include <string_view>

namespace cuexis::json {

// An invalid schema returns an operation error; instance violations are appended to
// diagnostics
[[nodiscard]] core::Result<void> validateAgainstSchema(const Value& instance, const Value& schema,
                                                       core::Diagnostics& diagnostics,
                                                       std::string_view rootFieldPath = "$");

} // namespace cuexis::json
