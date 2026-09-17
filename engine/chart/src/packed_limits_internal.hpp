#pragma once

// Foundation Packed budget resolution (Spec 3.3).
//
// Every PackedChartLimits field is a ceiling that a caller may tighten but never relax: the
// struct defaults are the budgets frozen by the Foundation profile, and the effective budget of
// an entry point is min(requested, frozen). This keeps the 16 MiB file gate and the 40,000
// entity gate executable no matter what a host passes in, while still letting a host run the
// candidate reader and writer under stricter limits.

#include <cuexis/chart/limits.hpp>

#include <algorithm>

namespace cuexis::chart::packed::limits_detail {

// The frozen Foundation budgets. Kept in one place so tightening a default tightens every entry
// point at once.
[[nodiscard]] inline auto frozenLimits() noexcept -> PackedChartLimits {
    return PackedChartLimits{};
}

[[nodiscard]] inline auto effectiveLimits(const PackedChartLimits& requested) noexcept
    -> PackedChartLimits {
    const PackedChartLimits frozen = frozenLimits();
    return PackedChartLimits{
        std::min(requested.maxPackedFileBytes, frozen.maxPackedFileBytes),
        std::min(requested.maxPackedDecodedBytes, frozen.maxPackedDecodedBytes),
        std::min(requested.maxPackedSectionBytes, frozen.maxPackedSectionBytes),
        std::min(requested.maxPackedStrings, frozen.maxPackedStrings),
        std::min(requested.maxPackedReferences, frozen.maxPackedReferences),
        std::min(requested.maxPackedEntities, frozen.maxPackedEntities),
        std::min(requested.maxPackedRequirements, frozen.maxPackedRequirements)};
}

} // namespace cuexis::chart::packed::limits_detail
