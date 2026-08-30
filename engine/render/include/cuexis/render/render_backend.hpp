#pragma once

//  RenderBackend - abstract base class for render backends
//  The only implementation today is the OpenGL backend (cuexis_render_opengl); a Vulkan
//  backend is reserved for the future
//  Callers must never invoke OpenGL directly; RenderScene contains no backend types
//  RenderFrame/renderFrame are legacy diagnostic-only compatibility interfaces retained in
//  SDK 0.7.0. The current presentation path is renderPresentationFrame on the OpenGL adapter.
//  RenderFrame holds a non-owning scene reference valid only for the duration of
//  renderFrame().

#include <cuexis/core/math.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/render/render_scene.hpp>

#include <cstdint>

namespace cuexis::render {

struct Extent final {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct RenderFrame final {
    Extent extent{};
    Color clearColor{};
    core::Mat4 viewProjection{};
    // Non-owning scene input that remains valid for the duration of renderFrame().
    const RenderScene* scene{};
};

class RenderBackend {
  public:
    virtual ~RenderBackend() = default;

    RenderBackend(const RenderBackend&) = delete;
    auto operator=(const RenderBackend&) -> RenderBackend& = delete;
    RenderBackend(RenderBackend&&) = delete;
    auto operator=(RenderBackend&&) -> RenderBackend& = delete;

    // Legacy diagnostic-only entry point retained for SDK 0.7.0 compatibility. New callers
    // use the versioned Presentation path exposed by the active render adapter.
    virtual auto renderFrame(const RenderFrame& frame) -> core::Result<void> = 0;

  protected:
    RenderBackend() = default;
};

} // namespace cuexis::render
