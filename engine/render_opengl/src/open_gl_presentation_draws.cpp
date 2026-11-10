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

#ifdef CUEXIS_HAS_SHADER_TOOLS
[[nodiscard]] auto compileParameterizedArtifact(const playback::PortableShader& shader,
                                                std::span<const std::string> selectedKeywords)
    -> core::Result<shader::ShaderCompileArtifact> {
    std::vector<std::string_view> declaredKeywords;
    declaredKeywords.reserve(shader.variantKeywords.size());
    for (const auto& keyword : shader.variantKeywords) {
        declaredKeywords.emplace_back(keyword);
    }
    std::vector<std::string_view> selectedViews;
    selectedViews.reserve(selectedKeywords.size());
    for (const auto& keyword : selectedKeywords) {
        selectedViews.emplace_back(keyword);
    }
    std::vector<shader::ShaderDeclaredBinding> bindings;
    bindings.reserve(shader.bindings.size());
    for (const auto& binding : shader.bindings) {
        bindings.push_back(shader::ShaderDeclaredBinding{
            .set = binding.set,
            .binding = binding.binding,
            .type = static_cast<shader::ShaderParameterType>(binding.type),
            .name = binding.name,
        });
    }
    std::vector<shader::ShaderDeclaredParameter> parameters;
    parameters.reserve(shader.parameters.size());
    for (const auto& parameter : shader.parameters) {
        parameters.push_back(shader::ShaderDeclaredParameter{
            .name = parameter.name,
            .type = static_cast<shader::ShaderParameterType>(parameter.type),
            .set = parameter.set,
            .binding = parameter.binding,
        });
    }
    return shader::ShaderCompiler::compile(shader::ShaderCompileRequest{
        .vertexSource = shader.vertexSource,
        .fragmentSource = shader.fragmentSource,
        .vertexEntry = shader.vertexEntry,
        .fragmentEntry = shader.fragmentEntry,
        .declaredKeywords = declaredKeywords,
        .selectedKeywords = selectedViews,
        .declaredBindings = bindings,
        .declaredParameters = parameters,
    });
}
#endif

