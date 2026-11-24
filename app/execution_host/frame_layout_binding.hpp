#pragma once

// Source-only bridge between the frame chain and the experimental laid-out submit path. The
// Player assembly installs one binding on the final OpenGlBackend instance; the execution host
// support target stores only an opaque pointer and two function pointers, so it neither includes
// GL headers nor links the OpenGL target.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>
#include <cuexis/playback/presentation_candidate.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <cuexis/render/render_scene.hpp>

#include <span>

namespace cuexis::playback::detail {

struct FrameLayoutRendererBinding final {
    using PrepareOwnedFn = core::Result<presentation_renderer::PreparedPresentation> (*)(
        void*, const PresentationCandidateInput&, std::span<const experimental::FrameFraming>);
    using SubmitFn = core::Result<presentation_renderer::DrawSummary> (*)(
        void*, const experimental::LaidOutFrame&, const experimental::FrameTarget&,
        const render::RenderScene*);

    presentation_renderer::IPresentationRenderer* renderer{};
    // The same renderer as its concrete backend object, restored by the assembly factory.
    void* object{};
    PrepareOwnedFn prepareOwned{};
    SubmitFn submit{};

    [[nodiscard]] auto valid() const noexcept -> bool {
        return renderer != nullptr && object != nullptr && prepareOwned != nullptr &&
               submit != nullptr;
    }
};

} // namespace cuexis::playback::detail
