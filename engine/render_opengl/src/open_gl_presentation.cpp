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

auto detail::probeBuildDraws(const playback::FrameSnapshot& snapshot,
                             const playback::PresentationResourceManifest& manifest,
                             std::span<const playback::PortableResourcePtr> resources,
                             detail::BoundsProbeStats* stats)
    -> core::Result<presentation_renderer::DrawSummary> {
    detail::PresentationResourceSet set;
    set.manifest = manifest;
    set.resources.assign(resources.begin(), resources.end());
    set.meshes.reserve(resources.size());
    set.materials.reserve(resources.size());

    for (const auto& resource : resources) {
        if (!resource) {
            continue;
        }
        if (std::holds_alternative<playback::PortableMesh>(resource->value)) {
            detail::GpuMesh mesh;
            mesh.reference = resource->reference;
            const auto& portableMesh = std::get<playback::PortableMesh>(resource->value);
            mesh.indexCount = static_cast<GLsizei>(portableMesh.indices.size());
            if (auto cached = cacheMeshBounds(*resource, portableMesh, mesh, stats); !cached) {
                return core::unexpected(std::move(cached.error()));
            }
            set.meshes.push_back(std::move(mesh));
        }
        if (const auto* material = std::get_if<playback::PortableUnlitMaterial>(&resource->value);
            material != nullptr) {
            detail::GpuMaterial materialCopy;
            materialCopy.reference = resource->reference;
            materialCopy.material = *material;
            set.materials.push_back(std::move(materialCopy));
        }
    }

    const auto byReference = [](const auto& left, const auto& right) {
        return referenceKey(left.reference) < referenceKey(right.reference);
    };
    std::sort(set.meshes.begin(), set.meshes.end(), byReference);
    std::sort(set.materials.begin(), set.materials.end(), byReference);

    // Characterization seam: the real backend-neutral builder produces the summary. Only the
    // bounds-cache statistics are adapter-specific.
    return presentation_renderer::buildPresentationCommands(
        snapshot, set.resources, set.manifest.entries.empty(), false, nullptr);
}

auto builtInPresentationCapabilities(std::uint32_t maxTextureDimension, bool debugPass) noexcept
    -> playback::PresentationCapabilities {
    playback::PresentationCapabilities capabilities{
        .version = 2,
        .opaquePass = true,
        .transparentPass = true,
        .linearTexture = true,
        .srgbTexture = true,
        .straightAlphaBlend = true,
        .backFaceCulling = true,
        .doubleSided = true,
        .debugPass = debugPass,
        .maxResourceBytes = 64ULL * 1024ULL * 1024ULL,
        .maxTotalDecodedBytes = 512ULL * 1024ULL * 1024ULL,
        .maxTextureDimension = maxTextureDimension,
        .maxMeshVertices = 1'048'576,
        .maxMeshIndices = 3'145'728,
        .builtInRendererProfileVersion = 1,
        .parameterizedMaterial = true,
        .shaderGlsl330 = true,
        .declaredVariants = true,
        .maxShaderSourceBytes = playback::presentationMaxShaderSourceBytes,
        .maxSpirvBytes = playback::presentationMaxSpirvBytes,
        .maxVariantKeywords = playback::presentationMaxVariantKeywords,
        .maxVariantsPerShader = playback::presentationMaxVariantsPerShader,
        .maxMaterialParameters = playback::presentationMaxMaterialParameters,
        .maxTextureBindings = playback::presentationMaxTextureBindings,
    };
#ifdef CUEXIS_HAS_SHADER_TOOLS
    capabilities.shaderGlsl450Source = true;
    capabilities.shaderSpirv = true;
#endif
    return capabilities;
}