[[nodiscard]] auto
uploadParameterizedProgram(const playback::PortableResource& shaderResource,
                           const playback::PortableParameterizedMaterial& material,
                           const std::filesystem::path& cacheDirectory, bool compileEnabled)
    -> core::Result<detail::GpuParameterizedProgram> {
    const auto* shader = std::get_if<playback::PortableShader>(&shaderResource.value);
    if (shader == nullptr) {
        return core::unexpected(resourceError("render.opengl.presentation.resource_type_invalid",
                                              "Shader resource has an incompatible portable value",
                                              &shaderResource.reference));
    }

    std::vector<std::string_view> selectedViews;
    selectedViews.reserve(material.selectedKeywords.size());
    for (const auto& keyword : material.selectedKeywords) {
        selectedViews.emplace_back(keyword);
    }
    const auto targets = shader::defaultTargetProfiles();
    shader::ShaderCacheKeyInput keyInput{
        .sourceIdentity = shaderResource.reference.identity.sha256,
        .targetProfiles = targets,
        .selectedKeywords = selectedViews,
        .vertexEntry = shader->vertexEntry,
        .fragmentEntry = shader->fragmentEntry,
    };

#ifdef CUEXIS_HAS_SHADER_TOOLS
    std::optional<shader::ShaderPipelineCache> pipeline;
    std::optional<shader::ShaderCacheRecord> ownedRecord;
    const shader::ShaderCacheRecord* record = nullptr;
    if (!cacheDirectory.empty()) {
        std::vector<std::string_view> declaredKeywords;
        declaredKeywords.reserve(shader->variantKeywords.size());
        for (const auto& keyword : shader->variantKeywords) {
            declaredKeywords.emplace_back(keyword);
        }
        std::vector<shader::ShaderDeclaredBinding> declaredBindings;
        declaredBindings.reserve(shader->bindings.size());
        for (const auto& binding : shader->bindings) {
            declaredBindings.push_back(shader::ShaderDeclaredBinding{
                .set = binding.set,
                .binding = binding.binding,
                .type = static_cast<shader::ShaderParameterType>(binding.type),
                .name = binding.name,
            });
        }
        std::vector<shader::ShaderDeclaredParameter> parameters;
        parameters.reserve(shader->parameters.size());
        for (const auto& parameter : shader->parameters) {
            parameters.push_back(shader::ShaderDeclaredParameter{
                .name = parameter.name,
                .type = static_cast<shader::ShaderParameterType>(parameter.type),
                .set = parameter.set,
                .binding = parameter.binding,
            });
        }
        const shader::ShaderCompileRequest compileRequest{
            .vertexSource = shader->vertexSource,
            .fragmentSource = shader->fragmentSource,
            .vertexEntry = shader->vertexEntry,
            .fragmentEntry = shader->fragmentEntry,
            .declaredKeywords = declaredKeywords,
            .selectedKeywords = selectedViews,
            .declaredBindings = declaredBindings,
            .declaredParameters = parameters,
        };
        pipeline.emplace(cacheDirectory);
        auto prepared = pipeline->prepareCandidate(compileRequest, keyInput, compileEnabled);
        if (!prepared) {
            auto error = std::move(prepared.error());
            error.withContext("asset_id", shaderResource.reference.assetId)
                .withContext("resource_type", "shader");
            return core::unexpected(std::move(error));
        }
        record = pipeline->candidate();
    } else if (compileEnabled) {
        auto compiled = compileParameterizedArtifact(*shader, material.selectedKeywords);
        if (!compiled) {
            return core::unexpected(std::move(compiled.error()));
        }
        ownedRecord.emplace();
        ownedRecord->sourceIdentity = shaderResource.reference.identity.sha256;
        ownedRecord->vertexEntry = shader->vertexEntry;
        ownedRecord->fragmentEntry = shader->fragmentEntry;
        ownedRecord->selectedKeywords = material.selectedKeywords;
        ownedRecord->artifact = std::move(*compiled);
        record = &*ownedRecord;
    }
    if (record == nullptr) {
        return core::unexpected(resourceError(
            std::string{shader::diagnosticCacheMissing},
            "Parameterized OpenGL prepare requires a CXSCCH01 cache or opt-in compile",
            &shaderResource.reference));
    }
#else
    std::optional<shader::ShaderCacheRecord> ownedRecord;
    if (!cacheDirectory.empty()) {
        shader::ShaderCacheStore store{cacheDirectory};
        auto loaded = store.load(keyInput);
        if (!loaded) {
            return core::unexpected(std::move(loaded.error()));
        }
        ownedRecord = std::move(*loaded);
    }
    if (!ownedRecord) {
        (void)compileEnabled;
        return core::unexpected(resourceError(
            std::string{shader::diagnosticCacheMissing},
            "Parameterized OpenGL prepare requires a CXSCCH01 cache", &shaderResource.reference));
    }
    const auto* record = &*ownedRecord;
#endif

    auto linked =
        linkGlsl330Program(record->artifact.vertexGlsl330.c_str(),
                           record->artifact.fragmentGlsl330.c_str(), &shaderResource.reference);
    if (!linked) {
        return core::unexpected(std::move(linked.error()));
    }

    detail::GpuParameterizedProgram uploaded;
    uploaded.shaderReference = shaderResource.reference;
    uploaded.selectedKeywords = material.selectedKeywords;
    uploaded.program = detail::UniqueProgram{*linked};
    uploaded.reflection = record->artifact.reflection;

    const GLuint blockIndex = glGetUniformBlockIndex(*linked, "CuexisObject");
    if (blockIndex == GL_INVALID_INDEX) {
        return core::unexpected(resourceError("render.opengl.presentation.uniform_missing",
                                              "Parameterized program is missing CuexisObject",
                                              &shaderResource.reference));
    }
    glUniformBlockBinding(*linked, blockIndex, 0);

    GLuint ubo = 0;
    glGenBuffers(1, &ubo);
    uploaded.cuexisObject = detail::UniqueBuffer{ubo};
    if (ubo == 0) {
        return core::unexpected(resourceError("render.opengl.presentation.ubo_create_failed",
                                              "OpenGL could not create the CuexisObject UBO",
                                              &shaderResource.reference));
    }
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    test_only::GpuStorage::charge(1, ubo, sizeof(CuexisObjectStd140));
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(sizeof(CuexisObjectStd140)), nullptr,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    uploaded.numericUniforms.reserve(uploaded.reflection.parameters.size());
    for (const auto& parameter : uploaded.reflection.parameters) {
        if (parameter.type == shader::ShaderParameterType::Texture2D) {
            continue;
        }
        detail::GpuNumericUniformBinding mapped;
        mapped.name = parameter.name;
        mapped.location = glGetUniformLocation(*linked, parameter.name.c_str());
        uploaded.numericUniforms.push_back(std::move(mapped));
    }

    auto reflectionBindings = record->artifact.reflection.bindings;
    std::sort(reflectionBindings.begin(), reflectionBindings.end(),
              [](const auto& left, const auto& right) {
                  return std::tie(left.set, left.binding, left.name) <
                         std::tie(right.set, right.binding, right.name);
              });
    GLint textureUnit = 0;
    uploaded.textures.reserve(reflectionBindings.size());
    glUseProgram(*linked);
    for (const auto& binding : reflectionBindings) {
        if (binding.set == 0 && binding.binding == 0) {
            continue;
        }
        if (binding.type != shader::ShaderParameterType::Texture2D) {
            continue;
        }
        if (uploaded.textures.size() >= playback::presentationMaxTextureBindings) {
            glUseProgram(0);
            return core::unexpected(
                resourceError("render.opengl.presentation.texture_binding_limit",
                              "Parameterized program exceeds the Built-in texture binding limit",
                              &shaderResource.reference));
        }
        detail::GpuTextureBinding mapped;
        mapped.set = binding.set;
        mapped.binding = binding.binding;
        mapped.name = binding.name;
        mapped.textureUnit = textureUnit;
        mapped.location = glGetUniformLocation(*linked, binding.name.c_str());
        if (mapped.location >= 0) {
            glUniform1i(mapped.location, textureUnit);
        }
        uploaded.textures.push_back(std::move(mapped));
        ++textureUnit;
    }
    glUseProgram(0);
    if (auto checked =
            checkGl("render.opengl.presentation.shader_upload_failed",
                    "OpenGL rejected parameterized program setup", &shaderResource.reference);
        !checked) {
        return core::unexpected(std::move(checked.error()));
    }
    return uploaded;
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

[[nodiscard]] auto cacheMeshBounds(const playback::PortableResource& resource,
                                   const playback::PortableMesh& mesh, detail::GpuMesh& uploaded,
                                   detail::BoundsProbeStats* stats) -> core::Result<void> {
    if (stats != nullptr) {
        ++stats->meshBoundsParses;
    }
    for (std::size_t component = 0; component < 3; ++component) {
        if (!std::isfinite(mesh.boundsMin[component]) ||
            !std::isfinite(mesh.boundsMax[component])) {
            return core::unexpected(nonFiniteError({}, "mesh_bounds")
                                        .withContext("asset_id", resource.reference.assetId));
        }
        uploaded.boundsMin[component] = mesh.boundsMin[component];
        uploaded.boundsMax[component] = mesh.boundsMax[component];
    }
    uploaded.boundsCenter[0] =
        (static_cast<double>(mesh.boundsMin[0]) + static_cast<double>(mesh.boundsMax[0])) / 2.0;
    uploaded.boundsCenter[1] =
        (static_cast<double>(mesh.boundsMin[1]) + static_cast<double>(mesh.boundsMax[1])) / 2.0;
    uploaded.boundsCenter[2] =
        (static_cast<double>(mesh.boundsMin[2]) + static_cast<double>(mesh.boundsMax[2])) / 2.0;
    if (!std::all_of(uploaded.boundsCenter.begin(), uploaded.boundsCenter.end(),
                     [](double value) { return std::isfinite(value); })) {
        return core::unexpected(
            nonFiniteError({}, "mesh_bounds").withContext("asset_id", resource.reference.assetId));
    }
    return {};
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

// Resolves the GL handles a neutral draw command needs. The neutral builder already validated that
// every reference is backed by the active set, so a miss here is an internal inconsistency rather
// than a caller error.
[[nodiscard]] auto resolveGpuDraw(const presentation_renderer::DrawCommand& command,
                                  const detail::PresentationResourceSet& resources,
                                  PreparedDraw& draw) -> core::Result<void> {
    draw.command = &command;
    draw.mesh = findGpuResource(resources.meshes, command.mesh);
    draw.material = findGpuResource(resources.materials, command.material);
    if (draw.mesh == nullptr || draw.material == nullptr) {
        return core::unexpected(
            frameError("Snapshot ref is not backed by the active OpenGL cache", command.objectId,
                       draw.mesh == nullptr ? &command.mesh : &command.material));
    }
    draw.texture = nullptr;
    draw.program = nullptr;
    draw.parameterizedTextures = {};
    if (draw.material->parameterized) {
        if (draw.material->programIndex >= resources.programs.size()) {
            return core::unexpected(
                frameError("Parameterized material is missing a compiled program", command.objectId,
                           &command.material));
        }
        draw.program = &resources.programs[draw.material->programIndex];
        for (std::size_t index = 0; index < draw.program->textures.size(); ++index) {
            const auto& binding = draw.program->textures[index];
            const auto parameter =
                std::find_if(draw.material->parameterizedMaterial.parameters.begin(),
                             draw.material->parameterizedMaterial.parameters.end(),
                             [&](const playback::ShaderParameterValue& candidate) {
                                 return candidate.name == binding.name &&
                                        candidate.type == playback::ShaderParameterType::Texture2D;
                             });
            if (parameter == draw.material->parameterizedMaterial.parameters.end() ||
                !parameter->texture) {
                return core::unexpected(
                    frameError("Parameterized texture binding is not supplied by the material",
                               command.objectId, &command.material));
            }
            const auto* bound = findGpuResource(resources.textures, *parameter->texture);
            if (bound == nullptr) {
                return core::unexpected(
                    frameError("Material texture ref is not backed by the active OpenGL cache",
                               command.objectId, &*parameter->texture));
            }
            draw.parameterizedTextures[index] = bound;
        }
    } else if (draw.material->material.baseColorTexture) {
        draw.texture =
            findGpuResource(resources.textures, *draw.material->material.baseColorTexture);
        if (draw.texture == nullptr) {
            return core::unexpected(
                frameError("Material texture ref is not backed by the active OpenGL cache",
                           command.objectId, &*draw.material->material.baseColorTexture));
        }
    }
    return {};
}

void applyPassState(bool transparent) noexcept {
    if (transparent) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    } else {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

void drawUnlitCommands(const detail::PresentationPipeline& pipeline,
                       const std::array<float, 16>& viewProjection,
                       std::span<const PreparedDraw> draws) noexcept {
    glUseProgram(pipeline.program.value);
    glUniformMatrix4fv(pipeline.viewProjectionLocation, 1, GL_FALSE, viewProjection.data());
    glUniform1i(pipeline.textureLocation, 0);
    for (const auto& draw : draws) {
        if (draw.program != nullptr) {
            continue;
        }
        if (draw.command->backFaceCulling) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        glUniformMatrix4fv(pipeline.worldLocation, 1, GL_FALSE, draw.command->worldMatrix.data());
        glUniform4f(pipeline.effectiveColorLocation,
                    static_cast<float>(draw.command->effectiveColor[0]),
                    static_cast<float>(draw.command->effectiveColor[1]),
                    static_cast<float>(draw.command->effectiveColor[2]),
                    static_cast<float>(draw.command->effectiveColor[3]));
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
}

void setNumericUniform(GLint location, const playback::ShaderParameterValue& parameter) noexcept {
    if (location < 0) {
        return;
    }
    switch (parameter.type) {
    case playback::ShaderParameterType::Float:
        glUniform1f(location, parameter.numeric[0]);
        break;
    case playback::ShaderParameterType::Vec2:
        glUniform2f(location, parameter.numeric[0], parameter.numeric[1]);
        break;
    case playback::ShaderParameterType::Vec3:
        glUniform3f(location, parameter.numeric[0], parameter.numeric[1], parameter.numeric[2]);
        break;
    case playback::ShaderParameterType::Vec4:
        glUniform4f(location, parameter.numeric[0], parameter.numeric[1], parameter.numeric[2],
                    parameter.numeric[3]);
        break;
    case playback::ShaderParameterType::Int:
        glUniform1i(location, parameter.integer);
        break;
    case playback::ShaderParameterType::Bool:
        glUniform1i(location, parameter.boolean ? 1 : 0);
        break;
    case playback::ShaderParameterType::Texture2D:
        break;
    }
}

void drawParameterizedCommands(const std::array<float, 16>& viewProjection,
                               std::span<const PreparedDraw> draws) noexcept {
    for (const auto& draw : draws) {
        if (draw.program == nullptr) {
            continue;
        }
        if (draw.command->backFaceCulling) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        glUseProgram(draw.program->program.value);
        CuexisObjectStd140 object{};
        std::memcpy(object.world, draw.command->worldMatrix.data(), sizeof(object.world));
        std::memcpy(object.viewProjection, viewProjection.data(), sizeof(object.viewProjection));
        object.tint[0] = static_cast<float>(draw.command->effectiveColor[0]);
        object.tint[1] = static_cast<float>(draw.command->effectiveColor[1]);
        object.tint[2] = static_cast<float>(draw.command->effectiveColor[2]);
        object.opacity = static_cast<float>(draw.command->effectiveColor[3]);
        glBindBuffer(GL_UNIFORM_BUFFER, draw.program->cuexisObject.value);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(object)), &object);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, draw.program->cuexisObject.value);
        for (std::size_t parameterIndex = 0;
             parameterIndex < draw.material->parameterizedMaterial.parameters.size();
             ++parameterIndex) {
            const auto& parameter = draw.material->parameterizedMaterial.parameters[parameterIndex];
            if (parameter.type == playback::ShaderParameterType::Texture2D) {
                continue;
            }
            if (parameterIndex < draw.material->numericUniformLocations.size()) {
                setNumericUniform(draw.material->numericUniformLocations[parameterIndex],
                                  parameter);
            }
        }
        for (std::size_t index = 0; index < draw.program->textures.size(); ++index) {
            const auto& binding = draw.program->textures[index];
            const auto* texture = draw.parameterizedTextures[index];
            glActiveTexture(GL_TEXTURE0 + binding.textureUnit);
            glBindTexture(GL_TEXTURE_2D, texture != nullptr ? texture->texture.value : 0);
            if (binding.location >= 0) {
                glUniform1i(binding.location, binding.textureUnit);
            }
        }
        glBindVertexArray(draw.mesh->vertexArray.value);
        glDrawElements(GL_TRIANGLES, draw.mesh->indexCount, GL_UNSIGNED_INT, nullptr);
    }
}

void drawPresentationCommands(const detail::PresentationPipeline& pipeline,
                              const std::array<float, 16>& viewProjection,
                              std::span<const PreparedDraw> draws, bool transparent) noexcept {
    if (draws.empty()) {
        return;
    }
    applyPassState(transparent);
    drawUnlitCommands(pipeline, viewProjection, draws);
    drawParameterizedCommands(viewProjection, draws);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
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

} // namespace cuexis::render_opengl
