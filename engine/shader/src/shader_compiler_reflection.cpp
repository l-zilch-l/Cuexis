#include <cuexis/shader/shader_compiler.hpp>

#include "shader_compiler_units_internal.hpp"

#include <cuexis/core/error.hpp>

#include <shaderc/shaderc.h>
#include <spirv-tools/libspirv.h>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <compare>
#include <cstdint>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::shader {

[[nodiscard]] auto spirTypeMatches(const spirv_cross::SPIRType& type, ShaderParameterType expected)
    -> bool {
    if (!type.array.empty()) {
        return false;
    }
    switch (expected) {
    case ShaderParameterType::Float:
        return type.basetype == spirv_cross::SPIRType::Float && type.vecsize == 1 &&
               type.columns == 1;
    case ShaderParameterType::Vec2:
        return type.basetype == spirv_cross::SPIRType::Float && type.vecsize == 2 &&
               type.columns == 1;
    case ShaderParameterType::Vec3:
        return type.basetype == spirv_cross::SPIRType::Float && type.vecsize == 3 &&
               type.columns == 1;
    case ShaderParameterType::Vec4:
        return type.basetype == spirv_cross::SPIRType::Float && type.vecsize == 4 &&
               type.columns == 1;
    case ShaderParameterType::Int:
        return type.basetype == spirv_cross::SPIRType::Int && type.vecsize == 1 &&
               type.columns == 1 && type.width == 32;
    case ShaderParameterType::Bool:
        return type.basetype == spirv_cross::SPIRType::Boolean && type.vecsize == 1 &&
               type.columns == 1;
    case ShaderParameterType::Texture2D:
        return (type.basetype == spirv_cross::SPIRType::SampledImage ||
                type.basetype == spirv_cross::SPIRType::Image) &&
               type.image.dim == spv::Dim2D && !type.image.arrayed && !type.image.ms;
    }
    return false;
}

[[nodiscard]] auto isMat4(const spirv_cross::SPIRType& type) noexcept -> bool {
    return type.basetype == spirv_cross::SPIRType::Float && type.vecsize == 4 &&
           type.columns == 4 && type.array.empty();
}

[[nodiscard]] auto resourceName(const spirv_cross::Compiler& compiler,
                                const spirv_cross::Resource& resource) -> std::string {
    if (!resource.name.empty()) {
        return resource.name;
    }
    const auto& blockName = compiler.get_name(resource.base_type_id);
    if (!blockName.empty()) {
        return blockName;
    }
    return compiler.get_fallback_name(resource.id);
}

[[nodiscard]] auto validateCuexisObject(const spirv_cross::Compiler& compiler,
                                        const spirv_cross::Resource& resource,
                                        std::string_view stage) -> core::Result<void> {
    const auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
    const auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
    if (resourceName(compiler, resource) != cuexisObjectName || set != reservedSet ||
        binding != reservedBinding) {
        return core::unexpected(makeError(diagnosticReflectMismatch,
                                          "Reserved CuexisObject must occupy set 0 binding 0",
                                          toolSpirvCross, stage));
    }

    const auto& blockType = compiler.get_type(resource.base_type_id);
    if (blockType.member_types.size() != 4) {
        return core::unexpected(makeError(diagnosticReflectMismatch,
                                          "CuexisObject must declare four members", toolSpirvCross,
                                          stage));
    }

    const std::array<std::string_view, 4> names{"world", "viewProjection", "tint", "opacity"};
    for (std::uint32_t index = 0; index < 4; ++index) {
        if (compiler.get_member_name(resource.base_type_id, index) != names[index]) {
            return core::unexpected(makeError(diagnosticReflectMismatch,
                                              "CuexisObject member names do not match the contract",
                                              toolSpirvCross, stage));
        }
        const auto& member = compiler.get_type(blockType.member_types[index]);
        const bool ok = index < 2
                            ? isMat4(member)
                            : (index == 2 ? spirTypeMatches(member, ShaderParameterType::Vec3)
                                          : spirTypeMatches(member, ShaderParameterType::Float));
        if (!ok) {
            return core::unexpected(makeError(diagnosticReflectMismatch,
                                              "CuexisObject member types do not match the contract",
                                              toolSpirvCross, stage));
        }
    }
    return {};
}

