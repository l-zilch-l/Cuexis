#pragma once

//  RenderBackend — 渲染后端抽象基类
//  当前唯一实现为 OpenGL Backend（cuexis_render_opengl），Vulkan 后端为未来预留
//  业务层不得直接调用 OpenGL；RenderScene 不包含后端类型
//  RenderFrame: 单帧渲染输入，包含非拥有场景引用（仅 renderFrame() 期间有效）

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
