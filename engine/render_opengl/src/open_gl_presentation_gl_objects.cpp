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

static_assert(sizeof(CuexisObjectStd140) == 144);

[[nodiscard]] auto sdlError() -> std::string {
    const char* message = SDL_GetError();
    return message != nullptr && message[0] != '\0' ? message : "unknown SDL error";
}

[[nodiscard]] auto requireMainThread(std::string_view operation) -> core::Result<void> {
    if (SDL_IsMainThread()) {
        return {};
    }
    return core::unexpected(
        core::Error{"render.opengl.not_main_thread",
                    "OpenGL presentation operations require the SDL main thread"}
            .withContext("operation", std::string{operation}));
}

[[nodiscard]] auto resourceTypeName(playback::PresentationResourceType type) noexcept
    -> std::string_view {
    switch (type) {
    case playback::PresentationResourceType::Mesh:
        return "mesh";
    case playback::PresentationResourceType::Texture2D:
        return "texture2d";
    case playback::PresentationResourceType::UnlitMaterial:
        return "unlit_material";
    case playback::PresentationResourceType::Shader:
        return "shader";
    case playback::PresentationResourceType::ParameterizedMaterial:
        return "parameterized_material";
    }
    return "unknown";
}

[[nodiscard]] auto resourceError(std::string code, std::string message,
                                 const playback::PresentationResourceRef* reference)
    -> core::Error {
    auto error = core::Error{std::move(code), std::move(message)};
    if (reference != nullptr) {
        error.withContext("asset_id", reference->assetId)
            .withContext("resource_type", std::string{resourceTypeName(reference->type)});
    }
    return error;
}

[[nodiscard]] auto frameError(std::string message, std::string_view objectId,
                              const playback::PresentationResourceRef* reference) -> core::Error {
    auto error = resourceError("playback.presentation.frame.resource_mismatch", std::move(message),
                               reference);
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

[[nodiscard]] auto nonFiniteError(std::string_view objectId, std::string_view field)
    -> core::Error {
    auto error = core::Error{"playback.presentation.frame.non_finite",
                             "OpenGL presentation calculation contains a non-finite value"}
                     .withContext("field", std::string{field});
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

void clearGlErrors() noexcept {
    while (glGetError() != GL_NO_ERROR) {
    }
}

[[nodiscard]] auto checkGl(std::string code, std::string message,
                           const playback::PresentationResourceRef* reference)
    -> core::Result<void> {
    const GLenum errorCode = glGetError();
    if (errorCode == GL_NO_ERROR) {
        return {};
    }
    return core::unexpected(resourceError(std::move(code), std::move(message), reference)
                                .withContext("gl_error", std::to_string(errorCode)));
}

[[nodiscard]] auto cacheMeshBounds(const playback::PortableResource& resource,
                                   const playback::PortableMesh& mesh, detail::GpuMesh& uploaded,
                                   detail::BoundsProbeStats* stats) -> core::Result<void>;

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

[[nodiscard]] auto compileShader(GLenum type, const char* source, std::string_view stage)
    -> core::Result<GLuint> {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return core::unexpected(core::Error{"render.opengl.presentation.shader_create_failed",
                                            "OpenGL could not create a presentation shader"}
                                    .withContext("stage", std::string{stage}));
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        auto error =
            core::Error{"render.opengl.presentation.shader_compile_failed", shaderLog(shader)}
                .withContext("stage", std::string{stage});
        glDeleteShader(shader);
        return core::unexpected(std::move(error));
    }
    return shader;
}

[[nodiscard]] auto createPresentationPipeline() -> core::Result<detail::PresentationPipeline> {
    static constexpr const char* vertexSource = R"GLSL(#version 330 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;
uniform mat4 world;
uniform mat4 viewProjection;
out vec2 vertexUv;
void main() {
    gl_Position = viewProjection * world * vec4(inPosition, 1.0);
    vertexUv = inUv;
}
)GLSL";
    static constexpr const char* fragmentSource = R"GLSL(#version 330 core
in vec2 vertexUv;
uniform vec4 effectiveColor;
uniform sampler2D baseTexture;
uniform bool useTexture;
uniform bool useTextureAlpha;
out vec4 outColor;
void main() {
    vec4 sampled = useTexture ? texture(baseTexture, vertexUv) : vec4(1.0);
    float sampledAlpha = useTextureAlpha ? sampled.a : 1.0;
    outColor = vec4(effectiveColor.rgb * sampled.rgb, effectiveColor.a * sampledAlpha);
}
)GLSL";

    auto vertex = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex");
    if (!vertex) {
        return core::unexpected(std::move(vertex.error()));
    }
    auto fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment");
    if (!fragment) {
        glDeleteShader(*vertex);
        return core::unexpected(std::move(fragment.error()));
    }

    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(*fragment);
        glDeleteShader(*vertex);
        return core::unexpected(core::Error{"render.opengl.presentation.program_create_failed",
                                            "OpenGL could not create the presentation program"});
    }
    glAttachShader(program, *vertex);
    glAttachShader(program, *fragment);
    glLinkProgram(program);
    glDeleteShader(*fragment);
    glDeleteShader(*vertex);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        auto error =
            core::Error{"render.opengl.presentation.program_link_failed", programLog(program)};
        glDeleteProgram(program);
        return core::unexpected(std::move(error));
    }

    detail::PresentationPipeline pipeline;
    pipeline.program = detail::UniqueProgram{program};
    pipeline.worldLocation = glGetUniformLocation(program, "world");
    pipeline.viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
    pipeline.effectiveColorLocation = glGetUniformLocation(program, "effectiveColor");
    pipeline.useTextureLocation = glGetUniformLocation(program, "useTexture");
    pipeline.useTextureAlphaLocation = glGetUniformLocation(program, "useTextureAlpha");
    pipeline.textureLocation = glGetUniformLocation(program, "baseTexture");
    if (pipeline.worldLocation < 0 || pipeline.viewProjectionLocation < 0 ||
        pipeline.effectiveColorLocation < 0 || pipeline.useTextureLocation < 0 ||
        pipeline.useTextureAlphaLocation < 0 || pipeline.textureLocation < 0) {
        return core::unexpected(
            core::Error{"render.opengl.presentation.uniform_missing",
                        "The presentation program does not expose all required uniforms"});
    }
    return pipeline;
}

