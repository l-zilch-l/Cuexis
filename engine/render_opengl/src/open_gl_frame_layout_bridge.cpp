// Experimental OpenGL frame bridge: owning prepare with a framing table and the laid-out submit
// path for the window and caller-owned framebuffers. The installed OpenGL surface is unchanged.

#include <cuexis/render_opengl/experimental/frame_layout.hpp>

#include "open_gl_frame_layout_internal.hpp"
#include "open_gl_presentation_internal.hpp"
#include "open_gl_presentation_units_internal.hpp"
#include <cuexis/render_opengl/open_gl_backend.hpp>

#include "../internal/cuexis_internal/gpu_storage_observer.hpp"

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::render_opengl {

namespace detail {
// The experimental frame bridge needs owner-private access to the backend cache. The source-only
// header keeps this seam as a forward declaration so installed/source consumers do not include the
// playback experimental header.
struct OpenGlFrameLayoutAccess final {
    [[nodiscard]] static auto prepare(OpenGlBackend&, const playback::PresentationCandidateInput&,
                                      std::span<const playback::experimental::FrameFraming>)
        -> core::Result<presentation_renderer::PreparedPresentation>;
    [[nodiscard]] static auto submit(OpenGlBackend&, const playback::experimental::LaidOutFrame&,
                                     const playback::experimental::FrameTarget& currentTarget,
                                     const render::RenderScene* debugScene, bool borrowed,
                                     std::uint32_t borrowedName,
                                     playback::FrameViewport borrowedExtent)
        -> core::Result<presentation_renderer::DrawSummary>;
};
} // namespace detail

namespace {

constexpr std::string_view frameLayoutCapability = "cuexis.presentation.frame-layout.v1";

[[nodiscard]] auto hasFrameLayoutCapability(const playback::PresentationCapabilities& c) -> bool {
    return std::find(c.hostExtensionIds.begin(), c.hostExtensionIds.end(), frameLayoutCapability) !=
           c.hostExtensionIds.end();
}

[[nodiscard]] auto layoutMismatch(std::string_view reason,
                                  const playback::experimental::FrameTarget& expected,
                                  const playback::experimental::FrameTarget& actual)
    -> core::Error {
    return core::Error{"render.opengl.presentation.target_mismatch",
                       "The current frame target does not match the laid-out target"}
        .withContext("reason", std::string{reason})
        .withContext("expected_extent_width", std::to_string(expected.extent.width))
        .withContext("expected_extent_height", std::to_string(expected.extent.height))
        .withContext("expected_host_x", std::to_string(expected.host.x))
        .withContext("expected_host_y", std::to_string(expected.host.y))
        .withContext("expected_host_width", std::to_string(expected.host.width))
        .withContext("expected_host_height", std::to_string(expected.host.height))
        .withContext("expected_rotation",
                     std::to_string(static_cast<std::uint16_t>(expected.rotation)))
        .withContext("actual_extent_width", std::to_string(actual.extent.width))
        .withContext("actual_extent_height", std::to_string(actual.extent.height))
        .withContext("actual_host_x", std::to_string(actual.host.x))
        .withContext("actual_host_y", std::to_string(actual.host.y))
        .withContext("actual_host_width", std::to_string(actual.host.width))
        .withContext("actual_host_height", std::to_string(actual.host.height))
        .withContext("actual_rotation",
                     std::to_string(static_cast<std::uint16_t>(actual.rotation)));
}

[[nodiscard]] auto sameTarget(const playback::experimental::FrameTarget& left,
                              const playback::experimental::FrameTarget& right) noexcept -> bool {
    return left.extent.width == right.extent.width && left.extent.height == right.extent.height &&
           left.host.x == right.host.x && left.host.y == right.host.y &&
           left.host.width == right.host.width && left.host.height == right.host.height &&
           left.rotation == right.rotation;
}

[[nodiscard]] auto sameFraming(const playback::experimental::FrameFraming& left,
                               const playback::experimental::FrameFraming& right) noexcept -> bool {
    return left.aspect.width == right.aspect.width && left.aspect.height == right.aspect.height &&
           left.verticalScale == right.verticalScale;
}

struct ScissorReset final {
    ~ScissorReset() {
        glDisable(GL_SCISSOR_TEST);
    }
};

[[nodiscard]] auto drawLaidOutContent(
    const playback::FrameSnapshot& snapshot, const detail::PresentationResourceSet& active,
    const detail::PresentationPipeline& pipeline, const render::RenderScene* debugScene,
    std::uint32_t debugProgram, std::uint32_t debugVertexArray, std::uint32_t debugVertexBuffer,
    std::int32_t viewProjectionLocation, presentation_renderer::DrawSummary& summary)
    -> core::Result<void> {
    auto built = presentation_renderer::buildPresentationCommands(
        snapshot, active.resources, active.manifest.entries.empty(),
        active.settings.debugPassEnabled, debugScene);
    if (!built) {
        return core::unexpected(std::move(built.error()));
    }
    summary = std::move(*built);

    std::vector<detail::PreparedDraw> opaque;
    std::vector<detail::PreparedDraw> transparent;
    opaque.reserve(summary.opaque.size());
    transparent.reserve(summary.transparent.size());
    for (const auto& command : summary.opaque) {
        detail::PreparedDraw draw;
        if (auto resolved = resolveGpuDraw(command, active, draw); !resolved) {
            return core::unexpected(std::move(resolved.error()));
        }
        opaque.push_back(std::move(draw));
    }
    for (const auto& command : summary.transparent) {
        detail::PreparedDraw draw;
        if (auto resolved = resolveGpuDraw(command, active, draw); !resolved) {
            return core::unexpected(std::move(resolved.error()));
        }
        transparent.push_back(std::move(draw));
    }
    std::vector<detail::DebugVertex> debugVertices;
    if (summary.debugPassEnabled) {
        if (auto collected = collectDebugVertices(debugScene, debugVertices); !collected) {
            return core::unexpected(std::move(collected.error()));
        }
    }

    const auto viewProjection = multiplyMatrices(summary.projectionMatrix, summary.viewMatrix);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    drawPresentationCommands(pipeline, viewProjection, opaque, false);
    drawPresentationCommands(pipeline, viewProjection, transparent, true);
    if (!debugVertices.empty()) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glUseProgram(debugProgram);
        glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, viewProjection.data());
        glBindVertexArray(debugVertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, debugVertexBuffer);
        test_only::GpuStorage::charge(1, debugVertexBuffer,
                                      debugVertices.size() * sizeof(detail::DebugVertex));
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(debugVertices.size() * sizeof(detail::DebugVertex)),
                     debugVertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(debugVertices.size()));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
    }
    return checkGl("render.opengl.presentation.draw_failed",
                   "OpenGL rejected the laid-out frame drawing");
}

