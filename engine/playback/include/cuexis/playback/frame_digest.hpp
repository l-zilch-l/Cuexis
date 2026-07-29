#pragma once

// Versioned deterministic digest for cross-build RuntimeFrame and FrameSnapshot parity checks.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_session.hpp>

#include <cstdint>

namespace cuexis::playback {

struct FrameDigest final {
    std::uint32_t algorithmVersion{1};
    std::uint64_t value{};
};

[[nodiscard]] CUEXIS_PLAYBACK_API auto computeFrameDigest(const RuntimeFrame& frame,
                                                          const FrameSnapshot& snapshot)
    -> core::Result<FrameDigest>;

} // namespace cuexis::playback
