#pragma once

//  JSON parsing and serialization - wraps nlohmann::json and enforces depth/string budgets
//  through ParseObserver
//  ParseLimits is supplied by the caller (Chart/Project) because budgets differ per format
//  parse checks for duplicate keys, nesting depth and the string byte ceiling

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

// The budget comes from the owning format, because Chart and configuration budgets differ
[[nodiscard]] core::Result<Value> parse(std::string_view text, ParseLimits limits);
[[nodiscard]] core::Result<std::string> serialize(const Value& value,
                                                  SerializeStyle style = SerializeStyle::Compact);

} // namespace cuexis::json
