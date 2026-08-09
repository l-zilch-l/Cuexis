#include "open_gl_presentation_internal.hpp"

#include <cuexis/core/error.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::render_opengl {
namespace {

constexpr std::size_t maxNormalizedRecords = 100'000;
constexpr std::uint64_t maxResourceBytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maxSessionBytes = 512ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t maxMeshVertices = 1'048'576;
constexpr std::uint32_t maxMeshIndices = 3'145'728;
constexpr std::uint32_t portableMaxTextureDimension = 8'192;
constexpr double depthQuantization = 4096.0;
constexpr double signedIntegerLimit = 0x1p63;

struct PresentationVertex final {
    float x;
    float y;
    float z;
    float u;
    float v;
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

struct PreparedDraw final {
    std::size_t objectIndex{};
    OpenGlDrawCommand command;
    const detail::GpuMesh* mesh{};
    const detail::GpuMaterial* material{};
    const detail::GpuTexture* texture{};
};

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
    }
    return "unknown";
}

[[nodiscard]] auto referenceKey(const playback::PresentationResourceRef& reference) noexcept {
    return std::tie(reference.assetId, reference.type);
}

[[nodiscard]] auto resourceError(std::string code, std::string message,
                                 const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error {
    auto error = core::Error{std::move(code), std::move(message)};
    if (reference != nullptr) {
        error.withContext("asset_id", reference->assetId)
            .withContext("resource_type", std::string{resourceTypeName(reference->type)});
    }
    return error;
}

[[nodiscard]] auto frameError(std::string message, std::string_view objectId,
                              const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error {
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
                           const playback::PresentationResourceRef* reference = nullptr)
    -> core::Result<void> {
    const GLenum errorCode = glGetError();
    if (errorCode == GL_NO_ERROR) {
        return {};
    }
    return core::unexpected(resourceError(std::move(code), std::move(message), reference)
                                .withContext("gl_error", std::to_string(errorCode)));
}

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

    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    clearGlErrors();
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glGenBuffers(1, &indexBuffer);
    detail::GpuMesh uploaded;
    uploaded.reference = resource.reference;
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
    return detail::GpuMaterial{resource.reference, *material};
}

template <typename Resource>
[[nodiscard]] auto findGpuResource(const std::vector<Resource>& resources,
                                   const playback::PresentationResourceRef& reference) noexcept
    -> const Resource* {
    const auto found = std::lower_bound(
        resources.begin(), resources.end(), reference,
        [](const Resource& resource, const playback::PresentationResourceRef& candidate) {
            return referenceKey(resource.reference) < referenceKey(candidate);
        });
    return found != resources.end() && found->reference == reference ? &*found : nullptr;
}

[[nodiscard]] auto finiteMatrix(const float (&matrix)[16]) noexcept -> bool {
    return std::all_of(std::begin(matrix), std::end(matrix),
                       [](float value) { return std::isfinite(value); });
}

struct Point3 final {
    double x{};
    double y{};
    double z{};
};

[[nodiscard]] auto transformPoint(const float (&matrix)[16], const Point3& point) noexcept
    -> Point3 {
    return Point3{
        static_cast<double>(matrix[0]) * point.x + static_cast<double>(matrix[4]) * point.y +
            static_cast<double>(matrix[8]) * point.z + static_cast<double>(matrix[12]),
        static_cast<double>(matrix[1]) * point.x + static_cast<double>(matrix[5]) * point.y +
            static_cast<double>(matrix[9]) * point.z + static_cast<double>(matrix[13]),
        static_cast<double>(matrix[2]) * point.x + static_cast<double>(matrix[6]) * point.y +
            static_cast<double>(matrix[10]) * point.z + static_cast<double>(matrix[14])};
}

[[nodiscard]] auto finitePoint(const Point3& point) noexcept -> bool {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

[[nodiscard]] auto multiplyMatrices(const std::array<float, 16>& left,
                                    const std::array<float, 16>& right) noexcept
    -> std::array<float, 16> {
    std::array<float, 16> result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            double value = 0.0;
            for (std::size_t index = 0; index < 4; ++index) {
                value += static_cast<double>(left[index * 4 + row]) *
                         static_cast<double>(right[column * 4 + index]);
            }
            result[column * 4 + row] = static_cast<float>(value);
        }
    }
    return result;
}

class SummaryHash final {
  public:
    SummaryHash() noexcept {
        static constexpr char domain[] = "cuexis.validation.summary.v1";
        writeBytes(std::as_bytes(std::span{domain, sizeof(domain)}));
    }

