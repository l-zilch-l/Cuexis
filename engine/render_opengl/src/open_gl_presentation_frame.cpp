#include "../internal/cuexis_internal/gpu_storage_observer.hpp"
#include "../internal/cuexis_internal/pixel_probe.hpp"
#include "open_gl_presentation_internal.hpp"
#include "open_gl_presentation_units_internal.hpp"
#include <cuexis/playback/presentation_candidate.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/shader/shader_cache.hpp>
#include <cuexis/shader/shader_diagnostics.hpp>

#ifdef CUEXIS_HAS_SHADER_TOOLS
#include <cuexis/shader/shader_compiler.hpp>
#endif

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iterator>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::render_opengl {

bool OpenGlBackend::hasActivePresentation() const noexcept {
    ownerThread_.assertCurrent();
    return presentation_ && presentation_->active.has_value();
}

void OpenGlBackend::setShaderCacheDirectory(std::filesystem::path directory) {
    ownerThread_.assertCurrent();
    if (presentation_) {
        presentation_->shaderCacheDirectory = std::move(directory);
    }
}

auto OpenGlBackend::renderPresentationFrame(const playback::FrameSnapshot& snapshot,
                                            const render::RenderScene* debugScene,
                                            OpenGlDrawSummary* summary,
                                            OpenGlPixelProbe* pixelProbe) -> core::Result<void> {
    return renderPresentation(snapshot, debugScene, summary, pixelProbe, true);
}

auto OpenGlBackend::renderPresentation(const playback::FrameSnapshot& snapshot,
                                       const render::RenderScene* debugScene,
                                       OpenGlDrawSummary* summary, OpenGlPixelProbe* pixelProbe,
                                       bool presentFrame) -> core::Result<void> {
    if (auto mainThread = requireMainThread("render_presentation_frame"); !mainThread) {
        return core::unexpected(std::move(mainThread.error()));
    }
    if (!ownerThread_.isCurrent()) {
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "The OpenGL backend belongs to another thread"}
                                    .withContext("operation", "render_presentation_frame"));
    }
    if (!presentation_ || !presentation_->active || !window_.valid() || context_ == nullptr) {
        return core::unexpected(core::Error{"render.opengl.presentation.inactive",
                                            "No active OpenGL presentation cache is available"});
    }
    auto* nativeWindow = static_cast<SDL_Window*>(window_.nativeHandle());
    if (nativeWindow == nullptr ||
        !SDL_GL_MakeCurrent(nativeWindow, static_cast<SDL_GLContext>(context_))) {
        return core::unexpected(core::Error{"render.opengl.context_current_failed", sdlError()});
    }
    auto& state = *presentation_;
    state.opaqueScratch.clear();
    state.transparentScratch.clear();
    state.debugVerticesScratch.clear();
    presentation_->retired.reset();

    try {
        const auto reserveCount = std::min(snapshot.objects.size(), maxNormalizedRecords);
        state.opaqueScratch.reserve(reserveCount);
        state.transparentScratch.reserve(reserveCount);
        if (presentation_->active->settings.debugPassEnabled && debugScene != nullptr) {
            state.debugVerticesScratch.reserve(
                std::min(debugScene->size(), render::RenderScene::maxCommandCount) * 2U);
        }

        // Ordering, pass split, and validation all come from the backend-neutral builder. This
        // adapter only resolves the GL handles the neutral layer cannot carry.
        auto built = presentation_renderer::buildPresentationCommands(
            snapshot, presentation_->active->resources,
            presentation_->active->manifest.entries.empty(),
            presentation_->active->settings.debugPassEnabled, debugScene);
        if (!built) {
            return core::unexpected(std::move(built.error()));
        }
        presentation_renderer::DrawSummary preparedSummary = std::move(*built);

        state.opaqueScratch.reserve(preparedSummary.opaque.size());
        state.transparentScratch.reserve(preparedSummary.transparent.size());
        for (const auto& command : preparedSummary.opaque) {
            PreparedDraw draw;
            if (auto resolved = resolveGpuDraw(command, *presentation_->active, draw); !resolved) {
                return core::unexpected(std::move(resolved.error()));
            }
            state.opaqueScratch.push_back(std::move(draw));
        }
        for (const auto& command : preparedSummary.transparent) {
            PreparedDraw draw;
            if (auto resolved = resolveGpuDraw(command, *presentation_->active, draw); !resolved) {
                return core::unexpected(std::move(resolved.error()));
            }
            state.transparentScratch.push_back(std::move(draw));
        }

        if (preparedSummary.debugPassEnabled) {
            if (auto collected = collectDebugVertices(debugScene, state.debugVerticesScratch);
                !collected) {
                return core::unexpected(std::move(collected.error()));
            }
        }

        constexpr auto maxDimension =
            static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max());
        const auto width = static_cast<GLsizei>(std::min(snapshot.viewportWidth, maxDimension));
        const auto height = static_cast<GLsizei>(std::min(snapshot.viewportHeight, maxDimension));
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
        glClearColor(snapshot.clearRed, snapshot.clearGreen, snapshot.clearBlue,
                     snapshot.clearAlpha);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        clearGlErrors();

        const auto viewProjection =
            multiplyMatrices(preparedSummary.projectionMatrix, preparedSummary.viewMatrix);
        drawPresentationCommands(presentation_->pipeline, viewProjection, state.opaqueScratch,
                                 false);
        drawPresentationCommands(presentation_->pipeline, viewProjection, state.transparentScratch,
                                 true);
        if (auto checked = checkGl("render.opengl.presentation.draw_failed",
                                   "OpenGL rejected Portable Presentation drawing");
            !checked) {
            return core::unexpected(std::move(checked.error()));
        }

        OpenGlPixelProbe preparedProbe;
        preparedProbe.presentationDrawn =
            !state.opaqueScratch.empty() || !state.transparentScratch.empty();
        if (pixelProbe != nullptr && width > 0 && height > 0) {
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         preparedProbe.rgba.data());
            if (auto checked = checkGl("render.opengl.presentation.readback_failed",
                                       "OpenGL rejected presentation pixel readback");
                !checked) {
                return core::unexpected(std::move(checked.error()));
            }
        }

        if (!state.debugVerticesScratch.empty()) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glUseProgram(debugProgram_);
            glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, viewProjection.data());
            glBindVertexArray(debugVertexArray_);
            glBindBuffer(GL_ARRAY_BUFFER, debugVertexBuffer_);
            test_only::GpuStorage::charge(1, debugVertexBuffer_,
                                          state.debugVerticesScratch.size() * sizeof(DebugVertex));
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(state.debugVerticesScratch.size() * sizeof(DebugVertex)),
                state.debugVerticesScratch.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(state.debugVerticesScratch.size()));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            glUseProgram(0);
            if (auto checked =
                    checkGl("render.opengl.draw_failed", "OpenGL rejected Debug Pass drawing");
                !checked) {
                return core::unexpected(std::move(checked.error()));
            }
        }
        if (presentFrame && !SDL_GL_SwapWindow(nativeWindow)) {
            return core::unexpected(core::Error{"render.opengl.swap_failed", sdlError()});
        }
        lastProbe_ = preparedProbe;
        if (summary != nullptr) {
            *summary = std::move(preparedSummary);
        }
        if (pixelProbe != nullptr) {
            *pixelProbe = preparedProbe;
        }
        // PreparedDraw holds non-owning command pointers into preparedSummary.opaque/transparent,
        // which the move above consumes. Drop them with the summary they came from so the
        // persistent scratch never outlives the commands it points at.
        state.opaqueScratch.clear();
        state.transparentScratch.clear();
        return {};
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"playback.presentation.frame.command_budget_exceeded",
                        "OpenGL presentation frame allocation could not be satisfied"}
                .withContext("limit", std::to_string(maxNormalizedRecords)));
    } catch (const std::exception& exception) {
        return core::unexpected(core::Error{"render.opengl.presentation.draw_failed",
                                            "OpenGL presentation frame failed"}
                                    .withContext("exception", exception.what()));
    } catch (...) {
        return core::unexpected(core::Error{"render.opengl.presentation.draw_failed",
                                            "OpenGL presentation frame failed"});
    }
}

