#include "frame_layout_test_support.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::render_opengl::test_support {
namespace {

void clearGlErrors() {
    while (glGetError() != GL_NO_ERROR) {
    }
}

[[nodiscard]] auto compileShader(GLenum type, const char* source) -> GLuint {
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

[[nodiscard]] auto linkProgram(const char* vertexSource, const char* fragmentSource) -> GLuint {
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

[[nodiscard]] auto colorAttachment(std::uint32_t attachmentIndex) -> GLenum {
    return GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(attachmentIndex);
}

void attachRenderbuffer(std::uint32_t framebuffer, std::uint32_t attachment,
                        std::uint32_t renderbuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, static_cast<GLenum>(attachment), GL_RENDERBUFFER,
                              renderbuffer);
}

} // namespace

GlObjectPool::~GlObjectPool() {
    for (const auto value : framebuffers_) {
        glDeleteFramebuffers(1, &value);
    }
    for (const auto value : renderbuffers_) {
        glDeleteRenderbuffers(1, &value);
    }
    for (const auto value : textures_) {
        glDeleteTextures(1, &value);
    }
    for (const auto value : buffers_) {
        glDeleteBuffers(1, &value);
    }
    for (const auto value : vertexArrays_) {
        glDeleteVertexArrays(1, &value);
    }
    for (const auto value : programs_) {
        glDeleteProgram(static_cast<GLuint>(value));
    }
}

auto GlObjectPool::framebuffer() -> std::uint32_t {
    GLuint value = 0;
    glGenFramebuffers(1, &value);
    framebuffers_.push_back(value);
    return value;
}

auto GlObjectPool::renderbuffer(std::uint32_t internalFormat, std::uint32_t width,
                                std::uint32_t height, std::uint32_t samples) -> std::uint32_t {
    GLuint value = 0;
    glGenRenderbuffers(1, &value);
    renderbuffers_.push_back(value);
    glBindRenderbuffer(GL_RENDERBUFFER, value);
    if (samples > 0) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, static_cast<GLsizei>(samples),
                                         static_cast<GLenum>(internalFormat),
                                         static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    } else {
        glRenderbufferStorage(GL_RENDERBUFFER, static_cast<GLenum>(internalFormat),
                              static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
    return value;
}

auto GlObjectPool::colorRenderbuffer(std::uint32_t width, std::uint32_t height,
                                     std::uint32_t samples) -> std::uint32_t {
    return renderbuffer(GL_RGBA8, width, height, samples);
}

auto GlObjectPool::depthRenderbuffer(std::uint32_t width, std::uint32_t height,
                                     std::uint32_t samples) -> std::uint32_t {
    return renderbuffer(GL_DEPTH_COMPONENT24, width, height, samples);
}

auto GlObjectPool::depthStencilRenderbuffer(std::uint32_t width, std::uint32_t height)
    -> std::uint32_t {
    return renderbuffer(GL_DEPTH24_STENCIL8, width, height, 0);
}

auto GlObjectPool::texture(std::uint32_t width, std::uint32_t height) -> std::uint32_t {
    GLuint value = 0;
    glGenTextures(1, &value);
    textures_.push_back(value);
    glBindTexture(GL_TEXTURE_2D, value);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    return value;
}

auto GlObjectPool::buffer() -> std::uint32_t {
    GLuint value = 0;
    glGenBuffers(1, &value);
    buffers_.push_back(value);
    return value;
}

auto GlObjectPool::vertexArray() -> std::uint32_t {
    GLuint value = 0;
    glGenVertexArrays(1, &value);
    vertexArrays_.push_back(value);
    return value;
}

auto GlObjectPool::program(const char* vertexSource, const char* fragmentSource) -> std::uint32_t {
    const auto value = linkProgram(vertexSource, fragmentSource);
    programs_.push_back(value);
    return value;
}

auto GlObjectPool::completeColorDepthFramebuffer(std::uint32_t width, std::uint32_t height)
    -> Framebuffer {
    Framebuffer result;
    result.name = framebuffer();
    result.color = colorRenderbuffer(width, height);
    result.depth = depthRenderbuffer(width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, result.name);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, result.color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, result.depth);
    return result;
}

auto contextSummary() -> std::string {
    const auto* version = glGetString(GL_VERSION);
    const auto* renderer = glGetString(GL_RENDERER);
    return std::string{version != nullptr ? reinterpret_cast<const char*>(version) : "unknown"} +
           " / " + (renderer != nullptr ? reinterpret_cast<const char*>(renderer) : "unknown");
}

auto maxSamples() -> std::int32_t {
    GLint value = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &value);
    return static_cast<std::int32_t>(value);
}

