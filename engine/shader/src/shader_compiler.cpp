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

auto ShaderCompiler::compile(const ShaderCompileRequest& request)
    -> core::Result<ShaderCompileArtifact> {
    if (const auto vertex = validateSource(request.vertexSource, vertexStageName); !vertex) {
        return core::unexpected(vertex.error());
    }
    if (const auto fragment = validateSource(request.fragmentSource, fragmentStageName);
        !fragment) {
        return core::unexpected(fragment.error());
    }
    if (const auto keywords = validateKeywords(request); !keywords) {
        return core::unexpected(keywords.error());
    }
    const auto declaredParameters = validateDeclaredSchema(request);
    if (!declaredParameters) {
        return core::unexpected(declaredParameters.error());
    }

    ShadercCompilerPtr compiler{shaderc_compiler_initialize()};
    if (!compiler) {
        return core::unexpected(makeError(diagnosticCompileFailed,
                                          "shaderc compiler failed to initialize", toolShaderc));
    }
    const auto options = makeCompileOptions(request);
    if (!options) {
        return core::unexpected(options.error());
    }

    auto vertexSpirv =
        compileStage(compiler.get(), options->get(), request.vertexSource, request.vertexEntry,
                     shaderc_vertex_shader, vertexFileName, vertexStageName);
    if (!vertexSpirv) {
        return core::unexpected(vertexSpirv.error());
    }
    auto fragmentSpirv =
        compileStage(compiler.get(), options->get(), request.fragmentSource, request.fragmentEntry,
                     shaderc_fragment_shader, fragmentFileName, fragmentStageName);
    if (!fragmentSpirv) {
        return core::unexpected(fragmentSpirv.error());
    }
    if (const auto validated = validateSpirv(*vertexSpirv, vertexStageName); !validated) {
        return core::unexpected(validated.error());
    }
    if (const auto validated = validateSpirv(*fragmentSpirv, fragmentStageName); !validated) {
        return core::unexpected(validated.error());
    }

    const auto vertexResources = collectStageResources(*vertexSpirv, vertexStageName);
    if (!vertexResources) {
        return core::unexpected(vertexResources.error());
    }
    const auto fragmentResources = collectStageResources(*fragmentSpirv, fragmentStageName);
    if (!fragmentResources) {
        return core::unexpected(fragmentResources.error());
    }
    const auto merged = mergeStageResources(*vertexResources, *fragmentResources);
    if (!merged) {
        return core::unexpected(merged.error());
    }
    if (const auto matched = matchDeclaredBindings(request, *merged); !matched) {
        return core::unexpected(matched.error());
    }

    auto vertexGlsl330 = crossCompileGlsl(*vertexSpirv, 330, false, vertexStageName);
    if (!vertexGlsl330) {
        return core::unexpected(vertexGlsl330.error());
    }
    auto fragmentGlsl330 = crossCompileGlsl(*fragmentSpirv, 330, false, fragmentStageName);
    if (!fragmentGlsl330) {
        return core::unexpected(fragmentGlsl330.error());
    }
    auto vertexGlslEs300 = crossCompileGlsl(*vertexSpirv, 300, true, vertexStageName);
    if (!vertexGlslEs300) {
        return core::unexpected(vertexGlslEs300.error());
    }
    auto fragmentGlslEs300 = crossCompileGlsl(*fragmentSpirv, 300, true, fragmentStageName);
    if (!fragmentGlslEs300) {
        return core::unexpected(fragmentGlslEs300.error());
    }

    ShaderCompileArtifact artifact{};
    artifact.vertexSpirv = std::move(*vertexSpirv);
    artifact.fragmentSpirv = std::move(*fragmentSpirv);
    artifact.vertexGlsl330 = std::move(*vertexGlsl330);
    artifact.fragmentGlsl330 = std::move(*fragmentGlsl330);
    artifact.vertexGlslEs300 = std::move(*vertexGlslEs300);
    artifact.fragmentGlslEs300 = std::move(*fragmentGlslEs300);
    artifact.reflection.hasCuexisObject = true;

    for (const auto& binding : request.declaredBindings) {
        artifact.reflection.bindings.push_back(ShaderReflectedBinding{
            .set = binding.set,
            .binding = binding.binding,
            .type = binding.type,
            .name = std::string{binding.name},
        });
    }
    std::sort(artifact.reflection.bindings.begin(), artifact.reflection.bindings.end(),
              [](const ShaderReflectedBinding& left, const ShaderReflectedBinding& right) {
                  return left.name < right.name;
              });

    for (const auto& parameter : *declaredParameters) {
        artifact.reflection.parameters.push_back(ShaderReflectedParameter{
            .name = parameter.name,
            .type = parameter.type,
            .set = parameter.set,
            .binding = parameter.binding,
        });
    }
    std::sort(artifact.reflection.parameters.begin(), artifact.reflection.parameters.end(),
              [](const ShaderReflectedParameter& left, const ShaderReflectedParameter& right) {
                  return left.name < right.name;
              });
    return artifact;
}

} // namespace cuexis::shader
