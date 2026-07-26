#pragma once

//  Cuexis-owned JSON Value - a variant-based typed JSON value
//  The public interface exposes only this owned type, never nlohmann::json or any other
//  third-party JSON DOM
//  The type distinguishes SignedInteger (int64_t) from UnsignedInteger (uint64_t) to support
//  exact serialization

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cuexis::json {

enum class ValueType {
    Null,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Number,
    String,
    Array,
    Object,
};

[[nodiscard]] std::string_view valueTypeName(ValueType type) noexcept;

class Value final {
  public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value, std::less<>>;

    Value() noexcept;
    Value(std::nullptr_t) noexcept;
    explicit Value(bool value) noexcept;
    explicit Value(std::int64_t value) noexcept;
    explicit Value(std::uint64_t value) noexcept;
    explicit Value(double value) noexcept;
    explicit Value(std::string value);
    explicit Value(const char* value);
    explicit Value(Array value);
    explicit Value(Object value);

    [[nodiscard]] ValueType type() const noexcept;
    [[nodiscard]] bool isNull() const noexcept;

    [[nodiscard]] const bool* boolean() const noexcept;
    [[nodiscard]] const std::int64_t* signedInteger() const noexcept;
    [[nodiscard]] const std::uint64_t* unsignedInteger() const noexcept;
    [[nodiscard]] const double* number() const noexcept;
    [[nodiscard]] const std::string* string() const noexcept;
    [[nodiscard]] const Array* array() const noexcept;
    [[nodiscard]] const Object* object() const noexcept;

    [[nodiscard]] Array* array() noexcept;
    [[nodiscard]] Object* object() noexcept;
    [[nodiscard]] const Value* find(std::string_view key) const noexcept;
    [[nodiscard]] Value* find(std::string_view key) noexcept;

    friend bool operator==(const Value&, const Value&) = default;

  private:
    using Storage = std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double,
                                 std::string, Array, Object>;

    Storage storage_;
};

} // namespace cuexis::json
