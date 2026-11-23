#pragma once

// Experimental frame layout values and geometry for the R5 frame bridge.
// This header is not part of the default installed SDK surface. Targets that actually consume
// the frame bridge opt in through the playback experimental include root.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_session.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace cuexis::playback::experimental {

struct AspectRatio final {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct FrameFraming final {
    AspectRatio aspect;
    double verticalScale{1.0};
};

enum class FrameRotation : std::uint16_t {
    Deg0 = 0,
    Deg90 = 90,
    Deg180 = 180,
    Deg270 = 270,
};

enum class FramingSelection : std::uint8_t {
    Default,
    BestFit,
    Exact,
};

struct FramingRequest final {
    FramingSelection selection{FramingSelection::Default};
    std::optional<AspectRatio> exact;
};

// Absolute target pixel rectangle. The origin is the top-left corner and the range is half-open.
struct PixelRect final {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

struct FrameTarget final {
    FrameViewport extent;
    PixelRect host;
    FrameRotation rotation{FrameRotation::Deg0};
};

// The content rectangle is relative to host; target is copied from the request.
struct FrameLayout final {
    FrameTarget target;
    PixelRect content;
};

struct SelectedFrameLayout final {
    FrameLayout layout;
    std::optional<FrameFraming> framing;
};

// Both functions export from every Playback flavor. The shared gate names these two symbols
// individually; the rest of the frame bridge is added by its own implementation task.
[[nodiscard]] CUEXIS_PLAYBACK_API auto computeContentRect(std::optional<AspectRatio> aspect,
                                                          FrameTarget target)
    -> core::Result<std::optional<FrameLayout>>;

[[nodiscard]] CUEXIS_PLAYBACK_API auto computeFrameLayout(std::span<const FrameFraming> framings,
                                                          FrameTarget target,
                                                          FramingRequest request = {})
    -> core::Result<std::optional<SelectedFrameLayout>>;

} // namespace cuexis::playback::experimental
