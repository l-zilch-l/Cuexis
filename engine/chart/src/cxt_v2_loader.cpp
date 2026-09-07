#include <cuexis/chart/cxt_v2_loader.hpp>

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
namespace {

using json::Value;

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
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
        addError(diagnostics, "cxt.v2.type_mismatch", "Expected an object", std::string{path});
    }
    return result;
}

[[nodiscard]] auto array(const Value& value, core::Diagnostics& diagnostics, std::string_view path)
    -> const Value::Array* {
    const auto* result = value.array();
    if (result == nullptr) {
        addError(diagnostics, "cxt.v2.type_mismatch", "Expected an array", std::string{path});
    }
    return result;
}

[[nodiscard]] auto field(const Value::Object& value, std::string_view name,
                         core::Diagnostics& diagnostics, std::string_view path) -> const Value* {
    const auto found = value.find(name);
    if (found == value.end()) {
        addError(diagnostics, "cxt.v2.field_missing", "Required field is missing",
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
            addError(diagnostics, "cxt.v2.field_unknown", "Unknown CXT v2 core field",
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
        addError(diagnostics, "cxt.v2.extension_unsupported",
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
        addError(diagnostics, "cxt.v2.extension_unsupported",
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
        addError(diagnostics, "cxt.v2.id_invalid", "Expected a portable stable ID",
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
        addError(diagnostics, "cxt.v2.beat_invalid", "Beat values must be signed integers",
                 std::string{path});
        return std::nullopt;
    }
    auto result = RationalBeat::create(*n, *d);
    if (!result) {
        addError(diagnostics, std::string{result.error().code()},
                 std::string{result.error().message()}, std::string{path});
        return std::nullopt;
    }
    return *result;
}

enum class ValueKind : std::uint8_t { Integer, Beat };
enum class SourceKind : std::uint8_t { Literal, Parameter, Slot, Index, Affine };

struct Source final {
    SourceKind kind{};
    ValueKind valueKind{};
    CxtV2Value literal{std::int64_t{0}};
    std::string id;
    std::optional<std::unique_ptr<Source>> input;
    std::optional<std::unique_ptr<Source>> scale;
    std::optional<std::unique_ptr<Source>> offset;
};

struct Slot final {
    std::string id;
    ValueKind kind{};
    bool required{};
    std::optional<CxtV2Value> defaultValue;
    std::optional<CxtV2Value> minimum;
    std::optional<CxtV2Value> maximum;
};

struct Requirement final {
    std::string id;
    Source beat;
    Source lane;
};

struct Prototype final {
    std::string id;
    std::map<std::string, Slot, std::less<>> slots;
    std::vector<Requirement> requirements;
    bool hasTransform{};
    std::optional<Source> positionX;
};

struct Node final {
    enum class Op : std::uint8_t { Emit, Repeat };
    enum class ParentKind : std::uint8_t { Invocation, Root, Emission };
    Op op{};
    std::string nodeId;
    std::string prototype;
    std::map<std::string, Source, std::less<>> bindings;
    Source count;
    std::string indexId;
    std::vector<Node> body;
    ParentKind parentKind{ParentKind::Invocation};
    std::string parentNodeId;
};

struct Pattern final {
    std::string id;
    std::vector<Node> nodes;
};

struct Parameter final {
    ValueKind kind{};
    CxtV2Value defaultValue{std::int64_t{0}};
    std::optional<CxtV2Value> minimum;
    std::optional<CxtV2Value> maximum;
};

struct Module final {
    std::string id;
    std::string moduleKind;
    std::string exportId;
    std::map<std::string, Parameter, std::less<>> parameters;
    std::map<std::string, Prototype, std::less<>> prototypes;
    std::map<std::string, Pattern, std::less<>> patterns;
};

[[nodiscard]] auto parseSource(const Value& value, core::Diagnostics& diagnostics,
                               std::string_view path, bool allowIndex, bool allowSlot,
                               bool allowParameter) -> std::optional<Source> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    const auto* kindValue = field(*values, "kind", diagnostics, path);
    if (kindValue == nullptr || !string(*kindValue)) {
        addError(diagnostics, "cxt.v2.source_invalid", "Value source kind must be a string",
                 childPath(path, "kind"));
        return std::nullopt;
    }
    const auto kind = *string(*kindValue);
    if (kind == "literal") {
        rejectUnknown(*values, {"kind", "value"}, diagnostics, path);
        const auto* literal = field(*values, "value", diagnostics, path);
        if (literal == nullptr)
            return std::nullopt;
        if (const auto integerValue = integer(*literal)) {
            return Source{SourceKind::Literal, ValueKind::Integer, *integerValue, {}, {}, {}, {}};
        }
        if (literal->object() == nullptr) {
            addError(diagnostics, "cxt.v2.literal_invalid", "Expected an integer or beat literal",
                     childPath(path, "value"));
            return std::nullopt;
        }
        const auto beat = readBeat(*literal, diagnostics, childPath(path, "value"));
        if (!beat)
            return std::nullopt;
        return Source{SourceKind::Literal, ValueKind::Beat, *beat, {}, {}, {}, {}};
    }
    if (kind == "parameter" || kind == "slot" || kind == "index") {
        rejectUnknown(*values, {"kind", "id"}, diagnostics, path);
        const auto* idValue = field(*values, "id", diagnostics, path);
        const auto id =
            idValue ? readId(*idValue, diagnostics, childPath(path, "id")) : std::nullopt;
        if (!id)
            return std::nullopt;
        if ((kind == "slot" && !allowSlot) || (kind == "index" && !allowIndex) ||
            (kind == "parameter" && !allowParameter)) {
            addError(diagnostics, "cxt.v2.source_scope_invalid",
                     "Value source is not valid in this scope", std::string{path});
            return std::nullopt;
        }
        const auto sourceKind = kind == "parameter" ? SourceKind::Parameter
                                : kind == "slot"    ? SourceKind::Slot
                                                    : SourceKind::Index;
        return Source{sourceKind, ValueKind::Integer, std::int64_t{0}, *id, {}, {}, {}};
    }
    if (kind == "affine") {
        rejectUnknown(*values, {"kind", "input", "scale", "offset"}, diagnostics, path);
        const auto* input = field(*values, "input", diagnostics, path);
        const auto* scale = field(*values, "scale", diagnostics, path);
        const auto* offset = field(*values, "offset", diagnostics, path);
        if (input == nullptr || scale == nullptr || offset == nullptr)
            return std::nullopt;
        auto parsedInput = parseSource(*input, diagnostics, childPath(path, "input"), allowIndex,
                                       allowSlot, allowParameter);
        auto parsedScale = parseSource(*scale, diagnostics, childPath(path, "scale"), false,
                                       allowSlot, allowParameter);
        auto parsedOffset = parseSource(*offset, diagnostics, childPath(path, "offset"), false,
                                        allowSlot, allowParameter);
        if (!parsedInput || !parsedScale || !parsedOffset ||
            parsedInput->kind == SourceKind::Affine || parsedScale->kind == SourceKind::Affine ||
            parsedOffset->kind == SourceKind::Affine) {
            addError(diagnostics, "cxt.v2.affine_invalid", "Affine inputs must be non-nested atoms",
                     std::string{path});
            return std::nullopt;
        }
        return Source{SourceKind::Affine,
                      parsedInput->valueKind,
                      std::int64_t{0},
                      {},
                      std::make_unique<Source>(std::move(*parsedInput)),
                      std::make_unique<Source>(std::move(*parsedScale)),
                      std::make_unique<Source>(std::move(*parsedOffset))};
    }
    addError(diagnostics, "cxt.v2.source_invalid", "Unsupported value source kind",
             childPath(path, "kind"));
    return std::nullopt;
}

[[nodiscard]] auto inDeclaredRange(const CxtV2Value& value,
                                   const std::optional<CxtV2Value>& minimum,
                                   const std::optional<CxtV2Value>& maximum) -> bool {
    if (const auto* integerValue = std::get_if<std::int64_t>(&value)) {
        if (minimum) {
            const auto* bound = std::get_if<std::int64_t>(&*minimum);
            if (bound == nullptr || *integerValue < *bound) {
                return false;
            }
        }
        if (maximum) {
            const auto* bound = std::get_if<std::int64_t>(&*maximum);
            if (bound == nullptr || *integerValue > *bound) {
                return false;
            }
        }
        return true;
    }
    const auto* beatValue = std::get_if<RationalBeat>(&value);
    if (beatValue == nullptr) {
        return false;
    }
    if (minimum) {
        const auto* bound = std::get_if<RationalBeat>(&*minimum);
        if (bound == nullptr || *beatValue < *bound) {
            return false;
        }
    }
    if (maximum) {
        const auto* bound = std::get_if<RationalBeat>(&*maximum);
        if (bound == nullptr || *beatValue > *bound) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto parseParameter(const Value& value, core::Diagnostics& diagnostics,
                                  std::string_view path)
    -> std::optional<std::pair<std::string, Parameter>> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*values, {"id", "type", "default", "minimum", "maximum"}, diagnostics, path);
    const auto* idValue = field(*values, "id", diagnostics, path);
    const auto* typeValue = field(*values, "type", diagnostics, path);
    const auto* defaultValue = field(*values, "default", diagnostics, path);
    const auto id = idValue ? readId(*idValue, diagnostics, childPath(path, "id")) : std::nullopt;
    const auto type = typeValue ? string(*typeValue) : std::nullopt;
    if (!id || !type || defaultValue == nullptr) {
        return std::nullopt;
    }
    if (*type != "integer" && *type != "beat") {
        addError(diagnostics, "cxt.v2.parameter_type_unsupported",
                 "Foundation CXT v2 supports integer and beat parameters only",
                 childPath(path, "type"));
        return std::nullopt;
    }
    Parameter parameter;
    parameter.kind = *type == "integer" ? ValueKind::Integer : ValueKind::Beat;
    if (parameter.kind == ValueKind::Integer) {
        const auto* minimumValue = field(*values, "minimum", diagnostics, path);
        const auto* maximumValue = field(*values, "maximum", diagnostics, path);
        const auto defaultInteger = integer(*defaultValue);
        const auto minimum = minimumValue ? integer(*minimumValue) : std::nullopt;
        const auto maximum = maximumValue ? integer(*maximumValue) : std::nullopt;
        if (!defaultInteger || !minimum || !maximum || *minimum > *maximum ||
            *defaultInteger < *minimum || *defaultInteger > *maximum) {
            addError(diagnostics, "cxt.v2.parameter_invalid",
                     "Integer parameter default and bounds must be valid", std::string{path});
            return std::nullopt;
        }
        parameter.defaultValue = *defaultInteger;
        parameter.minimum = *minimum;
        parameter.maximum = *maximum;
        return std::pair{*id, std::move(parameter)};
    }
    auto defaultBeat = readBeat(*defaultValue, diagnostics, childPath(path, "default"));
    if (!defaultBeat) {
        return std::nullopt;
    }
    const auto minimumIt = values->find("minimum");
    const auto maximumIt = values->find("maximum");
    if (minimumIt != values->end()) {
        auto minimum = readBeat(minimumIt->second, diagnostics, childPath(path, "minimum"));
        if (!minimum) {
            return std::nullopt;
        }
        parameter.minimum = *minimum;
    }
    if (maximumIt != values->end()) {
        auto maximum = readBeat(maximumIt->second, diagnostics, childPath(path, "maximum"));
        if (!maximum) {
            return std::nullopt;
        }
        parameter.maximum = *maximum;
    }
    parameter.defaultValue = *defaultBeat;
    if (parameter.minimum && parameter.maximum) {
        const auto* minimumBeat = std::get_if<RationalBeat>(&*parameter.minimum);
        const auto* maximumBeat = std::get_if<RationalBeat>(&*parameter.maximum);
        if (minimumBeat == nullptr || maximumBeat == nullptr || *minimumBeat > *maximumBeat) {
            addError(diagnostics, "cxt.v2.parameter_invalid", "Beat parameter bounds must be valid",
                     std::string{path});
            return std::nullopt;
        }
    }
    if (!inDeclaredRange(parameter.defaultValue, parameter.minimum, parameter.maximum)) {
        addError(diagnostics, "cxt.v2.parameter_invalid",
                 "Beat parameter default is outside the declared range", std::string{path});
        return std::nullopt;
    }
    return std::pair{*id, std::move(parameter)};
}

[[nodiscard]] auto parseSlot(const Value& value, core::Diagnostics& diagnostics,
                             std::string_view path) -> std::optional<std::pair<std::string, Slot>> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*values, {"id", "type", "required", "default", "minimum", "maximum"}, diagnostics,
                  path);
    const auto* idValue = field(*values, "id", diagnostics, path);
    const auto* typeValue = field(*values, "type", diagnostics, path);
    const auto* requiredValue = field(*values, "required", diagnostics, path);
    const auto id = idValue ? readId(*idValue, diagnostics, childPath(path, "id")) : std::nullopt;
    const auto type = typeValue ? string(*typeValue) : std::nullopt;
    const auto required = requiredValue && requiredValue->boolean()
                              ? std::optional<bool>{*requiredValue->boolean()}
                              : std::nullopt;
    if (!id || !type || !required) {
        if (requiredValue && !requiredValue->boolean()) {
            addError(diagnostics, "cxt.v2.slot_invalid", "Slot required flag must be a boolean",
                     childPath(path, "required"));
        }
        return std::nullopt;
    }
    if (*type != "integer" && *type != "beat") {
        addError(diagnostics, "cxt.v2.slot_type_unsupported",
                 "Foundation CXT v2 supports integer and beat slots only", childPath(path, "type"));
        return std::nullopt;
    }
    Slot result{*id,          *type == "integer" ? ValueKind::Integer : ValueKind::Beat,
                *required,    std::nullopt,
                std::nullopt, std::nullopt};
    const auto defaultIt = values->find("default");
    if (*required && defaultIt != values->end()) {
        addError(diagnostics, "cxt.v2.slot_default_invalid",
                 "A required slot must not declare a default", childPath(path, "default"));
        return std::nullopt;
    }
    if (!*required && defaultIt == values->end()) {
        addError(diagnostics, "cxt.v2.slot_default_missing",
                 "An optional slot must declare a literal default", std::string{path});
        return std::nullopt;
    }
    if (defaultIt != values->end()) {
        if (result.kind == ValueKind::Integer) {
            if (const auto parsed = integer(defaultIt->second)) {
                result.defaultValue = *parsed;
            } else {
                addError(diagnostics, "cxt.v2.slot_default_invalid", "Slot default type is invalid",
                         childPath(path, "default"));
                return std::nullopt;
            }
        } else if (auto beat =
                       readBeat(defaultIt->second, diagnostics, childPath(path, "default"))) {
            result.defaultValue = *beat;
        } else {
            return std::nullopt;
        }
    }
    const auto minimumIt = values->find("minimum");
    const auto maximumIt = values->find("maximum");
    if (result.kind == ValueKind::Integer) {
        if (minimumIt != values->end()) {
            result.minimum = integer(minimumIt->second);
            if (!result.minimum) {
                addError(diagnostics, "cxt.v2.slot_range_invalid",
                         "Slot integer bounds are invalid", childPath(path, "minimum"));
                return std::nullopt;
            }
        }
        if (maximumIt != values->end()) {
            result.maximum = integer(maximumIt->second);
            if (!result.maximum) {
                addError(diagnostics, "cxt.v2.slot_range_invalid",
                         "Slot integer bounds are invalid", childPath(path, "maximum"));
                return std::nullopt;
            }
        }
    } else {
        if (minimumIt != values->end()) {
            auto minimum = readBeat(minimumIt->second, diagnostics, childPath(path, "minimum"));
            if (!minimum) {
                return std::nullopt;
            }
            result.minimum = *minimum;
        }
        if (maximumIt != values->end()) {
            auto maximum = readBeat(maximumIt->second, diagnostics, childPath(path, "maximum"));
            if (!maximum) {
                return std::nullopt;
            }
            result.maximum = *maximum;
        }
    }
    if (result.minimum && result.maximum &&
        !inDeclaredRange(*result.maximum, result.minimum, std::nullopt)) {
        addError(diagnostics, "cxt.v2.slot_range_invalid", "Slot bounds are invalid",
                 std::string{path});
        return std::nullopt;
    }
    if (result.defaultValue &&
        !inDeclaredRange(*result.defaultValue, result.minimum, result.maximum)) {
        addError(diagnostics, "cxt.v2.slot_default_invalid",
                 "Slot default is outside the declared range", childPath(path, "default"));
        return std::nullopt;
    }
    return std::pair{*id, std::move(result)};
}

[[nodiscard]] auto isLiteralReference(const Value& value, std::string_view domain,
                                      std::string_view id, core::Diagnostics& diagnostics,
                                      std::string_view path) -> bool {
    const auto* source = object(value, diagnostics, path);
    if (source == nullptr)
        return false;
    rejectUnknown(*source, {"kind", "value"}, diagnostics, path);
    const auto* kind = field(*source, "kind", diagnostics, path);
    const auto* literal = field(*source, "value", diagnostics, path);
    if (kind == nullptr || literal == nullptr || !string(*kind) || *string(*kind) != "literal")
        return false;
    const auto* reference = object(*literal, diagnostics, childPath(path, "value"));
    if (reference == nullptr)
        return false;
    rejectUnknown(*reference, {"domain", "id"}, diagnostics, childPath(path, "value"));
    const auto domainIt = reference->find("domain");
    const auto idIt = reference->find("id");
    return domainIt != reference->end() && idIt != reference->end() && string(domainIt->second) &&
           string(idIt->second) && *string(domainIt->second) == domain &&
           *string(idIt->second) == id;
}

[[nodiscard]] auto parsePrototype(const Value& value, core::Diagnostics& diagnostics,
                                  std::string_view path, const ChartLimits& limits)
    -> std::optional<std::pair<std::string, Prototype>> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*values, {"id", "slots", "components", "requirements", "extensions"}, diagnostics,
                  path);
    const auto* idValue = field(*values, "id", diagnostics, path);
    const auto* slotsValue = field(*values, "slots", diagnostics, path);
    const auto* componentsValue = field(*values, "components", diagnostics, path);
    const auto* requirementsValue = field(*values, "requirements", diagnostics, path);
    if (!requireEmptyObject(*values, "extensions", diagnostics, path)) {
        return std::nullopt;
    }
    const auto id = idValue ? readId(*idValue, diagnostics, childPath(path, "id")) : std::nullopt;
    if (!id || slotsValue == nullptr || componentsValue == nullptr ||
        requirementsValue == nullptr) {
        return std::nullopt;
    }
    Prototype result;
    result.id = *id;
    const auto* slots = array(*slotsValue, diagnostics, childPath(path, "slots"));
    if (slots == nullptr) {
        return std::nullopt;
    }
    if (slots->size() > limits.maxCxtV2SlotsPerPrototype) {
        addError(diagnostics, "cxt.v2.budget.slots", "Slot budget exceeded",
                 childPath(path, "slots"));
        return std::nullopt;
    }
    for (std::size_t i = 0; i < slots->size(); ++i) {
        auto slot = parseSlot((*slots)[i], diagnostics, indexPath(childPath(path, "slots"), i));
        if (!slot) {
            return std::nullopt;
        }
        if (!result.slots.emplace(slot->first, std::move(slot->second)).second) {
            addError(diagnostics, "cxt.v2.id_duplicate", "Slot ID is duplicated",
                     indexPath(childPath(path, "slots"), i));
            return std::nullopt;
        }
    }
    const auto* components = array(*componentsValue, diagnostics, childPath(path, "components"));
    if (components == nullptr) {
        return std::nullopt;
    }
    std::set<std::string> componentTypes;
    for (std::size_t i = 0; i < components->size(); ++i) {
        const auto componentPath = indexPath(childPath(path, "components"), i);
        const auto* component = object((*components)[i], diagnostics, componentPath);
        if (component == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*component, {"type", "version", "fields", "extensions"}, diagnostics,
                      componentPath);
        const auto* typeValue = field(*component, "type", diagnostics, componentPath);
        const auto* versionValue = field(*component, "version", diagnostics, componentPath);
        const auto* fieldsValue = field(*component, "fields", diagnostics, componentPath);
        if (!requireEmptyObject(*component, "extensions", diagnostics, componentPath)) {
            return std::nullopt;
        }
        const auto type = typeValue ? string(*typeValue) : std::nullopt;
        const auto version = versionValue ? integer(*versionValue) : std::nullopt;
        if (!type || fieldsValue == nullptr) {
            addError(diagnostics, "cxt.v2.component_invalid", "Component requires type and fields",
                     componentPath);
            return std::nullopt;
        }
        if (*type != "cuexis.transform" || !version || *version != 1) {
            addError(diagnostics, "cxt.v2.component_unsupported",
                     "Foundation CXT v2 supports cuexis.transform version 1 only",
                     childPath(componentPath, "type"));
            return std::nullopt;
        }
        if (!componentTypes.insert(std::string{*type}).second) {
            addError(diagnostics, "cxt.v2.id_duplicate", "Component type is duplicated",
                     childPath(componentPath, "type"));
            return std::nullopt;
        }
        result.hasTransform = true;
        const auto* fields = array(*fieldsValue, diagnostics, childPath(componentPath, "fields"));
        if (fields == nullptr) {
            return std::nullopt;
        }
        for (std::size_t j = 0; j < fields->size(); ++j) {
            const auto fieldPath = indexPath(childPath(componentPath, "fields"), j);
            const auto* entry = object((*fields)[j], diagnostics, fieldPath);
            if (entry == nullptr) {
                return std::nullopt;
            }
            rejectUnknown(*entry, {"path", "source"}, diagnostics, fieldPath);
            const auto* pathValue = field(*entry, "path", diagnostics, fieldPath);
            const auto* sourceValue = field(*entry, "source", diagnostics, fieldPath);
            const auto fieldName = pathValue ? string(*pathValue) : std::nullopt;
            if (!fieldName || sourceValue == nullptr) {
                addError(diagnostics, "cxt.v2.component_invalid",
                         "Component field requires path and source", fieldPath);
                return std::nullopt;
            }
            if (*fieldName != "position[0]") {
                addError(diagnostics, "cxt.v2.component_path_unsupported",
                         "Foundation CXT v2 supports transform position[0] only",
                         childPath(fieldPath, "path"));
                return std::nullopt;
            }
            if (result.positionX) {
                addError(diagnostics, "cxt.v2.id_duplicate", "Component field path is duplicated",
                         childPath(fieldPath, "path"));
                return std::nullopt;
            }
            result.positionX = parseSource(*sourceValue, diagnostics,
                                           childPath(fieldPath, "source"), false, true, false);
            if (!result.positionX) {
                return std::nullopt;
            }
        }
    }
    const auto* requirements =
        array(*requirementsValue, diagnostics, childPath(path, "requirements"));
    if (requirements == nullptr) {
        return std::nullopt;
    }
    std::set<std::string> requirementIds;
    for (std::size_t i = 0; i < requirements->size(); ++i) {
        const auto requirementPath = indexPath(childPath(path, "requirements"), i);
        const auto* requirement = object((*requirements)[i], diagnostics, requirementPath);
        if (requirement == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*requirement,
                      {"id", "kind", "interval", "judgementDomain", "requiredAction", "constraints",
                       "effects", "extensions"},
                      diagnostics, requirementPath);
        const auto* idField = field(*requirement, "id", diagnostics, requirementPath);
        const auto* kindField = field(*requirement, "kind", diagnostics, requirementPath);
        const auto* intervalField = field(*requirement, "interval", diagnostics, requirementPath);
        const auto* domainField =
            field(*requirement, "judgementDomain", diagnostics, requirementPath);
        const auto* actionField =
            field(*requirement, "requiredAction", diagnostics, requirementPath);
        const auto* constraintsField =
            field(*requirement, "constraints", diagnostics, requirementPath);
        const auto* effectsField = field(*requirement, "effects", diagnostics, requirementPath);
        if (!requireEmptyObject(*requirement, "extensions", diagnostics, requirementPath)) {
            return std::nullopt;
        }
        if (idField == nullptr || kindField == nullptr || intervalField == nullptr ||
            domainField == nullptr || actionField == nullptr || constraintsField == nullptr ||
            effectsField == nullptr) {
            return std::nullopt;
        }
        const auto* effects =
            array(*effectsField, diagnostics, childPath(requirementPath, "effects"));
        if (effects == nullptr) {
            return std::nullopt;
        }
        if (!effects->empty()) {
            addError(diagnostics, "cxt.v2.requirement_unsupported",
                     "Foundation requirements must not declare effects",
                     childPath(requirementPath, "effects"));
            return std::nullopt;
        }
        const auto requirementId = readId(*idField, diagnostics, childPath(requirementPath, "id"));
        const auto kind = string(*kindField);
        if (!requirementId || !kind || *kind != "tap" ||
            !isLiteralReference(*domainField, "judgement-domain", "candidate.lanes4", diagnostics,
                                childPath(requirementPath, "judgementDomain")) ||
            !isLiteralReference(*actionField, "action", "press", diagnostics,
                                childPath(requirementPath, "requiredAction"))) {
            addError(
                diagnostics, "cxt.v2.requirement_unsupported",
                "Foundation CXT v2 supports tap/point candidate.lanes4 press requirements only",
                requirementPath);
            return std::nullopt;
        }
        if (!requirementIds.insert(*requirementId).second) {
            addError(diagnostics, "cxt.v2.id_duplicate", "Requirement ID is duplicated",
                     childPath(requirementPath, "id"));
            return std::nullopt;
        }
        const auto* interval =
            object(*intervalField, diagnostics, childPath(requirementPath, "interval"));
        const auto* constraints =
            array(*constraintsField, diagnostics, childPath(requirementPath, "constraints"));
        if (interval == nullptr || constraints == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*interval, {"kind", "startBeat"}, diagnostics,
                      childPath(requirementPath, "interval"));
        const auto* intervalKind =
            field(*interval, "kind", diagnostics, childPath(requirementPath, "interval"));
        const auto* startBeat =
            field(*interval, "startBeat", diagnostics, childPath(requirementPath, "interval"));
        if (intervalKind == nullptr || startBeat == nullptr || !string(*intervalKind) ||
            *string(*intervalKind) != "point") {
            addError(diagnostics, "cxt.v2.requirement_unsupported",
                     "Foundation CXT v2 supports point intervals only",
                     childPath(requirementPath, "interval"));
            return std::nullopt;
        }
        if (constraints->size() != 1) {
            addError(diagnostics, "cxt.v2.requirement_unsupported",
                     "Foundation CXT v2 requires exactly one lane constraint",
                     childPath(requirementPath, "constraints"));
            return std::nullopt;
        }
        const auto constraintPath = indexPath(childPath(requirementPath, "constraints"), 0);
        const auto* constraint = object((*constraints)[0], diagnostics, constraintPath);
        if (constraint == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*constraint, {"kind", "value"}, diagnostics, constraintPath);
        const auto* constraintKind = field(*constraint, "kind", diagnostics, constraintPath);
        const auto* constraintValue = field(*constraint, "value", diagnostics, constraintPath);
        if (constraintKind == nullptr || constraintValue == nullptr || !string(*constraintKind) ||
            *string(*constraintKind) != "lane") {
            addError(diagnostics, "cxt.v2.requirement_unsupported",
                     "Foundation CXT v2 supports lane constraints only", constraintPath);
            return std::nullopt;
        }
        auto beat = parseSource(*startBeat, diagnostics,
                                childPath(childPath(requirementPath, "interval"), "startBeat"),
                                false, true, false);
        auto lane = parseSource(*constraintValue, diagnostics, childPath(constraintPath, "value"),
                                false, true, false);
        if (!beat || !lane) {
            return std::nullopt;
        }
        result.requirements.push_back(
            Requirement{*requirementId, std::move(*beat), std::move(*lane)});
    }
    return std::pair{*id, std::move(result)};
}

