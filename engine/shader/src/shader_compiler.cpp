#include <cuexis/shader/shader_compiler.hpp>

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
namespace {

constexpr std::uint32_t spirvMagic{0x07230203u};
constexpr std::string_view cuexisObjectName{"CuexisObject"};
constexpr std::uint32_t reservedSet{0};
constexpr std::uint32_t reservedBinding{0};
constexpr std::uint32_t maxUserBinding{16};
constexpr std::string_view vertexFileName{"vertex.glsl"};
constexpr std::string_view fragmentFileName{"fragment.glsl"};
constexpr std::string_view vertexStageName{"vertex"};
constexpr std::string_view fragmentStageName{"fragment"};
constexpr std::string_view macroOne{"1"};

struct ShadercCompilerDeleter final {
    void operator()(shaderc_compiler* compiler) const noexcept {
        shaderc_compiler_release(compiler);
    }
};

struct ShadercOptionsDeleter final {
    void operator()(shaderc_compile_options* options) const noexcept {
        shaderc_compile_options_release(options);
    }
};

struct ShadercResultDeleter final {
    void operator()(shaderc_compilation_result* result) const noexcept {
        shaderc_result_release(result);
    }
};

struct SpvContextDeleter final {
    void operator()(spv_context_t* context) const noexcept {
        spvContextDestroy(context);
    }
};

using ShadercCompilerPtr = std::unique_ptr<shaderc_compiler, ShadercCompilerDeleter>;
using ShadercOptionsPtr = std::unique_ptr<shaderc_compile_options, ShadercOptionsDeleter>;
using ShadercResultPtr = std::unique_ptr<shaderc_compilation_result, ShadercResultDeleter>;
using SpvContextPtr = std::unique_ptr<spv_context_t, SpvContextDeleter>;

[[nodiscard]] auto makeError(std::string_view code, std::string message, std::string_view tool,
                             std::string_view stage = {}) -> core::Error {
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

struct BindingKey final {
    std::uint32_t set{};
    std::uint32_t binding{};

    friend auto operator<=>(const BindingKey&, const BindingKey&) = default;
};

struct DeclaredResource final {
    std::string name;
    ShaderParameterType type{ShaderParameterType::Float};
    std::uint32_t set{};
    std::uint32_t binding{};
};

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

struct StageResources final {
    bool hasCuexisObject{};
    std::map<BindingKey, DeclaredResource> userResources;
};

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

} // namespace

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
