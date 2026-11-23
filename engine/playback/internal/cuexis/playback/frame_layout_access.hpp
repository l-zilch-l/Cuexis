#pragma once

// Library-internal access to the committed framing table and the laid-out frame builder. Tests use
// attachFramings to place an already validated in-memory table on a fresh PreparedPlayback before
// the real commit, extraction and renderer path. Nothing here is exported or installed.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>

#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <vector>

namespace cuexis::playback {
class PlaybackSession;
class PreparedPlayback;
namespace detail {

struct FrameLayoutAccess final {
    // Owning copy through the existing Prepared owner/validity checks.
    [[nodiscard]] static auto framings(const PreparedPlayback&)
        -> core::Result<std::vector<experimental::FrameFraming>>;
    // Test/library wiring: accepts only an empty or already reduced, unique table with values in
    // domain. It never replaces the active session; commit applies it.
    [[nodiscard]] static auto attachFramings(PreparedPlayback&,
                                             std::span<const experimental::FrameFraming>)
        -> core::Result<void>;
    [[nodiscard]] static auto extract(const PlaybackSession&, experimental::FrameTarget,
                                      std::optional<std::uint64_t> exactResultVersion,
                                      experimental::FramingRequest)
        -> core::Result<std::optional<experimental::LaidOutFrame>>;
    [[nodiscard]] static auto relayout(const experimental::LaidOutFrame&, experimental::FrameTarget)
        -> core::Result<std::optional<experimental::LaidOutFrame>>;
    // Compares one request with the selection the frame was produced by, both normalized the same
    // way. A retained handle keeps its mode and selection, so the host uses this to reject a
    // retained request that no longer matches the handle instead of relaying it out under another
    // selection. The definition is inline so the static execution-host library never needs a
    // Playback symbol that the shared build does not export.
    [[nodiscard]] static auto matchesRequest(const experimental::LaidOutFrame& frame,
                                             experimental::FramingRequest request) -> bool {
        if (request.exact) {
            const auto divisor = std::gcd(request.exact->width, request.exact->height);
            if (divisor == 0)
                return false;
            request.exact = experimental::AspectRatio{request.exact->width / divisor,
                                                      request.exact->height / divisor};
        }
        if (frame.request_.selection != request.selection)
            return false;
        if (request.selection != experimental::FramingSelection::Exact)
            return !frame.request_.exact.has_value() && !request.exact.has_value();
        return frame.request_.exact.has_value() && request.exact.has_value() &&
               frame.request_.exact->width == request.exact->width &&
               frame.request_.exact->height == request.exact->height;
    }
};

} // namespace detail
} // namespace cuexis::playback
