#pragma once

//  JSON Schema 验证适配器 — 基于 nlohmann/json-schema-validator
//  无效 Schema 本身作为操作错误返回；实例违规追加到 diagnostics
//  当前 loader 尚未调用此 adapter，但 Schema artifact 和独立测试已建立

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/json/value.hpp>

#include <string_view>

namespace cuexis::json {

// 无效 Schema 返回操作错误；实例违规追加到 diagnostics
[[nodiscard]] core::Result<void> validateAgainstSchema(const Value& instance, const Value& schema,
                                                       core::Diagnostics& diagnostics,
                                                       std::string_view rootFieldPath = "$");

} // namespace cuexis::json
