#include "open_gl_frame_layout_internal.hpp"

#include "open_gl_presentation_units_internal.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::render_opengl::detail {
namespace {

constexpr std::size_t maxTrackedDrawBuffers = 16;
constexpr std::size_t maxTrackedTextureUnits = 8;

[[nodiscard]] auto mismatchError(std::string message) -> core::Error {
    return core::Error{"render.opengl.presentation.target_mismatch", std::move(message)};
}

[[nodiscard]] auto attachmentError(std::string message) -> core::Error {
    return mismatchError(std::move(message)).withContext("reason", "attachment_layout");
}

[[nodiscard]] auto incompleteError(GLenum status) -> core::Error {
    return mismatchError("Borrowed OpenGL framebuffer is not complete")
        .withContext("reason", "framebuffer_incomplete")
        .withContext("framebuffer_status", std::to_string(static_cast<unsigned>(status)));
}

[[nodiscard]] auto formatText(GLenum format) -> std::string {
    switch (format) {
    case GL_RGBA8:
        return "GL_RGBA8";
    case GL_DEPTH_COMPONENT24:
        return "GL_DEPTH_COMPONENT24";
    case GL_DEPTH_COMPONENT32F:
        return "GL_DEPTH_COMPONENT32F";
    case GL_DEPTH24_STENCIL8:
        return "GL_DEPTH24_STENCIL8";
    case GL_NONE:
        return "GL_NONE";
    default:
        return std::to_string(static_cast<unsigned>(format));
    }
}

struct AttachmentInfo final {
    GLenum type{GL_NONE};
    GLuint name{};
    GLenum format{GL_NONE};
    GLsizei width{};
    GLsizei height{};
    GLsizei samples{};
};

[[nodiscard]] auto queryAttachment(GLenum attachment) -> AttachmentInfo {
    AttachmentInfo info;
    GLint objectType = GL_NONE;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objectType);
    info.type = static_cast<GLenum>(objectType);
    if (info.type != GL_RENDERBUFFER) {
        return info;
    }
    GLint name = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
    info.name = static_cast<GLuint>(name);
    GLint previousRenderbuffer = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, info.name);
    GLint format = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
    info.format = static_cast<GLenum>(format);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &info.width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &info.height);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &info.samples);
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(previousRenderbuffer));
    return info;
}

[[nodiscard]] auto describeAttachment(std::string_view attachment, const AttachmentInfo& info)
    -> core::Error {
    auto error = attachmentError("Borrowed OpenGL framebuffer attachment layout is unsupported");
    error.withContext("attachment", std::string{attachment})
        .withContext("actual_type", std::to_string(static_cast<unsigned>(info.type)))
        .withContext("actual_format", formatText(info.format))
        .withContext("actual_width", std::to_string(info.width))
        .withContext("actual_height", std::to_string(info.height))
        .withContext("actual_samples", std::to_string(info.samples));
    return error;
}

[[nodiscard]] auto validateColorAttachment(const AttachmentInfo& color,
                                           playback::FrameViewport extent) -> core::Result<void> {
    if (color.type != GL_RENDERBUFFER || color.format != GL_RGBA8 || color.samples != 0 ||
        color.width != static_cast<GLsizei>(extent.width) ||
        color.height != static_cast<GLsizei>(extent.height)) {
        return core::unexpected(describeAttachment("color0", color));
    }
    return {};
}

[[nodiscard]] auto validateDepthAttachment(const AttachmentInfo& depth,
                                           playback::FrameViewport extent) -> core::Result<void> {
    const bool formatSupported =
        depth.format == GL_DEPTH_COMPONENT24 || depth.format == GL_DEPTH_COMPONENT32F;
    if (depth.type != GL_RENDERBUFFER || !formatSupported || depth.samples != 0 ||
        depth.width != static_cast<GLsizei>(extent.width) ||
        depth.height != static_cast<GLsizei>(extent.height)) {
        return core::unexpected(describeAttachment("depth", depth));
    }
    return {};
}

class GlStateGuard final {
  public:
    GlStateGuard() = default;
    GlStateGuard(const GlStateGuard&) = delete;
    auto operator=(const GlStateGuard&) -> GlStateGuard& = delete;
    ~GlStateGuard() {
        restore();
    }

