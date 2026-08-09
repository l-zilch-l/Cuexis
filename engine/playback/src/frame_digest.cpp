#include <cuexis/playback/frame_digest.hpp>

#include "frame_digest_internal.hpp"

namespace cuexis::playback {
namespace {

constexpr std::uint32_t currentFrameDigestVersion = 3;

} // namespace

auto computeFrameDigest(const RuntimeFrame& frame, const FrameSnapshot& snapshot)
    -> core::Result<FrameDigest> {
    return detail::computeFrameDigestVersion(currentFrameDigestVersion, frame, snapshot);
}

} // namespace cuexis::playback
