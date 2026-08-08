//  OpenGLBackend 实现 — OpenGL 3.3 Core Profile 渲染后端
//  configureOpenGlContext: 在 SDL 主线程设置进程级 GL 属性并返回一次性配置令牌
//  create: 创建 SDL GL Context，加载 glad，验证版本和 Core Profile
//  createDebugPipeline: 编译内联 GLSL 330 Vertex/Fragment Shader，创建 VAO+VBO
//  renderFrame: 清屏 → 上传 DebugVertex → glDrawArrays(GL_LINES) → SwapWindow
//  release: 在 SDL 主线程释放 GPU 资源，非主线程泄漏日志警告

#include <cuexis/render_opengl/open_gl_backend.hpp>

#include "open_gl_presentation_internal.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::render_opengl {
namespace {

struct ConfigurationState final {
    std::uint64_t latestGeneration{};
    std::uint64_t activeGeneration{};
};

ConfigurationState configurationState;

[[nodiscard]] auto sdlError() -> std::string {
    const char* message = SDL_GetError();
    return message != nullptr && message[0] != '\0' ? message : "unknown SDL error";
}

[[nodiscard]] auto requireMainThread(const char* operation) -> core::Result<void> {
    if (SDL_IsMainThread()) {
        return {};
    }

    return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                        "Stage 0 OpenGL operations must run on the SDL main thread"}
                                .withContext("operation", operation));
}

[[nodiscard]] auto beginConfiguration() -> core::Result<std::uint64_t> {
    configurationState.activeGeneration = 0;
    if (configurationState.latestGeneration == std::numeric_limits<std::uint64_t>::max()) {
        return core::unexpected(
            core::Error{"render.opengl.configuration_generation_exhausted",
                        "OpenGL configuration generation counter is exhausted"});
    }

    return ++configurationState.latestGeneration;
}

[[nodiscard]] auto setAttribute(SDL_GLAttr attribute, int value, const char* name)
    -> core::Result<void> {
    if (SDL_GL_SetAttribute(attribute, value)) {
        return {};
    }

    return core::unexpected(
        core::Error{"render.opengl.attribute_failed", sdlError()}.withContext("attribute", name));
}

[[nodiscard]] auto loadOpenGlProcedure(const char* name) -> void* {
    return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
}

[[nodiscard]] auto glString(GLenum name) -> std::string {
    const auto* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "unknown";
}

[[nodiscard]] auto supportsRequestedVersion(int actualMajor, int actualMinor, int requestedMajor,
                                            int requestedMinor) noexcept -> bool {
    return actualMajor > requestedMajor ||
           (actualMajor == requestedMajor && actualMinor >= requestedMinor);
}

void logWarning(const std::shared_ptr<const core::LogSink>& sink,
                std::string_view message) noexcept {
    if (sink) {
        sink->write(core::LogSeverity::Warning, "render.opengl", message);
    }
}

class ContextGuard final {
  public:
    ContextGuard(SDL_GLContext context, std::shared_ptr<const core::LogSink> logSink) noexcept
        : context_(context), logSink_(std::move(logSink)) {}

    ContextGuard(const ContextGuard&) = delete;
    auto operator=(const ContextGuard&) -> ContextGuard& = delete;

    ~ContextGuard() {
        if (context_ != nullptr && !SDL_GL_DestroyContext(context_)) {
            logWarning(logSink_, std::string{"Could not destroy a rolled-back OpenGL context: "} +
                                     sdlError());
        }
    }

    [[nodiscard]] auto release() noexcept -> SDL_GLContext {
        return std::exchange(context_, nullptr);
    }

  private:
    SDL_GLContext context_{};
    std::shared_ptr<const core::LogSink> logSink_;
};

struct DebugVertex final {
    float x;
    float y;
    float z;
    float red;
    float green;
    float blue;
    float alpha;
};

struct DebugPipeline final {
    GLuint program{};
    GLuint vertexArray{};
    GLuint vertexBuffer{};
    GLint viewProjectionLocation{-1};
};

[[nodiscard]] auto shaderLog(GLuint shader) -> std::string {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    length = std::clamp(length, 0, 4096);
    if (length == 0) {
        return "no shader compiler log";
    }

    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    glGetShaderInfoLog(shader, length, &written, log.data());
    log.resize(static_cast<std::size_t>(std::max<GLsizei>(written, 0)));
    return log;
}

[[nodiscard]] auto programLog(GLuint program) -> std::string {
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    length = std::clamp(length, 0, 4096);
    if (length == 0) {
        return "no program linker log";
    }

    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    glGetProgramInfoLog(program, length, &written, log.data());
    log.resize(static_cast<std::size_t>(std::max<GLsizei>(written, 0)));
    return log;
}