namespace detail {

UniqueBuffer::~UniqueBuffer() {
    if (value != 0) {
        test_only::GpuStorage::release(1, value);
        glDeleteBuffers(1, &value);
    }
}

UniqueBuffer::UniqueBuffer(UniqueBuffer&& other) noexcept : value(std::exchange(other.value, 0)) {}

auto UniqueBuffer::operator=(UniqueBuffer&& other) noexcept -> UniqueBuffer& {
    if (this != &other) {
        if (value != 0) {
            test_only::GpuStorage::release(1, value);
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
        test_only::GpuStorage::release(2, value);
        glDeleteTextures(1, &value);
    }
}

UniqueTexture::UniqueTexture(UniqueTexture&& other) noexcept
    : value(std::exchange(other.value, 0)) {}

auto UniqueTexture::operator=(UniqueTexture&& other) noexcept -> UniqueTexture& {
    if (this != &other) {
        if (value != 0) {
            test_only::GpuStorage::release(2, value);
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
    if (auto mainThread = requireMainThread("prepare_presentation"); !mainThread)
        return core::unexpected(std::move(mainThread.error()));
    if (!ownerThread_.isCurrent())
        return core::unexpected(core::Error{"render.opengl.not_main_thread",
                                            "The OpenGL backend belongs to another thread"});
    auto validation = prepared.validatePresentation(capabilities(), request);
    if (!validation.hasValue())
        return core::unexpected(validationError(validation));
    auto input = prepared.materializePresentation(capabilities(), request, identity().instance,
                                                  identity().generation, 1);
    if (!input)
        return core::unexpected(input.error());
    return preparePresentationOwned(*input);
}
auto OpenGlBackend::preparePresentationOwned(const playback::PresentationCandidateInput& input)
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
    const auto capabilities = builtInPresentationCapabilities(textureLimit, debugProgram_ != 0);
    capabilities_ = capabilities;
    capabilitiesReady_ = true;
    if (input.rendererInstance != identity().instance ||
        input.context.rendererGeneration != rendererGeneration_ ||
        input.capabilities != capabilities ||
        input.resources.size() != input.manifest.entries.size())
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Owning GPU candidate context mismatch"});
    const auto* manifest = &input.manifest;
    if (presentation_->nextGeneration == std::numeric_limits<std::uint64_t>::max()) {
        return core::unexpected(
            core::Error{"render.opengl.presentation.generation_exhausted",
                        "OpenGL presentation candidate generation is exhausted"});
    }

    try {
        detail::PresentationResourceSet candidate;
        candidate.settings = input.settings;
        candidate.manifest = *manifest;
        candidate.token = input.token;
        candidate.resources.reserve(manifest->entries.size());
        candidate.meshes.reserve(manifest->entries.size());
        candidate.textures.reserve(manifest->entries.size());
        candidate.materials.reserve(manifest->entries.size());
        for (std::size_t i = 0; i < input.resources.size(); ++i) {
            const auto& entry = manifest->entries[i];
            core::Result<playback::PortableResourcePtr> resource{input.resources[i]};
            if (!*resource)
                return core::unexpected(
                    core::Error{"host.rpa.context_stale", "Missing GPU candidate resource"});
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
            case playback::PresentationResourceType::Shader:
                break;
            case playback::PresentationResourceType::ParameterizedMaterial: {
                auto copied = copyParameterizedMaterial(**resource);
                if (!copied) {
                    return core::unexpected(std::move(copied.error()));
                }
                candidate.materials.push_back(std::move(*copied));
                break;
            }
            }
        }

        std::map<std::string, std::size_t> programIndexByKey;
        for (auto& material : candidate.materials) {
            if (!material.parameterized) {
                continue;
            }
            std::string key = material.parameterizedMaterial.shader.assetId;
            key.push_back('\0');
            key.append(reinterpret_cast<const char*>(
                           material.parameterizedMaterial.shader.identity.sha256.data()),
                       material.parameterizedMaterial.shader.identity.sha256.size());
            key.push_back('\0');
            for (const auto& keyword : material.parameterizedMaterial.selectedKeywords) {
                key.append(keyword);
                key.push_back('\0');
            }
            const auto existing = programIndexByKey.find(key);
            if (existing != programIndexByKey.end()) {
                material.programIndex = existing->second;
                const auto& cachedProgram = candidate.programs[material.programIndex];
                material.numericUniformLocations.reserve(
                    material.parameterizedMaterial.parameters.size());
                for (const auto& parameter : material.parameterizedMaterial.parameters) {
                    if (parameter.type == playback::ShaderParameterType::Texture2D) {
                        material.numericUniformLocations.push_back(-1);
                        continue;
                    }
                    const auto binding = std::find_if(
                        cachedProgram.numericUniforms.begin(), cachedProgram.numericUniforms.end(),
                        [&](const detail::GpuNumericUniformBinding& candidateBinding) {
                            return candidateBinding.name == parameter.name;
                        });
                    material.numericUniformLocations.push_back(
                        binding == cachedProgram.numericUniforms.end() ? -1 : binding->location);
                }
                continue;
            }
            const auto shaderResource = std::find_if(
                candidate.resources.begin(), candidate.resources.end(),
                [&](const playback::PortableResourcePtr& resource) {
                    return resource && resource->reference == material.parameterizedMaterial.shader;
                });
            if (shaderResource == candidate.resources.end()) {
                return core::unexpected(
                    resourceError("render.opengl.presentation.resource_mismatch",
                                  "Parameterized Material shader is not in the candidate",
                                  &material.parameterizedMaterial.shader));
            }
            auto program = uploadParameterizedProgram(
                **shaderResource, material.parameterizedMaterial,
                presentation_->shaderCacheDirectory, candidate.settings.shaderCompileEnabled);
            if (!program) {
                return core::unexpected(std::move(program.error()));
            }
            material.programIndex = candidate.programs.size();
            programIndexByKey.emplace(std::move(key), material.programIndex);
            candidate.programs.push_back(std::move(*program));
            const auto& cachedProgram = candidate.programs[material.programIndex];
            material.numericUniformLocations.reserve(
                material.parameterizedMaterial.parameters.size());
            for (const auto& parameter : material.parameterizedMaterial.parameters) {
                if (parameter.type == playback::ShaderParameterType::Texture2D) {
                    material.numericUniformLocations.push_back(-1);
                    continue;
                }
                const auto binding = std::find_if(
                    cachedProgram.numericUniforms.begin(), cachedProgram.numericUniforms.end(),
                    [&](const detail::GpuNumericUniformBinding& candidateBinding) {
                        return candidateBinding.name == parameter.name;
                    });
                material.numericUniformLocations.push_back(
                    binding == cachedProgram.numericUniforms.end() ? -1 : binding->location);
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

// Activation and discard are owner-thread, single-pending-candidate operations. Contract violations
// terminate so a stale or foreign GPU resource set cannot corrupt the active cache.
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

} // namespace cuexis::render_opengl
