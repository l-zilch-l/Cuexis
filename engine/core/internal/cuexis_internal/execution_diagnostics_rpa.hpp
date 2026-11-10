#pragma once

// F5/R0 owner-local read-only observation. Fixed-size aggregates only: no allocation, no
// formatting and no locks on the recording path. The channel is disabled at compile time
// unless CUEXIS_RPA_OBSERVATION_TEST_ONLY is defined, and a build with it defined is never
// acceptable as formal performance evidence.

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cuexis::internal::execution_diagnostics::rpa {

inline constexpr std::size_t closeMarkCapacity = 32;
inline constexpr std::size_t closeWaitCapacity = 8;
inline constexpr std::size_t errorCodeCapacity = 48;

inline auto nowUs() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace cuexis::internal::execution_diagnostics::rpa
#endif

#include "execution_diagnostics_rpa_close.hpp"
#include "execution_diagnostics_rpa_seek.hpp"