[[nodiscard]] auto parseRepeatCount(const Value& value, core::Diagnostics& diagnostics,
                                    std::string_view path) -> std::optional<Source> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    const auto* kindValue = field(*values, "kind", diagnostics, path);
    if (kindValue == nullptr || !string(*kindValue)) {
        addError(diagnostics, "cxt.v2.source_invalid", "Repeat count kind must be a string",
                 childPath(path, "kind"));
        return std::nullopt;
    }
    const auto kind = *string(*kindValue);
    if (kind == "literal") {
        rejectUnknown(*values, {"kind", "value"}, diagnostics, path);
        const auto* literal = field(*values, "value", diagnostics, path);
        if (literal == nullptr) {
            return std::nullopt;
        }
        const auto integerValue = integer(*literal);
        if (!integerValue) {
            addError(diagnostics, "cxt.v2.repeat_count_invalid",
                     "Repeat count must be a literal integer or integer parameter",
                     childPath(path, "value"));
            return std::nullopt;
        }
        return Source{SourceKind::Literal, ValueKind::Integer, *integerValue, {}, {}, {}, {}};
    }
    if (kind == "parameter") {
        rejectUnknown(*values, {"kind", "id"}, diagnostics, path);
        const auto* idValue = field(*values, "id", diagnostics, path);
        const auto id =
            idValue ? readId(*idValue, diagnostics, childPath(path, "id")) : std::nullopt;
        if (!id) {
            return std::nullopt;
        }
        return Source{SourceKind::Parameter, ValueKind::Integer, std::int64_t{0}, *id, {}, {}, {}};
    }
    addError(diagnostics, "cxt.v2.repeat_count_invalid",
             "Repeat count must be a literal integer or integer parameter", std::string{path});
    return std::nullopt;
}

