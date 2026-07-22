#pragma once

//  OpenGLBackend — OpenGL 3.3 Core Profile 渲染后端
//  当前唯一 RenderBackend 实现；配置/创建/渲染/销毁均在 SDL 主线程
//  OpenGL 类型（GLuint、SDL_GLContext）仅在 cuexis_render_opengl 模块内持有
//  业务层不得直接调用 OpenGL；RenderScene 不暴露后端类型
//  Debug Pipeline: 内联 GLSL 330 顶点/片段着色器，支持 DebugLine 渲染

#include <cuexis/core/result.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render/render_backend.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace cuexis::render_opengl {

struct OpenGlConfig final {
    int majorVersion{3};
    int minorVersion{3};
    bool debugContext{
#ifndef NDEBUG
        true
#else
        false
#endif
    };
    bool vsync{true};
};

struct OpenGlInfo final {
    std::string version;
    std::string vendor;
    std::string renderer;
};

class OpenGlContextConfiguration final {
  public:
    OpenGlContextConfiguration(const OpenGlContextConfiguration&) = delete;
    auto operator=(const OpenGlContextConfiguration&) -> OpenGlContextConfiguration& = delete;
    OpenGlContextConfiguration(OpenGlContextConfiguration&& other) noexcept
        : config_(std::exchange(other.config_, std::nullopt)),
          generation_(std::exchange(other.generation_, 0)) {}
    auto operator=(OpenGlContextConfiguration&&) -> OpenGlContextConfiguration& = delete;

  private:
    friend auto configureOpenGlContext(platform_sdl::SdlRuntime&, const OpenGlConfig&)
        -> core::Result<OpenGlContextConfiguration>;
    friend class OpenGlBackend;

    OpenGlContextConfiguration(const OpenGlConfig& config, std::uint64_t generation)
        : config_(config), generation_(generation) {}

    std::optional<OpenGlConfig> config_;
    std::uint64_t generation_{};
};

// Prepares SDL's process-wide GL attributes on the SDL main thread and returns a one-shot token.
[[nodiscard]] auto configureOpenGlContext(platform_sdl::SdlRuntime& runtime,
                                          const OpenGlConfig& config = {})
    -> core::Result<OpenGlContextConfiguration>;

// Owns an SDL GL context and must be used and destroyed on its SDL main thread.
class OpenGlBackend final : public render::RenderBackend {
  public:
    // Consumes the configuration on the SDL main thread after the OpenGL window has been created.
    [[nodiscard]] static auto create(platform_sdl::SdlWindow& window,
                                     OpenGlContextConfiguration&& configuration)
        -> core::Result<OpenGlBackend>;

    ~OpenGlBackend() override;

    OpenGlBackend(const OpenGlBackend&) = delete;
    auto operator=(const OpenGlBackend&) -> OpenGlBackend& = delete;
    OpenGlBackend(OpenGlBackend&& other) noexcept;
    auto operator=(OpenGlBackend&& other) noexcept -> OpenGlBackend& = delete;

    [[nodiscard]] auto info() const noexcept -> const OpenGlInfo&;
    // Stage 0 rendering is bound to the SDL main thread that created this backend.
    auto renderFrame(const render::RenderFrame& frame) -> core::Result<void> override;

  private:
    OpenGlBackend(platform_sdl::SdlWindowLease window, void* context, OpenGlInfo info,
                  std::uint32_t debugProgram, std::uint32_t debugVertexArray,
                  std::uint32_t debugVertexBuffer, int viewProjectionLocation) noexcept;

    void release() noexcept;

    platform_sdl::SdlWindowLease window_{};
    void* context_{};
    OpenGlInfo info_{};
    std::uint32_t debugProgram_{};
    std::uint32_t debugVertexArray_{};
    std::uint32_t debugVertexBuffer_{};
    int viewProjectionLocation_{-1};
    core::ThreadChecker ownerThread_{};
};

} // namespace cuexis::render_opengl
