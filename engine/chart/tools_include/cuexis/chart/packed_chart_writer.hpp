#pragma once

// Typed Packed Chart Writer entry point. Source compilation and file output belong to
// cuexis_chart_tools; the codec library only reads and inspects artifacts.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <vector>

namespace cuexis::chart::packed {

// Encodes a chart after validating the Spec 7.6 Foundation profile. A model outside the
// registered subset is refused before any identity or artifact bytes are produced. The Spec 3.3
// budgets apply with the frozen Foundation ceilings: a caller may tighten "limits" but never
// relax them. Count budgets are checked before the identity preimage, and the exact artifact
// envelope before the file is assembled, so an over-budget model produces neither a partial
// artifact nor a wrapped fixed-width wire counter.
[[nodiscard]] auto encode(const CanonicalSemanticChart& chart, PackedChartProfile profile = {},
                          PackedChartLimits limits = {}) -> core::Result<std::vector<std::byte>>;

} // namespace cuexis::chart::packed
