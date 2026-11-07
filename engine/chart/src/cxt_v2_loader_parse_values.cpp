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

[[nodiscard]] auto parseSource(const Value& value, core::Diagnostics& diagnostics,
                               std::string_view path, bool allowIndex, bool allowSlot,
                               bool allowParameter) -> std::optional<Source> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    const auto* kindValue = field(*values, "kind", diagnostics, path);
    if (kindValue == nullptr || !string(*kindValue)) {
        addCxtError(diagnostics, "cxt.v2.source_invalid", "Value source kind must be a string",
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
            addCxtError(diagnostics, "cxt.v2.literal_invalid",
                        "Expected an integer or beat literal", childPath(path, "value"));
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
            addCxtError(diagnostics, "cxt.v2.source_scope_invalid",
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
            addCxtError(diagnostics, "cxt.v2.affine_invalid",
                        "Affine inputs must be non-nested atoms", std::string{path});
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
    addCxtError(diagnostics, "cxt.v2.source_invalid", "Unsupported value source kind",
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
        addCxtError(diagnostics, "cxt.v2.parameter_type_unsupported",
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
            addCxtError(diagnostics, "cxt.v2.parameter_invalid",
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
            addCxtError(diagnostics, "cxt.v2.parameter_invalid",
                        "Beat parameter bounds must be valid", std::string{path});
            return std::nullopt;
        }
    }
    if (!inDeclaredRange(parameter.defaultValue, parameter.minimum, parameter.maximum)) {
        addCxtError(diagnostics, "cxt.v2.parameter_invalid",
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
            addCxtError(diagnostics, "cxt.v2.slot_invalid", "Slot required flag must be a boolean",
                        childPath(path, "required"));
        }
        return std::nullopt;
    }
    if (*type != "integer" && *type != "beat") {
        addCxtError(diagnostics, "cxt.v2.slot_type_unsupported",
                    "Foundation CXT v2 supports integer and beat slots only",
                    childPath(path, "type"));
        return std::nullopt;
    }
    Slot result{*id,          *type == "integer" ? ValueKind::Integer : ValueKind::Beat,
                *required,    std::nullopt,
                std::nullopt, std::nullopt};
    const auto defaultIt = values->find("default");
    if (*required && defaultIt != values->end()) {
        addCxtError(diagnostics, "cxt.v2.slot_default_invalid",
                    "A required slot must not declare a default", childPath(path, "default"));
        return std::nullopt;
    }
    if (!*required && defaultIt == values->end()) {
        addCxtError(diagnostics, "cxt.v2.slot_default_missing",
                    "An optional slot must declare a literal default", std::string{path});
        return std::nullopt;
    }
    if (defaultIt != values->end()) {
        if (result.kind == ValueKind::Integer) {
            if (const auto parsed = integer(defaultIt->second)) {
                result.defaultValue = *parsed;
            } else {
                addCxtError(diagnostics, "cxt.v2.slot_default_invalid",
                            "Slot default type is invalid", childPath(path, "default"));
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
                addCxtError(diagnostics, "cxt.v2.slot_range_invalid",
                            "Slot integer bounds are invalid", childPath(path, "minimum"));
                return std::nullopt;
            }
        }
        if (maximumIt != values->end()) {
            result.maximum = integer(maximumIt->second);
            if (!result.maximum) {
                addCxtError(diagnostics, "cxt.v2.slot_range_invalid",
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
        addCxtError(diagnostics, "cxt.v2.slot_range_invalid", "Slot bounds are invalid",
                    std::string{path});
        return std::nullopt;
    }
    if (result.defaultValue &&
        !inDeclaredRange(*result.defaultValue, result.minimum, result.maximum)) {
        addCxtError(diagnostics, "cxt.v2.slot_default_invalid",
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

} // namespace cuexis::chart