auto maxDrawBuffers() -> std::int32_t {
    GLint value = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &value);
    return static_cast<std::int32_t>(value);
}

void bindFramebuffer(std::uint32_t name) {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(name));
}

void attachColorRenderbuffer(std::uint32_t framebuffer, std::uint32_t renderbuffer) {
    attachColorRenderbufferAt(framebuffer, 0, renderbuffer);
}

void attachColorRenderbufferAt(std::uint32_t framebuffer, std::uint32_t attachmentIndex,
                               std::uint32_t renderbuffer) {
    attachRenderbuffer(framebuffer, GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(attachmentIndex),
                       renderbuffer);
}

void attachDepthRenderbuffer(std::uint32_t framebuffer, std::uint32_t renderbuffer) {
    attachRenderbuffer(framebuffer, GL_DEPTH_ATTACHMENT, renderbuffer);
}

void attachDepthStencilRenderbuffer(std::uint32_t framebuffer, std::uint32_t renderbuffer) {
    attachRenderbuffer(framebuffer, GL_DEPTH_STENCIL_ATTACHMENT, renderbuffer);
}

void attachColorTexture(std::uint32_t framebuffer, std::uint32_t texture) {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           static_cast<GLuint>(texture), 0);
}

void setAllDrawBuffersNone() {
    GLint count = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count);
    const std::vector<GLenum> none(static_cast<std::size_t>(std::max(count, 1)), GL_NONE);
    glDrawBuffers(static_cast<GLsizei>(none.size()), none.data());
}

void setDrawBufferNone() {
    glDrawBuffer(GL_NONE);
}

void setDrawBuffersSingle(std::uint32_t attachmentIndex) {
    GLenum target = colorAttachment(attachmentIndex);
    glDrawBuffers(1, &target);
}

auto framebufferComplete() -> bool {
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

auto drawRoutes() -> std::vector<std::uint32_t> {
    GLint count = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count);
    std::vector<std::uint32_t> routes(static_cast<std::size_t>(count), noAttachment);
    for (GLint index = 0; index < count; ++index) {
        GLint value = GL_NONE;
        glGetIntegerv(GL_DRAW_BUFFER0 + static_cast<GLenum>(index), &value);
        routes[static_cast<std::size_t>(index)] = static_cast<std::uint32_t>(value);
    }
    return routes;
}

void clearFramebuffer(std::uint32_t framebuffer, std::uint32_t attachmentIndex, float red,
                      float green, float blue) {
    clearGlErrors();
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDrawBuffer(colorAttachment(attachmentIndex));
    glClearColor(red, green, blue, 1.0F);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

auto readPixel(std::uint32_t framebuffer, std::uint32_t attachmentIndex, std::int32_t x,
               std::int32_t y) -> Pixel {
    clearGlErrors();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    glReadBuffer(colorAttachment(attachmentIndex));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    Pixel rgba{};
    glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba.data());
    return rgba;
}

