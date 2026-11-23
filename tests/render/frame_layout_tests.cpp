#include "frame_layout_vectors.hpp"
#include "open_gl_frame_layout_internal.hpp"

#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>

#include <catch2/catch_test_macros.hpp>

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using cuexis::core::Result;
using cuexis::core::unexpected;
using cuexis::playback::FrameViewport;
using cuexis::playback::experimental::AspectRatio;
using cuexis::playback::experimental::computeContentRect;
using cuexis::playback::experimental::FrameFraming;
using cuexis::playback::experimental::FrameLayout;
using cuexis::playback::experimental::FrameRotation;
using cuexis::playback::experimental::FramingRequest;
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
    const auto* version = glGetString(GL_VERSION);
    const auto* renderer = glGetString(GL_RENDERER);
    WARN("OpenGL context: "
         << (version != nullptr ? reinterpret_cast<const char*>(version) : "unknown") << " / "
         << (renderer != nullptr ? reinterpret_cast<const char*>(renderer) : "unknown"));
    return std::make_unique<GlFixture>(
        GlFixture{std::move(*runtime), std::move(*window), std::move(*backend)});
}

auto compileShader(GLenum type, const char* source) -> GLuint {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

auto linkProgram(const char* vertexSource, const char* fragmentSource) -> GLuint {
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

struct GlObjects final {
    std::vector<GLuint> framebuffers;
    std::vector<GLuint> renderbuffers;
    std::vector<GLuint> textures;
    std::vector<GLuint> buffers;
    std::vector<GLuint> vertexArrays;
    std::vector<GLuint> programs;

    GlObjects() = default;
    GlObjects(const GlObjects&) = delete;
    auto operator=(const GlObjects&) -> GlObjects& = delete;
    ~GlObjects() {
        for (const auto value : framebuffers) {
            glDeleteFramebuffers(1, &value);
        }
        for (const auto value : renderbuffers) {
            glDeleteRenderbuffers(1, &value);
        }
        for (const auto value : textures) {
            glDeleteTextures(1, &value);
        }
        for (const auto value : buffers) {
            glDeleteBuffers(1, &value);
        }
        for (const auto value : vertexArrays) {
            glDeleteVertexArrays(1, &value);
        }
        for (const auto value : programs) {
            glDeleteProgram(value);
        }
    }

    auto framebuffer() -> GLuint {
        GLuint value = 0;
        glGenFramebuffers(1, &value);
        framebuffers.push_back(value);
        return value;
    }
    auto renderbuffer(GLenum format, GLsizei width, GLsizei height, GLsizei samples = 0) -> GLuint {
        GLuint value = 0;
        glGenRenderbuffers(1, &value);
        renderbuffers.push_back(value);
        glBindRenderbuffer(GL_RENDERBUFFER, value);
        if (samples > 0) {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width, height);
        } else {
            glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
        }
        return value;
    }
    auto texture(GLenum format, GLsizei width, GLsizei height) -> GLuint {
        GLuint value = 0;
        glGenTextures(1, &value);
        textures.push_back(value);
        glBindTexture(GL_TEXTURE_2D, value);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format), width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        return value;
    }
    auto buffer() -> GLuint {
        GLuint value = 0;
        glGenBuffers(1, &value);
        buffers.push_back(value);
        return value;
    }
    auto vertexArray() -> GLuint {
        GLuint value = 0;
        glGenVertexArrays(1, &value);
        vertexArrays.push_back(value);
        return value;
    }
    auto program(const char* vertexSource, const char* fragmentSource) -> GLuint {
        const auto value = linkProgram(vertexSource, fragmentSource);
        programs.push_back(value);
        return value;
    }
};

struct Fbo final {
    GLuint name{};
    GLuint color{};
    GLuint depth{};
};

auto setAllDrawBuffersNone() {
    GLint count = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count);
    std::vector<GLenum> none(static_cast<std::size_t>(std::max(count, 1)), GL_NONE);
    glDrawBuffers(static_cast<GLsizei>(none.size()), none.data());
}

