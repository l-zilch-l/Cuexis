// Experimental OpenGL frame bridge: owning prepare with framings, target/identity validation and
// borrowed-framebuffer drawing. Requires a real GL context; the test skips with a warning when the
// driver/window is unavailable.

#include "frame_layout_access.hpp"
#include "presentation_fixture.hpp"

#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>
#include <cuexis/render_opengl/experimental/frame_layout.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>

#include <catch2/catch_test_macros.hpp>

#include <glad/glad.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using cuexis::playback::experimental::AspectRatio;
using cuexis::playback::experimental::FrameFraming;
using cuexis::playback::experimental::FrameRotation;
using cuexis::playback::experimental::FrameTarget;
using cuexis::playback::experimental::PixelRect;

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
    windowConfig.title = "Cuexis frame bridge probe";
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
        WARN("OpenGL backend unavailable: " << backend.error().message());
        return nullptr;
    }
    return std::make_unique<GlFixture>(
        GlFixture{std::move(*runtime), std::move(*window), std::move(*backend)});
}

struct BorrowedFbo final {
    GLuint framebuffer{};
    GLuint color{};
    GLuint depth{};
    std::uint32_t width{};
    std::uint32_t height{};

    BorrowedFbo() = default;
    BorrowedFbo(BorrowedFbo&& other) noexcept
        : framebuffer(std::exchange(other.framebuffer, 0)), color(std::exchange(other.color, 0)),
          depth(std::exchange(other.depth, 0)), width(other.width), height(other.height) {}
    auto operator=(BorrowedFbo&& other) noexcept -> BorrowedFbo& {
        if (this != &other) {
            framebuffer = std::exchange(other.framebuffer, 0);
            color = std::exchange(other.color, 0);
            depth = std::exchange(other.depth, 0);
            width = other.width;
            height = other.height;
        }
        return *this;
    }
    BorrowedFbo(const BorrowedFbo&) = delete;
    auto operator=(const BorrowedFbo&) -> BorrowedFbo& = delete;
    ~BorrowedFbo() {
        if (depth != 0) {
            glDeleteRenderbuffers(1, &depth);
        }
        if (color != 0) {
            glDeleteRenderbuffers(1, &color);
        }
        if (framebuffer != 0) {
            glDeleteFramebuffers(1, &framebuffer);
        }
    }
};

auto createBorrowedFbo(std::uint32_t width, std::uint32_t height) -> std::optional<BorrowedFbo> {
    BorrowedFbo fbo;
    fbo.width = width;
    fbo.height = height;
    glGenRenderbuffers(1, &fbo.color);
    glBindRenderbuffer(GL_RENDERBUFFER, fbo.color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, static_cast<GLsizei>(width),
                          static_cast<GLsizei>(height));
    glGenRenderbuffers(1, &fbo.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, fbo.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(width),
                          static_cast<GLsizei>(height));
    glGenFramebuffers(1, &fbo.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fbo.color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo.depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        return std::nullopt;
    }
    // The bridge must explicitly route to color0 even when every draw buffer starts disabled.
    glDrawBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

auto glDrawRoutes() -> std::vector<GLenum> {
    GLint count = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count);
    std::vector<GLenum> routes(static_cast<std::size_t>(count), GL_NONE);
    for (GLint index = 0; index < count; ++index) {
        GLint value = GL_NONE;
        glGetIntegerv(GL_DRAW_BUFFER0 + index, &value);
        routes[static_cast<std::size_t>(index)] = static_cast<GLenum>(value);
    }
    return routes;
}

auto readPixel(std::uint32_t x, std::uint32_t y) -> std::array<std::uint8_t, 4> {
    std::array<std::uint8_t, 4> pixel{};
    glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixel.data());
    return pixel;
}

} // namespace