[[nodiscard]] auto compileShader(GLenum type, const char* source, const char* stage)
    -> core::Result<GLuint> {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return core::unexpected(core::Error{"render.opengl.shader_create_failed",
                                            "OpenGL could not create a debug shader"}
                                    .withContext("stage", stage));
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        auto error =
            core::Error{"render.opengl.shader_compile_failed", shaderLog(shader)}.withContext(
                "stage", stage);
        glDeleteShader(shader);
        return core::unexpected(std::move(error));
    }
    return shader;
}

void destroyDebugPipeline(DebugPipeline& pipeline) noexcept {
    if (pipeline.vertexBuffer != 0) {
        glDeleteBuffers(1, &pipeline.vertexBuffer);
    }
    if (pipeline.vertexArray != 0) {
        glDeleteVertexArrays(1, &pipeline.vertexArray);
    }
    if (pipeline.program != 0) {
        glDeleteProgram(pipeline.program);
    }
    pipeline = {};
}

[[nodiscard]] auto createDebugPipeline() -> core::Result<DebugPipeline> {
    static constexpr const char* vertexSource = R"GLSL(#version 330 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
uniform mat4 viewProjection;
out vec4 vertexColor;
void main() {
    gl_Position = viewProjection * vec4(inPosition, 1.0);
    vertexColor = inColor;
}
)GLSL";
    static constexpr const char* fragmentSource = R"GLSL(#version 330 core
