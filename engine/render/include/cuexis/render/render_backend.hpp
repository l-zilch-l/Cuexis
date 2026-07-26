#pragma once

//  RenderBackend - abstract base class for render backends
//  The only implementation today is the OpenGL backend (cuexis_render_opengl); a Vulkan
//  backend is reserved for the future
//  Callers must never invoke OpenGL directly; RenderScene contains no backend types
//  RenderFrame: single-frame render input holding a non-owning scene reference (valid only
//  for the duration of renderFrame())

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

    virtual auto renderFrame(const RenderFrame& frame) -> core::Result<void> = 0;

  protected:
    RenderBackend() = default;
};

} // namespace cuexis::render
