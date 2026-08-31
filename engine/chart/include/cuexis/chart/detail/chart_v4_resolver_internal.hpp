#pragma once

#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/detail/chart_dispatch_internal.hpp>

namespace cuexis::chart::detail {

// Resolves a v4 document from the parsed input retained by the prepare-only dispatch path.
[[nodiscard]] auto resolveV4Parsed(const ChartV4SourceDocument& source,
                                   const ParsedChartInput& parsedInput,
                                   std::span<const ChartParameterInput> parameters = {},
                                   std::span<const ProjectDocument> projectDocuments = {},
                                   std::span<const RequiredExtension> supportedExtensions = {},
                                   const ChartLimits& limits = {}) -> ChartV4ResolveResult;

} // namespace cuexis::chart::detail