in vec4 vertexColor;
out vec4 outColor;
void main() {
    outColor = vertexColor;
}
)GLSL";

    auto vertexResult = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex");
    if (!vertexResult) {
        return core::unexpected(std::move(vertexResult.error()));
    }
    const GLuint vertexShader = *vertexResult;

    auto fragmentResult = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment");
    if (!fragmentResult) {
        glDeleteShader(vertexShader);
        return core::unexpected(std::move(fragmentResult.error()));
    }
    const GLuint fragmentShader = *fragmentResult;

    DebugPipeline pipeline;
    pipeline.program = glCreateProgram();
    if (pipeline.program == 0) {
        glDeleteShader(fragmentShader);
        glDeleteShader(vertexShader);
        return core::unexpected(core::Error{"render.opengl.program_create_failed",
                                            "OpenGL could not create the debug program"});
    }

    glAttachShader(pipeline.program, vertexShader);
    glAttachShader(pipeline.program, fragmentShader);
    glLinkProgram(pipeline.program);
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(pipeline.program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        auto error = core::Error{"render.opengl.program_link_failed", programLog(pipeline.program)};
        destroyDebugPipeline(pipeline);
        return core::unexpected(std::move(error));
    }

    pipeline.viewProjectionLocation = glGetUniformLocation(pipeline.program, "viewProjection");
    if (pipeline.viewProjectionLocation < 0) {
        destroyDebugPipeline(pipeline);
        return core::unexpected(
            core::Error{"render.opengl.uniform_missing",
                        "The debug program does not expose its viewProjection uniform"});
    }

    glGenVertexArrays(1, &pipeline.vertexArray);
    glGenBuffers(1, &pipeline.vertexBuffer);
    if (pipeline.vertexArray == 0 || pipeline.vertexBuffer == 0) {
        destroyDebugPipeline(pipeline);
        return core::unexpected(core::Error{"render.opengl.buffer_create_failed",
                                            "OpenGL could not create debug draw buffers"});
    }

    glBindVertexArray(pipeline.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, pipeline.vertexBuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                          reinterpret_cast<const void*>(offsetof(DebugVertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                          reinterpret_cast<const void*>(offsetof(DebugVertex, red)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    if (glGetError() != GL_NO_ERROR) {
        destroyDebugPipeline(pipeline);
        return core::unexpected(core::Error{"render.opengl.pipeline_setup_failed",
                                            "OpenGL rejected the debug pipeline setup"});
    }
    return pipeline;
}

[[nodiscard]] auto makeDebugVertex(const core::Vec3& position, const render::Color& color) noexcept
    -> DebugVertex {
    return DebugVertex{position.x,  position.y, position.z, color.red,
                       color.green, color.blue, color.alpha};
}

} // namespace

auto configureOpenGlContext(platform_sdl::SdlRuntime& runtime, const OpenGlConfig& config)
    -> core::Result<OpenGlContextConfiguration> {
    if (auto result = requireMainThread("configure_context"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (runtime.videoDriver().empty()) {
        return core::unexpected(core::Error{"render.opengl.runtime_unavailable",
                                            "SDL video must be initialized before OpenGL setup"});
    }

    auto generationResult = beginConfiguration();
    if (!generationResult) {
        return core::unexpected(std::move(generationResult.error()));
    }
    const std::uint64_t generation = *generationResult;

    if (config.minorVersion < 0 || config.majorVersion < 3 ||
        (config.majorVersion == 3 && config.minorVersion < 2)) {
        return core::unexpected(
            core::Error{"render.opengl.invalid_config",
                        "OpenGL Core Profile requires version 3.2 or newer"}
                .withContext("major_version", std::to_string(config.majorVersion))
                .withContext("minor_version", std::to_string(config.minorVersion)));
    }

    SDL_GL_ResetAttributes();

    if (auto result = setAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config.majorVersion,
                                   "context_major_version");
        !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (auto result = setAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config.minorVersion,
                                   "context_minor_version");
        !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (auto result = setAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE,
                                   "context_profile_core");
        !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (auto result = setAttribute(SDL_GL_DOUBLEBUFFER, 1, "double_buffer"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (auto result =
            setAttribute(SDL_GL_CONTEXT_FLAGS, config.debugContext ? SDL_GL_CONTEXT_DEBUG_FLAG : 0,
                         "context_flags");
        !result) {
        return core::unexpected(std::move(result.error()));
    }

    configurationState.activeGeneration = generation;
    return OpenGlContextConfiguration{config, generation};
}

auto OpenGlBackend::create(platform_sdl::SdlWindow& window,
                           OpenGlContextConfiguration&& configuration)
    -> core::Result<OpenGlBackend> {
    if (auto result = requireMainThread("create_backend"); !result) {
        return core::unexpected(std::move(result.error()));
    }

    auto configured = std::exchange(configuration.config_, std::nullopt);
    const std::uint64_t generation = std::exchange(configuration.generation_, 0);
    if (!configured || generation == 0) {
        return core::unexpected(
            core::Error{"render.opengl.configuration_unavailable",
                        "OpenGL context configuration has already been consumed"});
    }
    if (generation != configurationState.activeGeneration) {
        return core::unexpected(
            core::Error{"render.opengl.configuration_stale",
                        "A newer or failed OpenGL configuration invalidated this token"}
                .withContext("configuration_generation", std::to_string(generation))
                .withContext("active_generation",
                             std::to_string(configurationState.activeGeneration)));
    }
    configurationState.activeGeneration = 0;
    const OpenGlConfig& config = *configured;

    auto windowLease = window.lease();
    auto* nativeWindow = static_cast<SDL_Window*>(windowLease.nativeHandle());
    if (nativeWindow == nullptr) {
        return core::unexpected(core::Error{"render.opengl.invalid_window",
                                            "Cannot create an OpenGL context for a null window"});
    }

    SDL_GLContext context = SDL_GL_CreateContext(nativeWindow);
    std::string debugContextError;
    if (context == nullptr && config.debugContext) {
        debugContextError = sdlError();
        logWarning(config.logSink,
                   std::string{"Debug context creation failed; retrying without it: "} +
                       debugContextError);
        if (auto result = setAttribute(SDL_GL_CONTEXT_FLAGS, 0, "context_flags_fallback");
            !result) {
            return core::unexpected(
                std::move(result.error()).withContext("debug_context_error", debugContextError));
        }
        context = SDL_GL_CreateContext(nativeWindow);
    }
    if (context == nullptr) {
        auto error = core::Error{"render.opengl.context_create_failed", sdlError()};
        if (!debugContextError.empty()) {
            error.withContext("debug_context_error", std::move(debugContextError));
        }
        return core::unexpected(std::move(error));
    }
    ContextGuard contextGuard{context, config.logSink};

    if (!SDL_GL_MakeCurrent(nativeWindow, context)) {
        return core::unexpected(core::Error{"render.opengl.context_current_failed", sdlError()});
    }

    if (gladLoadGLLoader(&loadOpenGlProcedure) == 0) {
        return core::unexpected(
            core::Error{"render.opengl.loader_failed", "glad could not load OpenGL procedures"});
    }

    GLint actualMajor = 0;
    GLint actualMinor = 0;
    GLint profileMask = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &actualMajor);
    glGetIntegerv(GL_MINOR_VERSION, &actualMinor);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);

    if (!supportsRequestedVersion(actualMajor, actualMinor, config.majorVersion,
                                  config.minorVersion)) {
        return core::unexpected(
            core::Error{"render.opengl.version_unsupported",
                        "The active OpenGL context is older than the requested version"}
                .withContext("requested_version", std::to_string(config.majorVersion) + "." +
                                                      std::to_string(config.minorVersion))
                .withContext("actual_version",
                             std::to_string(actualMajor) + "." + std::to_string(actualMinor)));
    }
    if ((profileMask & GL_CONTEXT_CORE_PROFILE_BIT) == 0) {
        return core::unexpected(
            core::Error{"render.opengl.profile_unsupported",
                        "The active OpenGL context is not a Core Profile context"}
                .withContext("profile_mask", std::to_string(profileMask)));
    }

    if (!SDL_GL_SetSwapInterval(config.vsync ? 1 : 0)) {
        logWarning(config.logSink,
                   std::string{"Could not configure the requested swap interval: "} + sdlError());
    }

    OpenGlInfo info{
        .version = glString(GL_VERSION),
        .vendor = glString(GL_VENDOR),
        .renderer = glString(GL_RENDERER),
    };

    auto pipelineResult = createDebugPipeline();
    if (!pipelineResult) {
        return core::unexpected(std::move(pipelineResult.error()));
    }
    auto presentationState = detail::createPresentationBackendState();
    if (!presentationState) {
        destroyDebugPipeline(*pipelineResult);
        return core::unexpected(std::move(presentationState.error()));
    }
    const DebugPipeline pipeline = *pipelineResult;

    return OpenGlBackend{std::move(windowLease),
                         contextGuard.release(),
                         std::move(info),
                         pipeline.program,
                         pipeline.vertexArray,
                         pipeline.vertexBuffer,
                         pipeline.viewProjectionLocation,
                         std::move(*presentationState),
                         config.logSink};
}

OpenGlBackend::OpenGlBackend(platform_sdl::SdlWindowLease window, void* context, OpenGlInfo info,
                             std::uint32_t debugProgram, std::uint32_t debugVertexArray,
                             std::uint32_t debugVertexBuffer, int viewProjectionLocation,
                             std::unique_ptr<detail::OpenGlPresentationBackendState> presentation,
                             std::shared_ptr<const core::LogSink> logSink) noexcept
    : window_(std::move(window)), context_(context), info_(std::move(info)),
      logSink_(std::move(logSink)), debugProgram_(debugProgram),
      debugVertexArray_(debugVertexArray), debugVertexBuffer_(debugVertexBuffer),
      viewProjectionLocation_(viewProjectionLocation), presentation_(std::move(presentation)) {}

OpenGlBackend::~OpenGlBackend() {
    if (context_ != nullptr && (!SDL_IsMainThread() || !ownerThread_.isCurrent())) {
        std::terminate();
    }
    release();
}

OpenGlBackend::OpenGlBackend(OpenGlBackend&& other) noexcept
    : window_(std::move(other.window_)), context_(std::exchange(other.context_, nullptr)),
      info_(std::move(other.info_)), logSink_(std::move(other.logSink_)),
      debugProgram_(std::exchange(other.debugProgram_, 0)),
      debugVertexArray_(std::exchange(other.debugVertexArray_, 0)),
      debugVertexBuffer_(std::exchange(other.debugVertexBuffer_, 0)),
      viewProjectionLocation_(std::exchange(other.viewProjectionLocation_, -1)),
      presentation_(std::move(other.presentation_)) {
    if (!SDL_IsMainThread() || !other.ownerThread_.isCurrent()) {
        std::terminate();
    }
}

auto OpenGlBackend::info() const noexcept -> const OpenGlInfo& {
    ownerThread_.assertCurrent();
    return info_;
}

auto OpenGlBackend::close() -> core::Result<void> {
    if (!SDL_IsMainThread() || !ownerThread_.isCurrent()) {
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "OpenGL backend must be closed on its owner thread"}
                                    .withContext("operation", "close"));
    }
    release();
    return {};
}

auto OpenGlBackend::renderFrame(const render::RenderFrame& frame) -> core::Result<void> {
    if (auto result = requireMainThread("render_frame"); !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (!ownerThread_.isCurrent()) {
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "The OpenGL backend belongs to another thread"}
                                    .withContext("operation", "render_frame"));
    }
    ownerThread_.assertCurrent();

    if (!window_.valid() || context_ == nullptr) {
        return core::unexpected(core::Error{"render.opengl.backend_unavailable",
                                            "The OpenGL backend has no active context"});
    }

    auto* nativeWindow = static_cast<SDL_Window*>(window_.nativeHandle());
    if (nativeWindow == nullptr) {
        return core::unexpected(core::Error{"render.opengl.invalid_window",
                                            "The OpenGL backend window is no longer available"});
    }
    if (!SDL_GL_MakeCurrent(nativeWindow, static_cast<SDL_GLContext>(context_))) {
        return core::unexpected(core::Error{"render.opengl.context_current_failed", sdlError()});
    }

    if (!render::isValidColor(frame.clearColor)) {
        return core::unexpected(
            core::Error{"render.opengl.invalid_frame",
                        "Clear color components must be finite values in the range [0, 1]"});
    }
    if (!core::isFinite(frame.viewProjection)) {
        return core::unexpected(
            core::Error{"render.opengl.invalid_frame", "View-projection matrix must be finite"});
    }

    std::vector<DebugVertex> vertices;
    if (frame.scene != nullptr) {
        if (frame.scene->size() > render::RenderScene::maxCommandCount) {
            return core::unexpected(core::Error{"render.opengl.command_limit_exceeded",
                                                "RenderScene command limit was exceeded"});
        }
        vertices.reserve(frame.scene->size() * 2);
        for (const auto& command : frame.scene->commands()) {
            if (command.type != render::RenderCommandType::DebugLine ||
                !core::isFinite(command.start) || !core::isFinite(command.end) ||
                !render::isValidColor(command.color)) {
                return core::unexpected(core::Error{"render.opengl.invalid_command",
                                                    "RenderScene contains an invalid command"});
            }
            vertices.push_back(makeDebugVertex(command.start, command.color));
            vertices.push_back(makeDebugVertex(command.end, command.color));
        }
    }

    constexpr auto maxDimension = static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max());
    const auto width = static_cast<GLsizei>(std::min(frame.extent.width, maxDimension));
    const auto height = static_cast<GLsizei>(std::min(frame.extent.height, maxDimension));

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(frame.clearColor.red, frame.clearColor.green, frame.clearColor.blue,
                 frame.clearColor.alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!vertices.empty()) {
        glUseProgram(debugProgram_);
        glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE,
                           frame.viewProjection.values.data());
        glBindVertexArray(debugVertexArray_);
        glBindBuffer(GL_ARRAY_BUFFER, debugVertexBuffer_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(DebugVertex)),
                     vertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);

        const GLenum drawError = glGetError();
        if (drawError != GL_NO_ERROR) {
            return core::unexpected(
                core::Error{"render.opengl.draw_failed", "OpenGL rejected debug line drawing"}
                    .withContext("gl_error", std::to_string(drawError)));
        }
    }

    if (!SDL_GL_SwapWindow(nativeWindow)) {
        return core::unexpected(core::Error{"render.opengl.swap_failed", sdlError()});
    }

    return {};
}

