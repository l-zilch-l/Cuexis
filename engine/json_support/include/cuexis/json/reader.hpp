#pragma once

//  typed Reader — 从 Cuexis Value 读取结构化数据，自动生成以 `$` 为根的字段路径诊断
//  requiredField 缺失或类型错误时自动报告，optionalField 静默返回 nullopt
//  rejectUnknownFields 用于检测未知字段，确保格式版本兼容性

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/json/value.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace cuexis::json {

[[nodiscard]] std::string appendFieldPath(std::string_view parentPath, std::string_view field);
[[nodiscard]] std::string appendIndexPath(std::string_view parentPath, std::size_t index);

class Reader final {
  public:
    Reader(const Value& value, core::Diagnostics& diagnostics, std::string fieldPath = "$");

    [[nodiscard]] const Value& value() const noexcept;
    [[nodiscard]] std::string_view fieldPath() const noexcept;

    [[nodiscard]] std::optional<Reader> requiredField(std::string_view name) const;
    [[nodiscard]] std::optional<Reader> optionalField(std::string_view name) const;
    [[nodiscard]] std::optional<Reader> element(std::size_t index) const;

    [[nodiscard]] const Value::Object* readObject() const;
    [[nodiscard]] const Value::Array* readArray() const;
    [[nodiscard]] std::optional<std::string_view> readString() const;
    [[nodiscard]] std::optional<bool> readBoolean() const;
    [[nodiscard]] std::optional<std::int64_t> readInt64() const;
    [[nodiscard]] std::optional<std::uint64_t> readUInt64() const;
    [[nodiscard]] std::optional<double> readNumber() const;

    void
    rejectUnknownFields(std::span<const std::string_view> knownFields,
                        core::DiagnosticSeverity severity = core::DiagnosticSeverity::Error) const;

  private:
    void reportTypeMismatch(std::string_view expected) const;

    const Value* value_;
    core::Diagnostics* diagnostics_;
    std::string fieldPath_;
};

} // namespace cuexis::json
