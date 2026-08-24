#pragma once

#include <cuexis/chart/chart_v4_resolver.hpp>

#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cuexis::chart::detail {

struct ResolvedParameterSet final {
    std::vector<ResolvedChartParameter> values;
    std::map<std::string, ChartParameterValue, std::less<>> byId;
    CanonicalContentIdentity identity;
};

[[nodiscard]] auto
resolveChartParameters(const std::vector<ChartParameterDeclaration>& declarations,
                       const std::vector<ParameterUse>& uses,
                       std::span<const ChartParameterInput> inputs, const ChartLimits& limits,
                       core::Diagnostics& diagnostics) -> std::optional<ResolvedParameterSet>;

} // namespace cuexis::chart::detail