[[nodiscard]] auto parseNode(const Value& value, core::Diagnostics& diagnostics,
                             std::string_view path, std::size_t& nodeCount,
                             const ChartLimits& limits, const std::set<std::string>& indexScope)
    -> std::optional<Node> {
    if (++nodeCount > limits.maxCxtV2Nodes) {
        addError(diagnostics, "cxt.v2.budget.nodes", "CXT v2 node budget exceeded",
                 std::string{path});
        return std::nullopt;
    }
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    const auto* opValue = field(*values, "op", diagnostics, path);
    const auto* idValue = field(*values, "nodeId", diagnostics, path);
    if (opValue == nullptr || idValue == nullptr || !string(*opValue)) {
        if (opValue != nullptr && !string(*opValue)) {
            addError(diagnostics, "cxt.v2.node_unsupported", "Pattern node op must be a string",
                     childPath(path, "op"));
        }
        return std::nullopt;
    }
    const auto id = readId(*idValue, diagnostics, childPath(path, "nodeId"));
    if (!id) {
        return std::nullopt;
    }
    Node node;
    node.nodeId = *id;
    if (*string(*opValue) == "emit") {
        rejectUnknown(*values, {"op", "nodeId", "prototype", "bindings", "parent"}, diagnostics,
                      path);
        const auto* protoValue = field(*values, "prototype", diagnostics, path);
        const auto* bindingsValue = field(*values, "bindings", diagnostics, path);
        const auto* parentValue = field(*values, "parent", diagnostics, path);
        const auto prototypeId = protoValue ? string(*protoValue) : std::nullopt;
        if (!prototypeId || !stableId(*prototypeId) || bindingsValue == nullptr ||
            parentValue == nullptr) {
            if (prototypeId && !stableId(*prototypeId)) {
                addError(diagnostics, "cxt.v2.id_invalid", "Expected a portable stable ID",
                         childPath(path, "prototype"));
            } else if (protoValue != nullptr && !prototypeId) {
                addError(diagnostics, "cxt.v2.prototype_missing", "Emit prototype must be a string",
                         childPath(path, "prototype"));
            }
            return std::nullopt;
        }
        node.op = Node::Op::Emit;
        node.prototype = std::string{*prototypeId};
        const auto* bindings = array(*bindingsValue, diagnostics, childPath(path, "bindings"));
        if (bindings == nullptr) {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < bindings->size(); ++i) {
            const auto bindingPath = indexPath(childPath(path, "bindings"), i);
            const auto* binding = object((*bindings)[i], diagnostics, bindingPath);
            if (binding == nullptr) {
                return std::nullopt;
            }
            rejectUnknown(*binding, {"slot", "source"}, diagnostics, bindingPath);
            const auto* slotValue = field(*binding, "slot", diagnostics, bindingPath);
            const auto* sourceValue = field(*binding, "source", diagnostics, bindingPath);
            if (slotValue == nullptr || sourceValue == nullptr) {
                return std::nullopt;
            }
            const auto slot = readId(*slotValue, diagnostics, childPath(bindingPath, "slot"));
            auto source = parseSource(*sourceValue, diagnostics, childPath(bindingPath, "source"),
                                      true, false, true);
            if (!slot || !source) {
                return std::nullopt;
            }
            if (!node.bindings.emplace(*slot, std::move(*source)).second) {
                addError(diagnostics, "cxt.v2.id_duplicate", "Slot binding is duplicated",
                         bindingPath);
                return std::nullopt;
            }
        }
        const auto* parent = object(*parentValue, diagnostics, childPath(path, "parent"));
        if (parent == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*parent, {"kind", "nodeId"}, diagnostics, childPath(path, "parent"));
        const auto* kindValue = field(*parent, "kind", diagnostics, childPath(path, "parent"));
        if (kindValue == nullptr || !string(*kindValue)) {
            addError(diagnostics, "cxt.v2.parent_invalid", "Parent kind must be a string",
                     childPath(path, "parent/kind"));
            return std::nullopt;
        }
        if (*string(*kindValue) == "root" || *string(*kindValue) == "invocation-parent") {
            if (parent->find("nodeId") != parent->end()) {
                addError(diagnostics, "cxt.v2.parent_invalid",
                         "Only emission parents may declare nodeId", childPath(path, "parent"));
                return std::nullopt;
            }
            node.parentKind = *string(*kindValue) == "root" ? Node::ParentKind::Root
                                                            : Node::ParentKind::Invocation;
        } else if (*string(*kindValue) == "emission") {
            const auto* targetValue =
                field(*parent, "nodeId", diagnostics, childPath(path, "parent"));
            const auto target =
                targetValue ? readId(*targetValue, diagnostics, childPath(path, "parent/nodeId"))
                            : std::nullopt;
            if (!target) {
                return std::nullopt;
            }
            node.parentKind = Node::ParentKind::Emission;
            node.parentNodeId = *target;
        } else {
            addError(diagnostics, "cxt.v2.parent_invalid", "Unsupported parent kind",
                     childPath(path, "parent/kind"));
            return std::nullopt;
        }
        return node;
    }
    if (*string(*opValue) != "repeat") {
        addError(diagnostics, "cxt.v2.node_unsupported", "Unsupported pattern node operation",
                 childPath(path, "op"));
        return std::nullopt;
    }
    rejectUnknown(*values, {"op", "nodeId", "count", "index", "body"}, diagnostics, path);
    const auto* countValue = field(*values, "count", diagnostics, path);
    const auto* indexValue = field(*values, "index", diagnostics, path);
    const auto* bodyValue = field(*values, "body", diagnostics, path);
    if (countValue == nullptr || indexValue == nullptr || bodyValue == nullptr) {
        return std::nullopt;
    }
    const auto indexId = readId(*indexValue, diagnostics, childPath(path, "index"));
    if (!indexId) {
        return std::nullopt;
    }
    if (indexScope.contains(*indexId)) {
        addError(diagnostics, "cxt.v2.index_shadowed", "Repeat index ID shadows an enclosing index",
                 childPath(path, "index"));
        return std::nullopt;
    }
    node.op = Node::Op::Repeat;
    node.indexId = *indexId;
    auto count = parseRepeatCount(*countValue, diagnostics, childPath(path, "count"));
    if (!count) {
        return std::nullopt;
    }
    node.count = std::move(*count);
    const auto* body = array(*bodyValue, diagnostics, childPath(path, "body"));
    if (body == nullptr || body->empty()) {
        addError(diagnostics, "cxt.v2.repeat_body_empty", "Repeat body must not be empty",
                 childPath(path, "body"));
        return std::nullopt;
    }
    auto nestedScope = indexScope;
    nestedScope.insert(node.indexId);
    std::set<std::string> ids;
    for (std::size_t i = 0; i < body->size(); ++i) {
        auto child = parseNode((*body)[i], diagnostics, indexPath(childPath(path, "body"), i),
                               nodeCount, limits, nestedScope);
        if (!child) {
            return std::nullopt;
        }
        if (!ids.insert(child->nodeId).second) {
            addError(diagnostics, "cxt.v2.id_duplicate", "Pattern node ID is duplicated",
                     indexPath(childPath(path, "body"), i));
            return std::nullopt;
        }
        node.body.push_back(std::move(*child));
    }
    return node;
}

