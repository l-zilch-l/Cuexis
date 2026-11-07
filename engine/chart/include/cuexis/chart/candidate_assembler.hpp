#pragma once

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/result.hpp>

#include <span>

namespace cuexis::chart {

// Offline Foundation candidate assembly. Metadata and expanded fragments are explicit inputs.
// Features and resource uses are derived here; the Writer continues to validate declarations.
// This does not compile Gameplay v2 requirements or change the production Chart format.
[[nodiscard]] auto assembleCandidateChart(CanonicalSemanticChart metadata,
                                          std::span<const CanonicalSemanticChart> expanded,
                                          PackedChartLimits limits = {})
    -> core::Result<CanonicalSemanticChart>;

} // namespace cuexis::chart
