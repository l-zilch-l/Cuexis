//  ChartLoader implementation - route canonical charts by the explicit format field
//  未知 format 返回 UnsupportedFormat 诊断，不根据字段猜测格式

#include <cuexis/chart/chart_loader.hpp>

#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include "diagnostic_limit.hpp"

#include <optional>
#include <string>
#include <utility>

namespace cuexis::chart {
namespace {

void addParseError(core::Diagnostics& diagnostics, const core::Error& error) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, "$"};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

} // namespace

auto ChartLoader::load(std::string_view jsonText, const ChartLimits& limits)
    -> ChartDocumentResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addParseError(diagnostics, parsed.error());
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }

    json::Reader root{*parsed, diagnostics};
    const auto formatReader = root.requiredField("format");
    const auto format = formatReader ? formatReader->readString() : std::nullopt;
    if (diagnostics.hasErrors() || !format) {
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    if (*format == "cuexis.chart") {
        return CanonicalChartLoader::load(jsonText, limits);
    }
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, "chart.format.unsupported",
                                     "Chart format is unsupported",
                                     std::string{formatReader->fieldPath()}}
                        .withContext("format", std::string{*format}));
    diagnostics.sortDeterministically();
    return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
}

} // namespace cuexis::chart