    void writeU8(std::uint8_t value) noexcept {
        value_ ^= value;
        value_ *= 1099511628211ULL;
    }

    void writeU32(std::uint32_t value) noexcept {
        for (std::size_t index = 0; index < 4; ++index) {
            writeU8(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
        }
    }

    void writeU64(std::uint64_t value) noexcept {
        for (std::size_t index = 0; index < 8; ++index) {
            writeU8(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
        }
    }

    void writeBool(bool value) noexcept {
        writeU8(value ? 1U : 0U);
    }

    void writeFloat(float value) noexcept {
        if (value == 0.0F) {
            value = 0.0F;
        }
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeDouble(double value) noexcept {
        if (value == 0.0) {
            value = 0.0;
        }
        writeU64(std::bit_cast<std::uint64_t>(value));
    }

    void writeBytes(std::span<const std::byte> bytes) noexcept {
        for (const auto value : bytes) {
            writeU8(std::to_integer<std::uint8_t>(value));
        }
    }

    void writeString(std::string_view value) noexcept {
        writeU32(static_cast<std::uint32_t>(value.size()));
        writeBytes(std::as_bytes(std::span{value.data(), value.size()}));
    }

    void writeReference(const playback::PresentationResourceRef& reference) noexcept {
        writeU32(static_cast<std::uint32_t>(reference.type));
        writeString(reference.assetId);
        writeBytes(std::as_bytes(std::span{reference.identity.sha256}));
    }

    [[nodiscard]] auto value() const noexcept -> std::uint64_t {
        return value_;
    }

  private:
    std::uint64_t value_{14695981039346656037ULL};
};

void hashCommand(SummaryHash& hash, const OpenGlDrawCommand& command) noexcept {
    hash.writeString(command.objectId);
    for (const auto value : command.worldMatrix) {
        hash.writeFloat(value);
    }
    hash.writeReference(command.mesh);
    hash.writeReference(command.material);
    for (const auto value : command.effectiveColor) {
        hash.writeDouble(value);
    }
    hash.writeU8(static_cast<std::uint8_t>(command.pass));
    hash.writeBool(command.backFaceCulling);
    hash.writeBool(command.depthTest);
    hash.writeBool(command.depthWrite);
    hash.writeBool(command.sourceOverBlend);
    hash.writeDouble(command.depthMeters);
    hash.writeU64(std::bit_cast<std::uint64_t>(command.transparentDepthKey));
}

[[nodiscard]] auto summaryDigest(const OpenGlDrawSummary& summary) noexcept -> std::uint64_t {
    SummaryHash hash;
    hash.writeU32(summary.version);
    hash.writeU32(summary.viewportWidth);
    hash.writeU32(summary.viewportHeight);
    for (const auto value : summary.clearColor) {
        hash.writeFloat(value);
    }
    hash.writeBool(summary.cameraActive);
    for (const auto value : summary.viewMatrix) {
        hash.writeFloat(value);
    }
    for (const auto value : summary.projectionMatrix) {
        hash.writeFloat(value);
    }
    hash.writeBool(summary.debugPassEnabled);
    hash.writeU32(static_cast<std::uint32_t>(summary.opaque.size()));
    for (const auto& command : summary.opaque) {
        hashCommand(hash, command);
    }
    hash.writeU32(static_cast<std::uint32_t>(summary.transparent.size()));
    for (const auto& command : summary.transparent) {
        hashCommand(hash, command);
    }
    return hash.value();
}

[[nodiscard]] auto buildDraws(const playback::FrameSnapshot& snapshot,
                              const detail::PresentationResourceSet& resources,
                              OpenGlDrawSummary& summary, std::vector<PreparedDraw>& opaque,
                              std::vector<PreparedDraw>& transparent) -> core::Result<void> {
    if (snapshot.objects.size() > maxNormalizedRecords) {
        return core::unexpected(
            core::Error{"playback.presentation.frame.command_budget_exceeded",
                        "OpenGL presentation command count exceeds the Portable v1 limit"}
                .withContext("limit", std::to_string(maxNormalizedRecords))
                .withContext("actual", std::to_string(snapshot.objects.size())));
    }
    if (resources.manifest.entries.empty()) {
        return {};
    }
    opaque.reserve(snapshot.objects.size());
    transparent.reserve(snapshot.objects.size());
    bool cameraValidated = false;
    for (std::size_t objectIndex = 0; objectIndex < snapshot.objects.size(); ++objectIndex) {
        const auto& object = snapshot.objects[objectIndex];
        if (object.mesh.has_value() != object.material.has_value()) {
            const auto* reference = object.mesh ? &*object.mesh : &*object.material;
            return core::unexpected(frameError("Renderable Mesh and Material refs must be paired",
                                               object.id, reference));
        }
        if (!object.mesh) {
            if (!object.materialAssetId.empty()) {
                return core::unexpected(
                    frameError("Renderable snapshot is missing portable refs", object.id));
            }
            continue;
        }
        if (object.mesh->type != playback::PresentationResourceType::Mesh ||
            object.material->type != playback::PresentationResourceType::UnlitMaterial ||
            object.materialAssetId != object.material->assetId) {
            return core::unexpected(
                frameError("Snapshot portable refs have incompatible types or IDs", object.id,
                           &*object.material));
        }
        const auto* mesh = findGpuResource(resources.meshes, *object.mesh);
        const auto* material = findGpuResource(resources.materials, *object.material);
        if (mesh == nullptr || material == nullptr) {
            return core::unexpected(
                frameError("Snapshot ref is not backed by the active OpenGL cache", object.id,
                           mesh == nullptr ? &*object.mesh : &*object.material));
        }
        const detail::GpuTexture* texture = nullptr;
        if (material->material.baseColorTexture) {
            texture = findGpuResource(resources.textures, *material->material.baseColorTexture);
            if (texture == nullptr) {
                return core::unexpected(
                    frameError("Material texture ref is not backed by the active OpenGL cache",
                               object.id, &*material->material.baseColorTexture));
            }
        }
        if (!object.visible) {
            continue;
        }
        if (!snapshot.camera.active) {
            return core::unexpected(core::Error{"playback.presentation.frame.camera_required",
                                                "Visible renderables require an active camera"}
                                        .withContext("object_id", object.id));
        }
        if (!cameraValidated) {
            if (!finiteMatrix(snapshot.camera.viewMatrix) ||
                !finiteMatrix(snapshot.camera.projectionMatrix)) {
                return core::unexpected(nonFiniteError({}, "camera_matrix"));
            }
            cameraValidated = true;
        }
        if (!finiteMatrix(object.worldMatrix)) {
            return core::unexpected(nonFiniteError(object.id, "world_matrix"));
        }
        if (!std::isfinite(object.materialOpacity)) {
            return core::unexpected(nonFiniteError(object.id, "material_opacity"));
        }

        PreparedDraw draw;
        draw.objectIndex = objectIndex;
        draw.mesh = mesh;
        draw.material = material;
        draw.texture = texture;
        draw.command.objectId = object.id;
        std::copy(std::begin(object.worldMatrix), std::end(object.worldMatrix),
                  draw.command.worldMatrix.begin());
        draw.command.mesh = *object.mesh;
        draw.command.material = *object.material;
        for (std::size_t component = 0; component < 3; ++component) {
            if (!std::isfinite(material->material.baseColor[component]) ||
                !std::isfinite(object.materialTint[component])) {
                return core::unexpected(nonFiniteError(object.id, "effective_rgb"));
            }
            draw.command.effectiveColor[component] =
                static_cast<double>(material->material.baseColor[component]) *
                static_cast<double>(object.materialTint[component]);
            if (!std::isfinite(draw.command.effectiveColor[component])) {
                return core::unexpected(nonFiniteError(object.id, "effective_rgb"));
            }
        }
        if (!std::isfinite(material->material.baseColor[3])) {
            return core::unexpected(nonFiniteError(object.id, "effective_alpha"));
        }
        draw.command.effectiveColor[3] =
            static_cast<double>(material->material.baseColor[3]) * object.materialOpacity;
        if (!std::isfinite(draw.command.effectiveColor[3])) {
            return core::unexpected(nonFiniteError(object.id, "effective_alpha"));
        }

        const auto meshResource = std::find_if(
            resources.resources.begin(), resources.resources.end(), [&](const auto& candidate) {
                return candidate != nullptr && candidate->reference == *object.mesh;
            });
        if (meshResource == resources.resources.end()) {
            return core::unexpected(
                frameError("Mesh bounds are unavailable", object.id, &*object.mesh));
        }
        const auto* portableMesh = std::get_if<playback::PortableMesh>(&(*meshResource)->value);
        if (portableMesh == nullptr) {
            return core::unexpected(
                frameError("Mesh resource value is incompatible", object.id, &*object.mesh));
        }
        Point3 localCenter;
        for (std::size_t component = 0; component < 3; ++component) {
            if (!std::isfinite(portableMesh->boundsMin[component]) ||
                !std::isfinite(portableMesh->boundsMax[component])) {
                return core::unexpected(nonFiniteError(object.id, "mesh_bounds"));
            }
        }
        localCenter.x = (static_cast<double>(portableMesh->boundsMin[0]) +
                         static_cast<double>(portableMesh->boundsMax[0])) /
                        2.0;
        localCenter.y = (static_cast<double>(portableMesh->boundsMin[1]) +
                         static_cast<double>(portableMesh->boundsMax[1])) /
                        2.0;
        localCenter.z = (static_cast<double>(portableMesh->boundsMin[2]) +
                         static_cast<double>(portableMesh->boundsMax[2])) /
                        2.0;
        const auto worldCenter = transformPoint(object.worldMatrix, localCenter);
        const auto viewCenter = transformPoint(snapshot.camera.viewMatrix, worldCenter);
        if (!finitePoint(localCenter) || !finitePoint(worldCenter) || !finitePoint(viewCenter)) {
            return core::unexpected(nonFiniteError(object.id, "depth_transform"));
        }
        draw.command.depthMeters = -viewCenter.z;
        const double scaledDepth = draw.command.depthMeters * depthQuantization;
        const double roundedDepth = std::round(scaledDepth);
        if (!std::isfinite(draw.command.depthMeters) || !std::isfinite(scaledDepth) ||
            !std::isfinite(roundedDepth) || roundedDepth < -signedIntegerLimit ||
            roundedDepth >= signedIntegerLimit) {
            return core::unexpected(nonFiniteError(object.id, "depth"));
        }
        draw.command.transparentDepthKey = static_cast<std::int64_t>(roundedDepth);
        draw.command.backFaceCulling = !material->material.doubleSided;
        draw.command.pass =
            material->material.alphaMode == playback::PresentationAlphaMode::Blend ||
                    draw.command.effectiveColor[3] < 1.0
                ? OpenGlPresentationPass::Transparent
                : OpenGlPresentationPass::Opaque;
        draw.command.depthWrite = draw.command.pass == OpenGlPresentationPass::Opaque;
        draw.command.sourceOverBlend = draw.command.pass == OpenGlPresentationPass::Transparent;
        if (draw.command.pass == OpenGlPresentationPass::Opaque) {
            opaque.push_back(std::move(draw));
        } else {
            transparent.push_back(std::move(draw));
        }
    }

    std::sort(opaque.begin(), opaque.end(), [](const auto& left, const auto& right) {
        return std::tie(left.command.objectId, left.objectIndex) <
               std::tie(right.command.objectId, right.objectIndex);
    });
    std::sort(transparent.begin(), transparent.end(), [](const auto& left, const auto& right) {
        if (left.command.transparentDepthKey != right.command.transparentDepthKey) {
            return left.command.transparentDepthKey > right.command.transparentDepthKey;
        }
        return std::tie(left.command.objectId, left.objectIndex) <
               std::tie(right.command.objectId, right.objectIndex);
    });

    summary.opaque.reserve(opaque.size());
    summary.transparent.reserve(transparent.size());
    for (const auto& draw : opaque) {
        summary.opaque.push_back(draw.command);
    }
    for (const auto& draw : transparent) {
        summary.transparent.push_back(draw.command);
    }
    return {};
}

void drawPresentationCommands(const detail::PresentationPipeline& pipeline,
                              const std::array<float, 16>& viewProjection,
                              std::span<const PreparedDraw> draws, bool transparent) noexcept {
    if (draws.empty()) {
        return;
    }
    glUseProgram(pipeline.program.value);
    glUniformMatrix4fv(pipeline.viewProjectionLocation, 1, GL_FALSE, viewProjection.data());
    glUniform1i(pipeline.textureLocation, 0);
    if (transparent) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    } else {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
    for (const auto& draw : draws) {
        if (draw.command.backFaceCulling) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        glUniformMatrix4fv(pipeline.worldLocation, 1, GL_FALSE, draw.command.worldMatrix.data());
        glUniform4f(pipeline.effectiveColorLocation,
                    static_cast<float>(draw.command.effectiveColor[0]),
                    static_cast<float>(draw.command.effectiveColor[1]),
                    static_cast<float>(draw.command.effectiveColor[2]),
                    static_cast<float>(draw.command.effectiveColor[3]));
        glUniform1i(pipeline.useTextureLocation, draw.texture != nullptr ? 1 : 0);
        glUniform1i(pipeline.useTextureAlphaLocation,
                    draw.texture != nullptr && draw.material->material.alphaMode ==
                                                   playback::PresentationAlphaMode::Blend
                        ? 1
                        : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, draw.texture != nullptr ? draw.texture->texture.value : 0);
        glBindVertexArray(draw.mesh->vertexArray.value);
        glDrawElements(GL_TRIANGLES, draw.mesh->indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

[[nodiscard]] auto collectDebugVertices(const render::RenderScene* scene,
                                        std::vector<DebugVertex>& vertices) -> core::Result<void> {
    if (scene == nullptr) {
        return {};
    }
    if (scene->size() > render::RenderScene::maxCommandCount) {
        return core::unexpected(core::Error{"render.opengl.command_limit_exceeded",
                                            "RenderScene command limit was exceeded"});
    }
    vertices.reserve(scene->size() * 2U);
    for (const auto& command : scene->commands()) {
        if (command.type != render::RenderCommandType::DebugLine ||
            !core::isFinite(command.start) || !core::isFinite(command.end) ||
            !render::isValidColor(command.color)) {
            return core::unexpected(core::Error{"render.opengl.invalid_command",
                                                "RenderScene contains an invalid Debug command"});
        }
        vertices.push_back(DebugVertex{command.start.x, command.start.y, command.start.z,
                                       command.color.red, command.color.green, command.color.blue,
                                       command.color.alpha});
        vertices.push_back(DebugVertex{command.end.x, command.end.y, command.end.z,
                                       command.color.red, command.color.green, command.color.blue,
                                       command.color.alpha});
    }
    return {};
}

} // namespace

namespace detail {

UniqueBuffer::~UniqueBuffer() {
    if (value != 0) {
        glDeleteBuffers(1, &value);
    }
}

UniqueBuffer::UniqueBuffer(UniqueBuffer&& other) noexcept : value(std::exchange(other.value, 0)) {}

auto UniqueBuffer::operator=(UniqueBuffer&& other) noexcept -> UniqueBuffer& {
    if (this != &other) {
        if (value != 0) {
            glDeleteBuffers(1, &value);
        }
        value = std::exchange(other.value, 0);
    }
    return *this;
}

UniqueVertexArray::~UniqueVertexArray() {
    if (value != 0) {
        glDeleteVertexArrays(1, &value);
    }
}

UniqueVertexArray::UniqueVertexArray(UniqueVertexArray&& other) noexcept
    : value(std::exchange(other.value, 0)) {}

auto UniqueVertexArray::operator=(UniqueVertexArray&& other) noexcept -> UniqueVertexArray& {
    if (this != &other) {
        if (value != 0) {
            glDeleteVertexArrays(1, &value);
        }
        value = std::exchange(other.value, 0);
    }
    return *this;
}

UniqueTexture::~UniqueTexture() {
    if (value != 0) {
        glDeleteTextures(1, &value);
    }
}

UniqueTexture::UniqueTexture(UniqueTexture&& other) noexcept
    : value(std::exchange(other.value, 0)) {}

auto UniqueTexture::operator=(UniqueTexture&& other) noexcept -> UniqueTexture& {
    if (this != &other) {
        if (value != 0) {
            glDeleteTextures(1, &value);
        }
        value = std::exchange(other.value, 0);
    }
    return *this;
}

UniqueProgram::~UniqueProgram() {
    if (value != 0) {
        glDeleteProgram(value);
    }
}

UniqueProgram::UniqueProgram(UniqueProgram&& other) noexcept
    : value(std::exchange(other.value, 0)) {}

auto UniqueProgram::operator=(UniqueProgram&& other) noexcept -> UniqueProgram& {
    if (this != &other) {
        if (value != 0) {
            glDeleteProgram(value);
        }
        value = std::exchange(other.value, 0);
    }
    return *this;
}

auto createPresentationBackendState(std::uint64_t backendToken)
    -> core::Result<std::unique_ptr<OpenGlPresentationBackendState>> {
    auto pipeline = createPresentationPipeline();
    if (!pipeline) {
        return core::unexpected(std::move(pipeline.error()));
    }
    try {
        auto state = std::make_unique<OpenGlPresentationBackendState>();
        state->pipeline = std::move(*pipeline);
        state->backendToken = backendToken;
        return state;
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"render.opengl.presentation.allocation_failed",
                        "OpenGL presentation backend state allocation could not be satisfied"});
    }
}

} // namespace detail

void OpenGlDrawSummary::clear() noexcept {
    version = 1;
    viewportWidth = 0;
    viewportHeight = 0;
    clearColor.fill(0.0F);
    cameraActive = false;
    viewMatrix.fill(0.0F);
    projectionMatrix.fill(0.0F);
    debugPassEnabled = false;
    opaque.clear();
    transparent.clear();
    debugCommandCount = 0;
    digest = 0;
}

OpenGlPresentationCandidate::OpenGlPresentationCandidate(
    OpenGlPresentationCandidate&& other) noexcept
    : token_(std::move(other.token_)), settings_(other.settings_),
      backendToken_(std::exchange(other.backendToken_, 0)),
      generation_(std::exchange(other.generation_, 0)) {}

auto OpenGlPresentationCandidate::operator=(OpenGlPresentationCandidate&& other) noexcept
    -> OpenGlPresentationCandidate& {
    if (this != &other) {
        token_ = std::move(other.token_);
        settings_ = other.settings_;
        backendToken_ = std::exchange(other.backendToken_, 0);
        generation_ = std::exchange(other.generation_, 0);
    }
    return *this;
}

bool OpenGlPresentationCandidate::valid() const noexcept {
    return backendToken_ != 0 && generation_ != 0;
}

auto OpenGlPresentationCandidate::token() const noexcept
    -> const playback::PresentationCandidateToken& {
    return token_;
}

auto OpenGlPresentationCandidate::settings() const noexcept
    -> const playback::EffectivePresentationSettings& {
    return settings_;
}

auto OpenGlBackend::preparePresentation(playback::PreparedPlayback& prepared,
                                        const playback::PresentationRequest& request)
    -> core::Result<OpenGlPresentationCandidate> {
    if (auto mainThread = requireMainThread("prepare_presentation"); !mainThread) {
        return core::unexpected(std::move(mainThread.error()));
    }
    if (!ownerThread_.isCurrent()) {
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "The OpenGL backend belongs to another thread"}
                                    .withContext("operation", "prepare_presentation"));
    }
    if (!presentation_ || !window_.valid() || context_ == nullptr) {
        return core::unexpected(core::Error{"render.opengl.backend_unavailable",
                                            "The OpenGL presentation backend is unavailable"});
    }
    if (presentation_->pending.has_value()) {
        return core::unexpected(
            core::Error{"render.opengl.presentation.candidate_outstanding",
                        "The previous OpenGL presentation candidate is still outstanding"}
                .withContext("candidate_generation",
                             std::to_string(presentation_->pendingGeneration)));
    }
    auto* nativeWindow = static_cast<SDL_Window*>(window_.nativeHandle());
    if (nativeWindow == nullptr ||
        !SDL_GL_MakeCurrent(nativeWindow, static_cast<SDL_GLContext>(context_))) {
        return core::unexpected(core::Error{"render.opengl.context_current_failed", sdlError()});
    }
    presentation_->retired.reset();

    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    const auto textureLimit = static_cast<std::uint32_t>(
        std::clamp(maxTextureSize, 0, static_cast<int>(portableMaxTextureDimension)));
    const playback::PresentationCapabilities capabilities{
        .opaquePass = true,
        .transparentPass = true,
        .linearTexture = true,
        .srgbTexture = true,
        .straightAlphaBlend = true,
        .backFaceCulling = true,
        .doubleSided = true,
        .debugPass = debugProgram_ != 0,
        .maxResourceBytes = maxResourceBytes,
        .maxTotalDecodedBytes = maxSessionBytes,
        .maxTextureDimension = textureLimit,
        .maxMeshVertices = maxMeshVertices,
        .maxMeshIndices = maxMeshIndices,
    };
    auto validation = prepared.validatePresentation(capabilities, request);
    if (!validation.hasValue()) {
        return core::unexpected(validationError(validation));
    }
    const auto* manifest = prepared.presentationManifest();
    if (manifest == nullptr) {
        return core::unexpected(
            core::Error{"render.opengl.presentation.portable_candidate_required",
                        "OpenGL presentation requires a Portable Presentation candidate"});
    }
    if (presentation_->nextGeneration == std::numeric_limits<std::uint64_t>::max()) {
        return core::unexpected(
            core::Error{"render.opengl.presentation.generation_exhausted",
                        "OpenGL presentation candidate generation is exhausted"});
    }

    try {
        detail::PresentationResourceSet candidate;
        candidate.settings = *validation.settings;
        candidate.manifest = *manifest;
        auto token = prepared.presentationCandidateToken();
        if (!token) {
            return core::unexpected(std::move(token.error()));
        }
        candidate.token = *token;
        candidate.resources.reserve(manifest->entries.size());
        candidate.meshes.reserve(manifest->entries.size());
        candidate.textures.reserve(manifest->entries.size());
        candidate.materials.reserve(manifest->entries.size());
        for (const auto& entry : manifest->entries) {
            auto resource = prepared.acquirePresentationResource(entry.reference);
            if (!resource) {
                return core::unexpected(std::move(resource.error()));
            }
            if ((*resource)->reference != entry.reference) {
                return core::unexpected(
                    resourceError("render.opengl.presentation.resource_mismatch",
                                  "Acquired portable resource does not match its manifest entry",
                                  &entry.reference));
            }
            candidate.resources.push_back(*resource);
            switch (entry.reference.type) {
            case playback::PresentationResourceType::Mesh: {
                auto uploaded = uploadMesh(**resource);
                if (!uploaded) {
                    return core::unexpected(std::move(uploaded.error()));
                }
                candidate.meshes.push_back(std::move(*uploaded));
                break;
            }
            case playback::PresentationResourceType::Texture2D: {
                auto uploaded = uploadTexture(**resource);
                if (!uploaded) {
                    return core::unexpected(std::move(uploaded.error()));
                }
                candidate.textures.push_back(std::move(*uploaded));
                break;
            }
            case playback::PresentationResourceType::UnlitMaterial: {
                auto copied = copyMaterial(**resource);
                if (!copied) {
                    return core::unexpected(std::move(copied.error()));
                }
                candidate.materials.push_back(std::move(*copied));
                break;
            }
            }
        }

        const std::uint64_t generation = ++presentation_->nextGeneration;
        presentation_->pending = std::move(candidate);
        presentation_->pendingGeneration = generation;
        return OpenGlPresentationCandidate{presentation_->pending->token,
                                           presentation_->pending->settings,
                                           presentation_->backendToken, generation};
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            core::Error{"render.opengl.presentation.upload_budget_exceeded",
                        "OpenGL presentation candidate allocation could not be satisfied"}
                .withContext("limit", std::to_string(maxSessionBytes)));
    } catch (const std::exception& exception) {
        return core::unexpected(core::Error{"render.opengl.presentation.prepare_failed",
                                            "OpenGL presentation candidate preparation failed"}
                                    .withContext("exception", exception.what()));
    } catch (...) {
        return core::unexpected(core::Error{"render.opengl.presentation.prepare_failed",
                                            "OpenGL presentation candidate preparation failed"});
    }
}

