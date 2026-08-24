#include "chart_parameter_resolver_internal.hpp"

#include "sha256_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::chart::detail {
namespace {

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(path)});
}

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

[[nodiscard]] auto valueMatchesType(const ChartParameterValue& value, ChartParameterType type)
    -> bool {
    if (type == ChartParameterType::Rational) {
        return std::holds_alternative<RationalBeat>(value);
    }
    return std::holds_alternative<double>(value);
}

[[nodiscard]] auto normalizeValue(ChartParameterValue value, ChartParameterType type,
                                  core::Diagnostics& diagnostics, std::string_view path)
    -> std::optional<ChartParameterValue> {
    if (!valueMatchesType(value, type)) {
        addError(diagnostics, "chart.parameter.type_mismatch",
                 "Chart parameter input does not match its declared type", std::string{path});
        return std::nullopt;
    }
    if (type == ChartParameterType::Rational) {
        return value;
    }
    auto number = std::get<double>(value);
    if (!std::isfinite(number)) {
        addError(diagnostics, "chart.parameter.out_of_range",
                 "Chart parameter input must be finite", std::string{path});
        return std::nullopt;
    }
    if (number == 0.0) {
        number = 0.0;
    }
    if (type == ChartParameterType::Weight && (number < 0.0 || number > 1.0)) {
        addError(diagnostics, "chart.parameter.out_of_range",
                 "Weight parameter input must be within [0, 1]", std::string{path});
        return std::nullopt;
    }
    return ChartParameterValue{number};
}

[[nodiscard]] auto compareValues(const ChartParameterValue& left, const ChartParameterValue& right)
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

[[nodiscard]] auto withinConstraints(const ChartParameterValue& value,
                                     const ChartParameterConstraints& constraints) -> bool {
    const auto violatesLower = [&](const std::optional<ChartParameterValue>& bound, bool strict) {
        if (!bound) {
            return false;
        }
        const auto order = compareValues(value, *bound);
        return !order || *order == std::strong_ordering::less ||
               (strict && *order == std::strong_ordering::equal);
    };
    const auto violatesUpper = [&](const std::optional<ChartParameterValue>& bound, bool strict) {
        if (!bound) {
            return false;
        }
        const auto order = compareValues(value, *bound);
        return !order || *order == std::strong_ordering::greater ||
               (strict && *order == std::strong_ordering::equal);
    };
    return !violatesLower(constraints.minimum, false) &&
           !violatesLower(constraints.exclusiveMinimum, true) &&
           !violatesUpper(constraints.maximum, false) &&
           !violatesUpper(constraints.exclusiveMaximum, true);
}

void writeU32(Sha256& hash, std::uint32_t value) noexcept {
    const std::array bytes{static_cast<std::byte>(value & 0xFFU),
                           static_cast<std::byte>((value >> 8U) & 0xFFU),
                           static_cast<std::byte>((value >> 16U) & 0xFFU),
                           static_cast<std::byte>((value >> 24U) & 0xFFU)};
    hash.update(bytes);
}

void writeU64(Sha256& hash, std::uint64_t value) noexcept {
    std::array<std::byte, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>(value >> (index * 8U));
    }
    hash.update(bytes);
}

[[nodiscard]] auto parameterIdentity(const std::vector<ResolvedChartParameter>& values)
    -> CanonicalContentIdentity {
    Sha256 hash;
    static constexpr char domain[] = "cuexis.parameter-set.v1";
    hash.update(std::as_bytes(std::span{domain, sizeof(domain)}));
    for (const auto& parameter : values) {
        writeU32(hash, static_cast<std::uint32_t>(parameter.id.size()));
        hash.update(std::as_bytes(std::span{parameter.id.data(), parameter.id.size()}));
        const auto typeTag = parameter.type == ChartParameterType::Number     ? 0x01U
                             : parameter.type == ChartParameterType::Rational ? 0x02U
                                                                              : 0x03U;
        const std::array typeByte{static_cast<std::byte>(typeTag)};
        hash.update(typeByte);
        if (parameter.type == ChartParameterType::Rational) {
            const auto& rational = std::get<RationalBeat>(parameter.value);
            writeU64(hash, std::bit_cast<std::uint64_t>(rational.numerator()));
            writeU64(hash, std::bit_cast<std::uint64_t>(rational.denominator()));
        } else {
            auto number = std::get<double>(parameter.value);
            if (number == 0.0) {
                number = 0.0;
            }
            writeU64(hash, std::bit_cast<std::uint64_t>(number));
        }
    }
    return CanonicalContentIdentity{hash.finish()};
}

} // namespace