    [[nodiscard]] auto initialize(GLuint framebuffer) -> core::Result<void> {
        GLint maxDrawBuffers = 0;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        if (maxDrawBuffers <= 0 ||
            static_cast<std::size_t>(maxDrawBuffers) > maxTrackedDrawBuffers) {
            return core::unexpected(
                mismatchError("OpenGL draw-buffer count exceeds the supported tracked range")
                    .withContext("max_draw_buffers", std::to_string(maxDrawBuffers)));
        }
        drawBufferCount_ = static_cast<std::size_t>(maxDrawBuffers);
        framebuffer_ = framebuffer;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer_);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer_);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer_);
        glGetIntegerv(GL_VIEWPORT, viewport_.data());
        glGetIntegerv(GL_SCISSOR_BOX, scissorBox_.data());
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask_.data());
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask_);
        glGetBooleanv(GL_SCISSOR_TEST, &scissorTest_);
        glGetBooleanv(GL_DEPTH_TEST, &depthTest_);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc_);
        glGetBooleanv(GL_BLEND, &blend_);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb_);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb_);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha_);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha_);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb_);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha_);
        glGetBooleanv(GL_CULL_FACE, &cullFace_);
        glGetIntegerv(GL_CULL_FACE_MODE, &cullMode_);
        glGetIntegerv(GL_FRONT_FACE, &frontFace_);
        glGetBooleanv(GL_STENCIL_TEST, &stencilTest_);
        glGetBooleanv(GL_RASTERIZER_DISCARD, &rasterizerDiscard_);
        glGetBooleanv(GL_COLOR_LOGIC_OP, &colorLogicOp_);
        glGetBooleanv(GL_FRAMEBUFFER_SRGB, &framebufferSrgb_);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor_.data());
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth_);
        glGetIntegerv(GL_POLYGON_MODE, polygonMode_.data());
        glGetFloatv(GL_DEPTH_RANGE, depthRange_.data());
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray_);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniformBuffer_);
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &uniformBufferSlot0_);
        glGetInteger64i_v(GL_UNIFORM_BUFFER_START, 0, &uniformBufferSlot0Start_);
        glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, 0, &uniformBufferSlot0Size_);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
        GLint maxTextureUnits = 0;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
        textureUnitCount_ = std::min(maxTrackedTextureUnits,
                                     static_cast<std::size_t>(std::max(maxTextureUnits, 0)));
        for (std::size_t index = 0; index < textureUnitCount_; ++index) {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBindings_[index]);
        }
        glActiveTexture(static_cast<GLenum>(activeTexture_));

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        initialized_ = true;
        for (std::size_t index = 0; index < drawBufferCount_; ++index) {
            GLint route = 0;
            glGetIntegerv(GL_DRAW_BUFFER0 + static_cast<GLenum>(index), &route);
            drawBuffers_[index] = static_cast<GLenum>(route);
        }
        return {};
    }

    auto markDrawBufferChanged() noexcept -> void {
        drawBufferChanged_ = true;
    }

    void restore() noexcept {
        if (!initialized_) {
            return;
        }
        if (drawBufferChanged_) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
            glDrawBuffers(static_cast<GLsizei>(drawBufferCount_), drawBuffers_.data());
        }
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer_));
        for (std::size_t index = 0; index < textureUnitCount_; ++index) {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textureBindings_[index]));
        }
        glActiveTexture(static_cast<GLenum>(activeTexture_));
        glUseProgram(static_cast<GLuint>(program_));
        glBindVertexArray(static_cast<GLuint>(vertexArray_));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer_));
        glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(uniformBuffer_));
        if (uniformBufferSlot0Size_ > 0) {
            glBindBufferRange(GL_UNIFORM_BUFFER, 0, static_cast<GLuint>(uniformBufferSlot0_),
                              static_cast<GLintptr>(uniformBufferSlot0Start_),
                              static_cast<GLsizeiptr>(uniformBufferSlot0Size_));
        } else {
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, static_cast<GLuint>(uniformBufferSlot0_));
        }
        glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
        glScissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
        glClearColor(clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]);
        glClearDepth(static_cast<GLdouble>(clearDepth_));
        glColorMask(colorMask_[0], colorMask_[1], colorMask_[2], colorMask_[3]);
        glDepthMask(depthMask_);
        glDepthFunc(static_cast<GLenum>(depthFunc_));
        glBlendFuncSeparate(
            static_cast<GLenum>(blendSourceRgb_), static_cast<GLenum>(blendDestinationRgb_),
            static_cast<GLenum>(blendSourceAlpha_), static_cast<GLenum>(blendDestinationAlpha_));
        glBlendEquationSeparate(static_cast<GLenum>(blendEquationRgb_),
                                static_cast<GLenum>(blendEquationAlpha_));
        glCullFace(static_cast<GLenum>(cullMode_));
        glFrontFace(static_cast<GLenum>(frontFace_));
        glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(polygonMode_[0]));
        glDepthRange(static_cast<GLdouble>(depthRange_[0]), static_cast<GLdouble>(depthRange_[1]));
        setEnabled(GL_SCISSOR_TEST, scissorTest_ != GL_FALSE);
        setEnabled(GL_DEPTH_TEST, depthTest_ != GL_FALSE);
        setEnabled(GL_BLEND, blend_ != GL_FALSE);
        setEnabled(GL_CULL_FACE, cullFace_ != GL_FALSE);
        setEnabled(GL_STENCIL_TEST, stencilTest_ != GL_FALSE);
        setEnabled(GL_RASTERIZER_DISCARD, rasterizerDiscard_ != GL_FALSE);
        setEnabled(GL_COLOR_LOGIC_OP, colorLogicOp_ != GL_FALSE);
        setEnabled(GL_FRAMEBUFFER_SRGB, framebufferSrgb_ != GL_FALSE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer_));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer_));
        initialized_ = false;
        drawBufferChanged_ = false;
    }

  private:
    static void setEnabled(GLenum capability, bool enabled) noexcept {
        if (enabled) {
            glEnable(capability);
        } else {
            glDisable(capability);
        }
    }

    bool initialized_{};
    bool drawBufferChanged_{};
    GLuint framebuffer_{};
    GLint readFramebuffer_{};
    GLint drawFramebuffer_{};
    GLint renderbuffer_{};
    std::size_t drawBufferCount_{};
    std::array<GLenum, maxTrackedDrawBuffers> drawBuffers_{};
    std::array<GLint, 4> viewport_{};
    std::array<GLint, 4> scissorBox_{};
    std::array<GLboolean, 4> colorMask_{};
    GLboolean depthMask_{};
    GLboolean scissorTest_{};
    GLboolean depthTest_{};
    GLint depthFunc_{};
    GLboolean blend_{};
    GLint blendSourceRgb_{};
    GLint blendDestinationRgb_{};
    GLint blendSourceAlpha_{};
    GLint blendDestinationAlpha_{};
    GLint blendEquationRgb_{};
    GLint blendEquationAlpha_{};
    GLboolean cullFace_{};
    GLint cullMode_{};
    GLint frontFace_{};
    GLboolean stencilTest_{};
    GLboolean rasterizerDiscard_{};
    GLboolean colorLogicOp_{};
    GLboolean framebufferSrgb_{};
    std::array<GLfloat, 4> clearColor_{};
    GLfloat clearDepth_{};
    std::array<GLint, 2> polygonMode_{};
    std::array<GLfloat, 2> depthRange_{};
    GLint program_{};
    GLint vertexArray_{};
    GLint arrayBuffer_{};
    GLint uniformBuffer_{};
    GLint uniformBufferSlot0_{};
    GLint64 uniformBufferSlot0Start_{};
    GLint64 uniformBufferSlot0Size_{};
    GLint activeTexture_{};
    std::size_t textureUnitCount_{};
    std::array<GLint, maxTrackedTextureUnits> textureBindings_{};
};

