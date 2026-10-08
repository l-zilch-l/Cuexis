#pragma once

//  JSON parsing and serialization - wraps nlohmann::json and enforces depth/string budgets
//  through ParseObserver
//  ParseLimits is supplied by the caller (Chart/Project) because budgets differ per format
//  parse checks for duplicate keys, nesting depth and the string byte ceiling

#include <cuexis/core/result.hpp>
#include <cuexis/json/value.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace cuexis::json {

struct ParseLimits {
    std::size_t maxBytes;
    std::size_t maxDepth;
    std::size_t maxStringBytes;
};

// Explicit owning-tree bounds. No production defaults are selected here.
struct ValueParseLimits {
    ParseLimits text;
    std::size_t maxValues;
    std::size_t maxContainerElements;
};

enum class SaxEventKind : std::uint8_t {
    Null,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Number,
    String,
    Key,
    ObjectStart,
    ObjectEnd,
    ArrayStart,
    ArrayEnd,
};

// Text is borrowed only for the onEvent call. No JSON DOM or third-party types escape.
struct SaxEvent final {
    SaxEventKind kind;
    bool boolean{};
    std::int64_t signedInteger{};
    std::uint64_t unsignedInteger{};
    double number{};
    std::string_view text{};
};

class ISaxEventSink {
  public:
    virtual ~ISaxEventSink() = default;
    [[nodiscard]] virtual auto onEvent(const SaxEvent&) -> core::Result<void> = 0;
};

// The sink constructs a temporary typed DTO and publishes it only after this succeeds.
[[nodiscard]] core::Result<void> parseEvents(std::string_view text, ValueParseLimits limits,
                                             ISaxEventSink& sink);

// SAX directly builds Cuexis-owned Values, rejecting duplicate keys before their values.
[[nodiscard]] core::Result<Value> parseBounded(std::string_view text, ValueParseLimits limits);

enum class SerializeStyle {
    Compact,
    Pretty,
};

// The budget comes from the owning format, because Chart and configuration budgets differ
[[nodiscard]] core::Result<Value> parse(std::string_view text, ParseLimits limits);
[[nodiscard]] core::Result<std::string> serialize(const Value& value,
                                                  SerializeStyle style = SerializeStyle::Compact);

} // namespace cuexis::json