auto resolveChartParameters(const std::vector<ChartParameterDeclaration>& declarations,
                            const std::vector<ParameterUse>& uses,
                            std::span<const ChartParameterInput> inputs, const ChartLimits& limits,
                            core::Diagnostics& diagnostics) -> std::optional<ResolvedParameterSet> {
    if (declarations.size() > limits.maxChartParameters ||
        inputs.size() > limits.maxChartParameters) {
        addError(diagnostics, "chart.parameter.out_of_range",
                 "Chart parameter count exceeds the configured limit", "$/parameters");
    }

    std::map<std::string, const ChartParameterDeclaration*, std::less<>> declarationById;
    for (const auto& declaration : declarations) {
        if (!declarationById.emplace(declaration.id, &declaration).second) {
            addError(diagnostics, "chart.parameter.duplicate",
                     "Chart parameter declaration is duplicated", declaration.fieldPath + "/id");
        }
    }

    std::map<std::string, ChartParameterValue, std::less<>> inputById;
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        const auto& input = inputs[index];
        const auto path = "$/parameterInputs/" + std::to_string(index);
        const auto declaration = declarationById.find(input.id);
        if (declaration == declarationById.end()) {
            addError(diagnostics, "chart.parameter.unknown",
                     "Host parameter input refers to an unknown declaration", path + "/id");
            continue;
        }
        if (input.type != declaration->second->type) {
            addError(diagnostics, "chart.parameter.type_mismatch",
                     "Host parameter input declares " + std::string{parameterTypeName(input.type)} +
                         " but the Chart declaration is " +
                         std::string{parameterTypeName(declaration->second->type)},
                     path + "/type");
            continue;
        }
        auto normalized = normalizeValue(input.value, input.type, diagnostics, path + "/value");
        if (!normalized) {
            continue;
        }
        if (!inputById.emplace(input.id, *normalized).second) {
            addError(diagnostics, "chart.parameter.duplicate", "Host parameter input is duplicated",
                     path + "/id");
        }
    }

    for (const auto& use : uses) {
        const auto declaration = declarationById.find(use.id);
        if (declaration == declarationById.end()) {
            addError(diagnostics, "chart.parameter.unknown",
                     "ChartParameterRef refers to an unknown declaration", use.fieldPath);
        } else if (declaration->second->type != use.expectedType) {
            addError(diagnostics, "chart.parameter.type_mismatch",
                     "ChartParameterRef type does not match its declaration", use.fieldPath);
        }
    }

    ResolvedParameterSet result;
    result.values.reserve(declarations.size());
    for (const auto& declaration : declarations) {
        std::optional<ChartParameterValue> value;
        if (const auto input = inputById.find(declaration.id); input != inputById.end()) {
            value = input->second;
        } else if (declaration.defaultValue) {
            value = normalizeValue(*declaration.defaultValue, declaration.type, diagnostics,
                                   declaration.fieldPath + "/default");
        }
        if (!value) {
            addError(diagnostics, "chart.parameter.missing",
                     "Chart parameter has no host input or default value",
                     declaration.fieldPath + "/id");
            continue;
        }
        if (!withinConstraints(*value, declaration.constraints)) {
            addError(diagnostics, "chart.parameter.out_of_range",
                     "Resolved Chart parameter is outside its declared constraints",
                     declaration.fieldPath + "/constraints");
            continue;
        }
        result.values.push_back(ResolvedChartParameter{declaration.id, declaration.type, *value});
    }

    std::ranges::sort(result.values, {}, &ResolvedChartParameter::id);
    for (const auto& parameter : result.values) {
        result.byId.emplace(parameter.id, parameter.value);
    }
    if (diagnostics.hasErrors() || result.values.size() != declarations.size()) {
        return std::nullopt;
    }
    result.identity = parameterIdentity(result.values);
    return result;
}

} // namespace cuexis::chart::detail