[[nodiscard]] auto userTypeFromResource(const spirv_cross::Compiler& compiler,
                                        const spirv_cross::Resource& resource, bool sampledImage)
    -> core::Result<ShaderParameterType> {
    if (sampledImage) {
        const auto& type = compiler.get_type(resource.type_id);
        if (!spirTypeMatches(type, ShaderParameterType::Texture2D)) {
            return core::unexpected(makeError(diagnosticSubsetInvalid,
                                              "Only sampler2D texture parameters are supported",
                                              toolSpirvCross));
        }
        return ShaderParameterType::Texture2D;
    }

    const auto& blockType = compiler.get_type(resource.base_type_id);
    if (blockType.member_types.size() != 1) {
        return core::unexpected(makeError(
            diagnosticReflectMismatch,
            "User uniform blocks must contain exactly one parameter member", toolSpirvCross));
    }
    const auto& member = compiler.get_type(blockType.member_types[0]);
    static constexpr std::array<ShaderParameterType, 6> numeric{
        ShaderParameterType::Float, ShaderParameterType::Vec2, ShaderParameterType::Vec3,
        ShaderParameterType::Vec4,  ShaderParameterType::Int,  ShaderParameterType::Bool,
    };
    for (const auto candidate : numeric) {
        if (spirTypeMatches(member, candidate)) {
            return candidate;
        }
    }
    return core::unexpected(makeError(diagnosticReflectMismatch,
                                      "User uniform block member type is not a v1 parameter type",
                                      toolSpirvCross));
}

[[nodiscard]] auto rejectUnsupportedResources(const spirv_cross::ShaderResources& resources,
                                              std::string_view stage) -> core::Result<void> {
    if (!resources.storage_buffers.empty() || !resources.storage_images.empty() ||
        !resources.subpass_inputs.empty() || !resources.atomic_counters.empty() ||
        !resources.acceleration_structures.empty() || !resources.push_constant_buffers.empty() ||
        !resources.separate_images.empty() || !resources.separate_samplers.empty() ||
        !resources.gl_plain_uniforms.empty()) {
        return core::unexpected(
            makeError(diagnosticSubsetInvalid,
                      "Shader uses a resource kind outside the portable GLSL 450 subset",
                      toolSpirvCross, stage));
    }
    return {};
}

[[nodiscard]] auto collectStageResources(const std::vector<std::byte>& spirv,
                                         std::string_view stage) -> core::Result<StageResources> {
    try {
        const auto words = asSpirvWords(spirv);
        const spirv_cross::Compiler compiler{words};
        const auto resources = compiler.get_shader_resources();
        if (const auto rejected = rejectUnsupportedResources(resources, stage); !rejected) {
            return core::unexpected(rejected.error());
        }

        StageResources collected{};
        for (const auto& block : resources.uniform_buffers) {
            const auto set = compiler.get_decoration(block.id, spv::DecorationDescriptorSet);
            const auto binding = compiler.get_decoration(block.id, spv::DecorationBinding);
            if (resourceName(compiler, block) == cuexisObjectName ||
                (set == reservedSet && binding == reservedBinding)) {
                if (const auto object = validateCuexisObject(compiler, block, stage); !object) {
                    return core::unexpected(object.error());
                }
                if (collected.hasCuexisObject) {
                    return core::unexpected(makeError(diagnosticReflectMismatch,
                                                      "CuexisObject is declared more than once",
                                                      toolSpirvCross, stage));
                }
                collected.hasCuexisObject = true;
                continue;
            }

            if (set == reservedSet && binding == reservedBinding) {
                return core::unexpected(makeError(diagnosticReservedBinding,
                                                  "User resource occupies set 0 binding 0",
                                                  toolSpirvCross, stage));
            }

            const auto type = userTypeFromResource(compiler, block, false);
            if (!type) {
                auto error = type.error();
                error.withContext(std::string{contextStage}, std::string{stage});
                return core::unexpected(std::move(error));
            }

            const BindingKey key{set, binding};
            DeclaredResource resource{
                .name = resourceName(compiler, block),
                .type = *type,
                .set = set,
                .binding = binding,
            };
            if (!collected.userResources.emplace(key, resource).second) {
                return core::unexpected(makeError(diagnosticReflectMismatch,
                                                  "SPIR-V binding slot is duplicated",
                                                  toolSpirvCross, stage));
            }
        }

        for (const auto& image : resources.sampled_images) {
            const auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
            const auto binding = compiler.get_decoration(image.id, spv::DecorationBinding);
            if (set == reservedSet && binding == reservedBinding) {
                return core::unexpected(makeError(diagnosticReservedBinding,
                                                  "User resource occupies set 0 binding 0",
                                                  toolSpirvCross, stage));
            }
            const auto type = userTypeFromResource(compiler, image, true);
            if (!type) {
                auto error = type.error();
                error.withContext(std::string{contextStage}, std::string{stage});
                return core::unexpected(std::move(error));
            }
            const BindingKey key{set, binding};
            DeclaredResource resource{
                .name = resourceName(compiler, image),
                .type = *type,
                .set = set,
                .binding = binding,
            };
            if (!collected.userResources.emplace(key, resource).second) {
                return core::unexpected(makeError(diagnosticReflectMismatch,
                                                  "SPIR-V binding slot is duplicated",
                                                  toolSpirvCross, stage));
            }
        }

        if (!collected.hasCuexisObject) {
            return core::unexpected(makeError(diagnosticReflectMismatch,
                                              "SPIR-V is missing the reserved CuexisObject block",
                                              toolSpirvCross, stage));
        }
        return collected;
    } catch (const std::exception& exception) {
        return core::unexpected(
            makeError(diagnosticCompileFailed, exception.what(), toolSpirvCross, stage));
    }
}

