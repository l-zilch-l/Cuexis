#pragma once

#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <string>
#include <utility>

namespace cuexis::chart::detail {

[[nodiscard]] inline auto makeDiagnostics(const ChartLimits& limits) -> core::Diagnostics {
    if (limits.maxDiagnostics == 0) {
        return {};
    }
    return core::Diagnostics{
        limits.maxDiagnostics,
        core::Diagnostic{core::DiagnosticSeverity::Error, "chart.diagnostics.limit_exceeded",
                         "Chart diagnostics reached the configured limit", "$"}
            .withContext("max_diagnostics", std::to_string(limits.maxDiagnostics))};
}

[[nodiscard]] inline auto rejectInvalidDiagnosticLimit(core::Diagnostics& diagnostics,
                                                       const ChartLimits& limits) -> bool {
    if (limits.maxDiagnostics != 0) {
        return false;
    }
    static_cast<void>(diagnostics.add(core::Diagnostic{
        core::DiagnosticSeverity::Error, "chart.limits.invalid",
        "Chart diagnostic limit must be greater than zero", "$/limits/maxDiagnostics"}));
    diagnostics.sortDeterministically();
    return true;
}

} // namespace cuexis::chart::detail
