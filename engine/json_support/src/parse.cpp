//  JSON parsing and serialization implementation.
//  parse() is one SAX pass that builds the owning Value directly: nesting depth, string byte
//  ceilings, container budgets and duplicate-key rejection are all enforced by the reader in
//  bounded_parse.cpp, so no third-party DOM is built and no value tree is copied afterwards.
//  serialize() still goes through the third-party DOM, which owns the escaping rules the canonical
//  identity depends on.
//  All third-party exceptions are caught at the module boundary and translated to
//  cuexis::core::Error.

#include <cuexis/json/parse.hpp>

#include "json_conversion.hpp"
#include "parse_probe_internal.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <exception>
#include <limits>
#include <string>
#include <utility>

namespace cuexis::json {
namespace {

#if defined(CUEXIS_JSON_PARSE_TEST_PROBE)
thread_local std::size_t* activeParseCounter{};

void recordParseCall() noexcept {
    if (activeParseCounter != nullptr) {
        ++*activeParseCounter;
    }
}
#else
void recordParseCall() noexcept {}
#endif

} // namespace

namespace detail {

Value fromNlohmann(const nlohmann::json& value) {
    if (value.is_null()) {
        return Value{};
    }
    if (value.is_boolean()) {
        return Value{value.get<bool>()};
    }
    if (value.is_number_unsigned()) {
        return Value{value.get<std::uint64_t>()};
    }
    if (value.is_number_integer()) {
        return Value{value.get<std::int64_t>()};
    }
    if (value.is_number_float()) {
        return Value{value.get<double>()};
    }
    if (value.is_string()) {
        return Value{value.get<std::string>()};
    }
    if (value.is_array()) {
        Value::Array result;
        result.reserve(value.size());
        for (const auto& item : value) {
            result.push_back(fromNlohmann(item));
        }
        return Value{std::move(result)};
    }

    Value::Object result;
    for (auto iterator = value.begin(); iterator != value.end(); ++iterator) {
        result.emplace(iterator.key(), fromNlohmann(iterator.value()));
    }
    return Value{std::move(result)};
}

nlohmann::json toNlohmann(const Value& value) {
    switch (value.type()) {
    case ValueType::Null:
        return nullptr;
    case ValueType::Boolean:
        return *value.boolean();
    case ValueType::SignedInteger:
        return *value.signedInteger();
    case ValueType::UnsignedInteger:
        return *value.unsignedInteger();
    case ValueType::Number:
        return *value.number();
    case ValueType::String:
        return *value.string();
    case ValueType::Array: {
        auto result = nlohmann::json::array();
        for (const auto& item : *value.array()) {
            result.push_back(toNlohmann(item));
        }
        return result;
    }
    case ValueType::Object: {
        auto result = nlohmann::json::object();
        for (const auto& [key, item] : *value.object()) {
            result[key] = toNlohmann(item);
        }
        return result;
    }
    }
    return nullptr;
}

} // namespace detail

#if defined(CUEXIS_JSON_PARSE_TEST_PROBE)
detail::ScopedParseCounter::ScopedParseCounter() noexcept : previous_(activeParseCounter) {
    activeParseCounter = &count_;
}

detail::ScopedParseCounter::~ScopedParseCounter() {
    activeParseCounter = previous_;
}
#else
detail::ScopedParseCounter::ScopedParseCounter() noexcept = default;

detail::ScopedParseCounter::~ScopedParseCounter() = default;
#endif

void detail::ScopedParseCounter::reset() noexcept {
    count_ = 0;
}

auto detail::ScopedParseCounter::count() const noexcept -> std::size_t {
    return count_;
}

core::Result<Value> parse(std::string_view text, ParseLimits limits) {
    recordParseCall();
    if (limits.maxBytes == 0 || limits.maxDepth == 0 || limits.maxStringBytes == 0) {
        return core::unexpected(core::Error{"json.parse.invalid_limits",
                                            "JSON parse limits must be greater than zero"});
    }
    if (text.size() > limits.maxBytes) {
        return core::unexpected(
            core::Error{"json.parse.size_limit", "JSON input exceeds the configured byte limit"}
                .withContext("actual_bytes", std::to_string(text.size()))
                .withContext("max_bytes", std::to_string(limits.maxBytes)));
    }

    // The owning-value budgets are the reader's own ceilings. A caller that only states text
    // budgets has not asked for a value budget, so the reader's are left unbounded rather than
    // invented here; the text budgets it does state are enforced by that same reader.
    return parseBounded(text, ValueParseLimits{limits, std::numeric_limits<std::size_t>::max(),
                                               std::numeric_limits<std::size_t>::max()});
}

core::Result<std::string> serialize(const Value& value, SerializeStyle style) {
    try {
        const auto converted = detail::toNlohmann(value);
        return converted.dump(style == SerializeStyle::Pretty ? 2 : -1);
    } catch (const std::exception& exception) {
        return core::unexpected(
            core::Error{"json.serialize.failed", "JSON serialization failed"}.withContext(
                "exception", exception.what()));
    }
}

} // namespace cuexis::json
