#pragma once

// The shader compiler validates a request, compiles both stages and reflects the result. The units
// share the request limits and the SPIR-V helpers, so they live here.

#include <cuexis/core/error.hpp>
#include <cuexis/shader/shader_compiler.hpp>
#include <map>
#include <shaderc/shaderc.h>
#include <spirv-tools/libspirv.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::shader {

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

struct StageResources final {
    bool hasCuexisObject{};
    std::map<BindingKey, DeclaredResource> userResources;
};

[[nodiscard]] auto makeCompileOptions(const ShaderCompileRequest& request)
    -> core::Result<ShadercOptionsPtr>;

[[nodiscard]] auto makeError(std::string_view code, std::string message, std::string_view tool,
                             std::string_view stage = {}) -> core::Error;

[[nodiscard]] auto validateSource(std::string_view source, std::string_view stage)
    -> core::Result<void>;

[[nodiscard]] auto validateKeywords(const ShaderCompileRequest& request) -> core::Result<void>;

[[nodiscard]] auto validateDeclaredSchema(const ShaderCompileRequest& request)
    -> core::Result<std::vector<DeclaredResource>>;

[[nodiscard]] auto compileStage(shaderc_compiler_t compiler,
                                const shaderc_compile_options_t options, std::string_view source,
                                std::string_view entry, shaderc_shader_kind kind,
                                std::string_view fileName, std::string_view stage)
    -> core::Result<std::vector<std::byte>>;

[[nodiscard]] auto asSpirvWords(const std::vector<std::byte>& spirv) -> std::vector<std::uint32_t>;

[[nodiscard]] auto validateSpirv(const std::vector<std::byte>& spirv, std::string_view stage)
    -> core::Result<void>;

[[nodiscard]] auto collectStageResources(const std::vector<std::byte>& spirv,
                                         std::string_view stage) -> core::Result<StageResources>;

[[nodiscard]] auto mergeStageResources(const StageResources& vertex, const StageResources& fragment)
    -> core::Result<std::map<BindingKey, DeclaredResource>>;

[[nodiscard]] auto matchDeclaredBindings(const ShaderCompileRequest& request,
                                         const std::map<BindingKey, DeclaredResource>& reflected)
    -> core::Result<void>;

[[nodiscard]] auto crossCompileGlsl(const std::vector<std::byte>& spirv, unsigned version, bool es,
                                    std::string_view stage) -> core::Result<std::string>;

} // namespace cuexis::shader