auto readResolvedPixel(std::uint32_t multisampleFramebuffer, std::uint32_t resolveFramebuffer,
                       std::uint32_t width, std::uint32_t height, std::int32_t x, std::int32_t y)
    -> Pixel {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(multisampleFramebuffer));
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(resolveFramebuffer));
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, static_cast<GLint>(width), static_cast<GLint>(height), 0, 0,
                      static_cast<GLint>(width), static_cast<GLint>(height), GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    return readPixel(resolveFramebuffer, 0, x, y);
}

auto makeDrawProbe(GlObjectPool& objects) -> DrawProbe {
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
        return probe;
    }
    const std::array<float, 9> vertices{-1.0F, -1.0F, 0.0F, 3.0F, -1.0F, 0.0F, -1.0F, 3.0F, 0.0F};
    glBindVertexArray(static_cast<GLuint>(probe.vertexArray));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(probe.vertexBuffer));
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(vertices)), vertices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return probe;
}

void createSentinelObjects(GlObjectPool& objects, SentinelState& sentinel) {
    sentinel.program = objects.program("#version 330 core\n"
                                       "void main() { gl_Position = vec4(0.0); }\n",
                                       "#version 330 core\n"
                                       "out vec4 color;\n"
                                       "void main() { color = vec4(0.0); }\n");
    sentinel.vertexArray = objects.vertexArray();
    sentinel.vertexBuffer = objects.buffer();
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(sentinel.vertexBuffer));
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(16), nullptr, GL_STATIC_DRAW);
    sentinel.uniformBuffer = objects.buffer();
    glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(sentinel.uniformBuffer));
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(16), nullptr, GL_STATIC_DRAW);
    sentinel.texture = objects.texture(1, 1);
}

auto probeDraw(void* context) -> core::Result<void> {
    auto& probe = *static_cast<DrawProbe*>(context);
    if (probe.fail) {
        return core::unexpected(cuexis::core::Error{"test.frame_layout.draw_failed",
                                                    "Injected borrowed frame draw failure"});
    }
    if (!probe.drawContent) {
        return {};
    }
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(static_cast<GLuint>(probe.program));
    glBindVertexArray(static_cast<GLuint>(probe.vertexArray));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(probe.vertexBuffer));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    return {};
}

auto makeSentinelState() -> SentinelState {
    SentinelState state;
    state.blendSourceRgb = GL_ONE;
    state.blendDestinationRgb = GL_ZERO;
    state.blendSourceAlpha = GL_SRC_ALPHA;
    state.blendDestinationAlpha = GL_ONE_MINUS_SRC_ALPHA;
    state.blendEquationRgb = GL_FUNC_SUBTRACT;
    state.blendEquationAlpha = GL_FUNC_ADD;
    state.logicOp = GL_XOR;
    state.depthFunc = GL_GREATER;
    state.cullMode = GL_FRONT;
    state.frontFace = GL_CW;
    state.polygonMode = GL_LINE;
    state.activeTexture = GL_TEXTURE3;
    return state;
}

