//  typed Reader 实现 — 从 Cuexis Value 读取结构化数据
//  字段路径使用 `$` 为根，路径段中的 `~` 和 `/` 按 JSON Pointer 规则转义
//  类型不匹配、字段缺失和索引越界均自动生成带字段路径的诊断

#include <cuexis/json/reader.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace cuexis::json {
namespace {

std::string escapePathSegment(std::string_view segment) {
    std::string escaped;
    escaped.reserve(segment.size());
    for (const char character : segment) {
        if (character == '~') {
            escaped.append("~0");
        } else if (character == '/') {
            escaped.append("~1");
        } else {
            escaped.push_back(character);
        }
    }
    return escaped;
}

} // namespace

std::string appendFieldPath(std::string_view parentPath, std::string_view field) {
    std::string result{parentPath};
    result.push_back('/');
    result.append(escapePathSegment(field));
    return result;
}

std::string appendIndexPath(std::string_view parentPath, std::size_t index) {
    std::string result{parentPath};
    result.push_back('/');
    result.append(std::to_string(index));
    return result;
}

Reader::Reader(const Value& value, core::Diagnostics& diagnostics, std::string fieldPath)
    : value_(&value), diagnostics_(&diagnostics), fieldPath_(std::move(fieldPath)) {}

const Value& Reader::value() const noexcept {
    return *value_;
}

std::string_view Reader::fieldPath() const noexcept {
    return fieldPath_;
}

std::optional<Reader> Reader::requiredField(std::string_view name) const {
    const auto* objectValue = readObject();
    if (objectValue == nullptr) {
        return std::nullopt;
    }

    const auto iterator = objectValue->find(name);
    if (iterator == objectValue->end()) {
        diagnostics_->add(core::Diagnostic{core::DiagnosticSeverity::Error, "json.field.missing",
                                           "Required JSON field is missing",
                                           appendFieldPath(fieldPath_, name)});
        return std::nullopt;
    }
    return Reader{iterator->second, *diagnostics_, appendFieldPath(fieldPath_, name)};
}

std::optional<Reader> Reader::optionalField(std::string_view name) const {
    const auto* objectValue = readObject();
    if (objectValue == nullptr) {
        return std::nullopt;
    }

    const auto iterator = objectValue->find(name);
    if (iterator == objectValue->end()) {
        return std::nullopt;
    }
    return Reader{iterator->second, *diagnostics_, appendFieldPath(fieldPath_, name)};
}

std::optional<Reader> Reader::element(std::size_t index) const {
    const auto* arrayValue = readArray();
    if (arrayValue == nullptr) {
        return std::nullopt;
    }
    if (index >= arrayValue->size()) {
        diagnostics_->add(core::Diagnostic{
            core::DiagnosticSeverity::Error, "json.array.index_out_of_range",
            "JSON array index is out of range", appendIndexPath(fieldPath_, index)});
        return std::nullopt;
    }
    return Reader{(*arrayValue)[index], *diagnostics_, appendIndexPath(fieldPath_, index)};
}

const Value::Object* Reader::readObject() const {
    const auto* result = value_->object();
    if (result == nullptr) {
        reportTypeMismatch("object");
    }
    return result;
}

const Value::Array* Reader::readArray() const {
    const auto* result = value_->array();
    if (result == nullptr) {
        reportTypeMismatch("array");
    }
    return result;
}

std::optional<std::string_view> Reader::readString() const {
    const auto* result = value_->string();
    if (result == nullptr) {
        reportTypeMismatch("string");
        return std::nullopt;
    }
    return *result;
}

std::optional<bool> Reader::readBoolean() const {
    const auto* result = value_->boolean();
    if (result == nullptr) {
        reportTypeMismatch("boolean");
        return std::nullopt;
    }
    return *result;
}

std::optional<std::int64_t> Reader::readInt64() const {
    if (const auto* result = value_->signedInteger(); result != nullptr) {
        return *result;
    }
    if (const auto* result = value_->unsignedInteger();
        result != nullptr &&
        *result <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return static_cast<std::int64_t>(*result);
    }
    reportTypeMismatch("signed_integer");
    return std::nullopt;
}

std::optional<std::uint64_t> Reader::readUInt64() const {
    if (const auto* result = value_->unsignedInteger(); result != nullptr) {
        return *result;
    }
    if (const auto* result = value_->signedInteger(); result != nullptr && *result >= 0) {
        return static_cast<std::uint64_t>(*result);
    }
    reportTypeMismatch("unsigned_integer");
    return std::nullopt;
}

std::optional<double> Reader::readNumber() const {
    if (const auto* result = value_->number(); result != nullptr) {
        return *result;
    }
    if (const auto* result = value_->signedInteger(); result != nullptr) {
        return static_cast<double>(*result);
    }
    if (const auto* result = value_->unsignedInteger(); result != nullptr) {
        return static_cast<double>(*result);
    }
    reportTypeMismatch("number");
    return std::nullopt;
}

void Reader::rejectUnknownFields(std::span<const std::string_view> knownFields,
                                 core::DiagnosticSeverity severity) const {
    const auto* objectValue = readObject();
    if (objectValue == nullptr) {
        return;
    }

    for (const auto& [name, ignored] : *objectValue) {
        static_cast<void>(ignored);
        if (std::find(knownFields.begin(), knownFields.end(), name) == knownFields.end()) {
            if (!diagnostics_->add(core::Diagnostic{severity, "json.field.unknown",
                                                    "JSON field is not recognized",
                                                    appendFieldPath(fieldPath_, name)})) {
                break;
            }
        }
    }
}

void Reader::reportTypeMismatch(std::string_view expected) const {
    diagnostics_->add(core::Diagnostic{core::DiagnosticSeverity::Error, "json.type.mismatch",
                                       "JSON value has an unexpected type", fieldPath_}
                          .withContext("expected", std::string{expected})
                          .withContext("actual", std::string{valueTypeName(value_->type())}));
}

} // namespace cuexis::json
