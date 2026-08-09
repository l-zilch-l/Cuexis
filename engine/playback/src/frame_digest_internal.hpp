#pragma once

#include <cstdint>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/frame_digest.hpp>

namespace cuexis::playback::detail {

[[nodiscard]] auto computeFrameDigestVersion(std::uint32_t algorithmVersion,
                                             const RuntimeFrame& frame,
                                             const FrameSnapshot& snapshot)
    -> core::Result<FrameDigest>;

} // namespace cuexis::playback::detail