auto OpenGlBackend::capabilities() const noexcept -> const playback::PresentationCapabilities& {
    if (!capabilitiesReady_ && presentation_ != nullptr && window_.valid() && context_ != nullptr &&
        SDL_IsMainThread() && ownerThread_.isCurrent()) {
        auto* nativeWindow = static_cast<SDL_Window*>(window_.nativeHandle());
        if (nativeWindow != nullptr &&
            SDL_GL_MakeCurrent(nativeWindow, static_cast<SDL_GLContext>(context_))) {
            GLint maxTextureSize = 0;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
            const auto textureLimit = static_cast<std::uint32_t>(
                std::clamp(maxTextureSize, 0, static_cast<int>(portableMaxTextureDimension)));
            capabilities_ = builtInPresentationCapabilities(textureLimit, debugProgram_ != 0);
            capabilitiesReady_ = true;
        }
    }
    return capabilities_;
}

auto OpenGlBackend::identity() const noexcept -> presentation_renderer::RendererIdentity {
    return presentation_renderer::RendererIdentity{
        presentation_ != nullptr ? presentation_->backendToken : 0, rendererGeneration_};
}

auto OpenGlBackend::prepare(playback::PreparedPlayback& prepared,
                            const playback::PresentationRequest& request)
    -> core::Result<presentation_renderer::PreparedPresentation> {
    if (deviceLost_) {
        return core::unexpected(core::Error{"presentation.renderer.surface.lost",
                                            "The presentation surface is lost until rebuild"});
    }
    if (interfaceCandidate_) {
        return core::unexpected(
            core::Error{"render.opengl.presentation.candidate_outstanding",
                        "The previous OpenGL presentation candidate is still outstanding"});
    }
    auto candidate = preparePresentation(prepared, request);
    if (!candidate) {
        return core::unexpected(std::move(candidate.error()));
    }
    const auto generation = candidate->generation_;
    const auto token = candidate->token();
    const auto settings = candidate->settings();
    interfaceCandidate_ = std::move(*candidate);
    return presentation_renderer::sealPreparedPresentation(
        *this, presentation_->backendToken, rendererGeneration_, generation, token, settings);
}