void OpenGlBackend::release() noexcept {
    if (context_ == nullptr) {
        return;
    }

    if (!SDL_IsMainThread() || !ownerThread_.isCurrent()) {
        return;
    }

    ownerThread_.assertCurrent();
    auto* nativeWindow = static_cast<SDL_Window*>(window_.nativeHandle());
    bool canReleaseGpuResources = nativeWindow != nullptr;
    if (canReleaseGpuResources && SDL_GL_GetCurrentContext() != context_) {
        canReleaseGpuResources =
            SDL_GL_MakeCurrent(nativeWindow, static_cast<SDL_GLContext>(context_));
    }
    if (canReleaseGpuResources) {
        presentation_.reset();
        if (debugVertexBuffer_ != 0) {
            glDeleteBuffers(1, &debugVertexBuffer_);
        }
        if (debugVertexArray_ != 0) {
            glDeleteVertexArrays(1, &debugVertexArray_);
        }
        if (debugProgram_ != 0) {
            glDeleteProgram(debugProgram_);
        }
    } else if (presentation_ || debugProgram_ != 0 || debugVertexArray_ != 0 ||
               debugVertexBuffer_ != 0) {
        logWarning(logSink_, "Could not make the context current to release OpenGL resources");
        presentation_.release();
    }
    if (nativeWindow != nullptr && SDL_GL_GetCurrentContext() == context_ &&
        !SDL_GL_MakeCurrent(nativeWindow, nullptr)) {
        logWarning(logSink_,
                   std::string{"Could not release the current OpenGL context: "} + sdlError());
    }
    if (!SDL_GL_DestroyContext(static_cast<SDL_GLContext>(context_))) {
        logWarning(logSink_,
                   std::string{"Could not destroy the OpenGL context cleanly: "} + sdlError());
    }

    context_ = nullptr;
    debugProgram_ = 0;
    debugVertexArray_ = 0;
    debugVertexBuffer_ = 0;
    viewProjectionLocation_ = -1;
    window_ = {};
    logSink_.reset();
}

} // namespace cuexis::render_opengl