[[nodiscard]] auto parseModule(const Value& root, core::Diagnostics& diagnostics,
                               const ChartLimits& limits) -> std::optional<Module> {
    const auto* values = object(root, diagnostics, "$");
    if (values == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*values,
                  {"format", "version", "moduleId", "moduleKind", "metadata", "parameters",
                   "prototypes", "patterns", "animations", "exports", "requiredExtensions",
                   "extensions"},
                  diagnostics, "$");
    const auto* format = field(*values, "format", diagnostics, "$");
    const auto* version = field(*values, "version", diagnostics, "$");
    if (format == nullptr || version == nullptr || !string(*format) ||
        *string(*format) != "cuexis.animation-template" || !integer(*version) ||
        *integer(*version) != 2) {
        addError(diagnostics, "cxt.v2.version_unsupported", "Expected CXT v2 module", "$/version");
        return std::nullopt;
    }
    if (limits.maxCxtV2Modules < 1) {
        addError(diagnostics, "cxt.v2.budget.modules", "CXT v2 module budget exceeded", "$");
        return std::nullopt;
    }
    const auto* moduleId = field(*values, "moduleId", diagnostics, "$");
    const auto* kind = field(*values, "moduleKind", diagnostics, "$");
    const auto* metadata = field(*values, "metadata", diagnostics, "$");
    const auto* parameters = field(*values, "parameters", diagnostics, "$");
    const auto* prototypes = field(*values, "prototypes", diagnostics, "$");
    const auto* patterns = field(*values, "patterns", diagnostics, "$");
    const auto* animations = field(*values, "animations", diagnostics, "$");
    const auto* exports = field(*values, "exports", diagnostics, "$");
    if (!requireEmptyArray(*values, "requiredExtensions", diagnostics, "$") ||
        !requireEmptyObject(*values, "extensions", diagnostics, "$")) {
        return std::nullopt;
    }
    if (moduleId == nullptr || kind == nullptr || metadata == nullptr || parameters == nullptr ||
        prototypes == nullptr || patterns == nullptr || animations == nullptr ||
        exports == nullptr) {
        return std::nullopt;
    }
    if (object(*metadata, diagnostics, "$/metadata") == nullptr) {
        return std::nullopt;
    }
    auto id = readId(*moduleId, diagnostics, "$/moduleId");
    if (!id || !string(*kind)) {
        if (kind != nullptr && !string(*kind)) {
            addError(diagnostics, "cxt.v2.module_kind_unsupported",
                     "Foundation supports prototype and pattern modules", "$/moduleKind");
        }
        return std::nullopt;
    }
    if (*string(*kind) != "prototype" && *string(*kind) != "pattern") {
        addError(diagnostics, "cxt.v2.module_kind_unsupported",
                 "Foundation supports prototype and pattern modules", "$/moduleKind");
        return std::nullopt;
    }
    Module module;
    module.id = *id;
    module.moduleKind = std::string{*string(*kind)};
    const auto* parameterArray = array(*parameters, diagnostics, "$/parameters");
    if (parameterArray == nullptr) {
        return std::nullopt;
    }
    if (parameterArray->size() > limits.maxCxtV2Parameters) {
        addError(diagnostics, "cxt.v2.budget.parameters", "Parameter budget exceeded",
                 "$/parameters");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < parameterArray->size(); ++i) {
        auto p = parseParameter((*parameterArray)[i], diagnostics, indexPath("$/parameters", i));
        if (!p) {
            return std::nullopt;
        }
        if (!module.parameters.emplace(p->first, std::move(p->second)).second) {
            addError(diagnostics, "cxt.v2.id_duplicate", "Parameter ID is duplicated",
                     indexPath("$/parameters", i));
            return std::nullopt;
        }
    }
    const auto* prototypeArray = array(*prototypes, diagnostics, "$/prototypes");
    if (prototypeArray == nullptr) {
        return std::nullopt;
    }
    if (prototypeArray->size() > limits.maxCxtV2Prototypes) {
        addError(diagnostics, "cxt.v2.budget.prototypes", "Prototype budget exceeded",
                 "$/prototypes");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < prototypeArray->size(); ++i) {
        auto p =
            parsePrototype((*prototypeArray)[i], diagnostics, indexPath("$/prototypes", i), limits);
        if (!p) {
            return std::nullopt;
        }
        if (!module.prototypes.emplace(p->first, std::move(p->second)).second) {
            addError(diagnostics, "cxt.v2.id_duplicate", "Prototype ID is duplicated",
                     indexPath("$/prototypes", i));
            return std::nullopt;
        }
    }
    const auto* patternArray = array(*patterns, diagnostics, "$/patterns");
    if (patternArray == nullptr) {
        return std::nullopt;
    }
    std::size_t nodeCount = 0;
    if (patternArray->size() > limits.maxCxtV2Patterns) {
        addError(diagnostics, "cxt.v2.budget.patterns", "Pattern budget exceeded", "$/patterns");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < patternArray->size(); ++i) {
        const auto path = indexPath("$/patterns", i);
        const auto* p = object((*patternArray)[i], diagnostics, path);
        if (p == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*p, {"id", "nodes", "extensions"}, diagnostics, path);
        const auto* idField = field(*p, "id", diagnostics, path);
        const auto* nodesField = field(*p, "nodes", diagnostics, path);
        if (!requireEmptyObject(*p, "extensions", diagnostics, path) || idField == nullptr ||
            nodesField == nullptr) {
            return std::nullopt;
        }
        auto pid = readId(*idField, diagnostics, childPath(path, "id"));
        const auto* nodes = array(*nodesField, diagnostics, childPath(path, "nodes"));
        if (!pid || nodes == nullptr) {
            return std::nullopt;
        }
        if (nodes->empty()) {
            addError(diagnostics, "cxt.v2.pattern_empty", "Pattern nodes must not be empty",
                     childPath(path, "nodes"));
            return std::nullopt;
        }
        Pattern pattern;
        pattern.id = *pid;
        std::set<std::string> topLevelNodeIds;
        for (std::size_t j = 0; j < nodes->size(); ++j) {
            auto n = parseNode((*nodes)[j], diagnostics, indexPath(childPath(path, "nodes"), j),
                               nodeCount, limits, {});
            if (!n) {
                return std::nullopt;
            }
            if (!topLevelNodeIds.insert(n->nodeId).second) {
                addError(diagnostics, "cxt.v2.id_duplicate", "Pattern node ID is duplicated",
                         indexPath(childPath(path, "nodes"), j));
                return std::nullopt;
            }
            pattern.nodes.push_back(std::move(*n));
        }
        if (!module.patterns.emplace(pattern.id, std::move(pattern)).second) {
            addError(diagnostics, "cxt.v2.id_duplicate", "Pattern ID is duplicated", path);
            return std::nullopt;
        }
    }
    const auto* animationArray = array(*animations, diagnostics, "$/animations");
    if (animationArray == nullptr) {
        return std::nullopt;
    }
    if (!animationArray->empty()) {
        addError(diagnostics, "cxt.v2.animation_unsupported",
                 "Foundation CXT v2 does not support animations", "$/animations");
        return std::nullopt;
    }
    const auto* exportArray = array(*exports, diagnostics, "$/exports");
    if (exportArray == nullptr || exportArray->size() != 1) {
        addError(diagnostics, "cxt.v2.exports_invalid", "Exactly one export is required",
                 "$/exports");
        return std::nullopt;
    }
    const auto* ex = object((*exportArray)[0], diagnostics, "$/exports/0");
    if (ex == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*ex, {"kind", "id"}, diagnostics, "$/exports/0");
    const auto* exportKind = field(*ex, "kind", diagnostics, "$/exports/0");
    const auto* exportId = field(*ex, "id", diagnostics, "$/exports/0");
    const auto exportKindText = exportKind ? string(*exportKind) : std::nullopt;
    const auto exportIdText =
        exportId ? readId(*exportId, diagnostics, "$/exports/0/id") : std::nullopt;
    if (!exportKindText || !exportIdText || *exportKindText != module.moduleKind) {
        addError(diagnostics, "cxt.v2.exports_invalid", "Export kind must match module kind",
                 "$/exports/0");
        return std::nullopt;
    }
    module.exportId = *exportIdText;
    if (module.moduleKind == "pattern" &&
        module.patterns.find(module.exportId) == module.patterns.end()) {
        addError(diagnostics, "cxt.v2.exports_invalid", "Export ID is not declared",
                 "$/exports/0/id");
        return std::nullopt;
    }
    if (module.moduleKind == "prototype" &&
        module.prototypes.find(module.exportId) == module.prototypes.end()) {
        addError(diagnostics, "cxt.v2.exports_invalid", "Export ID is not declared",
                 "$/exports/0/id");
        return std::nullopt;
    }
    if (module.moduleKind == "prototype" && !module.patterns.empty()) {
        addError(diagnostics, "cxt.v2.module_shape_invalid",
                 "Prototype modules must not declare patterns", "$/patterns");
        return std::nullopt;
    }
    return module;
}