[[nodiscard]] auto validateRectFits(const GlRect& rect) -> core::Result<void> {
    constexpr auto maxGl = static_cast<std::int64_t>(std::numeric_limits<GLint>::max());
    if (rect.x < 0 || rect.y < 0 || rect.width < 0 || rect.height < 0 ||
        static_cast<std::int64_t>(rect.x) > maxGl || static_cast<std::int64_t>(rect.y) > maxGl ||
        static_cast<std::int64_t>(rect.width) > maxGl ||
        static_cast<std::int64_t>(rect.height) > maxGl) {
        return core::unexpected(mismatchError("Frame layout rectangle exceeds the OpenGL range")
                                    .withContext("x", std::to_string(rect.x))
                                    .withContext("y", std::to_string(rect.y))
                                    .withContext("width", std::to_string(rect.width))
                                    .withContext("height", std::to_string(rect.height)));
    }
    return {};
}

} // namespace

auto toGlRect(const playback::experimental::PixelRect& rect,
              playback::FrameViewport extent) noexcept -> GlRect {
    const auto bottom = static_cast<std::int64_t>(extent.height) -
                        static_cast<std::int64_t>(rect.y) - static_cast<std::int64_t>(rect.height);
    return GlRect{static_cast<std::int32_t>(rect.x), static_cast<std::int32_t>(bottom),
                  static_cast<std::int32_t>(rect.width), static_cast<std::int32_t>(rect.height)};
}

