#pragma once

// Test-only OpenGL boundary for the laid-out frame bridge and the borrowed framebuffer checks.
// The header exposes plain C++ values only: the tests keep their own assertions without including
// a GL header or naming a GL function, and every handle lives in the matching implementation unit.
// This is not part of the installed SDK surface.

#include <cuexis/core/result.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cuexis::render_opengl::test_support {

// One RGBA byte sample.
using Pixel = std::array<std::uint8_t, 4>;

// A draw-buffer route that names no color attachment.
inline constexpr std::uint32_t noAttachment = 0;

// A framebuffer together with the renderbuffers it references. The pool owns the handles; this
// only names them so a test can describe one attachment layout.
struct Framebuffer final {
    std::uint32_t name{};
    std::uint32_t color{};
    std::uint32_t depth{};
};

// A draw callback probe: the handles of the triangle program and the two injected outcomes.
struct DrawProbe final {
    std::uint32_t program{};
    std::uint32_t vertexArray{};
    std::uint32_t vertexBuffer{};
    bool fail{};
    bool drawContent{true};
};

// The GL state a test sets as a sentinel before a borrowed draw. Every member is a plain value the
// test compares field by field after the draw returns.
struct SentinelState final {
    std::uint32_t framebuffer{};
    std::uint32_t renderbuffer{};
    std::uint32_t program{};
    std::uint32_t vertexArray{};
    std::uint32_t vertexBuffer{};
    std::uint32_t uniformBuffer{};
    std::uint32_t uniformBufferSlot0{};
    std::uint32_t texture{};
    std::uint32_t readFramebuffer{};
    std::array<std::int32_t, 4> viewport{7, 9, 13, 11};
    std::array<std::int32_t, 4> scissorBox{3, 4, 5, 6};
    std::array<float, 4> clearColor{0.25F, 0.5F, 0.75F, 0.5F};
    std::array<bool, 4> colorMask{true, false, true, true};
    std::array<float, 2> depthRange{0.1F, 0.9F};
    float clearDepth{0.25F};
    bool scissorTest{false};
    bool depthTest{true};
    bool blend{true};
    bool cullFace{true};
    bool stencilTest{true};
    bool rasterizerDiscard{true};
    bool colorLogicOp{true};
    bool framebufferSrgb{true};
    bool depthMask{false};
    std::int32_t blendSourceRgb{};
    std::int32_t blendDestinationRgb{};
    std::int32_t blendSourceAlpha{};
    std::int32_t blendDestinationAlpha{};
    std::int32_t blendEquationRgb{};
    std::int32_t blendEquationAlpha{};
    std::int32_t logicOp{};
    std::int32_t depthFunc{};
    std::int32_t cullMode{};
    std::int32_t frontFace{};
    std::int32_t polygonMode{};
    std::int32_t activeTexture{};
};

// Owns every GL handle a test creates and releases them in one place.
class GlObjectPool final {
  public:
    GlObjectPool() = default;
    ~GlObjectPool();
    GlObjectPool(const GlObjectPool&) = delete;
    auto operator=(const GlObjectPool&) -> GlObjectPool& = delete;
    GlObjectPool(GlObjectPool&&) = delete;
    auto operator=(GlObjectPool&&) -> GlObjectPool& = delete;

    [[nodiscard]] auto framebuffer() -> std::uint32_t;
    [[nodiscard]] auto colorRenderbuffer(std::uint32_t width, std::uint32_t height,
                                         std::uint32_t samples = 0) -> std::uint32_t;
    [[nodiscard]] auto depthRenderbuffer(std::uint32_t width, std::uint32_t height,
                                         std::uint32_t samples = 0) -> std::uint32_t;
    [[nodiscard]] auto depthStencilRenderbuffer(std::uint32_t width, std::uint32_t height)
        -> std::uint32_t;
    [[nodiscard]] auto texture(std::uint32_t width, std::uint32_t height) -> std::uint32_t;
    [[nodiscard]] auto buffer() -> std::uint32_t;
    [[nodiscard]] auto vertexArray() -> std::uint32_t;
    [[nodiscard]] auto program(const char* vertexSource, const char* fragmentSource)
        -> std::uint32_t;