auto completeFramebuffer(GlObjects& objects, GLsizei width, GLsizei height,
                         GLenum depthFormat = GL_DEPTH_COMPONENT24) -> Fbo {
    Fbo fbo;
    fbo.name = objects.framebuffer();
    fbo.color = objects.renderbuffer(GL_RGBA8, width, height);
    fbo.depth = objects.renderbuffer(depthFormat, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.name);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fbo.color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo.depth);
    return fbo;
}

auto framebufferStatus() -> GLenum {
    return glCheckFramebufferStatus(GL_FRAMEBUFFER);
}

auto readPixel(GLuint framebuffer, GLenum attachment, GLint x, GLint y)
    -> std::array<std::uint8_t, 4> {
    while (glGetError() != GL_NO_ERROR) {
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glReadBuffer(attachment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    std::array<std::uint8_t, 4> rgba{};
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    return rgba;
}

auto readResolvedPixel(GLuint multisampleFramebuffer, GLuint resolveFramebuffer, GLsizei width,
                       GLsizei height, GLint x, GLint y) -> std::array<std::uint8_t, 4> {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFramebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFramebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    return readPixel(resolveFramebuffer, GL_COLOR_ATTACHMENT0, x, y);
}

void requirePixel(const std::array<std::uint8_t, 4>& actual, std::array<std::uint8_t, 4> expected) {
    CHECK(actual == expected);
}

struct DrawProbe final {
    GLuint program{};
    GLuint vertexArray{};
    GLuint vertexBuffer{};
    bool fail{};
    bool drawContent{true};
};

auto drawProbe(void* context) -> Result<void> {
    auto& probe = *static_cast<DrawProbe*>(context);
    if (probe.fail) {
        return unexpected(cuexis::core::Error{"test.frame_layout.draw_failed",
                                              "Injected borrowed frame draw failure"});
    }
    if (!probe.drawContent) {
        return {};
    }
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(probe.program);
    glBindVertexArray(probe.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, probe.vertexBuffer);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    return {};
}

struct SentinelState final {
    GLuint framebuffer{};
    GLuint renderbuffer{};
    GLuint program{};
    GLuint vertexArray{};
    GLuint vertexBuffer{};
    GLuint uniformBuffer{};
    GLuint texture{};
    std::array<GLint, 4> viewport{7, 9, 13, 11};
    std::array<GLint, 4> scissorBox{3, 4, 5, 6};
    std::array<GLfloat, 4> clearColor{0.25F, 0.5F, 0.75F, 0.5F};
    std::array<GLboolean, 4> colorMask{GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE};
    std::array<GLfloat, 2> depthRange{0.1F, 0.9F};
    GLfloat clearDepth{0.25F};
    GLint blendSourceRgb{GL_ONE};
    GLint blendDestinationRgb{GL_ZERO};
    GLint blendSourceAlpha{GL_SRC_ALPHA};
    GLint blendDestinationAlpha{GL_ONE_MINUS_SRC_ALPHA};
    GLint blendEquationRgb{GL_FUNC_SUBTRACT};
    GLint blendEquationAlpha{GL_FUNC_ADD};
    GLint logicOp{GL_XOR};
    GLint depthFunc{GL_GREATER};
    GLint cullMode{GL_FRONT};
    GLint frontFace{GL_CW};
    GLint polygonMode{GL_LINE};
    GLint activeTexture{GL_TEXTURE3};
};

void setSentinelState(const SentinelState& sentinel) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sentinel.framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sentinel.framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, sentinel.renderbuffer);
    glViewport(sentinel.viewport[0], sentinel.viewport[1], sentinel.viewport[2],
               sentinel.viewport[3]);
    glScissor(sentinel.scissorBox[0], sentinel.scissorBox[1], sentinel.scissorBox[2],
              sentinel.scissorBox[3]);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(sentinel.clearColor[0], sentinel.clearColor[1], sentinel.clearColor[2],
                 sentinel.clearColor[3]);
    glClearDepth(static_cast<GLdouble>(sentinel.clearDepth));
    glClearStencil(0);
    glColorMask(sentinel.colorMask[0], sentinel.colorMask[1], sentinel.colorMask[2],
                sentinel.colorMask[3]);
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(static_cast<GLenum>(sentinel.depthFunc));
    glEnable(GL_BLEND);
    glBlendFuncSeparate(static_cast<GLenum>(sentinel.blendSourceRgb),
                        static_cast<GLenum>(sentinel.blendDestinationRgb),
                        static_cast<GLenum>(sentinel.blendSourceAlpha),
                        static_cast<GLenum>(sentinel.blendDestinationAlpha));
    glBlendEquationSeparate(static_cast<GLenum>(sentinel.blendEquationRgb),
                            static_cast<GLenum>(sentinel.blendEquationAlpha));
    glEnable(GL_CULL_FACE);
    glCullFace(static_cast<GLenum>(sentinel.cullMode));
    glFrontFace(static_cast<GLenum>(sentinel.frontFace));
    glEnable(GL_STENCIL_TEST);
    glEnable(GL_RASTERIZER_DISCARD);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(static_cast<GLenum>(sentinel.logicOp));
    glEnable(GL_FRAMEBUFFER_SRGB);
    glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(sentinel.polygonMode));
    glDepthRange(static_cast<GLdouble>(sentinel.depthRange[0]),
                 static_cast<GLdouble>(sentinel.depthRange[1]));
    glUseProgram(sentinel.program);
    glBindVertexArray(sentinel.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, sentinel.vertexBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, sentinel.uniformBuffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, sentinel.uniformBuffer);
    glActiveTexture(static_cast<GLenum>(sentinel.activeTexture));
    glBindTexture(GL_TEXTURE_2D, sentinel.texture);
}