void OpenGlBackend::activatePresentation(OpenGlPresentationCandidate&& candidate) noexcept {
    if (!SDL_IsMainThread() || !ownerThread_.isCurrent()) {
        std::terminate();
    }
    if (!presentation_ || !candidate.valid() ||
        candidate.backendToken_ != presentation_->backendToken) {
        return;
    }
    if (presentation_->pendingGeneration != candidate.generation_ ||
        !presentation_->pending.has_value() || presentation_->pending->token != candidate.token_) {
        std::terminate();
    }
    if (presentation_->active.has_value()) {
        if (presentation_->retired.has_value()) {
            std::terminate();
        }
        presentation_->retired.emplace(std::move(*presentation_->active));
        presentation_->active.reset();
    }
    presentation_->active.emplace(std::move(*presentation_->pending));
    presentation_->pending.reset();
    presentation_->pendingGeneration = 0;
    candidate.backendToken_ = 0;
    candidate.generation_ = 0;
}

void OpenGlBackend::discardPresentation(OpenGlPresentationCandidate&& candidate) noexcept {
    if (!SDL_IsMainThread() || !ownerThread_.isCurrent()) {
        std::terminate();
    }
    if (!presentation_ || !candidate.valid() ||
        candidate.backendToken_ != presentation_->backendToken) {
        return;
    }
    if (presentation_->pendingGeneration != candidate.generation_ ||
        !presentation_->pending.has_value() || presentation_->pending->token != candidate.token_ ||
        presentation_->retired.has_value()) {
        std::terminate();
    }
    presentation_->retired.emplace(std::move(*presentation_->pending));
    presentation_->pending.reset();
    presentation_->pendingGeneration = 0;
    candidate.backendToken_ = 0;
    candidate.generation_ = 0;
}

