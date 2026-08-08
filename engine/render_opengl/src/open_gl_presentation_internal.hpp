#pragma once

#include <cuexis/render_opengl/open_gl_backend.hpp>

#include <glad/glad.h>

#include <optional>
#include <vector>

namespace cuexis::render_opengl::detail {

struct UniqueBuffer final {
    GLuint value{};

    UniqueBuffer() = default;
    explicit UniqueBuffer(GLuint handle) noexcept : value(handle) {}
    ~UniqueBuffer();
    UniqueBuffer(const UniqueBuffer&) = delete;
    auto operator=(const UniqueBuffer&) -> UniqueBuffer& = delete;
    UniqueBuffer(UniqueBuffer&& other) noexcept;
    auto operator=(UniqueBuffer&& other) noexcept -> UniqueBuffer&;
};

struct UniqueVertexArray final {
    GLuint value{};

    UniqueVertexArray() = default;
    explicit UniqueVertexArray(GLuint handle) noexcept : value(handle) {}
    ~UniqueVertexArray();
    UniqueVertexArray(const UniqueVertexArray&) = delete;
    auto operator=(const UniqueVertexArray&) -> UniqueVertexArray& = delete;
    UniqueVertexArray(UniqueVertexArray&& other) noexcept;
    auto operator=(UniqueVertexArray&& other) noexcept -> UniqueVertexArray&;
};

struct UniqueTexture final {
    GLuint value{};

    UniqueTexture() = default;
    explicit UniqueTexture(GLuint handle) noexcept : value(handle) {}
    ~UniqueTexture();
    UniqueTexture(const UniqueTexture&) = delete;
    auto operator=(const UniqueTexture&) -> UniqueTexture& = delete;
    UniqueTexture(UniqueTexture&& other) noexcept;
    auto operator=(UniqueTexture&& other) noexcept -> UniqueTexture&;
};

struct UniqueProgram final {
    GLuint value{};

    UniqueProgram() = default;
    explicit UniqueProgram(GLuint handle) noexcept : value(handle) {}
    ~UniqueProgram();
    UniqueProgram(const UniqueProgram&) = delete;
    auto operator=(const UniqueProgram&) -> UniqueProgram& = delete;
    UniqueProgram(UniqueProgram&& other) noexcept;
    auto operator=(UniqueProgram&& other) noexcept -> UniqueProgram&;
};

struct PresentationPipeline final {
    UniqueProgram program;
    GLint worldLocation{-1};
    GLint viewProjectionLocation{-1};
    GLint effectiveColorLocation{-1};
    GLint useTextureLocation{-1};
    GLint useTextureAlphaLocation{-1};
    GLint textureLocation{-1};
};

struct GpuMesh final {
    playback::PresentationResourceRef reference;
    UniqueVertexArray vertexArray;
    UniqueBuffer vertexBuffer;
    UniqueBuffer indexBuffer;
    GLsizei indexCount{};
};

struct GpuTexture final {
    playback::PresentationResourceRef reference;
    UniqueTexture texture;
};

struct GpuMaterial final {
    playback::PresentationResourceRef reference;
    playback::PortableUnlitMaterial material;
};

struct PresentationResourceSet final {
    playback::PresentationCandidateToken token;
    playback::PresentationResourceManifest manifest;
    std::vector<playback::PortableResourcePtr> resources;
    playback::EffectivePresentationSettings settings;
    std::vector<GpuMesh> meshes;
    std::vector<GpuTexture> textures;
    std::vector<GpuMaterial> materials;
};

struct OpenGlPresentationBackendState final {
    PresentationPipeline pipeline;
    std::optional<PresentationResourceSet> active;
    std::optional<PresentationResourceSet> pending;
    std::optional<PresentationResourceSet> retired;
    std::uint64_t nextGeneration{};
    std::uint64_t pendingGeneration{};
};

[[nodiscard]] auto createPresentationBackendState()
    -> core::Result<std::unique_ptr<OpenGlPresentationBackendState>>;

} // namespace cuexis::render_opengl::detail
