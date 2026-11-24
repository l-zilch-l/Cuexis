#include "frame_layout_vectors.hpp"
#include "open_gl_frame_layout_internal.hpp"

#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>

#include "frame_layout_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

namespace support = cuexis::render_opengl::test_support;

using cuexis::playback::FrameViewport;
using cuexis::playback::experimental::AspectRatio;
using cuexis::playback::experimental::computeContentRect;
using cuexis::playback::experimental::FrameFraming;
using cuexis::playback::experimental::FrameLayout;
using cuexis::playback::experimental::FrameRotation;
using cuexis::playback::experimental::FramingRequest;
using cuexis::playback::experimental::PixelRect;
using cuexis::render_opengl::test_support::DrawProbe;
using cuexis::render_opengl::test_support::Framebuffer;
using cuexis::render_opengl::test_support::GlObjectPool;
using cuexis::render_opengl::test_support::SentinelState;

void clearVideoDriverOverride() {
#if defined(_WIN32)
    _putenv_s("SDL_VIDEODRIVER", "");
#else
    unsetenv("SDL_VIDEODRIVER");
#endif
}

struct GlFixture final {
    cuexis::platform_sdl::SdlRuntime runtime;
    cuexis::platform_sdl::SdlWindow window;
    cuexis::render_opengl::OpenGlBackend backend;
};

auto requireGlFixture() -> std::unique_ptr<GlFixture> {
    clearVideoDriverOverride();
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    if (!runtime) {
        WARN("real SDL video runtime unavailable: " << runtime.error().message());
        return nullptr;
    }
    auto configuration = cuexis::render_opengl::configureOpenGlContext(*runtime);
    if (!configuration) {
        WARN("OpenGL context configuration unavailable: " << configuration.error().message());
        return nullptr;
    }
    cuexis::platform_sdl::WindowConfig windowConfig;
    windowConfig.title = "Cuexis frame layout probe";
    windowConfig.width = 320;
    windowConfig.height = 240;
    windowConfig.resizable = false;
    windowConfig.highDpi = false;
    windowConfig.openGl = true;
    auto window = cuexis::platform_sdl::SdlWindow::create(*runtime, windowConfig);
    if (!window) {
        WARN("OpenGL window unavailable: " << window.error().message());
        return nullptr;
    }
    auto backend = cuexis::render_opengl::OpenGlBackend::create(*window, std::move(*configuration));
    if (!backend) {
        WARN("OpenGL context unavailable: " << backend.error().message());
        return nullptr;
    }
    WARN("OpenGL context: " << support::contextSummary());
    return std::make_unique<GlFixture>(
        GlFixture{std::move(*runtime), std::move(*window), std::move(*backend)});
}

void requirePixel(const std::array<std::uint8_t, 4>& actual, std::array<std::uint8_t, 4> expected) {
    CHECK(actual == expected);
}

void checkSentinelState(const SentinelState& sentinel) {
    const auto actual = support::captureSentinelState();
    CHECK(actual.scissorTest == sentinel.scissorTest);
    CHECK(actual.depthTest == sentinel.depthTest);
    CHECK(actual.blend == sentinel.blend);
    CHECK(actual.cullFace == sentinel.cullFace);
    CHECK(actual.stencilTest == sentinel.stencilTest);
    CHECK(actual.rasterizerDiscard == sentinel.rasterizerDiscard);
    CHECK(actual.colorLogicOp == sentinel.colorLogicOp);
    CHECK(actual.framebufferSrgb == sentinel.framebufferSrgb);

    CHECK(actual.viewport == sentinel.viewport);
    CHECK(actual.scissorBox == sentinel.scissorBox);
    CHECK(actual.clearColor == sentinel.clearColor);
    CHECK(actual.clearDepth == sentinel.clearDepth);
    CHECK(actual.colorMask == sentinel.colorMask);
    CHECK(actual.depthMask == sentinel.depthMask);
    CHECK(actual.depthRange == sentinel.depthRange);

    CHECK(actual.blendSourceRgb == sentinel.blendSourceRgb);
    CHECK(actual.blendDestinationRgb == sentinel.blendDestinationRgb);
    CHECK(actual.blendSourceAlpha == sentinel.blendSourceAlpha);
    CHECK(actual.blendDestinationAlpha == sentinel.blendDestinationAlpha);
    CHECK(actual.blendEquationRgb == sentinel.blendEquationRgb);
    CHECK(actual.blendEquationAlpha == sentinel.blendEquationAlpha);
    CHECK(actual.logicOp == sentinel.logicOp);
    CHECK(actual.depthFunc == sentinel.depthFunc);
    CHECK(actual.cullMode == sentinel.cullMode);
    CHECK(actual.frontFace == sentinel.frontFace);
    CHECK(actual.polygonMode == sentinel.polygonMode);
    CHECK(actual.renderbuffer == sentinel.renderbuffer);
    CHECK(actual.program == sentinel.program);
    CHECK(actual.vertexArray == sentinel.vertexArray);
    CHECK(actual.vertexBuffer == sentinel.vertexBuffer);
    CHECK(actual.uniformBuffer == sentinel.uniformBuffer);
    CHECK(actual.uniformBufferSlot0 == sentinel.uniformBuffer);
    CHECK(actual.activeTexture == sentinel.activeTexture);
    CHECK(actual.texture == sentinel.texture);
    CHECK(actual.framebuffer == sentinel.framebuffer);
    CHECK(actual.readFramebuffer == sentinel.framebuffer);
}

