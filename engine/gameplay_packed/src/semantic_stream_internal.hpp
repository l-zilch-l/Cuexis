#pragma once

#include "hash_work_internal.hpp"
#include "packed_gameplay_internal.hpp"
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>

namespace cuexis::gameplay_packed::detail {
// Private convergence probe: no wire reparse, placeholder values or alternate field schema.
// The caller supplies metered scratch and first counts/checks the immutable input before SHA.
// Cost memoization and complete revision-4 admission are not yet connected to this entry.
[[nodiscard]] auto streamSemanticPreimage(const EncodeRequest& request,
                                          chart::packed::gameplay_detail::SemanticByteSink sink,
                                          chart::PackedChartLimits limits = {})
    -> core::Result<HashCost>;
} // namespace cuexis::gameplay_packed::detail