auto OpenGlBackend::prepareOwned(const playback::PresentationCandidateInput& input)
    -> core::Result<presentation_renderer::PreparedPresentation> {
    if (deviceLost_ || interfaceCandidate_)
        return core::unexpected(
            core::Error{"host.rpa.device_failed", "GPU owner cannot prepare candidate"});
    auto candidate = preparePresentationOwned(input);
    if (!candidate)
        return core::unexpected(candidate.error());
    const auto generation = candidate->generation_;
    const auto token = candidate->token();
    const auto settings = candidate->settings();
    interfaceCandidate_ = std::move(*candidate);
    return presentation_renderer::sealPreparedPresentation(
        *this, presentation_->backendToken, rendererGeneration_, generation, token, settings);
}

auto OpenGlBackend::accepts(const presentation_renderer::PreparedPresentation& candidate) const
    -> core::Result<void> {
    if (!SDL_IsMainThread() || !ownerThread_.isCurrent()) {
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "The OpenGL backend belongs to another thread"}
                                    .withContext("operation", "accepts"));
    }
    if (!candidate.valid() || candidate.playbackToken() == nullptr || presentation_ == nullptr ||
        !interfaceCandidate_ || !interfaceCandidate_->valid()) {
        return core::unexpected(core::Error{"presentation.renderer.token.invalid",
                                            "The presentation candidate is not outstanding"});
    }
    const auto identity = candidate.rendererIdentity();
    if (identity.instance != presentation_->backendToken ||
        identity.generation != rendererGeneration_ ||
        !(*candidate.playbackToken() == interfaceCandidate_->token())) {
        return core::unexpected(core::Error{"presentation.renderer.token.stale",
                                            "The presentation candidate token is stale"});
    }
    return {};
}

