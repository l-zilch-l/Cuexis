//  Cuexis JSON Value 实现 — variant-based 类型化 JSON 值
//  Object 使用 std::map<std::string, Value, std::less<>> 以支持异构查找（string_view 查找）

#include <cuexis/json/value.hpp>

#include <utility>

namespace cuexis::json {

std::string_view valueTypeName(ValueType type) noexcept {
    switch (type) {
    case ValueType::Null:
        return "null";
    case ValueType::Boolean:
        return "boolean";
    case ValueType::SignedInteger:
        return "signed_integer";
    case ValueType::UnsignedInteger:
        return "unsigned_integer";
    case ValueType::Number:
        return "number";
    case ValueType::String:
        return "string";
    case ValueType::Array:
        return "array";
    case ValueType::Object:
        return "object";
    }
    return "unknown";
}

Value::Value() noexcept : storage_(nullptr) {}

Value::Value(std::nullptr_t) noexcept : storage_(nullptr) {}

Value::Value(bool value) noexcept : storage_(value) {}

Value::Value(std::int64_t value) noexcept : storage_(value) {}

Value::Value(std::uint64_t value) noexcept : storage_(value) {}

Value::Value(double value) noexcept : storage_(value) {}

Value::Value(std::string value) : storage_(std::move(value)) {}

Value::Value(const char* value) : storage_(std::string{value}) {}

Value::Value(Array value) : storage_(std::move(value)) {}

Value::Value(Object value) : storage_(std::move(value)) {}

ValueType Value::type() const noexcept {
    return static_cast<ValueType>(storage_.index());
}

bool Value::isNull() const noexcept {
    return std::holds_alternative<std::nullptr_t>(storage_);
}

const bool* Value::boolean() const noexcept {
    return std::get_if<bool>(&storage_);
}

const std::int64_t* Value::signedInteger() const noexcept {
    return std::get_if<std::int64_t>(&storage_);
}

const std::uint64_t* Value::unsignedInteger() const noexcept {
    return std::get_if<std::uint64_t>(&storage_);
}

const double* Value::number() const noexcept {
    return std::get_if<double>(&storage_);
}

const std::string* Value::string() const noexcept {
    return std::get_if<std::string>(&storage_);
}

const Value::Array* Value::array() const noexcept {
    return std::get_if<Array>(&storage_);
}

const Value::Object* Value::object() const noexcept {
    return std::get_if<Object>(&storage_);
}

Value::Array* Value::array() noexcept {
    return std::get_if<Array>(&storage_);
}

Value::Object* Value::object() noexcept {
    return std::get_if<Object>(&storage_);
}

const Value* Value::find(std::string_view key) const noexcept {
    const auto* objectValue = object();
    if (objectValue == nullptr) {
        return nullptr;
    }
    const auto iterator = objectValue->find(key);
    return iterator == objectValue->end() ? nullptr : &iterator->second;
}

Value* Value::find(std::string_view key) noexcept {
    auto* objectValue = object();
    if (objectValue == nullptr) {
        return nullptr;
    }
    const auto iterator = objectValue->find(key);
    return iterator == objectValue->end() ? nullptr : &iterator->second;
}

} // namespace cuexis::json
