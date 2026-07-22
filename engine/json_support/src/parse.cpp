//  JSON 解析实现 — 使用 nlohmann::json 的 SAX 回调接口
//  ParseObserver 在解析过程中检测重复键、嵌套深度和字符串大小限制
//  所有 nlohmann 异常在模块边界捕获并转换为 cuexis::core::Error

#include <cuexis/json/parse.hpp>

#include "json_conversion.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <exception>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::json {
namespace {

class ParseLimitExceeded final : public std::runtime_error {
  public:
    explicit ParseLimitExceeded(std::string reason) : std::runtime_error(std::move(reason)) {}
};

enum class JsonStringKind {
    ObjectKey,
    StringValue,
};

class StringLimitExceeded final : public std::runtime_error {
  public:
    StringLimitExceeded(JsonStringKind kind, std::size_t actualBytes)
        : std::runtime_error(kind == JsonStringKind::ObjectKey
                                 ? "JSON object key exceeds the configured string byte limit"
                                 : "JSON string value exceeds the configured string byte limit"),
          kind_(kind), actualBytes_(actualBytes) {}

    [[nodiscard]] std::string_view kind() const noexcept {
        return kind_ == JsonStringKind::ObjectKey ? "object_key" : "string_value";
    }

    [[nodiscard]] std::size_t actualBytes() const noexcept {
        return actualBytes_;
    }

  private:
    JsonStringKind kind_;
    std::size_t actualBytes_;
};

class ParseObserver final {
  public:
    ParseObserver(std::size_t maxDepth, std::size_t maxStringBytes)
        : maxDepth_(maxDepth), maxStringBytes_(maxStringBytes) {}

    bool operator()(int, nlohmann::json::parse_event_t event, nlohmann::json& parsed) {
        switch (event) {
        case nlohmann::json::parse_event_t::object_start:
            beginContainer();
            objectKeys_.emplace_back();
            break;
        case nlohmann::json::parse_event_t::array_start:
            beginContainer();
            break;
        case nlohmann::json::parse_event_t::object_end:
            objectKeys_.pop_back();
            endContainer();
            break;
        case nlohmann::json::parse_event_t::array_end:
            endContainer();
            break;
        case nlohmann::json::parse_event_t::key: {
            const auto& key = parsed.get_ref<const std::string&>();
            enforceStringLimit(key, JsonStringKind::ObjectKey);
            if (!objectKeys_.back().insert(key).second && duplicateKey_.empty()) {
                duplicateKey_ = key;
            }
            break;
        }
        case nlohmann::json::parse_event_t::value:
            if (parsed.is_string()) {
                enforceStringLimit(parsed.get_ref<const std::string&>(),
                                   JsonStringKind::StringValue);
            }
            break;
        default:
            break;
        }
        return true;
    }

    [[nodiscard]] std::string_view duplicateKey() const noexcept {
        return duplicateKey_;
    }

  private:
    void beginContainer() {
        if (containerDepth_ >= maxDepth_) {
            throw ParseLimitExceeded{"JSON nesting depth exceeds the configured limit"};
        }
        ++containerDepth_;
    }

    void endContainer() noexcept {
        --containerDepth_;
    }

    void enforceStringLimit(std::string_view value, JsonStringKind kind) const {
        if (value.size() > maxStringBytes_) {
            throw StringLimitExceeded{kind, value.size()};
        }
    }

    std::size_t maxDepth_;
    std::size_t maxStringBytes_;
    std::size_t containerDepth_{0};
    std::vector<std::set<std::string, std::less<>>> objectKeys_;
    std::string duplicateKey_;
};

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

core::Result<Value> parse(std::string_view text, ParseLimits limits) {
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

    try {
        ParseObserver observer{limits.maxDepth, limits.maxStringBytes};
        const auto parsed =
            nlohmann::json::parse(text.begin(), text.end(), std::ref(observer), true, false);
        if (!observer.duplicateKey().empty()) {
            return core::unexpected(
                core::Error{"json.parse.duplicate_key", "JSON object keys must be unique"}
                    .withContext("key", std::string{observer.duplicateKey()}));
        }
        return detail::fromNlohmann(parsed);
    } catch (const ParseLimitExceeded& exception) {
        return core::unexpected(core::Error{"json.parse.depth_limit", exception.what()}.withContext(
            "max_depth", std::to_string(limits.maxDepth)));
    } catch (const StringLimitExceeded& exception) {
        return core::unexpected(
            core::Error{"json.parse.string_limit", exception.what()}
                .withContext("string_kind", std::string{exception.kind()})
                .withContext("actual_bytes", std::to_string(exception.actualBytes()))
                .withContext("max_string_bytes", std::to_string(limits.maxStringBytes)));
    } catch (const nlohmann::json::parse_error& exception) {
        return core::unexpected(
            core::Error{"json.parse.syntax_error", "JSON syntax is invalid"}.withContext(
                "byte_offset", std::to_string(exception.byte)));
    } catch (const std::exception& exception) {
        return core::unexpected(core::Error{"json.parse.failed", "JSON parsing failed"}.withContext(
            "exception", exception.what()));
    }
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