auto validateBorrowedFramebuffer(const BorrowedGlFramebuffer& target) -> core::Result<void> {
    if (target.name == 0 || target.extent.width == 0 || target.extent.height == 0) {
        return core::unexpected(
            attachmentError("Borrowed OpenGL framebuffer name or extent is invalid"));
    }
    if (glIsFramebuffer(target.name) == GL_FALSE) {
        return core::unexpected(attachmentError("Borrowed OpenGL framebuffer name does not exist")
                                    .withContext("framebuffer", std::to_string(target.name)));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, target.name);

    const auto color = queryAttachment(GL_COLOR_ATTACHMENT0);
    if (auto valid = validateColorAttachment(color, target.extent); !valid) {
        return valid;
    }

    GLint maxColorAttachments = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    for (GLint index = 1; index < maxColorAttachments; ++index) {
        const auto extra = queryAttachment(GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(index));
        if (extra.type != GL_NONE) {
            auto error =
                describeAttachment("color" + std::to_string(static_cast<unsigned>(index)), extra);
            error.withContext("reason", "attachment_layout");
            return core::unexpected(std::move(error));
        }
    }

    const auto depth = queryAttachment(GL_DEPTH_ATTACHMENT);
    if (auto valid = validateDepthAttachment(depth, target.extent); !valid) {
        return valid;
    }
    const auto stencil = queryAttachment(GL_STENCIL_ATTACHMENT);
    if (stencil.type != GL_NONE) {
        return core::unexpected(describeAttachment("stencil", stencil));
    }

    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        return core::unexpected(incompleteError(status));
    }
    return {};
}

auto drawBorrowedFrame(const BorrowedGlFramebuffer& target,
                       const playback::experimental::FrameLayout& layout,
                       const FrameClearValues& clear, BorrowedFrameDraw draw, void* context)
    -> core::Result<void> {
    if (layout.content.width == 0 || layout.content.height == 0) {
        return {};
    }
    constexpr auto maxGlExtent = static_cast<std::uint64_t>(std::numeric_limits<GLsizei>::max());
    if (layout.target.extent.width != target.extent.width ||
        layout.target.extent.height != target.extent.height) {
        return core::unexpected(
            mismatchError("Borrowed framebuffer extent differs from the laid-out target")
                .withContext("expected_width", std::to_string(target.extent.width))
                .withContext("expected_height", std::to_string(target.extent.height))
                .withContext("actual_width", std::to_string(layout.target.extent.width))
                .withContext("actual_height", std::to_string(layout.target.extent.height)));
    }
    if (static_cast<std::uint64_t>(target.extent.width) > maxGlExtent ||
        static_cast<std::uint64_t>(target.extent.height) > maxGlExtent) {
        return core::unexpected(
            mismatchError("Borrowed framebuffer extent exceeds the OpenGL range")
                .withContext("width", std::to_string(target.extent.width))
                .withContext("height", std::to_string(target.extent.height)));
    }

    const auto host = toGlRect(layout.target.host, target.extent);
    const auto absoluteContent = playback::experimental::PixelRect{
        layout.target.host.x + layout.content.x, layout.target.host.y + layout.content.y,
        layout.content.width, layout.content.height};
    const auto content = toGlRect(absoluteContent, target.extent);
    if (auto valid = validateRectFits(host); !valid) {
        return valid;
    }
    if (auto valid = validateRectFits(content); !valid) {
        return valid;
    }

    GlStateGuard state;
    if (auto saved = state.initialize(target.name); !saved) {
        return saved;
    }
    if (auto valid = validateBorrowedFramebuffer(target); !valid) {
        return valid;
    }

    clearGlErrors();
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    state.markDrawBufferChanged();
    glViewport(0, 0, static_cast<GLsizei>(target.extent.width),
               static_cast<GLsizei>(target.extent.height));
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthRange(0.0, 1.0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glScissor(host.x, host.y, host.width, host.height);
    glClearColor(clear.color[0], clear.color[1], clear.color[2], clear.color[3]);
    glClearDepth(static_cast<GLdouble>(clear.depth));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(content.x, content.y, content.width, content.height);
    glScissor(content.x, content.y, content.width, content.height);
    if (auto checked = checkGl("render.opengl.presentation.draw_failed",
                               "OpenGL rejected the borrowed frame host pass");
        !checked) {
        return checked;
    }
    if (auto drawn = draw(context); !drawn) {
        return drawn;
    }
    if (auto checked = checkGl("render.opengl.presentation.draw_failed",
                               "OpenGL rejected the borrowed frame content draw");
        !checked) {
        return checked;
    }
    return {};
}

} // namespace cuexis::render_opengl::detail