void setSentinelState(const SentinelState& state) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(state.framebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(state.framebuffer));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(state.renderbuffer));
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
    glScissor(state.scissorBox[0], state.scissorBox[1], state.scissorBox[2], state.scissorBox[3]);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(state.clearColor[0], state.clearColor[1], state.clearColor[2],
                 state.clearColor[3]);
    glClearDepth(static_cast<GLdouble>(state.clearDepth));
    glClearStencil(0);
    glColorMask(state.colorMask[0], state.colorMask[1], state.colorMask[2], state.colorMask[3]);
    glDepthMask(state.depthMask ? GL_TRUE : GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(static_cast<GLenum>(state.depthFunc));
    glEnable(GL_BLEND);
    glBlendFuncSeparate(static_cast<GLenum>(state.blendSourceRgb),
                        static_cast<GLenum>(state.blendDestinationRgb),
                        static_cast<GLenum>(state.blendSourceAlpha),
                        static_cast<GLenum>(state.blendDestinationAlpha));
    glBlendEquationSeparate(static_cast<GLenum>(state.blendEquationRgb),
                            static_cast<GLenum>(state.blendEquationAlpha));
    glEnable(GL_CULL_FACE);
    glCullFace(static_cast<GLenum>(state.cullMode));
    glFrontFace(static_cast<GLenum>(state.frontFace));
    glEnable(GL_STENCIL_TEST);
    glEnable(GL_RASTERIZER_DISCARD);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(static_cast<GLenum>(state.logicOp));
    glEnable(GL_FRAMEBUFFER_SRGB);
    glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(state.polygonMode));
    glDepthRange(static_cast<GLdouble>(state.depthRange[0]),
                 static_cast<GLdouble>(state.depthRange[1]));
    glUseProgram(static_cast<GLuint>(state.program));
    glBindVertexArray(static_cast<GLuint>(state.vertexArray));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(state.vertexBuffer));
    glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(state.uniformBuffer));
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, static_cast<GLuint>(state.uniformBuffer));
    glActiveTexture(static_cast<GLenum>(state.activeTexture));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.texture));
}

auto captureSentinelState() -> SentinelState {
    SentinelState state;
    state.scissorTest = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    state.depthTest = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    state.blend = glIsEnabled(GL_BLEND) == GL_TRUE;
    state.cullFace = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    state.stencilTest = glIsEnabled(GL_STENCIL_TEST) == GL_TRUE;
    state.rasterizerDiscard = glIsEnabled(GL_RASTERIZER_DISCARD) == GL_TRUE;
    state.colorLogicOp = glIsEnabled(GL_COLOR_LOGIC_OP) == GL_TRUE;
    state.framebufferSrgb = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;

    glGetIntegerv(GL_VIEWPORT, state.viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX, state.scissorBox.data());
    glGetFloatv(GL_COLOR_CLEAR_VALUE, state.clearColor.data());
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &state.clearDepth);
    std::array<GLboolean, 4> colorMask{};
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask.data());
    for (std::size_t index = 0; index < colorMask.size(); ++index) {
        state.colorMask[index] = colorMask[index] == GL_TRUE;
    }
    GLboolean depthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    state.depthMask = depthMask == GL_TRUE;
    glGetFloatv(GL_DEPTH_RANGE, state.depthRange.data());

    glGetIntegerv(GL_BLEND_SRC_RGB, &state.blendSourceRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &state.blendDestinationRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blendSourceAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blendDestinationAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &state.blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state.blendEquationAlpha);
    glGetIntegerv(GL_LOGIC_OP_MODE, &state.logicOp);
    glGetIntegerv(GL_DEPTH_FUNC, &state.depthFunc);
    glGetIntegerv(GL_CULL_FACE_MODE, &state.cullMode);
    glGetIntegerv(GL_FRONT_FACE, &state.frontFace);
    std::array<GLint, 2> polygonMode{};
    glGetIntegerv(GL_POLYGON_MODE, polygonMode.data());
    state.polygonMode = polygonMode[0];

    GLint value = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &value);
    state.renderbuffer = static_cast<std::uint32_t>(value);
    glGetIntegerv(GL_CURRENT_PROGRAM, &value);
    state.program = static_cast<std::uint32_t>(value);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
    state.vertexArray = static_cast<std::uint32_t>(value);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
    state.vertexBuffer = static_cast<std::uint32_t>(value);
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &value);
    state.uniformBuffer = static_cast<std::uint32_t>(value);
    GLint slot = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &slot);
    state.uniformBufferSlot0 = static_cast<std::uint32_t>(slot);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
    state.activeTexture = static_cast<std::int32_t>(value);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
    state.texture = static_cast<std::uint32_t>(value);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    state.framebuffer = static_cast<std::uint32_t>(value);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
    state.readFramebuffer = static_cast<std::uint32_t>(value);
    return state;
}

} // namespace cuexis::render_opengl::test_support
