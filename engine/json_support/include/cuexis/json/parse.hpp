#pragma once

//  JSON parsing and serialization - wraps nlohmann::json and enforces depth/string budgets
//  through ParseObserver
//  ParseLimits is supplied by the caller (Chart/Project) because budgets differ per format
//  parse checks for duplicate keys, nesting depth and the string byte ceiling

#include <cuexis/core/result.hpp>
#include <cuexis/json/value.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
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

// Reads the integer value of one top-level member of a JSON object without building a value tree.
// Scanning stops at that value, which is the whole point: a caller that only needs to know the
// format version of a large document pays for the prefix in front of the member instead of for the
// complete document.
//
// This is a hint scanner and never validates the document. It returns nullopt when the text does
// not start with a JSON object, when the member is absent from the part that was scanned, when its
// value is not an integer, when the text ends early, or when a scan limit is reached. Callers must
// still parse the document to validate it and to reject it.
[[nodiscard]] auto scanTopLevelInteger(std::string_view text, std::string_view member,
                                       ParseLimits limits) -> std::optional<std::int64_t>;

} // namespace cuexis::json
