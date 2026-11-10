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

[[nodiscard]] auto makeError(std::string_view code, std::string message, std::string_view tool,
                             std::string_view stage) -> core::Error {
    auto error = core::Error{std::string{code}, std::move(message)}.withContext(
        std::string{contextTool}, std::string{tool});
    if (!stage.empty()) {
        error.withContext(std::string{contextStage}, std::string{stage});
    }
    return error;
}

[[nodiscard]] auto startsWithIgnoreCase(std::string_view text, std::string_view prefix) noexcept
    -> bool {
    if (text.size() < prefix.size()) {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        const auto left = static_cast<unsigned char>(text[index]);
        const auto right = static_cast<unsigned char>(prefix[index]);
        if (std::tolower(left) != std::tolower(right)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto isAsciiLetter(char value) noexcept -> bool {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

[[nodiscard]] auto isAsciiIdentifierChar(char value) noexcept -> bool {
    return isAsciiLetter(value) || (value >= '0' && value <= '9') || value == '_';
}

[[nodiscard]] auto isLegalName(std::string_view name) noexcept -> bool {
    if (name.empty() || name.size() > 32 || !isAsciiLetter(name.front())) {
        return false;
    }
    for (const char value : name) {
        if (!isAsciiIdentifierChar(value)) {
            return false;
        }
    }
    return !startsWithIgnoreCase(name, "cuexis");
}

[[nodiscard]] auto firstLine(std::string_view source) noexcept -> std::string_view {
    const auto newline = source.find('\n');
    return newline == std::string_view::npos ? source : source.substr(0, newline);
}

[[nodiscard]] auto sourceHasBomOrCr(std::string_view source) noexcept -> bool {
    if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        return true;
    }
    return source.find('\r') != std::string_view::npos;
}

[[nodiscard]] auto sourceHasInclude(std::string_view source) noexcept -> bool {
    std::size_t lineStart = 0;
    while (lineStart < source.size()) {
        const auto lineEnd = source.find('\n', lineStart);
        const auto line = source.substr(
            lineStart, (lineEnd == std::string_view::npos ? source.size() : lineEnd) - lineStart);
        std::size_t cursor = 0;
        while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
            ++cursor;
        }
        if (cursor < line.size() && line[cursor] == '#') {
            ++cursor;
            while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
                ++cursor;
            }
            if (line.substr(cursor).starts_with("include")) {
                return true;
            }
        }
        if (lineEnd == std::string_view::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return false;
}

[[nodiscard]] auto validateSource(std::string_view source, std::string_view stage)
    -> core::Result<void> {
    if (source.size() > maxSourceBytesPerStage) {
        return core::unexpected(makeError(diagnosticCompileFailed,
                                          "Shader source exceeds the per-stage byte budget",
                                          toolCuexisShader, stage)
                                    .withContext("limit", std::to_string(maxSourceBytesPerStage))
                                    .withContext("actual", std::to_string(source.size())));
    }
    if (source.empty() || sourceHasBomOrCr(source)) {
        return core::unexpected(makeError(diagnosticSourceEncodingInvalid,
                                          "Shader source must be UTF-8 LF without a BOM",
                                          toolCuexisShader, stage));
    }
    if (firstLine(source) != "#version 450") {
        return core::unexpected(makeError(diagnosticSubsetInvalid,
                                          "Shader source must start with #version 450",
                                          toolCuexisShader, stage));
    }
    if (sourceHasInclude(source)) {
        return core::unexpected(makeError(diagnosticSubsetInvalid,
                                          "Shader source must not use #include", toolCuexisShader,
                                          stage));
    }
    return {};
}

[[nodiscard]] auto validateKeywords(const ShaderCompileRequest& request) -> core::Result<void> {
    if (request.declaredKeywords.size() > maxVariantKeywords) {
        return core::unexpected(makeError(diagnosticKeywordInvalid,
                                          "Shader declares more than 4 variant keywords",
                                          toolCuexisShader));
    }

    std::vector<std::string_view> declared{request.declaredKeywords.begin(),
                                           request.declaredKeywords.end()};
    std::sort(declared.begin(), declared.end());
    for (std::size_t index = 0; index < declared.size(); ++index) {
        if (!isLegalName(declared[index])) {
            return core::unexpected(makeError(diagnosticKeywordInvalid,
                                              "Shader keyword is not a legal identifier",
                                              toolCuexisShader)
                                        .withContext("keyword", std::string{declared[index]}));
        }
        if (index > 0 && declared[index] == declared[index - 1]) {
            return core::unexpected(makeError(diagnosticKeywordInvalid,
                                              "Shader keyword is duplicated", toolCuexisShader)
                                        .withContext("keyword", std::string{declared[index]}));
        }
    }

    std::vector<std::string_view> selected{request.selectedKeywords.begin(),
                                           request.selectedKeywords.end()};
    std::sort(selected.begin(), selected.end());
    for (std::size_t index = 0; index < selected.size(); ++index) {
        if (index > 0 && selected[index] == selected[index - 1]) {
            return core::unexpected(makeError(diagnosticKeywordInvalid,
                                              "Selected keyword is duplicated", toolCuexisShader)
                                        .withContext("keyword", std::string{selected[index]}));
        }
        if (std::find(declared.begin(), declared.end(), selected[index]) == declared.end()) {
            return core::unexpected(makeError(diagnosticKeywordInvalid,
                                              "Selected keyword is not declared by the shader",
                                              toolCuexisShader)
                                        .withContext("keyword", std::string{selected[index]}));
        }
    }
    return {};
}

[[nodiscard]] auto validateDeclaredSchema(const ShaderCompileRequest& request)
    -> core::Result<std::vector<DeclaredResource>> {
    std::map<BindingKey, DeclaredResource> bySlot;
    std::map<std::string, BindingKey, std::less<>> byName;

    for (const auto& binding : request.declaredBindings) {
        if (!isLegalName(binding.name)) {
            return core::unexpected(makeError(diagnosticSchemaInvalid,
                                              "Shader binding name is not a legal identifier",
                                              toolCuexisShader)
                                        .withContext("name", std::string{binding.name}));
        }
        if (binding.set == reservedSet && binding.binding == reservedBinding) {
            return core::unexpected(makeError(diagnosticReservedBinding,
                                              "User binding must not occupy set 0 binding 0",
                                              toolCuexisShader)
                                        .withContext("name", std::string{binding.name}));
        }
        if (binding.set != reservedSet || binding.binding < 1 || binding.binding > maxUserBinding) {
            return core::unexpected(
                makeError(diagnosticSchemaInvalid,
                          "User binding set must be 0 and binding must be in [1,16]",
                          toolCuexisShader)
                    .withContext("name", std::string{binding.name}));
        }

        const BindingKey key{binding.set, binding.binding};
        if (bySlot.contains(key)) {
            return core::unexpected(makeError(diagnosticSchemaInvalid,
                                              "Shader binding slot is duplicated", toolCuexisShader)
                                        .withContext("name", std::string{binding.name}));
        }
        if (byName.contains(binding.name)) {
            return core::unexpected(makeError(diagnosticSchemaInvalid,
                                              "Shader binding name is duplicated", toolCuexisShader)
                                        .withContext("name", std::string{binding.name}));
        }

        DeclaredResource resource{
            .name = std::string{binding.name},
            .type = binding.type,
            .set = binding.set,
            .binding = binding.binding,
        };
        bySlot.emplace(key, resource);
        byName.emplace(resource.name, key);
    }

    std::vector<DeclaredResource> parameters;
    std::map<std::string, std::size_t, std::less<>> parameterNames;
    for (const auto& parameter : request.declaredParameters) {
        if (!isLegalName(parameter.name)) {
            return core::unexpected(makeError(diagnosticSchemaInvalid,
                                              "Shader parameter name is not a legal identifier",
                                              toolCuexisShader)
                                        .withContext("name", std::string{parameter.name}));
        }
        if (parameter.set == reservedSet && parameter.binding == reservedBinding) {
            return core::unexpected(makeError(diagnosticReservedBinding,
                                              "User parameter must not occupy set 0 binding 0",
                                              toolCuexisShader)
                                        .withContext("name", std::string{parameter.name}));
        }
        if (parameterNames.contains(parameter.name)) {
            return core::unexpected(makeError(diagnosticSchemaInvalid,
                                              "Shader parameter name is duplicated",
                                              toolCuexisShader)
                                        .withContext("name", std::string{parameter.name}));
        }

        const auto found = bySlot.find(BindingKey{parameter.set, parameter.binding});
        if (found == bySlot.end() || found->second.type != parameter.type ||
            found->second.name != parameter.name) {
            return core::unexpected(
                makeError(diagnosticSchemaInvalid,
                          "Shader parameter set, binding and type must match a declared binding",
                          toolCuexisShader)
                    .withContext("name", std::string{parameter.name}));
        }
        parameterNames.emplace(std::string{parameter.name}, parameters.size());
        parameters.push_back(DeclaredResource{
            .name = std::string{parameter.name},
            .type = parameter.type,
            .set = parameter.set,
            .binding = parameter.binding,
        });
    }
    return parameters;
}

shaderc_include_result* rejectInclude(void*, const char*, int, const char*, std::size_t) {
    static thread_local std::string message{"#include is not allowed in Cuexis portable GLSL"};
    static thread_local shaderc_include_result result{};
    result.source_name = "";
    result.source_name_length = 0;
    result.content = message.c_str();
    result.content_length = message.size();
    result.user_data = nullptr;
    return &result;
}

void releaseInclude(void*, shaderc_include_result*) {}

[[nodiscard]] auto makeCompileOptions(const ShaderCompileRequest& request)
    -> core::Result<ShadercOptionsPtr> {
    ShadercOptionsPtr options{shaderc_compile_options_initialize()};
    if (!options) {
        return core::unexpected(makeError(
            diagnosticCompileFailed, "shaderc compile options failed to initialize", toolShaderc));
    }

    shaderc_compile_options_set_source_language(options.get(), shaderc_source_language_glsl);
    shaderc_compile_options_set_optimization_level(options.get(), shaderc_optimization_level_zero);
    shaderc_compile_options_set_target_env(options.get(), shaderc_target_env_vulkan,
                                           shaderc_env_version_vulkan_1_1);
    shaderc_compile_options_set_target_spirv(options.get(), shaderc_spirv_version_1_3);
    shaderc_compile_options_set_include_callbacks(options.get(), rejectInclude, releaseInclude,
                                                  nullptr);

    for (const auto keyword : request.selectedKeywords) {
        shaderc_compile_options_add_macro_definition(options.get(), keyword.data(), keyword.size(),
                                                     macroOne.data(), macroOne.size());
    }
    return options;
}

[[nodiscard]] auto copySpirvBytes(const char* bytes, std::size_t byteCount, std::string_view stage)
    -> core::Result<std::vector<std::byte>> {
    if (bytes == nullptr || byteCount == 0 || (byteCount % sizeof(std::uint32_t)) != 0) {
        return core::unexpected(makeError(diagnosticCompileFailed,
                                          "shaderc produced an empty or unaligned SPIR-V module",
                                          toolShaderc, stage));
    }
    if (byteCount > maxSpirvBytesPerStage) {
        return core::unexpected(makeError(diagnosticCompileFailed,
                                          "SPIR-V module exceeds the per-stage byte budget",
                                          toolShaderc, stage)
                                    .withContext("limit", std::to_string(maxSpirvBytesPerStage))
                                    .withContext("actual", std::to_string(byteCount)));
    }

    std::vector<std::byte> spirv(byteCount);
    std::memcpy(spirv.data(), bytes, byteCount);
    std::uint32_t magic = 0;
    std::memcpy(&magic, spirv.data(), sizeof(magic));
    if (magic != spirvMagic) {
        return core::unexpected(makeError(diagnosticCompileFailed,
                                          "shaderc produced a module without the SPIR-V magic",
                                          toolShaderc, stage));
    }
    return spirv;
}

[[nodiscard]] auto compileStage(shaderc_compiler_t compiler,
                                const shaderc_compile_options_t options, std::string_view source,
                                std::string_view entry, shaderc_shader_kind kind,
                                std::string_view fileName, std::string_view stage)
    -> core::Result<std::vector<std::byte>> {
    const std::string entryName{entry.empty() ? "main" : entry};
    ShadercResultPtr result{shaderc_compile_into_spv(compiler, source.data(), source.size(), kind,
                                                     fileName.data(), entryName.c_str(), options)};
    if (!result) {
        return core::unexpected(makeError(diagnosticCompileFailed,
                                          "shaderc failed to allocate a compilation result",
                                          toolShaderc, stage));
    }

    const auto status = shaderc_result_get_compilation_status(result.get());
    if (status != shaderc_compilation_status_success) {
        const char* message = shaderc_result_get_error_message(result.get());
        return core::unexpected(makeError(diagnosticCompileFailed,
                                          message == nullptr || message[0] == '\0'
                                              ? "shaderc failed to compile GLSL 450"
                                              : std::string{message},
                                          toolShaderc, stage));
    }

    return copySpirvBytes(shaderc_result_get_bytes(result.get()),
                          shaderc_result_get_length(result.get()), stage);
}

[[nodiscard]] auto asSpirvWords(const std::vector<std::byte>& spirv) -> std::vector<std::uint32_t> {
    std::vector<std::uint32_t> words(spirv.size() / sizeof(std::uint32_t));
    std::memcpy(words.data(), spirv.data(), spirv.size());
    return words;
}

[[nodiscard]] auto validateSpirv(const std::vector<std::byte>& spirv, std::string_view stage)
    -> core::Result<void> {
    SpvContextPtr context{spvContextCreate(SPV_ENV_VULKAN_1_1)};
    if (!context) {
        return core::unexpected(makeError(diagnosticCompileFailed,
                                          "SPIRV-Tools failed to create a Vulkan 1.1 context",
                                          toolSpirvTools, stage));
    }

    const auto words = asSpirvWords(spirv);
    spv_diagnostic diagnostic = nullptr;
    const spv_result_t status =
        spvValidateBinary(context.get(), words.data(), words.size(), &diagnostic);
    std::string message;
    if (diagnostic != nullptr) {
        if (diagnostic->error != nullptr) {
            message = diagnostic->error;
        }
        spvDiagnosticDestroy(diagnostic);
    }
    if (status != SPV_SUCCESS) {
        if (message.empty()) {
            message = "SPIRV-Tools rejected the SPIR-V module";
        }
        return core::unexpected(
            makeError(diagnosticCompileFailed, std::move(message), toolSpirvTools, stage));
    }
    return {};
}

} // namespace cuexis::shader