using Bindings = std::map<std::string, CxtV2Value, std::less<>>;

[[nodiscard]] auto asInteger(const CxtV2Value& value) -> std::optional<std::int64_t> {
    if (const auto* result = std::get_if<std::int64_t>(&value))
        return *result;
    return std::nullopt;
}

[[nodiscard]] auto asBeat(const CxtV2Value& value) -> std::optional<RationalBeat> {
    if (const auto* result = std::get_if<RationalBeat>(&value)) {
        return *result;
    }
    if (const auto* integerValue = std::get_if<std::int64_t>(&value)) {
        auto beat = RationalBeat::create(*integerValue, 1);
        if (!beat) {
            return std::nullopt;
        }
        return *beat;
    }
    return std::nullopt;
}

[[nodiscard]] auto valueMatches(const CxtV2Value& value, ValueKind kind) noexcept -> bool {
    return (kind == ValueKind::Integer && std::holds_alternative<std::int64_t>(value)) ||
           (kind == ValueKind::Beat && std::holds_alternative<RationalBeat>(value));
}

[[nodiscard]] auto evalSource(const Source& source, const Bindings& parameters,
                              const Bindings& slots, const Bindings& indices,
                              core::Diagnostics& diagnostics) -> std::optional<CxtV2Value> {
    if (source.kind == SourceKind::Literal)
        return source.literal;
    const Bindings* scope = source.kind == SourceKind::Parameter ? &parameters
                            : source.kind == SourceKind::Slot    ? &slots
                                                                 : &indices;
    if (source.kind == SourceKind::Parameter || source.kind == SourceKind::Slot ||
        source.kind == SourceKind::Index) {
        const auto found = scope->find(source.id);
        if (found == scope->end()) {
            addError(diagnostics, "cxt.v2.reference_missing", "Value source reference is unbound",
                     source.id);
            return std::nullopt;
        }
        return found->second;
    }
    if (!source.input || !source.scale || !source.offset)
        return std::nullopt;
    auto input = evalSource(**source.input, parameters, slots, indices, diagnostics);
    auto scale = evalSource(**source.scale, parameters, slots, indices, diagnostics);
    auto offset = evalSource(**source.offset, parameters, slots, indices, diagnostics);
    if (!input || !scale || !offset)
        return std::nullopt;
    if (const auto i = asInteger(*input)) {
        const auto s = asInteger(*scale);
        const auto o = asInteger(*offset);
        if (s && o) {
            auto product = checkedMultiply(*i, *s);
            if (!product) {
                addError(diagnostics, std::string{product.error().code()},
                         std::string{product.error().message()}, "$");
                return std::nullopt;
            }
            auto sum = checkedAdd(*product, *o);
            if (!sum) {
                addError(diagnostics, std::string{sum.error().code()},
                         std::string{sum.error().message()}, "$");
                return std::nullopt;
            }
            return *sum;
        }
    }
    const auto i = asBeat(*input);
    const auto s = asBeat(*scale);
    const auto o = asBeat(*offset);
    if (!i || !s || !o) {
        addError(diagnostics, "cxt.v2.affine_type_mismatch", "Affine source types are incompatible",
                 "$");
        return std::nullopt;
    }
    auto product = multiplyRationalBeats(*i, *s);
    if (!product) {
        addError(diagnostics, std::string{product.error().code()},
                 std::string{product.error().message()}, "$");
        return std::nullopt;
    }
    auto result = addRationalBeats(*product, *o);
    if (!result) {
        addError(diagnostics, std::string{result.error().code()},
                 std::string{result.error().message()}, "$");
        return std::nullopt;
    }
    return *result;
}