struct BorrowedDrawState final {
    const playback::FrameSnapshot* snapshot{};
    const detail::PresentationResourceSet* active{};
    const detail::PresentationPipeline* pipeline{};
    const render::RenderScene* debugScene{};
    std::uint32_t debugProgram{};
    std::uint32_t debugVertexArray{};
    std::uint32_t debugVertexBuffer{};
    std::int32_t viewProjectionLocation{};
    presentation_renderer::DrawSummary summary{};
};

auto borrowedDraw(void* context) -> core::Result<void> {
    auto& state = *static_cast<BorrowedDrawState*>(context);
    return drawLaidOutContent(*state.snapshot, *state.active, *state.pipeline, state.debugScene,
                              state.debugProgram, state.debugVertexArray, state.debugVertexBuffer,
                              state.viewProjectionLocation, state.summary);
}

} // namespace

auto experimental::prepareLaidOutOwned(
    OpenGlBackend& backend, const playback::PresentationCandidateInput& input,
    std::span<const playback::experimental::FrameFraming> framings)
    -> core::Result<presentation_renderer::PreparedPresentation> try {
    constexpr std::size_t maxFramingCount = 16;
    if (framings.size() > maxFramingCount) {
        return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                            "Prepared framing table exceeds its capacity"}
                                    .withContext("count", std::to_string(framings.size())));
    }
    std::vector<playback::experimental::AspectRatio> seen;
    seen.reserve(framings.size());
    for (std::size_t index = 0; index < framings.size(); ++index) {
        const auto& framing = framings[index];
        if (framing.aspect.width == 0 || framing.aspect.height == 0 ||
            framing.aspect.width > 2147483647U || framing.aspect.height > 2147483647U) {
            return core::unexpected(
                core::Error{"playback.presentation.layout.invalid_aspect",
                            "Frame aspect must use positive components within the accepted range"}
                    .withContext("index", std::to_string(index)));
        }
        if (!std::isfinite(framing.verticalScale) || framing.verticalScale < 0.125 ||
            framing.verticalScale > 8.0) {
            return core::unexpected(
                core::Error{"playback.presentation.layout.invalid_framing",
                            "A prepared framing entry is outside its accepted domain"}
                    .withContext("index", std::to_string(index))
                    .withContext("reason", "vertical_scale"));
        }
        const auto divisor = std::gcd(framing.aspect.width, framing.aspect.height);
        if (framing.aspect.width / divisor != framing.aspect.width ||
            framing.aspect.height / divisor != framing.aspect.height) {
            return core::unexpected(
                core::Error{"playback.presentation.layout.invalid_framing",
                            "A prepared framing entry is outside its accepted domain"}
                    .withContext("index", std::to_string(index))
                    .withContext("reason", "unreduced_aspect"));
        }
        const auto duplicate = std::find_if(seen.begin(), seen.end(), [&](const auto& candidate) {
            return candidate.width == framing.aspect.width &&
                   candidate.height == framing.aspect.height;
        });
        if (duplicate != seen.end()) {
            return core::unexpected(
                core::Error{"playback.presentation.layout.invalid_framing",
                            "A prepared framing entry is outside its accepted domain"}
                    .withContext("index", std::to_string(index))
                    .withContext("reason", "duplicate_aspect"));
        }
        seen.push_back(framing.aspect);
    }
    if (!framings.empty() && !hasFrameLayoutCapability(input.capabilities)) {
        return core::unexpected(
            core::Error{"playback.presentation.layout.unsupported",
                        "The active adapter does not declare the frame layout capability"});
    }
    return detail::OpenGlFrameLayoutAccess::prepare(backend, input, framings);
} catch (...) {
    return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                        "Framing table validation could not allocate memory"});
}

