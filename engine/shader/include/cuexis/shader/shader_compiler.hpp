#pragma once

// Optional shader compile facade. Implements cuexis.importer.shader.v1:
// GLSL 450 -> SPIR-V (Vulkan 1.1 / SPIR-V 1.3, opt 0) -> SPIRV-Tools validate ->
// SPIRV-Cross reflection and GLSL 330 Core / GLSL ES 300.
// This header is not installed. Numeric ShaderParameterType values match Playback.

#include <cuexis/core/result.hpp>
#include <cuexis/shader/shader_diagnostics.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::shader {

enum class ShaderParameterType : std::uint8_t {
    Float = 1,
    Vec2 = 2,
    Vec3 = 3,
    Vec4 = 4,
    Int = 5,
    Bool = 6,
    Texture2D = 7,
};

struct ShaderDeclaredBinding final {
    std::uint32_t set{};
    std::uint32_t binding{};
    ShaderParameterType type{ShaderParameterType::Float};
    std::string_view name;
};

struct ShaderDeclaredParameter final {
    std::string_view name;
    ShaderParameterType type{ShaderParameterType::Float};
    std::uint32_t set{};
    std::uint32_t binding{};
};

struct ShaderCompileRequest final {
    std::string_view vertexSource;
    std::string_view fragmentSource;
    std::string_view vertexEntry{"main"};
    std::string_view fragmentEntry{"main"};
    std::span<const std::string_view> declaredKeywords{};
    std::span<const std::string_view> selectedKeywords{};
    std::span<const ShaderDeclaredBinding> declaredBindings{};
    std::span<const ShaderDeclaredParameter> declaredParameters{};
};

struct ShaderReflectedBinding final {
    std::uint32_t set{};
    std::uint32_t binding{};
    ShaderParameterType type{ShaderParameterType::Float};
    std::string name;

    friend bool operator==(const ShaderReflectedBinding&, const ShaderReflectedBinding&) = default;
};

struct ShaderReflectedParameter final {
    std::string name;
    ShaderParameterType type{ShaderParameterType::Float};
    std::uint32_t set{};
    std::uint32_t binding{};

    friend bool operator==(const ShaderReflectedParameter&,
                           const ShaderReflectedParameter&) = default;
};

// Normalized reflection for cache and later OpenGL mapping. Sorted by name.
// Texture units and UBO slots stay adapter-private and must not appear here.
struct ShaderReflection final {
    std::vector<ShaderReflectedBinding> bindings;
    std::vector<ShaderReflectedParameter> parameters;
    bool hasCuexisObject{};

    friend bool operator==(const ShaderReflection&, const ShaderReflection&) = default;
};

struct ShaderCompileArtifact final {
    std::vector<std::byte> vertexSpirv;
    std::vector<std::byte> fragmentSpirv;
    std::string vertexGlsl330;
    std::string fragmentGlsl330;
    std::string vertexGlslEs300;
    std::string fragmentGlslEs300;
    ShaderReflection reflection;

    friend bool operator==(const ShaderCompileArtifact&, const ShaderCompileArtifact&) = default;
};

class ShaderCompiler final {
  public:
    ShaderCompiler() = delete;

    [[nodiscard]] static auto compile(const ShaderCompileRequest& request)
        -> core::Result<ShaderCompileArtifact>;
};

[[nodiscard]] auto encodeCanonicalReflection(const ShaderReflection& reflection)
    -> std::vector<std::byte>;

[[nodiscard]] auto decodeCanonicalReflection(std::span<const std::byte> bytes)
    -> core::Result<ShaderReflection>;

} // namespace cuexis::shader