TEST_CASE("rr_frame_layout_gl_bridge", "[render][opengl][frame-layout][bridge]") {
    const auto fixture = requireGlFixture();
    if (!fixture) {
        return;
    }
    auto& backend = fixture->backend;

    cuexis::playback::PlaybackSession session;
    auto source = cuexis::playback::PlaybackSource::fromFilesystemProject(
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets/projects/stage3_project");
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    auto input = prepared->materializePresentation(
        backend.capabilities(), {}, backend.identity().instance, backend.identity().generation, 1);
    REQUIRE(input.has_value());

    const std::array<FrameFraming, 2> table{FrameFraming{AspectRatio{7, 3}, 1.0},
                                            FrameFraming{AspectRatio{9, 16}, 2.0}};
    REQUIRE(
        cuexis::playback::detail::FrameLayoutAccess::attachFramings(*prepared, table).has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());

    // A non-empty table requires the declared host extension.
    auto withoutCapability = *input;
    withoutCapability.capabilities.hostExtensionIds.clear();
    auto rejected =
        cuexis::render_opengl::experimental::prepareLaidOutOwned(backend, withoutCapability, table);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "playback.presentation.layout.unsupported");

    const FrameTarget target{
        .extent = {256, 160}, .host = PixelRect{8, 8, 240, 144}, .rotation = FrameRotation::Deg0};
    auto laidOut = cuexis::playback::experimental::extractLaidOutFrame(session, target);
    REQUIRE(laidOut.has_value());
    REQUIRE(laidOut->has_value());

    auto gpu = cuexis::render_opengl::experimental::prepareLaidOutOwned(backend, *input, table);
    REQUIRE(gpu.has_value());
    REQUIRE(backend.accepts(*gpu).has_value());
    backend.activate(std::move(*gpu));

    auto fbo = createBorrowedFbo(target.extent.width, target.extent.height);
    if (!fbo) {
        WARN("borrowed framebuffer could not be completed; GPU frame checks skipped");
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClearColor(0.0F, 1.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawBuffer(GL_NONE);
    const auto routesBefore = glDrawRoutes();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const auto borrowed =
        cuexis::render_opengl::experimental::BorrowedGlFramebuffer{fbo->framebuffer, target.extent};
    // A target changed after extraction is rejected before any clear or draw.
    auto changed = target;
    changed.host.x = 9;
    auto mismatch = cuexis::render_opengl::experimental::submitLaidOut(backend, **laidOut, changed,
                                                                       nullptr, &borrowed);
    REQUIRE_FALSE(mismatch.has_value());
    CHECK(mismatch.error().code() == "render.opengl.presentation.target_mismatch");
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer);
    const auto untouched = readPixel(2, 2);
    CHECK(untouched[1] == 255);
    CHECK(untouched[0] == 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CHECK(backend.hasActivePresentation());

    auto drawn = cuexis::render_opengl::experimental::submitLaidOut(backend, **laidOut, target,
                                                                    nullptr, &borrowed);
    if (!drawn) {
        std::fprintf(stderr, "borrowed submit error: %s / %s\n",
                     std::string{drawn.error().code()}.c_str(),
                     std::string{drawn.error().message()}.c_str());
    }
    REQUIRE(drawn.has_value());
    // Borrowed success never arms the window present.
    CHECK_FALSE(backend.present().has_value());

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->framebuffer);
    const auto margin = readPixel(2, 2);
    CHECK(margin == untouched);
    const std::uint32_t contentX = target.host.x + (**laidOut).layout().content.x;
    const std::uint32_t contentY = target.host.y + (**laidOut).layout().content.y;
    const std::uint32_t glCenterX = contentX + (**laidOut).layout().content.width / 2;
    const std::uint32_t glCenterY =
        target.extent.height - (contentY + (**laidOut).layout().content.height / 2);
    const auto content = readPixel(glCenterX, glCenterY);
    CHECK(content[3] == 255);
    CHECK_FALSE(content == untouched); // the content area was cleared/drawn, the margin was not
    const auto routesAfter = glDrawRoutes();
    CHECK(routesAfter == routesBefore);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