[[nodiscard]] auto validationError(const playback::PresentationValidationResult& validation)
    -> core::Error {
    auto error = core::Error{"render.opengl.presentation.capability_failed",
                             "OpenGL presentation capability validation failed"};
    if (!validation.diagnostics.items().empty()) {
        const auto& diagnostic = validation.diagnostics.items().front();
        error.withContext("diagnostic_code", std::string{diagnostic.code()})
            .withContext("diagnostic_message", std::string{diagnostic.message()});
    }
    return error;
}

[[nodiscard]] auto uploadMesh(const playback::PortableResource& resource)
    -> core::Result<detail::GpuMesh> {
    const auto* mesh = std::get_if<playback::PortableMesh>(&resource.value);
    if (mesh == nullptr) {
        return core::unexpected(resourceError("render.opengl.presentation.resource_type_invalid",
                                              "Mesh resource has an incompatible portable value",
                                              &resource.reference));
    }
    const std::size_t vertexCount = mesh->positions.size() / 3U;
    std::vector<PresentationVertex> vertices;
    vertices.reserve(vertexCount);
    for (std::size_t index = 0; index < vertexCount; ++index) {
        const std::size_t positionOffset = index * 3U;
        const std::size_t uvOffset = index * 2U;
        vertices.push_back(PresentationVertex{
            mesh->positions[positionOffset], mesh->positions[positionOffset + 1U],
            mesh->positions[positionOffset + 2U], mesh->uv0.empty() ? 0.0F : mesh->uv0[uvOffset],
            mesh->uv0.empty() ? 0.0F : mesh->uv0[uvOffset + 1U]});
    }

    detail::GpuMesh uploaded;
    uploaded.reference = resource.reference;
    if (auto cached = cacheMeshBounds(resource, *mesh, uploaded, nullptr); !cached) {
        return core::unexpected(std::move(cached.error()));
    }

    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    clearGlErrors();
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glGenBuffers(1, &indexBuffer);
    uploaded.vertexArray = detail::UniqueVertexArray{vertexArray};
    uploaded.vertexBuffer = detail::UniqueBuffer{vertexBuffer};
    uploaded.indexBuffer = detail::UniqueBuffer{indexBuffer};
    if (vertexArray == 0 || vertexBuffer == 0 || indexBuffer == 0) {
        return core::unexpected(resourceError("render.opengl.presentation.mesh_create_failed",
                                              "OpenGL could not create Mesh objects",
                                              &resource.reference));
    }

    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    test_only::GpuStorage::charge(1, vertexBuffer, vertices.size() * sizeof(PresentationVertex));
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(PresentationVertex)),
                 vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PresentationVertex),
                          reinterpret_cast<const void*>(offsetof(PresentationVertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PresentationVertex),
                          reinterpret_cast<const void*>(offsetof(PresentationVertex, u)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    test_only::GpuStorage::charge(1, indexBuffer, mesh->indices.size() * sizeof(std::uint32_t));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh->indices.size() * sizeof(std::uint32_t)),
                 mesh->indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (auto checked = checkGl("render.opengl.presentation.mesh_upload_failed",
                               "OpenGL rejected Mesh upload", &resource.reference);
        !checked) {
        return core::unexpected(std::move(checked.error()));
    }
    uploaded.indexCount = static_cast<GLsizei>(mesh->indices.size());
    return uploaded;
}

