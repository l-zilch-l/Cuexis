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
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/presentation.hpp>
#include <cuexis/render/render_backend.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::render_opengl {

namespace detail {
struct OpenGlPresentationBackendState;
}

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

enum class OpenGlPresentationPass : std::uint8_t {
    Opaque,
    Transparent,
};

struct OpenGlDrawCommand final {
    std::string objectId;
    std::array<float, 16> worldMatrix{};
    playback::PresentationResourceRef mesh;
    playback::PresentationResourceRef material;
    std::array<double, 4> effectiveColor{};
    OpenGlPresentationPass pass{OpenGlPresentationPass::Opaque};
    bool backFaceCulling{true};
    bool depthTest{true};
    bool depthWrite{true};
    bool sourceOverBlend{};
    double depthMeters{};
    std::int64_t transparentDepthKey{};
};

struct OpenGlDrawSummary final {
    std::uint32_t version{1};
    std::uint32_t viewportWidth{};
    std::uint32_t viewportHeight{};
    std::array<float, 4> clearColor{};
    bool cameraActive{};
    std::array<float, 16> viewMatrix{};
    std::array<float, 16> projectionMatrix{};
    bool debugPassEnabled{};
    std::vector<OpenGlDrawCommand> opaque;
    std::vector<OpenGlDrawCommand> transparent;
    std::size_t debugCommandCount{};
    std::uint64_t digest{};

    void clear() noexcept;
};

struct OpenGlPixelProbe final {
    std::array<std::uint8_t, 4> rgba{};
    bool presentationDrawn{};
};

class OpenGlPresentationCandidate final {
  public:
    OpenGlPresentationCandidate(const OpenGlPresentationCandidate&) = delete;
    auto operator=(const OpenGlPresentationCandidate&) -> OpenGlPresentationCandidate& = delete;
    OpenGlPresentationCandidate(OpenGlPresentationCandidate&& other) noexcept;
    auto operator=(OpenGlPresentationCandidate&& other) noexcept -> OpenGlPresentationCandidate&;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] auto token() const noexcept -> const playback::PresentationCandidateToken&;
    [[nodiscard]] auto settings() const noexcept -> const playback::EffectivePresentationSettings&;

  private:
    friend class OpenGlBackend;

    OpenGlPresentationCandidate(playback::PresentationCandidateToken token,
                                playback::EffectivePresentationSettings settings,
                                std::uint64_t generation) noexcept
        : token_(std::move(token)), settings_(settings), generation_(generation) {}

    playback::PresentationCandidateToken token_;
    playback::EffectivePresentationSettings settings_;
    std::uint64_t generation_{};
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
    // Uploads a complete Portable Presentation candidate while the current cache remains active.
    [[nodiscard]] auto preparePresentation(playback::PreparedPlayback& prepared,
                                           const playback::PresentationRequest& request = {})
        -> core::Result<OpenGlPresentationCandidate>;
    // Activates a candidate prepared by this backend. Valid candidates cannot fail activation.
    void activatePresentation(OpenGlPresentationCandidate&& candidate) noexcept;
    // Discards a candidate after Playback commit failure without touching the active cache.
    void discardPresentation(OpenGlPresentationCandidate&& candidate) noexcept;
    [[nodiscard]] bool hasActivePresentation() const noexcept;
    // Draws the portable snapshot and optional Debug pass, then presents the SDL window.
    [[nodiscard]] auto renderPresentationFrame(const playback::FrameSnapshot& snapshot,
                                               const render::RenderScene* debugScene = nullptr,
                                               OpenGlDrawSummary* summary = nullptr,
                                               OpenGlPixelProbe* pixelProbe = nullptr)
        -> core::Result<void>;
    // Releases all GPU, context and window resources on the owner thread.
    [[nodiscard]] auto close() -> core::Result<void>;
    // Stage 0 rendering is bound to the SDL main thread that created this backend.
    auto renderFrame(const render::RenderFrame& frame) -> core::Result<void> override;

  private:
    OpenGlBackend(platform_sdl::SdlWindowLease window, void* context, OpenGlInfo info,
                  std::uint32_t debugProgram, std::uint32_t debugVertexArray,
                  std::uint32_t debugVertexBuffer, int viewProjectionLocation,
                  std::unique_ptr<detail::OpenGlPresentationBackendState> presentation,
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
    std::unique_ptr<detail::OpenGlPresentationBackendState> presentation_;
    core::ThreadChecker ownerThread_{};
};

} // namespace cuexis::render_opengl
