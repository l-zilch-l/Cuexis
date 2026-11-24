#pragma once

// Private OpenGL helpers for the experimental frame layout bridge. They validate and draw a
// laid-out frame into a caller-owned framebuffer without changing the installed OpenGL surface.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>

#include <array>
#include <cstdint>

namespace cuexis::render_opengl::detail {

struct GlRect final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
};

struct BorrowedGlFramebuffer final {
    std::uint32_t name{};
    playback::FrameViewport extent;
};

struct FrameClearValues final {
    std::array<float, 4> color{0.0F, 0.0F, 0.0F, 1.0F};
    float depth{1.0F};
};

// The callback runs with the borrowed framebuffer, draw-buffer route, viewport and scissor
// already set. Its GL changes are restored on success and failure.
using BorrowedFrameDraw = core::Result<void> (*)(void* context);

[[nodiscard]] auto toGlRect(const playback::experimental::PixelRect& rect,
                            playback::FrameViewport extent) noexcept -> GlRect;

[[nodiscard]] auto validateBorrowedFramebuffer(const BorrowedGlFramebuffer& target)
    -> core::Result<void>;

[[nodiscard]] auto drawBorrowedFrame(const BorrowedGlFramebuffer& target,
                                     const playback::experimental::FrameLayout& layout,
                                     const FrameClearValues& clear, BorrowedFrameDraw draw,
                                     void* context) -> core::Result<void>;

} // namespace cuexis::render_opengl::detail