[[nodiscard]] auto countFor(const Source& source, const Bindings& parameters,
                            core::Diagnostics& diagnostics) -> std::optional<std::size_t> {
    Bindings empty;
    auto value = evalSource(source, parameters, empty, empty, diagnostics);
    if (!value)
        return std::nullopt;
    const auto count = asInteger(*value);
    if (!count || *count < 0) {
        addError(diagnostics, "cxt.v2.repeat_count_invalid",
                 "Repeat count must be a non-negative integer", "$");
        return std::nullopt;
    }
    return static_cast<std::size_t>(*count);
}

struct ExpansionState final {
    CanonicalSemanticChart chart;
    std::size_t requirementCount{};
    struct PendingParent final {
        std::size_t entityIndex{};
        Node::ParentKind kind{Node::ParentKind::Invocation};
        std::string nodeId;
        std::vector<SemanticIdentityStep> scopePath;
    };
    std::vector<PendingParent> pendingParents;
};

[[nodiscard]] auto valueMatches(const CxtV2Value& value, const Slot& slot) -> bool {
    if (slot.kind == ValueKind::Integer) {
        return std::holds_alternative<std::int64_t>(value) &&
               inDeclaredRange(value, slot.minimum, slot.maximum);
    }
    return std::holds_alternative<RationalBeat>(value) &&
           inDeclaredRange(value, slot.minimum, slot.maximum);
}

[[nodiscard]] auto identityPathLess(const std::vector<SemanticIdentityStep>& left,
                                    const std::vector<SemanticIdentityStep>& right) noexcept
    -> bool {
    const auto count = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < count; ++index) {
        if (left[index].nodeId != right[index].nodeId)
            return left[index].nodeId < right[index].nodeId;
        if (left[index].iterationIndexPlusOne != right[index].iterationIndexPlusOne)
            return left[index].iterationIndexPlusOne < right[index].iterationIndexPlusOne;
    }
    return left.size() < right.size();
}

