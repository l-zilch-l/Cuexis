#pragma once

//  JSON 解析与序列化 — 封装 nlohmann::json，通过 ParseObserver 执行深度/字符串预算控制
//  ParseLimits 由调用方（Chart/Project）提供，因为不同格式的预算不同
//  parse 检查重复键、嵌套深度和字符串字节上限

#include <cuexis/core/result.hpp>
#include <cuexis/json/value.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace cuexis::json {

struct ParseLimits {
    std::size_t maxBytes;
    std::size_t maxDepth;
    std::size_t maxStringBytes;
};

enum class SerializeStyle {
    Compact,
    Pretty,
};

// 预算由所属格式提供，因为 Chart 和配置的预算不同
[[nodiscard]] core::Result<Value> parse(std::string_view text, ParseLimits limits);
[[nodiscard]] core::Result<std::string> serialize(const Value& value,
                                                  SerializeStyle style = SerializeStyle::Compact);

} // namespace cuexis::json
