#include "chart_v4_common_reader_internal.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <ranges>
#include <set>
#include <utility>

namespace cuexis::chart::detail {
namespace {

[[nodiscard]] auto isAsciiAlphaNumeric(char character) noexcept -> bool {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}

} // namespace

void addV4Error(core::Diagnostics& diagnostics, std::string code, std::string message,
                std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(path)});
}

auto readPortableStableId(const json::Reader& reader, const ChartLimits& limits,
                          core::Diagnostics& diagnostics, std::string_view purpose)
    -> std::optional<std::string> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (value->empty() || value->size() > limits.maxIdentifierBytes ||
        !isAsciiAlphaNumeric(value->front()) ||
        !std::ranges::all_of(value->substr(1), [](char character) {
            return isAsciiAlphaNumeric(character) || character == '.' || character == '_' ||
                   character == '-';
        })) {
        addV4Error(diagnostics, "chart.identifier.invalid", std::string{purpose} + " is invalid",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return std::string{*value};
}

auto readV4Rational(const json::Reader& reader, const ChartLimits& limits,
                    core::Diagnostics& diagnostics, bool requirePositive, bool allowNegative)
    -> std::optional<RationalBeat> {
    constexpr std::array fields{std::string_view{"numerator"}, std::string_view{"denominator"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto numeratorReader = reader.requiredField("numerator");
    const auto denominatorReader = reader.requiredField("denominator");
    auto numerator = std::optional<std::int64_t>{};
    auto denominator = std::optional<std::int64_t>{};
    if (numeratorReader) {
        numerator = numeratorReader->readInt64();
    }
    if (denominatorReader) {
        denominator = denominatorReader->readInt64();
    }
    if (!numerator || !denominator) {
        return std::nullopt;
    }
    const auto numeratorValue = *numerator;
    const auto denominatorValue = *denominator;
    if ((!allowNegative && numeratorValue < 0) || (requirePositive && numeratorValue <= 0) ||
        denominatorValue <= 0 || denominatorValue > limits.maxBeatDenominator ||
        numeratorValue < -limits.maxBeatNumeratorMagnitude ||
        numeratorValue > limits.maxBeatNumeratorMagnitude) {
        addV4Error(diagnostics, "chart.beat.out_of_range", "Rational beat is outside the limit",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    auto value = RationalBeat::create(numeratorValue, denominatorValue);
    if (!value) {
        addV4Error(diagnostics, std::string{value.error().code()},
                   std::string{value.error().message()}, std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return *value;
}

auto readRequiredExtensions(const json::Reader& reader, const ChartLimits& limits,
                            core::Diagnostics& diagnostics) -> std::vector<RequiredExtension> {
    std::vector<RequiredExtension> result;
    const auto* items = reader.readArray();
    if (items == nullptr) {
        return result;
    }
    if (items->size() > limits.maxExtensions) {
        addV4Error(diagnostics, "chart.extension.limit", "Required extension count exceeds limit",
                   std::string{reader.fieldPath()});
        return result;
    }
    std::set<std::string, std::less<>> ids;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{std::string_view{"id"}, std::string_view{"version"}};
        item->rejectUnknownFields(fields);
        const auto idReader = item->requiredField("id");
        const auto versionReader = item->requiredField("version");
        const auto id =
            idReader ? readPortableStableId(*idReader, limits, diagnostics, "Required extension ID")
                     : std::nullopt;
        auto version = std::optional<std::uint64_t>{};
        if (versionReader) {
            version = versionReader->readUInt64();
        }
        auto versionValue = std::uint64_t{};
        auto versionValid = false;
        if (version) {
            versionValue = *version;
            versionValid =
                versionValue > 0 && versionValue <= std::numeric_limits<std::uint32_t>::max();
            if (!versionValid) {
                addV4Error(diagnostics, "chart.version.unsupported",
                           "Required extension version is invalid",
                           std::string{versionReader->fieldPath()});
            }
        }
        if (!id || !versionValid) {
            continue;
        }
        if (!ids.emplace(*id).second) {
            addV4Error(diagnostics, "chart.extension.duplicate",
                       "Required extension ID is duplicated", std::string{item->fieldPath()});
            continue;
        }
        result.push_back(RequiredExtension{*id, static_cast<std::uint32_t>(versionValue)});
    }
    std::ranges::sort(result, {}, &RequiredExtension::id);
    return result;
}

} // namespace cuexis::chart::detail
