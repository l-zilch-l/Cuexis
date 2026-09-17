#pragma once

// Internal helpers shared by the candidate Packed Chart table codec and the Foundation
// semantic identity preimage. See docs/formats/PACKED_CHART_FORMAT.md sections 6.5 and 10.1.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/core/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cuexis::chart::packed::identity_detail {

// Raw 16 UUID bytes in canonical textual hex order, not platform GUID memory layout.
[[nodiscard]] auto uuidBytes(std::string_view text) -> core::Result<std::array<std::uint8_t, 16>>;

// Spec 6.5 canonical identity bytes. Used for IDN0 ordering and for I(identity) in the
// Spec 10.1 semantic preimage.
[[nodiscard]] auto canonicalIdentityBytes(const CanonicalEntityIdentity& identity)
    -> core::Result<std::vector<std::byte>>;

} // namespace cuexis::chart::packed::identity_detail
