#pragma once

// Internal helpers shared by the candidate Packed Chart table codec and the Foundation
// semantic identity preimage. See docs/formats/PACKED_CHART_FORMAT.md sections 6.5 and 10.1.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/core/result.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cuexis::chart::packed::identity_detail {

// Byte-wise lexicographic order for canonical key bytes: unsigned byte value first, then length.
//
// The C++20 three-way comparison of std::vector<std::byte> lowers to a __builtin_memcmp whose
// length an optimized GCC 16 build cannot bound, which turns into a spurious -Wstringop-overread
// and fails a -Werror release build. This comparator preserves the exact same ordering without
// that path.
struct ByteKeyLess final {
    [[nodiscard]] auto operator()(const std::vector<std::byte>& left,
                                  const std::vector<std::byte>& right) const -> bool {
        const auto common = std::min(left.size(), right.size());
        for (std::size_t index = 0U; index < common; ++index) {
            if (left[index] != right[index]) {
                return std::to_integer<unsigned int>(left[index]) <
                       std::to_integer<unsigned int>(right[index]);
            }
        }
        return left.size() < right.size();
    }
};

// Raw 16 UUID bytes in canonical textual hex order, not platform GUID memory layout.
[[nodiscard]] auto uuidBytes(std::string_view text) -> core::Result<std::array<std::uint8_t, 16>>;

// Spec 6.5 canonical identity bytes. Used for IDN0 ordering and for I(identity) in the
// Spec 10.1 semantic preimage.
[[nodiscard]] auto canonicalIdentityBytes(const CanonicalEntityIdentity& identity)
    -> core::Result<std::vector<std::byte>>;

} // namespace cuexis::chart::packed::identity_detail