auto experimental::submitLaidOut(OpenGlBackend& backend,
                                 const playback::experimental::LaidOutFrame& frame,
                                 const playback::experimental::FrameTarget& currentTarget,
                                 const render::RenderScene* debugScene,
                                 const experimental::BorrowedGlFramebuffer* borrowed)
    -> core::Result<presentation_renderer::DrawSummary> {
    if (borrowed == nullptr) {
        return detail::OpenGlFrameLayoutAccess::submit(backend, frame, currentTarget, debugScene,
                                                       false, 0, {});
    }
    return detail::OpenGlFrameLayoutAccess::submit(backend, frame, currentTarget, debugScene, true,
                                                   borrowed->name, borrowed->extent);
}

auto detail::OpenGlFrameLayoutAccess::prepare(
    OpenGlBackend& backend, const playback::PresentationCandidateInput& input,
    std::span<const playback::experimental::FrameFraming> framings)
    -> core::Result<presentation_renderer::PreparedPresentation> {
    if (backend.deviceLost_ || backend.interfaceCandidate_) {
        return core::unexpected(
            core::Error{"host.rpa.device_failed", "GPU owner cannot prepare laid-out candidate"});
    }
    auto candidate = backend.preparePresentationOwned(input);
    if (!candidate) {
        return core::unexpected(std::move(candidate.error()));
    }
    if (!backend.presentation_ || !backend.presentation_->pending) {
        return core::unexpected(
            core::Error{"render.opengl.backend_unavailable",
                        "The laid-out candidate did not produce a pending set"});
    }
    backend.presentation_->pending->framings.assign(framings.begin(), framings.end());
    const auto generation = candidate->generation_;
    const auto token = candidate->token();
    const auto settings = candidate->settings();
    backend.interfaceCandidate_ = std::move(*candidate);
    return presentation_renderer::sealPreparedPresentation(
        backend, backend.presentation_->backendToken, backend.rendererGeneration_, generation,
        token, settings);
}