bool OpenGlBackend::hasActivePresentation() const noexcept {
    ownerThread_.assertCurrent();
    return presentation_ && presentation_->active.has_value();
}

auto OpenGlBackend::renderPresentationFrame(const playback::FrameSnapshot& snapshot,
                                            const render::RenderScene* debugScene,
                                            OpenGlDrawSummary* summary,
                                            OpenGlPixelProbe* pixelProbe) -> core::Result<void> {
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
    presentation_->retired.reset();

    OpenGlDrawSummary preparedSummary;
    preparedSummary.viewportWidth = snapshot.viewportWidth;
    preparedSummary.viewportHeight = snapshot.viewportHeight;
    preparedSummary.clearColor = {snapshot.clearRed, snapshot.clearGreen, snapshot.clearBlue,
                                  snapshot.clearAlpha};
    preparedSummary.cameraActive = snapshot.camera.active;
    std::copy(std::begin(snapshot.camera.viewMatrix), std::end(snapshot.camera.viewMatrix),
              preparedSummary.viewMatrix.begin());
    std::copy(std::begin(snapshot.camera.projectionMatrix),
              std::end(snapshot.camera.projectionMatrix), preparedSummary.projectionMatrix.begin());
    preparedSummary.debugPassEnabled = presentation_->active->settings.debugPassEnabled;
    preparedSummary.debugCommandCount =
        preparedSummary.debugPassEnabled && debugScene != nullptr ? debugScene->size() : 0;
    if (!std::all_of(
            preparedSummary.clearColor.begin(), preparedSummary.clearColor.end(),
            [](float value) { return std::isfinite(value) && value >= 0.0F && value <= 1.0F; })) {
        return core::unexpected(core::Error{"render.opengl.invalid_frame",
                                            "Clear color must be finite and within [0, 1]"});
    }

    try {
        std::vector<PreparedDraw> opaque;
        std::vector<PreparedDraw> transparent;
        if (auto built =
                buildDraws(snapshot, *presentation_->active, preparedSummary, opaque, transparent);
            !built) {
            return core::unexpected(std::move(built.error()));
        }
        preparedSummary.digest = summaryDigest(preparedSummary);

        std::vector<DebugVertex> debugVertices;
        if (preparedSummary.debugPassEnabled) {
            if (auto collected = collectDebugVertices(debugScene, debugVertices); !collected) {
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
        drawPresentationCommands(presentation_->pipeline, viewProjection, opaque, false);
        drawPresentationCommands(presentation_->pipeline, viewProjection, transparent, true);
        if (auto checked = checkGl("render.opengl.presentation.draw_failed",
                                   "OpenGL rejected Portable Presentation drawing");
            !checked) {
            return core::unexpected(std::move(checked.error()));
        }

        OpenGlPixelProbe preparedProbe;
        preparedProbe.presentationDrawn = !opaque.empty() || !transparent.empty();
        if (pixelProbe != nullptr && width > 0 && height > 0) {
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                         preparedProbe.rgba.data());
            if (auto checked = checkGl("render.opengl.presentation.readback_failed",
                                       "OpenGL rejected presentation pixel readback");
                !checked) {
                return core::unexpected(std::move(checked.error()));
            }
        }

        if (!debugVertices.empty()) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glUseProgram(debugProgram_);
            glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, viewProjection.data());
            glBindVertexArray(debugVertexArray_);
            glBindBuffer(GL_ARRAY_BUFFER, debugVertexBuffer_);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(debugVertices.size() * sizeof(DebugVertex)),
                         debugVertices.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(debugVertices.size()));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            glUseProgram(0);
            if (auto checked =
                    checkGl("render.opengl.draw_failed", "OpenGL rejected Debug Pass drawing");
                !checked) {
                return core::unexpected(std::move(checked.error()));
            }
        }
        if (!SDL_GL_SwapWindow(nativeWindow)) {
            return core::unexpected(core::Error{"render.opengl.swap_failed", sdlError()});
        }
        if (summary != nullptr) {
            *summary = std::move(preparedSummary);
        }
        if (pixelProbe != nullptr) {
            *pixelProbe = preparedProbe;
        }
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

static_assert(std::is_nothrow_move_constructible_v<OpenGlPresentationCandidate>);
static_assert(std::is_nothrow_move_assignable_v<OpenGlPresentationCandidate>);
static_assert(std::is_nothrow_move_constructible_v<detail::PresentationResourceSet>);

} // namespace cuexis::render_opengl
