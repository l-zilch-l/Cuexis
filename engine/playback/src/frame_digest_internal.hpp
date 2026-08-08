#pragma once

#include <cuexis/core/result.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_export.hpp>

#include <cstdint>

namespace cuexis::playback::detail {

[[nodiscard]] CUEXIS_PLAYBACK_API auto computeFrameDigestVersion(std::uint32_t algorithmVersion,
                                                                 const RuntimeFrame& frame,
                                                                 const FrameSnapshot& snapshot)
    -> core::Result<FrameDigest>;

} // namespace cuexis::playback::detail