auto detail::OpenGlFrameLayoutAccess::submit(
    OpenGlBackend& backend, const playback::experimental::LaidOutFrame& frame,
    const playback::experimental::FrameTarget& currentTarget, const render::RenderScene* debugScene,
    bool borrowed, std::uint32_t borrowedName, playback::FrameViewport borrowedExtent)
    -> core::Result<presentation_renderer::DrawSummary> try {
    if (backend.deviceLost_) {
        return core::unexpected(core::Error{"presentation.renderer.surface.lost",
                                            "The presentation surface is lost until rebuild"});
    }
    if (!borrowed && (backend.surfaceWidth_ == 0 || backend.surfaceHeight_ == 0)) {
        return core::unexpected(
            core::Error{"presentation.renderer.surface.zero_size",
                        "A zero-size surface suspends presentation submission"});
    }
    if (!borrowed && backend.frameSubmitted_) {
        return core::unexpected(core::Error{"presentation.renderer.frame.already_submitted",
                                            "The current frame was already submitted"});
    }
    if (!backend.presentation_ || !backend.presentation_->active) {
        return core::unexpected(core::Error{"render.opengl.presentation.inactive",
                                            "No active OpenGL presentation cache is available"});
    }
    const auto& active = *backend.presentation_->active;
    const auto& target = frame.layout().target;
    if (!sameTarget(target, currentTarget)) {
        return core::unexpected(layoutMismatch("frame_target_changed", target, currentTarget));
    }
    if (!(frame.resourceToken() == active.token)) {
        return core::unexpected(
            core::Error{"playback.presentation.frame.resource_mismatch",
                        "The laid-out frame does not belong to the active presentation"}
                .withContext("reason", "token"));
    }
    const auto selected = frame.framing();
    if (active.framings.empty() != !selected.has_value()) {
        return core::unexpected(
            core::Error{"playback.presentation.frame.resource_mismatch",
                        "The laid-out frame framing does not belong to the active table"}
                .withContext("reason", "framing_membership"));
    }
    if (selected) {
        const auto found =
            std::find_if(active.framings.begin(), active.framings.end(),
                         [&](const auto& candidate) { return sameFraming(candidate, *selected); });
        if (found == active.framings.end()) {
            return core::unexpected(
                core::Error{"playback.presentation.frame.resource_mismatch",
                            "The laid-out frame framing does not belong to the active table"}
                    .withContext("reason", "framing_membership"));
        }
    }

    if (borrowed) {
        const detail::BorrowedGlFramebuffer targetFbo{borrowedName, borrowedExtent};
        detail::FrameClearValues clear;
        clear.color = {frame.snapshot().clearRed, frame.snapshot().clearGreen,
                       frame.snapshot().clearBlue, frame.snapshot().clearAlpha};
        BorrowedDrawState state{&frame.snapshot(),
                                &active,
                                &backend.presentation_->pipeline,
                                debugScene,
                                backend.debugProgram_,
                                backend.debugVertexArray_,
                                backend.debugVertexBuffer_,
                                backend.viewProjectionLocation_};
        auto drawn =
            detail::drawBorrowedFrame(targetFbo, frame.layout(), clear, &borrowedDraw, &state);
        if (!drawn) {
            return core::unexpected(std::move(drawn.error()));
        }
        return state.summary;
    }

    if (!backend.window_.valid() || backend.context_ == nullptr) {
        return core::unexpected(core::Error{"render.opengl.backend_unavailable",
                                            "The OpenGL presentation backend is unavailable"});
    }
    auto* nativeWindow = static_cast<SDL_Window*>(backend.window_.nativeHandle());
    if (nativeWindow == nullptr ||
        !SDL_GL_MakeCurrent(nativeWindow, static_cast<SDL_GLContext>(backend.context_))) {
        return core::unexpected(core::Error{"render.opengl.context_current_failed", sdlError()});
    }
    int drawableWidth = 0;
    int drawableHeight = 0;
    if (!SDL_GetWindowSizeInPixels(nativeWindow, &drawableWidth, &drawableHeight) ||
        drawableWidth <= 0 || drawableHeight <= 0) {
        return core::unexpected(core::Error{"presentation.renderer.surface.zero_size",
                                            "The window drawable size is not available"});
    }
    if (static_cast<std::uint32_t>(drawableWidth) != target.extent.width ||
        static_cast<std::uint32_t>(drawableHeight) != target.extent.height) {
        return core::unexpected(layoutMismatch(
            "drawable_extent", target,
            playback::experimental::FrameTarget{{static_cast<std::uint32_t>(drawableWidth),
                                                 static_cast<std::uint32_t>(drawableHeight)},
                                                target.host,
                                                target.rotation}));
    }
    constexpr auto maxDimension = static_cast<std::uint64_t>(std::numeric_limits<GLsizei>::max());
    if (target.extent.width > maxDimension || target.extent.height > maxDimension) {
        return core::unexpected(layoutMismatch("gl_extent_range", target, currentTarget));
    }

    const auto host = detail::toGlRect(target.host, target.extent);
    const auto absoluteContent = playback::experimental::PixelRect{
        target.host.x + frame.layout().content.x, target.host.y + frame.layout().content.y,
        frame.layout().content.width, frame.layout().content.height};
    const auto content = detail::toGlRect(absoluteContent, target.extent);

    presentation_renderer::DrawSummary summary{};
    {
        ScissorReset scissorReset;
        glViewport(0, 0, static_cast<GLsizei>(target.extent.width),
                   static_cast<GLsizei>(target.extent.height));
        glEnable(GL_SCISSOR_TEST);
        glScissor(host.x, host.y, host.width, host.height);
        glClearColor(frame.snapshot().clearRed, frame.snapshot().clearGreen,
                     frame.snapshot().clearBlue, frame.snapshot().clearAlpha);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(content.x, content.y, content.width, content.height);
        glScissor(content.x, content.y, content.width, content.height);
        auto drawn = drawLaidOutContent(frame.snapshot(), active, backend.presentation_->pipeline,
                                        debugScene, backend.debugProgram_,
                                        backend.debugVertexArray_, backend.debugVertexBuffer_,
                                        backend.viewProjectionLocation_, summary);
        if (!drawn) {
            return core::unexpected(std::move(drawn.error()));
        }
    }

    OpenGlPixelProbe probe;
    probe.presentationDrawn = !summary.opaque.empty() || !summary.transparent.empty();
    if (backend.presentation_->pixelReadbackEnabled && content.width > 0 && content.height > 0) {
        glReadPixels(content.x + content.width / 2, content.y + content.height / 2, 1, 1, GL_RGBA,
                     GL_UNSIGNED_BYTE, probe.rgba.data());
        if (auto checked = checkGl("render.opengl.presentation.readback_failed",
                                   "OpenGL rejected presentation pixel readback");
            !checked) {
            return core::unexpected(std::move(checked.error()));
        }
    }
    backend.lastProbe_ = probe;
    backend.frameSubmitted_ = true;
    return summary;
} catch (const std::bad_alloc&) {
    return core::unexpected(core::Error{"playback.presentation.frame.command_budget_exceeded",
                                        "OpenGL laid-out frame allocation could not be satisfied"});
} catch (const std::exception& exception) {
    return core::unexpected(
        core::Error{"render.opengl.presentation.draw_failed", "OpenGL laid-out frame failed"}
            .withContext("exception", exception.what()));
} catch (...) {
    return core::unexpected(
        core::Error{"render.opengl.presentation.draw_failed", "OpenGL laid-out frame failed"});
}

} // namespace cuexis::render_opengl