void checkDrawRoutesNone(std::uint32_t framebuffer) {
    support::bindFramebuffer(framebuffer);
    const auto routes = support::drawRoutes();
    for (const auto route : routes) {
        CHECK(route == support::noAttachment);
    }
}

void runBorrowedFramebufferChecks() {
    auto fixture = requireGlFixture();
    if (!fixture) {
        WARN("GL borrow checks were not executed because a real OpenGL context is unavailable");
        return;
    }
    GlObjectPool objects;
    auto sentinel = support::makeSentinelState();
    sentinel.framebuffer = objects.framebuffer();
    const auto sentinelColor = objects.colorRenderbuffer(64, 64);
    support::attachColorRenderbuffer(sentinel.framebuffer, sentinelColor);
    sentinel.renderbuffer = objects.colorRenderbuffer(16, 16);
    auto probe = support::makeDrawProbe(objects);
    if (probe.program == 0) {
        WARN("borrowed frame probe shader did not link");
    }
    REQUIRE(probe.program != 0);
    support::createSentinelObjects(objects, sentinel);

    constexpr std::uint32_t width = 1600;
    constexpr std::uint32_t height = 1000;
    const auto borrowed = objects.completeColorDepthFramebuffer(width, height);
    support::bindFramebuffer(borrowed.name);
    support::setAllDrawBuffersNone();
    REQUIRE(support::framebufferComplete());

    support::clearFramebuffer(borrowed.name, 0, 0.0F, 1.0F, 0.0F);
    support::setAllDrawBuffersNone();

    const auto layoutResult = computeContentRect(
        AspectRatio{21, 9},
        cuexis::playback::experimental::FrameTarget{
            FrameViewport{width, height}, PixelRect{100, 50, 1280, 721}, FrameRotation::Deg0});
    REQUIRE(layoutResult.has_value());
    REQUIRE(layoutResult->has_value());
    const auto& layout = **layoutResult;
    CHECK(layout.content.x == 0U);
    CHECK(layout.content.y == 86U);
    CHECK(layout.content.width == 1280U);
    CHECK(layout.content.height == 548U);
    const auto hostGl =
        cuexis::render_opengl::detail::toGlRect(layout.target.host, FrameViewport{width, height});
    CHECK(hostGl.x == 100);
    CHECK(hostGl.y == 229);
    CHECK(hostGl.width == 1280);
    CHECK(hostGl.height == 721);
    const auto contentGl = cuexis::render_opengl::detail::toGlRect(
        PixelRect{layout.content.x + layout.target.host.x, layout.content.y + layout.target.host.y,
                  layout.content.width, layout.content.height},
        FrameViewport{width, height});
    CHECK(contentGl.x == 100);
    CHECK(contentGl.y == 316);
    CHECK(contentGl.width == 1280);
    CHECK(contentGl.height == 548);

    using cuexis::render_opengl::detail::BorrowedGlFramebuffer;
    using cuexis::render_opengl::detail::FrameClearValues;
    const BorrowedGlFramebuffer borrowedTarget{borrowed.name, FrameViewport{width, height}};
    FrameClearValues clear;
    clear.color = {0.2F, 0.4F, 0.6F, 1.0F};
    clear.depth = 1.0F;

    support::setSentinelState(sentinel);
    auto drawn = cuexis::render_opengl::detail::drawBorrowedFrame(borrowedTarget, layout, clear,
                                                                  &support::probeDraw, &probe);
    if (!drawn) {
        std::string details{drawn.error().code()};
        details += " / ";
        details += drawn.error().message();
        for (const auto& context : drawn.error().context()) {
            details += " / ";
            details += context.key;
            details += "=";
            details += context.value;
        }
        FAIL("drawBorrowedFrame failed: " << details);
    }
    checkSentinelState(sentinel);
    checkDrawRoutesNone(borrowed.name);

    const auto center = support::readPixel(borrowed.name, 0, 100 + 640, 316 + 274);
    requirePixel(center, {255, 0, 255, 255});
    const auto hostMargin = support::readPixel(borrowed.name, 0, 110, 239);
    requirePixel(hostMargin, {51, 102, 153, 255});
    const auto outside = support::readPixel(borrowed.name, 0, 10, 10);
    requirePixel(outside, {0, 255, 0, 255});

    support::setSentinelState(sentinel);
    probe.fail = true;
    auto failed = cuexis::render_opengl::detail::drawBorrowedFrame(borrowedTarget, layout, clear,
                                                                   &support::probeDraw, &probe);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "test.frame_layout.draw_failed");
    checkSentinelState(sentinel);
    checkDrawRoutesNone(borrowed.name);
    probe.fail = false;

    // Each invalid attachment layout must be rejected before clear or draw and leave pixels alone.
    const auto resolveFramebuffer = objects.framebuffer();
    const auto resolveColor = objects.colorRenderbuffer(width, height);
    support::attachColorRenderbuffer(resolveFramebuffer, resolveColor);
    support::setAllDrawBuffersNone();
    CHECK(support::framebufferComplete());

    const auto rejectAttachment = [&](const Framebuffer& candidate, std::string_view reason,
                                      std::uint32_t resolve = 0, std::uint32_t attachment = 0) {
        support::clearFramebuffer(candidate.name, attachment, 0.0F, 1.0F, 0.0F);
        const auto readCandidate = [&]() {
            if (resolve != 0) {
                return support::readResolvedPixel(candidate.name, resolve, width, height, 10, 10);
            }
            return support::readPixel(candidate.name, attachment, 10, 10);
        };
        requirePixel(readCandidate(), {0, 255, 0, 255});
        support::setSentinelState(sentinel);
        auto rejected = cuexis::render_opengl::detail::drawBorrowedFrame(
            BorrowedGlFramebuffer{candidate.name, FrameViewport{width, height}}, layout, clear,
            &support::probeDraw, &probe);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "render.opengl.presentation.target_mismatch");
        CHECK(cuexis::test::frame_layout::hasContext(rejected.error(), "reason", reason));
        checkSentinelState(sentinel);
        support::bindFramebuffer(candidate.name);
        support::setAllDrawBuffersNone();
        requirePixel(readCandidate(), {0, 255, 0, 255});
    };

    {
        Framebuffer missingDepth;
        missingDepth.name = objects.framebuffer();
        missingDepth.color = objects.colorRenderbuffer(width, height);
        support::attachColorRenderbuffer(missingDepth.name, missingDepth.color);
        support::setAllDrawBuffersNone();
        CHECK(support::framebufferComplete());
        rejectAttachment(missingDepth, "attachment_layout");
    }

    {
        Framebuffer color1;
        color1.name = objects.framebuffer();
        color1.color = objects.colorRenderbuffer(width, height);
        color1.depth = objects.depthRenderbuffer(width, height);
        support::attachColorRenderbufferAt(color1.name, 1, color1.color);
        support::attachDepthRenderbuffer(color1.name, color1.depth);
        support::setDrawBuffersSingle(1);
        CHECK(support::framebufferComplete());
        support::clearFramebuffer(color1.name, 1, 0.0F, 1.0F, 0.0F);
        support::setSentinelState(sentinel);
        auto rejected = cuexis::render_opengl::detail::drawBorrowedFrame(
            BorrowedGlFramebuffer{color1.name, FrameViewport{width, height}}, layout, clear,
            &support::probeDraw, &probe);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "render.opengl.presentation.target_mismatch");
        CHECK(cuexis::test::frame_layout::hasContext(rejected.error(), "reason",
                                                     "attachment_layout"));
        CHECK(cuexis::test::frame_layout::hasContext(rejected.error(), "attachment", "color0"));
        checkSentinelState(sentinel);
        const auto pixel = support::readPixel(color1.name, 1, 10, 10);
        requirePixel(pixel, {0, 255, 0, 255});
    }

    {
        Framebuffer texture;
        texture.name = objects.framebuffer();
        texture.color = objects.texture(width, height);
        texture.depth = objects.depthRenderbuffer(width, height);
        support::attachColorTexture(texture.name, texture.color);
        support::attachDepthRenderbuffer(texture.name, texture.depth);
        support::setAllDrawBuffersNone();
        CHECK(support::framebufferComplete());
        rejectAttachment(texture, "attachment_layout");
    }

    {
        Framebuffer depthStencil;
        depthStencil.name = objects.framebuffer();
        depthStencil.color = objects.colorRenderbuffer(width, height);
        depthStencil.depth = objects.depthStencilRenderbuffer(width, height);
        support::attachColorRenderbuffer(depthStencil.name, depthStencil.color);
        support::attachDepthStencilRenderbuffer(depthStencil.name, depthStencil.depth);
        support::setAllDrawBuffersNone();
        CHECK(support::framebufferComplete());
        rejectAttachment(depthStencil, "attachment_layout");
    }

    {
        Framebuffer mismatched;
        mismatched.name = objects.framebuffer();
        mismatched.color = objects.colorRenderbuffer(width, height);
        mismatched.depth = objects.depthRenderbuffer(width / 2, height / 2);
        support::attachColorRenderbuffer(mismatched.name, mismatched.color);
        support::attachDepthRenderbuffer(mismatched.name, mismatched.depth);
        support::setAllDrawBuffersNone();
        CHECK(support::framebufferComplete());
        rejectAttachment(mismatched, "attachment_layout");
    }

    {
        const auto maxSamples = support::maxSamples();
        if (maxSamples >= 4) {
            Framebuffer multisample;
            multisample.name = objects.framebuffer();
            multisample.color = objects.colorRenderbuffer(width, height, 4);
            multisample.depth = objects.depthRenderbuffer(width, height, 4);
            support::attachColorRenderbuffer(multisample.name, multisample.color);
            support::attachDepthRenderbuffer(multisample.name, multisample.depth);
            support::setAllDrawBuffersNone();
            CHECK(support::framebufferComplete());
            rejectAttachment(multisample, "attachment_layout", resolveFramebuffer);
        }
    }

    {
        const auto incomplete = objects.completeColorDepthFramebuffer(width, height);
        support::bindFramebuffer(incomplete.name);
        const auto maxDrawBuffers = support::maxDrawBuffers();
        if (maxDrawBuffers > 5) {
            support::setDrawBuffersSingle(5);
            if (!support::framebufferComplete()) {
                support::clearFramebuffer(incomplete.name, 0, 0.0F, 1.0F, 0.0F);
                support::bindFramebuffer(incomplete.name);
                support::setDrawBuffersSingle(5);
                support::setSentinelState(sentinel);
                auto rejected = cuexis::render_opengl::detail::drawBorrowedFrame(
                    BorrowedGlFramebuffer{incomplete.name, FrameViewport{width, height}}, layout,
                    clear, &support::probeDraw, &probe);
                REQUIRE_FALSE(rejected.has_value());
                CHECK(rejected.error().code() == "render.opengl.presentation.target_mismatch");
                CHECK(cuexis::test::frame_layout::hasContext(rejected.error(), "reason",
                                                             "framebuffer_incomplete"));
                checkSentinelState(sentinel);
                const auto pixel = support::readPixel(incomplete.name, 0, 10, 10);
                requirePixel(pixel, {0, 255, 0, 255});
            } else {
                WARN("GL driver reports a complete framebuffer for a draw buffer that names a "
                     "missing color attachment; the framebuffer_incomplete branch was not "
                     "exercised");
            }
        }
    }
}

} // namespace

TEST_CASE("rr_frame_layout", "[render][frame_layout][r5]") {
    cuexis::test::frame_layout::runFrameLayoutGeometryChecks();
    cuexis::test::frame_layout::runFrameLayoutValidationChecks();
    cuexis::test::frame_layout::runFrameLayoutProjectionChecks();
    runBorrowedFramebufferChecks();
}