void OpenGlBackend::activate(presentation_renderer::PreparedPresentation&& candidate) noexcept {
    if (!accepts(candidate).has_value() || !interfaceCandidate_) {
        std::terminate();
    }
    activatePresentation(std::move(*interfaceCandidate_));
    interfaceCandidate_.reset();
}

void OpenGlBackend::discard(presentation_renderer::PreparedPresentation&& candidate) noexcept {
    if (!SDL_IsMainThread() || !ownerThread_.isCurrent() || !candidate.valid()) {
        std::terminate();
    }
    if (!interfaceCandidate_) {
        return;
    }
    const auto identity = candidate.rendererIdentity();
    if (presentation_ == nullptr || identity.instance != presentation_->backendToken ||
        identity.generation != rendererGeneration_ || candidate.playbackToken() == nullptr ||
        !(*candidate.playbackToken() == interfaceCandidate_->token())) {
        interfaceCandidate_.reset();
        return;
    }
    discardPresentation(std::move(*interfaceCandidate_));
    interfaceCandidate_.reset();
}

auto OpenGlBackend::submit(const playback::FrameSnapshot& snapshot,
                           const render::RenderScene* debugScene)
    -> core::Result<presentation_renderer::DrawSummary> {
    if (deviceLost_) {
        return core::unexpected(core::Error{"presentation.renderer.surface.lost",
                                            "The presentation surface is lost until rebuild"});
    }
    if (surfaceWidth_ == 0 || surfaceHeight_ == 0) {
        return core::unexpected(
            core::Error{"presentation.renderer.surface.zero_size",
                        "A zero-size surface suspends presentation submission"});
    }
    if (frameSubmitted_) {
        return core::unexpected(core::Error{"presentation.renderer.frame.already_submitted",
                                            "The current frame was already submitted"});
    }
    presentation_renderer::DrawSummary summary;
    OpenGlPixelProbe probe;
    auto drawn = renderPresentation(
        snapshot, debugScene, &summary,
        presentation_ && presentation_->pixelReadbackEnabled ? &probe : nullptr, false);
    if (!drawn) {
        return core::unexpected(std::move(drawn.error()));
    }
    frameSubmitted_ = true;
    // The neutral builder already produced the authoritative summary; no second conversion.
    return summary;
}

auto OpenGlBackend::present() -> core::Result<void> {
    if (!SDL_IsMainThread() || !ownerThread_.isCurrent()) {
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "The OpenGL backend belongs to another thread"}
                                    .withContext("operation", "present"));
    }
    if (deviceLost_) {
        return core::unexpected(core::Error{"presentation.renderer.surface.lost",
                                            "The presentation surface is lost until rebuild"});
    }
    if (!frameSubmitted_) {
        return core::unexpected(core::Error{"presentation.renderer.frame.not_submitted",
                                            "Present requires a submitted frame"});
    }
    frameSubmitted_ = false;
    if (presentFailure_.has_value()) {
        const auto failure = *presentFailure_;
        presentFailure_.reset();
        if (failure == presentation_renderer::PresentationFailureClass::Unrecoverable) {
            deviceLost_ = true;
            return core::unexpected(
                core::Error{"presentation.renderer.present.device_lost",
                            "Present failed because the presentation device was lost"});
        }
        return core::unexpected(
            core::Error{"presentation.renderer.present.failed",
                        "Present failed and the active presentation was left unchanged"});
    }
    if (!window_.valid() || context_ == nullptr) {
        return core::unexpected(core::Error{"render.opengl.backend_unavailable",
                                            "The OpenGL presentation backend is unavailable"});
    }
    auto* nativeWindow = static_cast<SDL_Window*>(window_.nativeHandle());
    if (nativeWindow == nullptr ||
        !SDL_GL_MakeCurrent(nativeWindow, static_cast<SDL_GLContext>(context_)) ||
        !SDL_GL_SwapWindow(nativeWindow)) {
        return core::unexpected(core::Error{"render.opengl.swap_failed", sdlError()});
    }
    return {};
}