void checkSentinelState(const SentinelState& sentinel) {
    CHECK(glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE);
    CHECK(glIsEnabled(GL_DEPTH_TEST) == GL_TRUE);
    CHECK(glIsEnabled(GL_BLEND) == GL_TRUE);
    CHECK(glIsEnabled(GL_CULL_FACE) == GL_TRUE);
    CHECK(glIsEnabled(GL_STENCIL_TEST) == GL_TRUE);
    CHECK(glIsEnabled(GL_RASTERIZER_DISCARD) == GL_TRUE);
    CHECK(glIsEnabled(GL_COLOR_LOGIC_OP) == GL_TRUE);
    CHECK(glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE);

    std::array<GLint, 4> viewport{};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    CHECK(viewport == sentinel.viewport);
    std::array<GLint, 4> scissorBox{};
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox.data());
    CHECK(scissorBox == sentinel.scissorBox);
    std::array<GLfloat, 4> clearColor{};
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor.data());
    CHECK(clearColor == sentinel.clearColor);
    GLfloat clearDepth = 0.0F;
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
    CHECK(clearDepth == sentinel.clearDepth);
    std::array<GLboolean, 4> colorMask{};
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask.data());
    CHECK(colorMask == sentinel.colorMask);
    GLboolean depthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    CHECK(depthMask == GL_FALSE);
    std::array<GLfloat, 2> depthRange{};
    glGetFloatv(GL_DEPTH_RANGE, depthRange.data());
    CHECK(depthRange == sentinel.depthRange);

    GLint value = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &value);
    CHECK(value == sentinel.blendSourceRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &value);
    CHECK(value == sentinel.blendDestinationRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &value);
    CHECK(value == sentinel.blendSourceAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &value);
    CHECK(value == sentinel.blendDestinationAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &value);
    CHECK(value == sentinel.blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &value);
    CHECK(value == sentinel.blendEquationAlpha);
    glGetIntegerv(GL_LOGIC_OP_MODE, &value);
    CHECK(value == sentinel.logicOp);
    glGetIntegerv(GL_DEPTH_FUNC, &value);
    CHECK(value == sentinel.depthFunc);
    glGetIntegerv(GL_CULL_FACE_MODE, &value);
    CHECK(value == sentinel.cullMode);
    glGetIntegerv(GL_FRONT_FACE, &value);
    CHECK(value == sentinel.frontFace);
    std::array<GLint, 2> polygonMode{};
    glGetIntegerv(GL_POLYGON_MODE, polygonMode.data());
    CHECK(polygonMode[0] == sentinel.polygonMode);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &value);
    CHECK(static_cast<GLuint>(value) == sentinel.renderbuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &value);
    CHECK(static_cast<GLuint>(value) == sentinel.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
    CHECK(static_cast<GLuint>(value) == sentinel.vertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
    CHECK(static_cast<GLuint>(value) == sentinel.vertexBuffer);
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &value);
    CHECK(static_cast<GLuint>(value) == sentinel.uniformBuffer);
    GLint slot = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &slot);
    CHECK(static_cast<GLuint>(slot) == sentinel.uniformBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
    CHECK(value == sentinel.activeTexture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    CHECK(static_cast<GLuint>(value) == sentinel.texture);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    CHECK(static_cast<GLuint>(value) == sentinel.framebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
    CHECK(static_cast<GLuint>(value) == sentinel.framebuffer);
}

void checkDrawRoutesNone(GLuint framebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    GLint count = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count);
    for (GLint index = 0; index < count; ++index) {
        GLint route = 0;
        glGetIntegerv(GL_DRAW_BUFFER0 + static_cast<GLenum>(index), &route);
        CHECK(route == GL_NONE);
    }
}

struct BorrowedProbe final {
    GLuint framebuffer{};
    GLuint color{};
    GLuint depth{};
};

auto makeSentinelObjects(GlObjects& objects, SentinelState& sentinel) -> DrawProbe {
    DrawProbe probe;
    probe.program = objects.program("#version 330 core\n"
                                    "layout(location = 0) in vec3 position;\n"
                                    "void main() { gl_Position = vec4(position.xy, 0.0, 1.0); }\n",
                                    "#version 330 core\n"
                                    "out vec4 color;\n"
                                    "void main() { color = vec4(1.0, 0.0, 1.0, 1.0); }\n");
    probe.vertexArray = objects.vertexArray();
    probe.vertexBuffer = objects.buffer();
    if (probe.program == 0) {
        WARN("borrowed frame probe shader did not link");
        return probe;
    }
    const std::array<float, 9> vertices{-1.0F, -1.0F, 0.0F, 3.0F, -1.0F, 0.0F, -1.0F, 3.0F, 0.0F};
    glBindVertexArray(probe.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, probe.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(vertices)), vertices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    sentinel.program = objects.program("#version 330 core\n"
                                       "void main() { gl_Position = vec4(0.0); }\n",
                                       "#version 330 core\n"
                                       "out vec4 color;\n"
                                       "void main() { color = vec4(0.0); }\n");
    sentinel.vertexArray = objects.vertexArray();
    sentinel.vertexBuffer = objects.buffer();
    glBindBuffer(GL_ARRAY_BUFFER, sentinel.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);
    sentinel.uniformBuffer = objects.buffer();
    glBindBuffer(GL_UNIFORM_BUFFER, sentinel.uniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, 16, nullptr, GL_STATIC_DRAW);
    sentinel.texture = objects.texture(GL_RGBA8, 1, 1);
    return probe;
}

void fillBorrowedColor(GLuint framebuffer, GLenum attachment, float red, float green, float blue) {
    while (glGetError() != GL_NO_ERROR) {
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDrawBuffer(attachment);
    glClearColor(red, green, blue, 1.0F);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void runBorrowedFramebufferChecks() {
    auto fixture = requireGlFixture();
    if (!fixture) {
        WARN("GL borrow checks were not executed because a real OpenGL context is unavailable");
        return;
    }
    GlObjects objects;
    SentinelState sentinel;
    sentinel.framebuffer = objects.framebuffer();
    const auto sentinelColor = objects.renderbuffer(GL_RGBA8, 64, 64);
    glBindFramebuffer(GL_FRAMEBUFFER, sentinel.framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sentinelColor);
    sentinel.renderbuffer = objects.renderbuffer(GL_RGBA8, 16, 16);
    auto probe = makeSentinelObjects(objects, sentinel);
    REQUIRE(probe.program != 0);

    constexpr GLsizei width = 1600;
    constexpr GLsizei height = 1000;
    const Fbo borrowed = completeFramebuffer(objects, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, borrowed.name);
    setAllDrawBuffersNone();
    REQUIRE(framebufferStatus() == GL_FRAMEBUFFER_COMPLETE);

    fillBorrowedColor(borrowed.name, GL_COLOR_ATTACHMENT0, 0.0F, 1.0F, 0.0F);
    setAllDrawBuffersNone();

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

    setSentinelState(sentinel);
    auto drawn = cuexis::render_opengl::detail::drawBorrowedFrame(borrowedTarget, layout, clear,
                                                                  &drawProbe, &probe);
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

    const auto center = readPixel(borrowed.name, GL_COLOR_ATTACHMENT0, 100 + 640, 316 + 274);
    requirePixel(center, {255, 0, 255, 255});
    const auto hostMargin = readPixel(borrowed.name, GL_COLOR_ATTACHMENT0, 110, 239);
    requirePixel(hostMargin, {51, 102, 153, 255});
    const auto outside = readPixel(borrowed.name, GL_COLOR_ATTACHMENT0, 10, 10);
    requirePixel(outside, {0, 255, 0, 255});

    setSentinelState(sentinel);
    probe.fail = true;
    auto failed = cuexis::render_opengl::detail::drawBorrowedFrame(borrowedTarget, layout, clear,
                                                                   &drawProbe, &probe);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "test.frame_layout.draw_failed");
    checkSentinelState(sentinel);
    checkDrawRoutesNone(borrowed.name);
    probe.fail = false;

    // Each invalid attachment layout must be rejected before clear or draw and leave pixels alone.
    const GLuint resolveFramebuffer = objects.framebuffer();
    const GLuint resolveColor = objects.renderbuffer(GL_RGBA8, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFramebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolveColor);
    setAllDrawBuffersNone();
    CHECK(framebufferStatus() == GL_FRAMEBUFFER_COMPLETE);

    const auto rejectAttachment = [&](const Fbo& candidate, std::string_view reason,
                                      GLuint resolveFramebuffer = 0,
                                      GLenum attachment = GL_COLOR_ATTACHMENT0) {
        while (glGetError() != GL_NO_ERROR) {
        }
        fillBorrowedColor(candidate.name, attachment, 0.0F, 1.0F, 0.0F);
        const auto readCandidate = [&]() {
            if (resolveFramebuffer != 0) {
                return readResolvedPixel(candidate.name, resolveFramebuffer, width, height, 10, 10);
            }
            return readPixel(candidate.name, attachment, 10, 10);
        };
        requirePixel(readCandidate(), {0, 255, 0, 255});
        setSentinelState(sentinel);
        auto rejected = cuexis::render_opengl::detail::drawBorrowedFrame(
            BorrowedGlFramebuffer{candidate.name, FrameViewport{width, height}}, layout, clear,
            &drawProbe, &probe);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "render.opengl.presentation.target_mismatch");
        CHECK(cuexis::test::frame_layout::hasContext(rejected.error(), "reason", reason));
        checkSentinelState(sentinel);
        glBindFramebuffer(GL_FRAMEBUFFER, candidate.name);
        setAllDrawBuffersNone();
        requirePixel(readCandidate(), {0, 255, 0, 255});
    };

    {
        Fbo missingDepth;
        missingDepth.name = objects.framebuffer();
        missingDepth.color = objects.renderbuffer(GL_RGBA8, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, missingDepth.name);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  missingDepth.color);
        setAllDrawBuffersNone();
        CHECK(framebufferStatus() == GL_FRAMEBUFFER_COMPLETE);
        rejectAttachment(missingDepth, "attachment_layout");
    }

    {
        Fbo color1;
        color1.name = objects.framebuffer();
        color1.color = objects.renderbuffer(GL_RGBA8, width, height);
        color1.depth = objects.renderbuffer(GL_DEPTH_COMPONENT24, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, color1.name);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER,
                                  color1.color);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  color1.depth);
        GLenum drawBuffer = GL_COLOR_ATTACHMENT1;
        glDrawBuffers(1, &drawBuffer);
        CHECK(framebufferStatus() == GL_FRAMEBUFFER_COMPLETE);
        fillBorrowedColor(color1.name, GL_COLOR_ATTACHMENT1, 0.0F, 1.0F, 0.0F);
        setSentinelState(sentinel);
        auto rejected = cuexis::render_opengl::detail::drawBorrowedFrame(
            BorrowedGlFramebuffer{color1.name, FrameViewport{width, height}}, layout, clear,
            &drawProbe, &probe);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "render.opengl.presentation.target_mismatch");
        CHECK(cuexis::test::frame_layout::hasContext(rejected.error(), "reason",
                                                     "attachment_layout"));
        CHECK(cuexis::test::frame_layout::hasContext(rejected.error(), "attachment", "color0"));
        checkSentinelState(sentinel);
        const auto pixel = readPixel(color1.name, GL_COLOR_ATTACHMENT1, 10, 10);
        requirePixel(pixel, {0, 255, 0, 255});
    }

    {
        Fbo texture;
        texture.name = objects.framebuffer();
        texture.color = objects.texture(GL_RGBA8, width, height);
        texture.depth = objects.renderbuffer(GL_DEPTH_COMPONENT24, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, texture.name);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.color,
                               0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  texture.depth);
        setAllDrawBuffersNone();
        CHECK(framebufferStatus() == GL_FRAMEBUFFER_COMPLETE);
        rejectAttachment(texture, "attachment_layout");
    }

    {
        Fbo depthStencil;
        depthStencil.name = objects.framebuffer();
        depthStencil.color = objects.renderbuffer(GL_RGBA8, width, height);
        depthStencil.depth = objects.renderbuffer(GL_DEPTH24_STENCIL8, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, depthStencil.name);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  depthStencil.color);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  depthStencil.depth);
        setAllDrawBuffersNone();
        CHECK(framebufferStatus() == GL_FRAMEBUFFER_COMPLETE);
        rejectAttachment(depthStencil, "attachment_layout");
    }

    {
        Fbo mismatched;
        mismatched.name = objects.framebuffer();
        mismatched.color = objects.renderbuffer(GL_RGBA8, width, height);
        mismatched.depth = objects.renderbuffer(GL_DEPTH_COMPONENT24, width / 2, height / 2);
        glBindFramebuffer(GL_FRAMEBUFFER, mismatched.name);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  mismatched.color);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  mismatched.depth);
        setAllDrawBuffersNone();
        CHECK(framebufferStatus() == GL_FRAMEBUFFER_COMPLETE);
        rejectAttachment(mismatched, "attachment_layout");
    }

    {
        GLint maxSamples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        if (maxSamples >= 4) {
            Fbo multisample;
            multisample.name = objects.framebuffer();
            multisample.color = objects.renderbuffer(GL_RGBA8, width, height, 4);
            multisample.depth = objects.renderbuffer(GL_DEPTH_COMPONENT24, width, height, 4);
            glBindFramebuffer(GL_FRAMEBUFFER, multisample.name);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                      multisample.color);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      multisample.depth);
            setAllDrawBuffersNone();
            CHECK(framebufferStatus() == GL_FRAMEBUFFER_COMPLETE);
            rejectAttachment(multisample, "attachment_layout", resolveFramebuffer);
        }
    }

    {
        Fbo incomplete = completeFramebuffer(objects, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, incomplete.name);
        GLint maxDrawBuffers = 0;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        if (maxDrawBuffers > 5) {
            GLenum target = GL_COLOR_ATTACHMENT5;
            glDrawBuffers(1, &target);
            if (framebufferStatus() != GL_FRAMEBUFFER_COMPLETE) {
                fillBorrowedColor(incomplete.name, GL_COLOR_ATTACHMENT0, 0.0F, 1.0F, 0.0F);
                glBindFramebuffer(GL_FRAMEBUFFER, incomplete.name);
                GLenum targetAgain = GL_COLOR_ATTACHMENT5;
                glDrawBuffers(1, &targetAgain);
                setSentinelState(sentinel);
                auto rejected = cuexis::render_opengl::detail::drawBorrowedFrame(
                    BorrowedGlFramebuffer{incomplete.name, FrameViewport{width, height}}, layout,
                    clear, &drawProbe, &probe);
                REQUIRE_FALSE(rejected.has_value());
                CHECK(rejected.error().code() == "render.opengl.presentation.target_mismatch");
                CHECK(cuexis::test::frame_layout::hasContext(rejected.error(), "reason",
                                                             "framebuffer_incomplete"));
                checkSentinelState(sentinel);
                const auto pixel = readPixel(incomplete.name, GL_COLOR_ATTACHMENT0, 10, 10);
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
