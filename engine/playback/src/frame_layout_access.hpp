#pragma once

// Library-internal access to the committed framing table and the laid-out frame builder. Tests use
// attachFramings to place an already validated in-memory table on a fresh PreparedPlayback before
// the real commit, extraction and renderer path. Nothing here is exported or installed.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>

#include <cstdint>
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
};

} // namespace detail
} // namespace cuexis::playback