auto OpenGlBackend::resize(std::uint32_t width, std::uint32_t height) -> core::Result<void> {
    if (!SDL_IsMainThread() || !ownerThread_.isCurrent()) {
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "The OpenGL backend belongs to another thread"}
                                    .withContext("operation", "resize"));
    }
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    return {};
}

auto OpenGlBackend::rebuild() -> core::Result<void> {
    if (!SDL_IsMainThread() || !ownerThread_.isCurrent()) {
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "The OpenGL backend belongs to another thread"}
                                    .withContext("operation", "rebuild"));
    }
    if (rendererGeneration_ == std::numeric_limits<std::uint64_t>::max()) {
        return core::unexpected(core::Error{"presentation.renderer.generation_exhausted",
                                            "Presentation renderer generation is exhausted"});
    }
    const bool retainedResources =
        presentation_ != nullptr &&
        (presentation_->active.has_value() || presentation_->pending.has_value() ||
         presentation_->retired.has_value());
    if (retainedResources) {
        if (!window_.valid() || context_ == nullptr) {
            return core::unexpected(
                core::Error{"render.opengl.cleanup_unavailable",
                            "Presentation resources require their OpenGL context for cleanup"});
        }
        auto* nativeWindow = static_cast<SDL_Window*>(window_.nativeHandle());
        if (nativeWindow == nullptr ||
            !SDL_GL_MakeCurrent(nativeWindow, static_cast<SDL_GLContext>(context_))) {
            return core::unexpected(
                core::Error{"render.opengl.cleanup_unavailable",
                            "Cannot make the OpenGL context current for presentation cleanup"});
        }
        presentation_->active.reset();
        presentation_->pending.reset();
        presentation_->retired.reset();
        presentation_->pendingGeneration = 0;
    }
    interfaceCandidate_.reset();
    frameSubmitted_ = false;
    deviceLost_ = false;
    presentFailure_.reset();
    ++rendererGeneration_;
    return {};
}

auto OpenGlBackend::lastPixelProbe() const noexcept -> const OpenGlPixelProbe& {
    return lastProbe_;
}

auto detail::OpenGlDiagnosticsAccess::setPixelReadback(OpenGlBackend& backend, bool enabled)
    -> core::Result<void> {
    if (!SDL_IsMainThread() || !backend.ownerThread_.isCurrent())
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "Pixel readback configuration belongs to the owner"});
    if (!backend.presentation_)
        return core::unexpected(core::Error{"render.opengl.backend_unavailable",
                                            "Pixel readback requires a live backend"});
    if (backend.presentation_->pixelReadbackEnabled != enabled) {
        backend.presentation_->pixelReadbackEnabled = enabled;
        backend.lastProbe_ = {};
    }
    return {};
}

void OpenGlBackend::injectPresentFailure(
    presentation_renderer::PresentationFailureClass failure) noexcept {
    presentFailure_ = failure;
}

void OpenGlBackend::detachCandidate(std::uint64_t rendererGeneration,
                                    std::uint64_t candidateGeneration,
                                    const playback::PresentationCandidateToken& token) noexcept {
    if (!interfaceCandidate_) {
        return;
    }
    if (presentation_ == nullptr || rendererGeneration != rendererGeneration_ ||
        !presentation_->pending.has_value() ||
        interfaceCandidate_->generation_ != candidateGeneration ||
        !(interfaceCandidate_->token() == token)) {
        interfaceCandidate_.reset();
        return;
    }
    discardPresentation(std::move(*interfaceCandidate_));
    interfaceCandidate_.reset();
}

static_assert(std::is_nothrow_move_constructible_v<OpenGlPresentationCandidate>);
static_assert(std::is_nothrow_move_assignable_v<OpenGlPresentationCandidate>);
static_assert(std::is_nothrow_move_constructible_v<detail::PresentationResourceSet>);

} // namespace cuexis::render_opengl
