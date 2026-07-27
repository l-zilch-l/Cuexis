#pragma once

// OpenGL 3.3 Core Profile RenderBackend implementation.
// Configuration, creation, rendering, and destruction run on the SDL main thread.
// OpenGL types such as GLuint and SDL_GLContext remain private to this module.
// Application code does not call OpenGL directly, and RenderScene exposes no backend types.
// The debug pipeline uses inline GLSL 330 shaders to render DebugLine commands.

#include <cuexis/core/log_sink.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render/render_backend.hpp>

#include <cstdint>
#include <memory>
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
    std::shared_ptr<const core::LogSink> logSink;
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
    // Releases all GPU, context and window resources on the owner thread.
    [[nodiscard]] auto close() -> core::Result<void>;
    // Stage 0 rendering is bound to the SDL main thread that created this backend.
    auto renderFrame(const render::RenderFrame& frame) -> core::Result<void> override;

  private:
    OpenGlBackend(platform_sdl::SdlWindowLease window, void* context, OpenGlInfo info,
                  std::uint32_t debugProgram, std::uint32_t debugVertexArray,
                  std::uint32_t debugVertexBuffer, int viewProjectionLocation,
                  std::shared_ptr<const core::LogSink> logSink) noexcept;

    void release() noexcept;

    platform_sdl::SdlWindowLease window_{};
    void* context_{};
    OpenGlInfo info_{};
    std::shared_ptr<const core::LogSink> logSink_;
    std::uint32_t debugProgram_{};
    std::uint32_t debugVertexArray_{};
    std::uint32_t debugVertexBuffer_{};
    int viewProjectionLocation_{-1};
    core::ThreadChecker ownerThread_{};
};

} // namespace cuexis::render_opengl
