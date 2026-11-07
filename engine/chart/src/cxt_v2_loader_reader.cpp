#include <cuexis/chart/cxt_v2_loader.hpp>

#include "cxt_v2_loader_units_internal.hpp"

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/value.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart {

void addCxtError(core::Diagnostics& diagnostics, std::string code, std::string message,
                 std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(path)});
}

[[nodiscard]] auto childPath(std::string_view parent, std::string_view child) -> std::string {
    return std::string{parent} + "/" + std::string{child};
}

[[nodiscard]] auto indexPath(std::string_view parent, std::size_t index) -> std::string {
    return std::string{parent} + "/" + std::to_string(index);
}

[[nodiscard]] auto integer(const Value& value) -> std::optional<std::int64_t> {
    if (const auto* signedValue = value.signedInteger()) {
        return *signedValue;
    }
    if (const auto* unsignedValue = value.unsignedInteger();
        unsignedValue != nullptr &&
        *unsignedValue <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return static_cast<std::int64_t>(*unsignedValue);
    }
    return std::nullopt;
}

[[nodiscard]] auto string(const Value& value) -> std::optional<std::string_view> {
    if (const auto* result = value.string()) {
        return *result;
    }
    return std::nullopt;
}

[[nodiscard]] auto object(const Value& value, core::Diagnostics& diagnostics, std::string_view path)
    -> const Value::Object* {
    const auto* result = value.object();
    if (result == nullptr) {
        addCxtError(diagnostics, "cxt.v2.type_mismatch", "Expected an object", std::string{path});
    }
    return result;
}

[[nodiscard]] auto array(const Value& value, core::Diagnostics& diagnostics, std::string_view path)
    -> const Value::Array* {
    const auto* result = value.array();
    if (result == nullptr) {
        addCxtError(diagnostics, "cxt.v2.type_mismatch", "Expected an array", std::string{path});
    }
    return result;
}

[[nodiscard]] auto field(const Value::Object& value, std::string_view name,
                         core::Diagnostics& diagnostics, std::string_view path) -> const Value* {
    const auto found = value.find(name);
    if (found == value.end()) {
        addCxtError(diagnostics, "cxt.v2.field_missing", "Required field is missing",
                    childPath(path, name));
        return nullptr;
    }
    return &found->second;
}

void rejectUnknown(const Value::Object& value, std::initializer_list<std::string_view> known,
                   core::Diagnostics& diagnostics, std::string_view path) {
    for (const auto& [key, unused] : value) {
        static_cast<void>(unused);
        if (std::find(known.begin(), known.end(), key) == known.end()) {
            addCxtError(diagnostics, "cxt.v2.field_unknown", "Unknown CXT v2 core field",
                        childPath(path, key));
        }
    }
}

[[nodiscard]] auto requireEmptyObject(const Value::Object& parent, std::string_view name,
                                      core::Diagnostics& diagnostics, std::string_view path)
    -> bool {
    const auto* value = field(parent, name, diagnostics, path);
    if (value == nullptr) {
        return false;
    }
    const auto* objectValue = object(*value, diagnostics, childPath(path, name));
    if (objectValue == nullptr) {
        return false;
    }
    if (!objectValue->empty()) {
        addCxtError(diagnostics, "cxt.v2.extension_unsupported",
                    "Foundation CXT v2 rejects non-empty extensions", childPath(path, name));
        return false;
    }
    return true;
}

[[nodiscard]] auto requireEmptyArray(const Value::Object& parent, std::string_view name,
                                     core::Diagnostics& diagnostics, std::string_view path)
    -> bool {
    const auto* value = field(parent, name, diagnostics, path);
    if (value == nullptr) {
        return false;
    }
    const auto* arrayValue = array(*value, diagnostics, childPath(path, name));
    if (arrayValue == nullptr) {
        return false;
    }
    if (!arrayValue->empty()) {
        addCxtError(diagnostics, "cxt.v2.extension_unsupported",
                    "Foundation CXT v2 rejects non-empty extensions", childPath(path, name));
        return false;
    }
    return true;
}

[[nodiscard]] auto checkedAdd(std::int64_t left, std::int64_t right) -> core::Result<std::int64_t> {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right)) {
        return core::unexpected(
            core::Error{"cxt.v2.arithmetic_overflow", "Affine integer addition overflowed"});
    }
    return left + right;
}

[[nodiscard]] auto checkedMultiply(std::int64_t left, std::int64_t right)
    -> core::Result<std::int64_t> {
    if (left == 0 || right == 0) {
        return 0;
    }
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((left == -1 && right == minimum) || (right == -1 && left == minimum) ||
        (left > 0 && right > 0 && left > maximum / right) ||
        (left > 0 && right < 0 && right < minimum / left) ||
        (left < 0 && right > 0 && left < minimum / right) ||
        (left < 0 && right < 0 && left < maximum / right)) {
        return core::unexpected(
            core::Error{"cxt.v2.arithmetic_overflow", "Affine integer multiplication overflowed"});
    }
    return left * right;
}

[[nodiscard]] auto stableId(std::string_view value) -> bool {
    const auto alphaNum = [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    };
    if (value.empty() || value.size() > 256 || !alphaNum(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(),
                       [&](char c) { return alphaNum(c) || c == '.' || c == '_' || c == '-'; });
}

[[nodiscard]] auto readId(const Value& value, core::Diagnostics& diagnostics, std::string_view path)
    -> std::optional<std::string> {
    const auto text = string(value);
    if (!text || !stableId(*text)) {
        addCxtError(diagnostics, "cxt.v2.id_invalid", "Expected a portable stable ID",
                    std::string{path});
        return std::nullopt;
    }
    return std::string{*text};
}

[[nodiscard]] auto readBeat(const Value& value, core::Diagnostics& diagnostics,
                            std::string_view path) -> std::optional<RationalBeat> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*values, {"numerator", "denominator"}, diagnostics, path);
    const auto* numerator = field(*values, "numerator", diagnostics, path);
    const auto* denominator = field(*values, "denominator", diagnostics, path);
    if (numerator == nullptr || denominator == nullptr) {
        return std::nullopt;
    }
    const auto n = integer(*numerator);
    const auto d = integer(*denominator);
    if (!n || !d) {
        addCxtError(diagnostics, "cxt.v2.beat_invalid", "Beat values must be signed integers",
                    std::string{path});
        return std::nullopt;
    }
    auto result = RationalBeat::create(*n, *d);
    if (!result) {
        addCxtError(diagnostics, std::string{result.error().code()},
                    std::string{result.error().message()}, std::string{path});
        return std::nullopt;
    }
    return *result;
}

} // namespace cuexis::chart