void expandNodes(const std::vector<Node>& nodes, const Module& module, const Pattern& pattern,
                 const CxtV2Invocation& invocation, const Bindings& parameters, Bindings& indices,
                 std::vector<SemanticIdentityStep>& path, ExpansionState& state,
                 core::Diagnostics& diagnostics, const ChartLimits& limits, std::size_t depth) {
    if (depth > limits.maxCxtV2RepeatDepth) {
        addError(diagnostics, "cxt.v2.budget.repeat_depth",
                 "Repeat depth exceeds configured budget", pattern.id);
        return;
    }
    std::vector<const Node*> ordered;
    ordered.reserve(nodes.size());
    for (const auto& node : nodes)
        ordered.push_back(&node);
    std::ranges::sort(ordered, {}, [](const Node* node) { return node->nodeId; });
    for (const auto* node : ordered) {
        if (node->op == Node::Op::Repeat) {
            auto count = countFor(node->count, parameters, diagnostics);
            if (!count)
                continue;
            if (*count > limits.maxCxtV2ExpansionEntities || *count > 100000000U) {
                addError(diagnostics, "cxt.v2.budget.expansion",
                         "Repeat expansion exceeds configured budget", node->nodeId);
                continue;
            }
            for (std::size_t i = 0; i < *count; ++i) {
                indices[node->indexId] = static_cast<std::int64_t>(i);
                path.push_back(
                    SemanticIdentityStep{node->nodeId, static_cast<std::uint32_t>(i + 1U)});
                expandNodes(node->body, module, pattern, invocation, parameters, indices, path,
                            state, diagnostics, limits, depth + 1U);
                path.pop_back();
            }
            indices.erase(node->indexId);
            continue;
        }
        const auto prototypeIt = module.prototypes.find(node->prototype);
        if (prototypeIt == module.prototypes.end()) {
            addError(diagnostics, "cxt.v2.prototype_missing", "Emit references a missing prototype",
                     node->prototype);
            continue;
        }
        const auto& prototype = prototypeIt->second;
        Bindings slots;
        for (const auto& [slotId, slot] : prototype.slots) {
            const auto binding = node->bindings.find(slotId);
            if (binding != node->bindings.end()) {
                auto value = evalSource(binding->second, parameters, slots, indices, diagnostics);
                if (value) {
                    if (valueMatches(*value, slot))
                        slots.emplace(slotId, *value);
                    else
                        addError(diagnostics, "cxt.v2.slot_type_invalid",
                                 "Slot binding type or range does not match declaration", slotId);
                }
            } else if (slot.defaultValue)
                slots.emplace(slotId, *slot.defaultValue);
            else if (slot.required)
                addError(diagnostics, "cxt.v2.slot_missing", "Required slot is not bound", slotId);
        }
        for (const auto& [slotId, unused] : node->bindings)
            if (prototype.slots.find(slotId) == prototype.slots.end()) {
                static_cast<void>(unused);
                addError(diagnostics, "cxt.v2.slot_unknown", "Emit binds an unknown slot", slotId);
            }
        const auto scopePath = path;
        path.push_back(SemanticIdentityStep{node->nodeId, 0U});
        CanonicalEntity entity;
        entity.identity = GeneratedEntityIdentity{invocation.chartId, invocation.bindingId,
                                                  module.id, invocation.exportId, path};
        if (node->parentKind == Node::ParentKind::Root)
            entity.parent = std::nullopt;
        else if (node->parentKind == Node::ParentKind::Invocation)
            entity.parent = invocation.parent;
        if (prototype.hasTransform) {
            CanonicalTransform transform;
            if (prototype.positionX) {
                if (auto value =
                        evalSource(*prototype.positionX, parameters, slots, indices, diagnostics)) {
                    if (auto x = asInteger(*value)) {
                        transform.position.x = static_cast<float>(*x);
                    } else {
                        addError(diagnostics, "cxt.v2.affine_type_mismatch",
                                 "Transform position[0] must evaluate to an integer", prototype.id);
                    }
                }
            }
            entity.components.emplace_back(transform);
        }
        for (const auto& requirement : prototype.requirements) {
            auto beat = evalSource(requirement.beat, parameters, slots, indices, diagnostics);
            auto lane = evalSource(requirement.lane, parameters, slots, indices, diagnostics);
            if (!beat || !lane)
                continue;
            auto localBeat = asBeat(*beat);
            auto laneValue = asInteger(*lane);
            if (!localBeat) {
                addError(diagnostics, "cxt.v2.affine_type_mismatch",
                         "Requirement startBeat must evaluate to a beat", requirement.id);
                continue;
            }
            if (!laneValue || *laneValue < 0 || *laneValue > 3) {
                addError(diagnostics, "cxt.v2.lane_out_of_range",
                         "lane must be in candidate.lanes4 range 0..3", requirement.id);
                continue;
            }
            auto absolute = addRationalBeats(*localBeat, invocation.startBeat);
            if (!absolute) {
                addError(diagnostics, std::string{absolute.error().code()},
                         std::string{absolute.error().message()}, requirement.id);
                continue;
            }
            CanonicalRequirement req;
            req.localId = requirement.id;
            req.kind = CanonicalRequirementKind::Tap;
            req.interval.startBeat = *absolute;
            req.judgementDomain = {"judgement-domain", "candidate.lanes4"};
            req.requiredAction = {"action", "press"};
            req.constraints.emplace_back(LaneConstraint{static_cast<std::uint32_t>(*laneValue)});
            entity.requirements.push_back(std::move(req));
            ++state.requirementCount;
        }
        if (state.requirementCount > limits.maxCxtV2ExpansionRequirements) {
            addError(diagnostics, "cxt.v2.budget.requirements",
                     "Expanded requirement count exceeds configured budget", node->nodeId);
        }
        const auto entityIndex = state.chart.entities.size();
        state.chart.entities.push_back(std::move(entity));
        if (node->parentKind == Node::ParentKind::Emission)
            state.pendingParents.push_back(
                {entityIndex, node->parentKind, node->parentNodeId, scopePath});
        path.pop_back();
    }
}

} // namespace

