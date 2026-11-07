#pragma once

#include <cstdint>

namespace cuexis::core::detail {
// Private compression backend. Requires SHA, SSSE3 and SSE4.1 checked by the caller.
void sha256ProcessX86(std::uint32_t state[8], const std::uint8_t data[], std::uint32_t length);
} // namespace cuexis::core::detail