    // A color/depth framebuffer with an RGBA8 renderbuffer at COLOR_ATTACHMENT0 and a 24-bit depth
    // renderbuffer at DEPTH_ATTACHMENT. The framebuffer is bound on return; completeness is left to
    // the caller so a test can report it.
    [[nodiscard]] auto completeColorDepthFramebuffer(std::uint32_t width, std::uint32_t height)
        -> Framebuffer;

  private:
    [[nodiscard]] auto renderbuffer(std::uint32_t internalFormat, std::uint32_t width,
                                    std::uint32_t height, std::uint32_t samples) -> std::uint32_t;

    std::vector<std::uint32_t> framebuffers_;
    std::vector<std::uint32_t> renderbuffers_;
    std::vector<std::uint32_t> textures_;
    std::vector<std::uint32_t> buffers_;
    std::vector<std::uint32_t> vertexArrays_;
    std::vector<std::uint32_t> programs_;
};

// The context pair the tests print when a real driver was found.
[[nodiscard]] auto contextSummary() -> std::string;

[[nodiscard]] auto maxSamples() -> std::int32_t;
[[nodiscard]] auto maxDrawBuffers() -> std::int32_t;

void bindFramebuffer(std::uint32_t name);
void attachColorRenderbuffer(std::uint32_t framebuffer, std::uint32_t renderbuffer);
void attachColorRenderbufferAt(std::uint32_t framebuffer, std::uint32_t attachmentIndex,
                               std::uint32_t renderbuffer);
void attachDepthRenderbuffer(std::uint32_t framebuffer, std::uint32_t renderbuffer);
void attachDepthStencilRenderbuffer(std::uint32_t framebuffer, std::uint32_t renderbuffer);
void attachColorTexture(std::uint32_t framebuffer, std::uint32_t texture);

void setAllDrawBuffersNone();
void setDrawBufferNone();
void setDrawBuffersSingle(std::uint32_t attachmentIndex);

[[nodiscard]] auto framebufferComplete() -> bool;

// The current draw-buffer routes of the bound draw framebuffer, one entry per draw buffer.
[[nodiscard]] auto drawRoutes() -> std::vector<std::uint32_t>;

// Clears the named attachment to an opaque color and the depth attachment to one.
void clearFramebuffer(std::uint32_t framebuffer, std::uint32_t attachmentIndex, float red,
                      float green, float blue);

[[nodiscard]] auto readPixel(std::uint32_t framebuffer, std::uint32_t attachmentIndex,
                             std::int32_t x, std::int32_t y) -> Pixel;

[[nodiscard]] auto readResolvedPixel(std::uint32_t multisampleFramebuffer,
                                     std::uint32_t resolveFramebuffer, std::uint32_t width,
                                     std::uint32_t height, std::int32_t x, std::int32_t y) -> Pixel;

// Creates the triangle program and its vertex array the borrowed draw callback executes.
[[nodiscard]] auto makeDrawProbe(GlObjectPool& objects) -> DrawProbe;

// Creates the program, the vertex array, the vertex/uniform buffers and the texture the sentinel
// state binds. The handles are written into the sentinel.
void createSentinelObjects(GlObjectPool& objects, SentinelState& sentinel);

// The callback passed to drawBorrowedFrame. It injects a failure, or draws the probe triangle.
[[nodiscard]] auto probeDraw(void* context) -> core::Result<void>;

[[nodiscard]] auto makeSentinelState() -> SentinelState;
void setSentinelState(const SentinelState& state);
[[nodiscard]] auto captureSentinelState() -> SentinelState;

} // namespace cuexis::render_opengl::test_support
