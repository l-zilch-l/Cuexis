#include "chart_v4_loader_helpers_internal.hpp"

#include "chart_project_path_internal.hpp"
#include "chart_v4_common_reader_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart::loader_internal {
namespace {

[[nodiscard]] auto parameterTypeName(ChartParameterType type) -> std::string_view {
    switch (type) {
    case ChartParameterType::Number:
        return "number";
    case ChartParameterType::Rational:
        return "rational";
    case ChartParameterType::Weight:
        return "weight";
    }
    return "unknown";
}

[[nodiscard]] auto readParameterType(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<ChartParameterType> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "number") {
        return ChartParameterType::Number;
    }
    if (*value == "rational") {
        return ChartParameterType::Rational;
    }
    if (*value == "weight") {
        return ChartParameterType::Weight;
    }
    detail::addV4Error(diagnostics, "chart.parameter.type_mismatch",
                       "Chart parameter type is unsupported", std::string{reader.fieldPath()});
    return std::nullopt;
}

[[nodiscard]] auto readFiniteNumber(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<double> {
    const auto value = reader.readNumber();
    if (!value) {
        return std::nullopt;
    }
    if (!std::isfinite(*value)) {
        detail::addV4Error(diagnostics, "chart.parameter.out_of_range",
                           "Chart parameter value must be finite", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] auto readParameterValue(const json::Reader& reader, ChartParameterType type,
                                      const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<ChartParameterValue> {
    if (type == ChartParameterType::Rational) {
        const auto value = detail::readV4Rational(reader, limits, diagnostics, false, true);
        return value ? std::optional<ChartParameterValue>{ChartParameterValue{*value}}
                     : std::nullopt;
    }
    const auto value = readFiniteNumber(reader, diagnostics);
    if (!value) {
        return std::nullopt;
    }
    if (type == ChartParameterType::Weight && (*value < 0.0 || *value > 1.0)) {
        detail::addV4Error(diagnostics, "chart.parameter.out_of_range",
                           "Weight parameter value must be within [0, 1]",
                           std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return ChartParameterValue{*value};
}

[[nodiscard]] auto compareParameterValues(const ChartParameterValue& left,
                                          const ChartParameterValue& right)
    -> std::optional<std::strong_ordering> {
    if (const auto* leftNumber = std::get_if<double>(&left)) {
        const auto* rightNumber = std::get_if<double>(&right);
        if (rightNumber == nullptr) {
            return std::nullopt;
        }
        if (*leftNumber < *rightNumber) {
            return std::strong_ordering::less;
        }
        if (*leftNumber > *rightNumber) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }
    const auto* leftRational = std::get_if<RationalBeat>(&left);
    const auto* rightRational = std::get_if<RationalBeat>(&right);
    if (leftRational == nullptr || rightRational == nullptr) {
        return std::nullopt;
    }
    return *leftRational <=> *rightRational;
}

void validateConstraints(const ChartParameterDeclaration& declaration,
                         core::Diagnostics& diagnostics) {
    const auto& constraints = declaration.constraints;
    const auto validatePair = [&](const std::optional<ChartParameterValue>& lower,
                                  const std::optional<ChartParameterValue>& upper, bool strict,
                                  std::string_view message) {
        if (!lower || !upper) {
            return;
        }
        const auto order = compareParameterValues(*lower, *upper);
        if (!order || *order == std::strong_ordering::greater ||
            (strict && *order == std::strong_ordering::equal)) {
            detail::addV4Error(diagnostics, "chart.parameter.out_of_range", std::string{message},
                               declaration.fieldPath + "/constraints");
        }
    };
    validatePair(constraints.minimum, constraints.maximum, false,
                 "Parameter minimum exceeds maximum");
    validatePair(constraints.minimum, constraints.exclusiveMaximum, true,
                 "Parameter minimum is not below exclusiveMaximum");
    validatePair(constraints.exclusiveMinimum, constraints.maximum, true,
                 "Parameter exclusiveMinimum is not below maximum");
    validatePair(constraints.exclusiveMinimum, constraints.exclusiveMaximum, true,
                 "Parameter exclusive bounds are empty");

    if (!declaration.defaultValue) {
        return;
    }
    const auto violatesLower = [&](const std::optional<ChartParameterValue>& bound, bool strict) {
        if (!bound) {
            return false;
        }
        const auto order = compareParameterValues(*declaration.defaultValue, *bound);
        return !order || *order == std::strong_ordering::less ||
               (strict && *order == std::strong_ordering::equal);
    };
    const auto violatesUpper = [&](const std::optional<ChartParameterValue>& bound, bool strict) {
        if (!bound) {
            return false;
        }
        const auto order = compareParameterValues(*declaration.defaultValue, *bound);
        return !order || *order == std::strong_ordering::greater ||
               (strict && *order == std::strong_ordering::equal);
    };
    if (violatesLower(constraints.minimum, false) ||
        violatesLower(constraints.exclusiveMinimum, true) ||
        violatesUpper(constraints.maximum, false) ||
        violatesUpper(constraints.exclusiveMaximum, true)) {
        detail::addV4Error(diagnostics, "chart.parameter.out_of_range",
                           "Chart parameter default is outside its constraints",
                           declaration.fieldPath + "/default");
    }
}

} // namespace

[[nodiscard]] auto readParameters(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics)
    -> std::vector<ChartParameterDeclaration> {
    std::vector<ChartParameterDeclaration> result;
    const auto* items = reader.readArray();
    if (items == nullptr) {
        return result;
    }
    if (items->size() > limits.maxChartParameters) {
        detail::addV4Error(diagnostics, "chart.parameter.out_of_range",
                           "Chart parameter declaration count exceeds the configured limit",
                           std::string{reader.fieldPath()});
    }
    std::set<std::string, std::less<>> ids;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{std::string_view{"id"}, std::string_view{"type"},
                                    std::string_view{"default"}, std::string_view{"constraints"}};
        item->rejectUnknownFields(fields);
        const auto idReader = item->requiredField("id");
        const auto typeReader = item->requiredField("type");
        const auto constraintsReader = item->requiredField("constraints");
        const auto id = idReader ? detail::readPortableStableId(*idReader, limits, diagnostics,
                                                                "Chart parameter ID")
                                 : std::nullopt;
        const auto type = typeReader ? readParameterType(*typeReader, diagnostics) : std::nullopt;
        if (!id || !type || !constraintsReader || constraintsReader->readObject() == nullptr) {
            continue;
        }
        if (!ids.emplace(*id).second) {
            detail::addV4Error(diagnostics, "chart.parameter.duplicate",
                               "Chart parameter ID is duplicated",
                               std::string{idReader->fieldPath()});
        }
        constexpr std::array constraintFields{
            std::string_view{"minimum"}, std::string_view{"exclusiveMinimum"},
            std::string_view{"maximum"}, std::string_view{"exclusiveMaximum"}};
        constraintsReader->rejectUnknownFields(constraintFields);
        ChartParameterConstraints constraints;
        const auto readConstraint =
            [&](std::string_view name) -> std::optional<ChartParameterValue> {
            const auto field = constraintsReader->optionalField(name);
            return field ? readParameterValue(*field, *type, limits, diagnostics) : std::nullopt;
        };
        constraints.minimum = readConstraint("minimum");
        constraints.exclusiveMinimum = readConstraint("exclusiveMinimum");
        constraints.maximum = readConstraint("maximum");
        constraints.exclusiveMaximum = readConstraint("exclusiveMaximum");
        const auto defaultReader = item->optionalField("default");
        const auto defaultValue =
            defaultReader ? readParameterValue(*defaultReader, *type, limits, diagnostics)
                          : std::nullopt;
        auto declaration = ChartParameterDeclaration{*id, *type, defaultValue, constraints,
                                                     std::string{item->fieldPath()}};
        validateConstraints(declaration, diagnostics);
        result.push_back(std::move(declaration));
    }
    std::ranges::sort(result, {}, &ChartParameterDeclaration::id);
    return result;
}

[[nodiscard]] auto readImports(const json::Reader& reader, const ChartLimits& limits,
                               core::Diagnostics& diagnostics)
    -> std::vector<AnimationTemplateImport> {
    std::vector<AnimationTemplateImport> result;
    const auto* items = reader.readArray();
    if (items == nullptr) {
        return result;
    }
    if (items->size() > limits.maxAnimationImports) {
        detail::addV4Error(diagnostics, "cxt.budget.exceeded",
                           "Animation template import count exceeds the configured limit",
                           std::string{reader.fieldPath()});
    }
    std::set<std::string, std::less<>> ids;
    std::set<std::string, std::less<>> paths;
    std::set<std::string, std::less<>> foldedPaths;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{std::string_view{"id"}, std::string_view{"source"}};
        item->rejectUnknownFields(fields);
        const auto idReader = item->requiredField("id");
        const auto sourceReader = item->requiredField("source");
        const auto id = idReader ? detail::readPortableStableId(*idReader, limits, diagnostics,
                                                                "Animation template import ID")
                                 : std::nullopt;
        const auto source = sourceReader ? sourceReader->readString() : std::nullopt;
        if (!id || !source) {
            continue;
        }
        if (!detail::isCxtProjectPath(*source)) {
            detail::addV4Error(
                diagnostics, "cxt.template.invalid",
                "Animation template source path must be a portable lowercase .cxt path",
                std::string{sourceReader->fieldPath()});
            continue;
        }
        if (!ids.emplace(*id).second || !paths.emplace(*source).second ||
            !foldedPaths.emplace(detail::portableProjectPathCaseKey(*source)).second) {
            detail::addV4Error(diagnostics, "cxt.import.duplicate",
                               "Animation template import ID or source is duplicated",
                               std::string{item->fieldPath()});
        }
        result.push_back(
            AnimationTemplateImport{*id, std::string{*source}, std::string{item->fieldPath()}});
    }
    std::ranges::sort(result, {}, &AnimationTemplateImport::id);
    return result;
}

void validateParameterUses(const std::vector<ChartParameterDeclaration>& parameters,
                           const std::vector<ParameterUse>& uses, core::Diagnostics& diagnostics) {
    std::map<std::string, ChartParameterType, std::less<>> types;
    for (const auto& parameter : parameters) {
        types.insert_or_assign(parameter.id, parameter.type);
    }
    for (const auto& use : uses) {
        const auto iterator = types.find(use.id);
        if (iterator == types.end()) {
            detail::addV4Error(diagnostics, "chart.parameter.unknown",
                               "ChartParameterRef refers to an unknown declaration", use.fieldPath);
            continue;
        }
        if (iterator->second != use.expectedType) {
            detail::addV4Error(
                diagnostics, "chart.parameter.type_mismatch",
                "ChartParameterRef expects " + std::string{parameterTypeName(use.expectedType)} +
                    " but declaration is " + std::string{parameterTypeName(iterator->second)},
                use.fieldPath);
        }
    }
}

} // namespace cuexis::chart::loader_internal