auto CxtV2Loader::expand(std::string_view jsonText, const CxtV2Invocation& invocation,
                         const ChartLimits& limits) -> CxtV2ExpansionResult {
    CxtV2ExpansionResult result;
    core::Diagnostics diagnostics{limits.maxDiagnostics,
                                  core::Diagnostic{core::DiagnosticSeverity::Error,
                                                   "cxt.v2.diagnostics.limit",
                                                   "CXT v2 diagnostic limit reached"}};
    const auto inputLimit = std::min(limits.maxInputBytes, limits.maxAnimationTemplateBytes);
    auto parsed = json::parse(
        jsonText, json::ParseLimits{inputLimit, limits.maxNestingDepth, limits.maxStringBytes});
    if (!parsed) {
        addError(diagnostics, std::string{parsed.error().code()},
                 std::string{parsed.error().message()}, "$");
        result.diagnostics = std::move(diagnostics);
        return result;
    }
    auto module = parseModule(*parsed, diagnostics, limits);
    if (!module) {
        result.diagnostics = std::move(diagnostics);
        return result;
    }
    if (module->id != invocation.moduleId) {
        addError(diagnostics, "cxt.v2.module_mismatch", "Invocation module does not match moduleId",
                 "$/moduleId");
    }
    Bindings parameters;
    for (const auto& [id, parameter] : module->parameters)
        parameters.emplace(id, parameter.defaultValue);
    std::set<std::string> supplied;
    for (const auto& binding : invocation.parameters) {
        const auto parameter = module->parameters.find(binding.id);
        if (parameter == module->parameters.end() || !supplied.insert(binding.id).second) {
            addError(diagnostics, "cxt.v2.parameter_invalid",
                     "Unknown or duplicate parameter binding", binding.id);
            continue;
        }
        parameters[binding.id] = binding.value;
    }
    if (invocation.exportId.empty() || invocation.exportId != module->exportId)
        addError(diagnostics, "cxt.v2.export_mismatch",
                 "Invocation export does not match module export", "$/exports/0/id");
    for (const auto& binding : invocation.parameters) {
        const auto parameter = module->parameters.find(binding.id);
        if (parameter != module->parameters.end()) {
            if (!valueMatches(binding.value, parameter->second.kind)) {
                addError(diagnostics, "cxt.v2.parameter_type_invalid",
                         "Parameter binding type does not match declaration", binding.id);
                continue;
            }
            if (!inDeclaredRange(binding.value, parameter->second.minimum,
                                 parameter->second.maximum)) {
                addError(diagnostics, "cxt.v2.parameter_range_invalid",
                         "Parameter binding is outside the declared range", binding.id);
            }
        }
    }
    Pattern syntheticPattern;
    const Pattern* pattern = nullptr;
    if (module->moduleKind == "pattern") {
        if (!invocation.slotBindings.empty())
            addError(diagnostics, "cxt.v2.slot_bindings_invalid",
                     "Pattern invocations must not provide slot bindings", "$/slotBindings");
        const auto patternIt = module->patterns.find(module->exportId);
        if (patternIt == module->patterns.end()) {
            addError(diagnostics, "cxt.v2.exports_invalid", "Pattern export is missing",
                     "$/exports/0/id");
        } else {
            pattern = &patternIt->second;
        }
    } else if (module->moduleKind == "prototype") {
        if (module->prototypes.find(module->exportId) == module->prototypes.end()) {
            addError(diagnostics, "cxt.v2.exports_invalid", "Prototype export is missing",
                     "$/exports/0/id");
        } else {
            Node direct;
            direct.op = Node::Op::Emit;
            direct.nodeId = "__direct__";
            direct.prototype = module->exportId;
            direct.parentKind =
                invocation.parent ? Node::ParentKind::Invocation : Node::ParentKind::Root;
            std::set<std::string> suppliedSlots;
            for (const auto& binding : invocation.slotBindings) {
                const auto slotIt = module->prototypes.at(module->exportId).slots.find(binding.id);
                if (slotIt == module->prototypes.at(module->exportId).slots.end() ||
                    !suppliedSlots.insert(binding.id).second) {
                    addError(diagnostics, "cxt.v2.slot_invalid",
                             "Unknown or duplicate prototype slot binding", binding.id);
                    continue;
                }
                if (!valueMatches(binding.value, slotIt->second)) {
                    addError(diagnostics, "cxt.v2.slot_type_invalid",
                             "Slot binding type does not match declaration", binding.id);
                    continue;
                }
                direct.bindings.emplace(
                    binding.id,
                    Source{
                        SourceKind::Literal, slotIt->second.kind, binding.value, {}, {}, {}, {}});
            }
            syntheticPattern.id = "__direct__";
            syntheticPattern.nodes.push_back(std::move(direct));
            pattern = &syntheticPattern;
        }
    } else {
        addError(diagnostics, "cxt.v2.module_kind_unsupported",
                 "Foundation does not expand animation modules", "$/moduleKind");
    }
    if (pattern == nullptr) {
        result.diagnostics = std::move(diagnostics);
        return result;
    }
    // Preflight uses checked products so Repeat count does not allocate semantic entities.
    struct Estimate final {
        std::size_t entities{};
        std::size_t requirements{};
    };
    std::function<Estimate(const std::vector<Node>&, std::size_t)> estimate =
        [&](const std::vector<Node>& nodes, std::size_t depth) -> Estimate {
        Estimate total;
        if (depth > limits.maxCxtV2RepeatDepth) {
            addError(diagnostics, "cxt.v2.budget.repeat_depth",
                     "Repeat depth exceeds configured budget", "$/patterns");
            return total;
        }
        for (const auto& node : nodes) {
            if (node.op == Node::Op::Emit) {
                const auto prototypeIt = module->prototypes.find(node.prototype);
                if (prototypeIt == module->prototypes.end()) {
                    addError(diagnostics, "cxt.v2.prototype_missing",
                             "Emit references a missing prototype", node.prototype);
                    continue;
                }
                if (total.entities == std::numeric_limits<std::size_t>::max() ||
                    total.requirements > std::numeric_limits<std::size_t>::max() -
                                             prototypeIt->second.requirements.size()) {
                    addError(diagnostics, "cxt.v2.budget.expansion",
                             "Expanded entity count exceeds configured budget", "$/patterns");
                    return total;
                }
                total.entities += 1U;
                total.requirements += prototypeIt->second.requirements.size();
                continue;
            }
            auto count = countFor(node.count, parameters, diagnostics);
            if (!count) {
                continue;
            }
            const auto body = estimate(node.body, depth + 1U);
            if (*count != 0 &&
                (body.entities > std::numeric_limits<std::size_t>::max() / *count ||
                 body.requirements > std::numeric_limits<std::size_t>::max() / *count)) {
                addError(diagnostics, "cxt.v2.budget.expansion",
                         "Expanded entity count exceeds configured budget", node.nodeId);
                return total;
            }
            const auto scaledEntities = body.entities * *count;
            const auto scaledRequirements = body.requirements * *count;
            if (total.entities > std::numeric_limits<std::size_t>::max() - scaledEntities ||
                total.requirements > std::numeric_limits<std::size_t>::max() - scaledRequirements) {
                addError(diagnostics, "cxt.v2.budget.expansion",
                         "Expanded entity count exceeds configured budget", node.nodeId);
                return total;
            }
            total.entities += scaledEntities;
            total.requirements += scaledRequirements;
        }
        return total;
    };
    const auto estimated = estimate(pattern->nodes, 0U);
    result.counts.entityCount = estimated.entities;
    result.counts.requirementCount = estimated.requirements;
    if (estimated.entities > limits.maxCxtV2ExpansionEntities) {
        addError(diagnostics, "cxt.v2.budget.expansion",
                 "Expanded entity count exceeds configured budget", "$/patterns");
    }
    if (estimated.requirements > limits.maxCxtV2ExpansionRequirements) {
        addError(diagnostics, "cxt.v2.budget.requirements",
                 "Expanded requirement count exceeds configured budget", "$/patterns");
    }
    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        result.diagnostics = std::move(diagnostics);
        return result;
    }
    ExpansionState state;
    state.chart.chartId = invocation.chartId;
    Bindings indices;
    std::vector<SemanticIdentityStep> path;
    expandNodes(pattern->nodes, *module, *pattern, invocation, parameters, indices, path, state,
                diagnostics, limits, 0U);

    // Resolve sibling emission parents after all entities exist, allowing forward references.
    for (const auto& pending : state.pendingParents) {
        std::vector<SemanticIdentityStep> targetPath = pending.scopePath;
        targetPath.push_back(SemanticIdentityStep{pending.nodeId, 0U});
        std::optional<CanonicalEntityIdentity> target;
        for (const auto& entity : state.chart.entities) {
            const auto* generated = std::get_if<GeneratedEntityIdentity>(&entity.identity);
            if (generated != nullptr && generated->path == targetPath) {
                target = entity.identity;
                break;
            }
        }
        if (!target) {
            addError(diagnostics, "cxt.v2.parent_missing",
                     "Emission parent does not resolve in the same scope", pending.nodeId);
        } else if (pending.entityIndex < state.chart.entities.size()) {
            state.chart.entities[pending.entityIndex].parent = *target;
        }
    }
    std::vector<std::size_t> parentIndices(state.chart.entities.size(),
                                           std::numeric_limits<std::size_t>::max());
    for (std::size_t i = 0; i < state.chart.entities.size(); ++i) {
        const auto& parent = state.chart.entities[i].parent;
        if (!parent)
            continue;
        for (std::size_t j = 0; j < state.chart.entities.size(); ++j) {
            if (state.chart.entities[j].identity == *parent) {
                parentIndices[i] = j;
                break;
            }
        }
    }
    std::vector<std::uint8_t> visit(state.chart.entities.size(), 0U);
    std::function<void(std::size_t)> checkParent = [&](std::size_t index) {
        if (visit[index] == 1U) {
            addError(diagnostics, "cxt.v2.parent_cycle", "Generated parent graph contains a cycle",
                     "$/patterns");
            return;
        }
        if (visit[index] == 2U)
            return;
        visit[index] = 1U;
        if (parentIndices[index] != std::numeric_limits<std::size_t>::max())
            checkParent(parentIndices[index]);
        visit[index] = 2U;
    };
    for (std::size_t i = 0; i < visit.size(); ++i)
        checkParent(i);
    std::ranges::sort(
        state.chart.entities, [](const CanonicalEntity& left, const CanonicalEntity& right) {
            return identityPathLess(std::get<GeneratedEntityIdentity>(left.identity).path,
                                    std::get<GeneratedEntityIdentity>(right.identity).path);
        });
    result.counts.entityCount = state.chart.entities.size();
    result.counts.requirementCount = state.requirementCount;
    diagnostics.sortDeterministically();
    if (!diagnostics.hasErrors())
        result.chart = std::move(state.chart);
    result.diagnostics = std::move(diagnostics);
    return result;
}

} // namespace cuexis::chart
