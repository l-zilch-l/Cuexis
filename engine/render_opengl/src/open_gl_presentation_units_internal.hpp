#pragma once

// The OpenGL presentation backend is split into the GL object helpers, the draw helpers, the
// backend state and the frame submission. This header names the helpers those units share.

#include "open_gl_presentation_internal.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis/playback/presentation_candidate.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::render_opengl {

constexpr std::size_t maxNormalizedRecords = 100'000;
constexpr std::uint64_t maxResourceBytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maxSessionBytes = 512ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t maxMeshVertices = 1'048'576;
constexpr std::uint32_t maxMeshIndices = 3'145'728;
constexpr std::uint32_t portableMaxTextureDimension = 8'192;
// Adapter-local transparent sort quantization: 4096 depth bins per meter. Changing this constant
// changes transparentDepthKey values and can change draw order for objects at nearby depths.
constexpr double depthQuantization = 4096.0;
constexpr double signedIntegerLimit = 0x1p63;

struct PresentationVertex final {
    float x;
    float y;
    float z;
    float u;
    float v;
};

using detail::DebugVertex;
using detail::PreparedDraw;

struct CuexisObjectStd140 final {
    float world[16]{};
    float viewProjection[16]{};
    float tint[3]{};
    float opacity{1.0F};
};

[[nodiscard]] inline auto
referenceKey(const playback::PresentationResourceRef& reference) noexcept {
    return std::tie(reference.assetId, reference.type);
}

[[nodiscard]] auto sdlError() -> std::string;

[[nodiscard]] auto requireMainThread(std::string_view operation) -> core::Result<void>;

[[nodiscard]] auto referenceKey(const playback::PresentationResourceRef& reference) noexcept;

[[nodiscard]] auto resourceError(std::string code, std::string message,
                                 const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error;

[[nodiscard]] auto frameError(std::string message, std::string_view objectId,
                              const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error;

[[nodiscard]] auto nonFiniteError(std::string_view objectId, std::string_view field) -> core::Error;

void clearGlErrors() noexcept;

[[nodiscard]] auto checkGl(std::string code, std::string message,
                           const playback::PresentationResourceRef* reference = nullptr)
    -> core::Result<void>;

[[nodiscard]] auto cacheMeshBounds(const playback::PortableResource& resource,
                                   const playback::PortableMesh& mesh, detail::GpuMesh& uploaded,
                                   detail::BoundsProbeStats* stats) -> core::Result<void>;
[[nodiscard]] auto shaderLog(GLuint shader) -> std::string;

[[nodiscard]] auto createPresentationPipeline() -> core::Result<detail::PresentationPipeline>;

[[nodiscard]] auto validationError(const playback::PresentationValidationResult& validation)
    -> core::Error;

[[nodiscard]] auto uploadMesh(const playback::PortableResource& resource)
    -> core::Result<detail::GpuMesh>;

[[nodiscard]] auto uploadTexture(const playback::PortableResource& resource)
    -> core::Result<detail::GpuTexture>;

[[nodiscard]] auto copyMaterial(const playback::PortableResource& resource)
    -> core::Result<detail::GpuMaterial>;

[[nodiscard]] auto copyParameterizedMaterial(const playback::PortableResource& resource)
    -> core::Result<detail::GpuMaterial>;

[[nodiscard]] auto linkGlsl330Program(const char* vertexSource, const char* fragmentSource,
                                      const playback::PresentationResourceRef* reference)
    -> core::Result<GLuint>;

[[nodiscard]] auto
uploadParameterizedProgram(const playback::PortableResource& shaderResource,
                           const playback::PortableParameterizedMaterial& material,
                           const std::filesystem::path& cacheDirectory, bool compileEnabled)
    -> core::Result<detail::GpuParameterizedProgram>;

[[nodiscard]] auto cacheMeshBounds(const playback::PortableResource& resource,
                                   const playback::PortableMesh& mesh, detail::GpuMesh& uploaded,
                                   detail::BoundsProbeStats* stats) -> core::Result<void>;

[[nodiscard]] auto multiplyMatrices(const std::array<float, 16>& left,
                                    const std::array<float, 16>& right) noexcept
    -> std::array<float, 16>;

[[nodiscard]] auto resolveGpuDraw(const presentation_renderer::DrawCommand& command,
                                  const detail::PresentationResourceSet& resources,
                                  PreparedDraw& draw) -> core::Result<void>;

void drawPresentationCommands(const detail::PresentationPipeline& pipeline,
                              const std::array<float, 16>& viewProjection,
                              std::span<const PreparedDraw> draws, bool transparent) noexcept;

[[nodiscard]] auto collectDebugVertices(const render::RenderScene* scene,
                                        std::vector<DebugVertex>& vertices) -> core::Result<void>;

} // namespace cuexis::render_opengl
