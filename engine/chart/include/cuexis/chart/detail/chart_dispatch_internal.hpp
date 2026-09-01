#pragma once

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <memory>
#include <optional>
#include <string_view>

namespace cuexis::chart::detail {

struct ParsedChartInput;

struct ChartDispatchResult final {
    bool isV4{};
    std::optional<ChartDocument> legacyDocument;
    std::optional<ChartV4SourceDocument> v4Document;
    std::shared_ptr<const ParsedChartInput> v4Input;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return !diagnostics.hasErrors() &&
               (isV4 ? v4Document.has_value() : legacyDocument.has_value());
    }
};

// Parses once and routes the value to the legacy or v4 typed reader.
[[nodiscard]] auto loadChartForPlayback(std::string_view jsonText, const ChartLimits& limits = {})
    -> ChartDispatchResult;

// Filesystem source discovery only needs a valid v4 document and must not parse legacy Charts.
[[nodiscard]] auto loadV4IfPresent(std::string_view jsonText, const ChartLimits& limits = {})
    -> std::optional<ChartV4SourceDocument>;

} // namespace cuexis::chart::detail
