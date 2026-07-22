#pragma once

//  内部转换工具 — Cuexis Value 与 nlohmann::json 之间的双向转换
//  仅由 parse.cpp 和 schema.cpp 使用，不暴露到公共接口

#include <cuexis/json/value.hpp>

#include <nlohmann/json.hpp>

namespace cuexis::json::detail {

[[nodiscard]] Value fromNlohmann(const nlohmann::json& value);
[[nodiscard]] nlohmann::json toNlohmann(const Value& value);

} // namespace cuexis::json::detail
