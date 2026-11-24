#pragma once

// Experimental OpenGL frame bridge. This header belongs to the source target only; GL/SDL types
// are not installed and this file is not part of the default SDK surface.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <cuexis/render/render_scene.hpp>

#include <cstdint>
#include <span>

namespace cuexis::render_opengl {
class OpenGlBackend;
}

namespace cuexis::render_opengl::experimental {

struct BorrowedGlFramebuffer final {
    std::uint32_t name{};
    playback::FrameViewport extent;
};

// Requires the cuexis.presentation.frame-layout.v1 host extension for a non-empty framing table.
[[nodiscard]] auto prepareLaidOutOwned(OpenGlBackend&, const playback::PresentationCandidateInput&,
                                       std::span<const playback::experimental::FrameFraming>)
    -> core::Result<presentation_renderer::PreparedPresentation>;

// Validates the complete current target, the active resource identity and the framing membership
// before clearing or drawing. Borrowed mode only draws; the window path arms the pending present.
[[nodiscard]] auto submitLaidOut(OpenGlBackend&, const playback::experimental::LaidOutFrame&,
                                 const playback::experimental::FrameTarget& currentTarget,
                                 const render::RenderScene* debugScene = nullptr,
                                 const BorrowedGlFramebuffer* borrowed = nullptr)
    -> core::Result<presentation_renderer::DrawSummary>;

} // namespace cuexis::render_opengl::experimental