[[nodiscard]] auto uploadTexture(const playback::PortableResource& resource)
    -> core::Result<detail::GpuTexture> {
    const auto* texture = std::get_if<playback::PortableTexture2D>(&resource.value);
    if (texture == nullptr) {
        return core::unexpected(resourceError(
            "render.opengl.presentation.resource_type_invalid",
            "Texture2D resource has an incompatible portable value", &resource.reference));
    }
    GLuint textureName = 0;
    clearGlErrors();
    glGenTextures(1, &textureName);
    detail::GpuTexture uploaded;
    uploaded.reference = resource.reference;
    uploaded.texture = detail::UniqueTexture{textureName};
    if (textureName == 0) {
        return core::unexpected(resourceError("render.opengl.presentation.texture_create_failed",
                                              "OpenGL could not create Texture2D object",
                                              &resource.reference));
    }
    glBindTexture(GL_TEXTURE_2D, textureName);
    test_only::GpuStorage::charge(2, textureName, texture->pixelsRgba8.size());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const GLint internalFormat =
        texture->colorSpace == playback::PresentationColorSpace::Srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, static_cast<GLsizei>(texture->width),
                 static_cast<GLsizei>(texture->height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 texture->pixelsRgba8.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    if (auto checked = checkGl("render.opengl.presentation.texture_upload_failed",
                               "OpenGL rejected Texture2D upload", &resource.reference);
        !checked) {
        return core::unexpected(std::move(checked.error()));
    }
    return uploaded;
}

[[nodiscard]] auto copyMaterial(const playback::PortableResource& resource)
    -> core::Result<detail::GpuMaterial> {
    const auto* material = std::get_if<playback::PortableUnlitMaterial>(&resource.value);
    if (material == nullptr) {
        return core::unexpected(resourceError(
            "render.opengl.presentation.resource_type_invalid",
            "Unlit Material resource has an incompatible portable value", &resource.reference));
    }
    detail::GpuMaterial uploaded;
    uploaded.reference = resource.reference;
    uploaded.material = *material;
    return uploaded;
}

[[nodiscard]] auto copyParameterizedMaterial(const playback::PortableResource& resource)
    -> core::Result<detail::GpuMaterial> {
    const auto* material = std::get_if<playback::PortableParameterizedMaterial>(&resource.value);
    if (material == nullptr) {
        return core::unexpected(
            resourceError("render.opengl.presentation.resource_type_invalid",
                          "Parameterized Material resource has an incompatible portable value",
                          &resource.reference));
    }
    detail::GpuMaterial uploaded;
    uploaded.reference = resource.reference;
    uploaded.material.baseColor[0] = 1.0F;
    uploaded.material.baseColor[1] = 1.0F;
    uploaded.material.baseColor[2] = 1.0F;
    uploaded.material.baseColor[3] = 1.0F;
    uploaded.material.alphaMode = material->alphaMode;
    uploaded.material.doubleSided = material->doubleSided;
    uploaded.parameterized = true;
    uploaded.parameterizedMaterial = *material;
    return uploaded;
}

[[nodiscard]] auto linkGlsl330Program(const char* vertexSource, const char* fragmentSource,
                                      const playback::PresentationResourceRef* reference)
    -> core::Result<GLuint> {
    auto vertex = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex");
    if (!vertex) {
        return core::unexpected(std::move(vertex.error()));
    }
    auto fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment");
    if (!fragment) {
        glDeleteShader(*vertex);
        return core::unexpected(std::move(fragment.error()));
    }
    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(*fragment);
        glDeleteShader(*vertex);
        return core::unexpected(resourceError("render.opengl.presentation.program_create_failed",
                                              "OpenGL could not create a parameterized program",
                                              reference));
    }
    glAttachShader(program, *vertex);
    glAttachShader(program, *fragment);
    glLinkProgram(program);
    glDeleteShader(*fragment);
    glDeleteShader(*vertex);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        auto error = resourceError("render.opengl.presentation.program_link_failed",
                                   programLog(program), reference);
        glDeleteProgram(program);
        return core::unexpected(std::move(error));
    }
    return program;
}

} // namespace cuexis::render_opengl