[[nodiscard]] auto mergeStageResources(const StageResources& vertex, const StageResources& fragment)
    -> core::Result<std::map<BindingKey, DeclaredResource>> {
    std::map<BindingKey, DeclaredResource> merged = vertex.userResources;
    for (const auto& [key, resource] : fragment.userResources) {
        const auto found = merged.find(key);
        if (found == merged.end()) {
            merged.emplace(key, resource);
            continue;
        }
        if (found->second.name != resource.name || found->second.type != resource.type) {
            return core::unexpected(makeError(
                diagnosticReflectMismatch,
                "Vertex and fragment resources conflict at the same set/binding", toolSpirvCross));
        }
    }
    return merged;
}

[[nodiscard]] auto matchDeclaredBindings(const ShaderCompileRequest& request,
                                         const std::map<BindingKey, DeclaredResource>& reflected)
    -> core::Result<void> {
    std::map<BindingKey, DeclaredResource> declared;
    for (const auto& binding : request.declaredBindings) {
        declared.emplace(BindingKey{binding.set, binding.binding},
                         DeclaredResource{
                             .name = std::string{binding.name},
                             .type = binding.type,
                             .set = binding.set,
                             .binding = binding.binding,
                         });
    }

    if (declared.size() != reflected.size()) {
        return core::unexpected(
            makeError(diagnosticReflectMismatch,
                      "SPIR-V resources do not match the declared binding table", toolSpirvCross));
    }
    for (const auto& [key, resource] : reflected) {
        const auto found = declared.find(key);
        if (found == declared.end() || found->second.name != resource.name ||
            found->second.type != resource.type) {
            return core::unexpected(
                makeError(diagnosticReflectMismatch,
                          "SPIR-V resource name or type does not match the declared schema",
                          toolSpirvCross)
                    .withContext("name", resource.name));
        }
    }
    return {};
}

[[nodiscard]] auto crossCompileGlsl(const std::vector<std::byte>& spirv, unsigned version, bool es,
                                    std::string_view stage) -> core::Result<std::string> {
    try {
        const auto words = asSpirvWords(spirv);
        spirv_cross::CompilerGLSL compiler{words};
        auto options = compiler.get_common_options();
        options.version = version;
        options.es = es;
        options.vulkan_semantics = false;
        options.enable_420pack_extension = false;
        options.emit_line_directives = false;
        options.fragment.default_float_precision =
            spirv_cross::CompilerGLSL::Options::Precision::Highp;
        options.fragment.default_int_precision =
            spirv_cross::CompilerGLSL::Options::Precision::Highp;
        compiler.set_common_options(options);
        compiler.build_combined_image_samplers();
        auto source = compiler.compile();
        if (source.empty()) {
            return core::unexpected(makeError(
                diagnosticCompileFailed, "SPIRV-Cross produced empty GLSL", toolSpirvCross, stage));
        }
        return source;
    } catch (const std::exception& exception) {
        return core::unexpected(
            makeError(diagnosticCompileFailed, exception.what(), toolSpirvCross, stage));
    }
}

} // namespace cuexis::shader
