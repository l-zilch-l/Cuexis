#include <cuexis/shader/shader_compiler.hpp>
#include <cuexis/shader/shader_diagnostics.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

static_assert(!std::is_default_constructible_v<cuexis::shader::ShaderCompiler>);
static_assert(static_cast<std::uint8_t>(cuexis::shader::ShaderParameterType::Texture2D) == 7);
static_assert(cuexis::shader::importerProfileShaderV1 == "cuexis.importer.shader.v1");
static_assert(cuexis::shader::targetProfileSpirvV1 == "cuexis.target.spirv.v1");
static_assert(cuexis::shader::targetProfileGlsl330V1 == "cuexis.target.glsl330.v1");
static_assert(cuexis::shader::targetProfileGlslEs300V1 == "cuexis.target.glsles300.v1");
static_assert(cuexis::shader::rendererProfilePortableV1 == "cuexis.renderer.portable.v1");
static_assert(cuexis::shader::rendererProfileBuiltInV1 == "cuexis.renderer.builtin.v1");

namespace {

constexpr std::string_view kCuexisObjectBlock =
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {\n"
    "    mat4 world;\n"
    "    mat4 viewProjection;\n"
    "    vec3 tint;\n"
    "    float opacity;\n"
    "} cuexisObject;\n";

constexpr std::string_view kSpriteVertex =
    "#version 450\n"
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {\n"
    "    mat4 world;\n"
    "    mat4 viewProjection;\n"
    "    vec3 tint;\n"
    "    float opacity;\n"
    "} cuexisObject;\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "layout(location = 0) out vec2 vTexCoord;\n"
    "void main() {\n"
    "    vTexCoord = aTexCoord;\n"
    "    gl_Position = cuexisObject.viewProjection * cuexisObject.world * vec4(aPosition, 1.0);\n"
    "}\n";

constexpr std::string_view kSpriteFragment =
    "#version 450\n"
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {\n"
    "    mat4 world;\n"
    "    mat4 viewProjection;\n"
    "    vec3 tint;\n"
    "    float opacity;\n"
    "} cuexisObject;\n"
    "layout(set = 0, binding = 1) uniform sampler2D albedo;\n"
    "layout(location = 0) in vec2 vTexCoord;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "void main() {\n"
    "    vec4 texel = texture(albedo, vTexCoord);\n"
    "    outColor = vec4(texel.rgb * cuexisObject.tint, texel.a * cuexisObject.opacity);\n"
    "#ifdef PREMULTIPLY\n"
    "    outColor.rgb *= outColor.a;\n"
    "#endif\n"
    "}\n";

constexpr std::string_view kPassthroughVertex =
    "#version 450\n"
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {\n"
    "    mat4 world;\n"
    "    mat4 viewProjection;\n"
    "    vec3 tint;\n"
    "    float opacity;\n"
    "} cuexisObject;\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "void main() {\n"
    "    gl_Position = cuexisObject.viewProjection * cuexisObject.world * vec4(aPosition, 1.0);\n"
    "}\n";

constexpr std::string_view kPassthroughFragment =
    "#version 450\n"
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {\n"
    "    mat4 world;\n"
    "    mat4 viewProjection;\n"
    "    vec3 tint;\n"
    "    float opacity;\n"
    "} cuexisObject;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "void main() {\n"
    "    outColor = vec4(cuexisObject.tint, cuexisObject.opacity);\n"
    "}\n";

[[nodiscard]] auto spriteRequest(std::span<const std::string_view> selected = {})
    -> cuexis::shader::ShaderCompileRequest {
    static const cuexis::shader::ShaderDeclaredBinding bindings[] = {
        {.set = 0,
         .binding = 1,
         .type = cuexis::shader::ShaderParameterType::Texture2D,
         .name = "albedo"},
    };
    static const cuexis::shader::ShaderDeclaredParameter parameters[] = {
        {.name = "albedo",
         .type = cuexis::shader::ShaderParameterType::Texture2D,
         .set = 0,
         .binding = 1},
    };
    static const std::string_view declared[] = {"PREMULTIPLY"};
    return cuexis::shader::ShaderCompileRequest{
        .vertexSource = kSpriteVertex,
        .fragmentSource = kSpriteFragment,
        .declaredKeywords = declared,
        .selectedKeywords = selected,
        .declaredBindings = bindings,
        .declaredParameters = parameters,
    };
}

} // namespace

TEST_CASE("S5-D freezes shader compile diagnostic codes", "[shader][s5-d]") {
    CHECK(cuexis::shader::diagnosticCompileFailed == "shader.compile.failed");
    CHECK(cuexis::shader::diagnosticReflectMismatch == "shader.reflect.mismatch");
    CHECK(cuexis::shader::diagnosticLimitExceeded == "shader.diagnostics.limit_exceeded");
    CHECK(cuexis::shader::diagnosticKeywordInvalid ==
          "playback.presentation.shader.keyword_invalid");
    CHECK(cuexis::shader::diagnosticSubsetInvalid == "playback.presentation.shader.subset_invalid");
    CHECK(cuexis::shader::diagnosticReservedBinding ==
          "playback.presentation.shader.reserved_binding");
    CHECK(cuexis::shader::maxDiagnostics == 1024);
    (void)kCuexisObjectBlock;
}

TEST_CASE("S5-D compiles an Unlit-like sprite shader to stable SPIR-V and reflection",
          "[shader][s5-d]") {
    const auto request = spriteRequest();
    const auto first = cuexis::shader::ShaderCompiler::compile(request);
    REQUIRE(first.has_value());
    const auto second = cuexis::shader::ShaderCompiler::compile(request);
    REQUIRE(second.has_value());

    CHECK(first->vertexSpirv == second->vertexSpirv);
    CHECK(first->fragmentSpirv == second->fragmentSpirv);
    CHECK(cuexis::shader::encodeCanonicalReflection(first->reflection) ==
          cuexis::shader::encodeCanonicalReflection(second->reflection));
    CHECK(*first == *second);

    CHECK(first->reflection.hasCuexisObject);
    REQUIRE(first->reflection.bindings.size() == 1);
    CHECK(first->reflection.bindings[0].name == "albedo");
    CHECK(first->reflection.bindings[0].set == 0);
    CHECK(first->reflection.bindings[0].binding == 1);
    CHECK(first->reflection.bindings[0].type == cuexis::shader::ShaderParameterType::Texture2D);
    REQUIRE(first->reflection.parameters.size() == 1);
    CHECK(first->reflection.parameters[0].name == "albedo");
    CHECK(first->vertexGlsl330.find("#version 330") != std::string::npos);
    CHECK(first->fragmentGlsl330.find("#version 330") != std::string::npos);
    CHECK(first->vertexGlslEs300.find("#version 300 es") != std::string::npos);
    CHECK(first->fragmentGlslEs300.find("#version 300 es") != std::string::npos);
    CHECK(first->vertexGlsl330.find("gl_TextureUnit") == std::string::npos);
    CHECK(!first->vertexSpirv.empty());
    CHECK(!first->fragmentSpirv.empty());
}

TEST_CASE("S5-D selected keywords change SPIR-V without changing reflection identity",
          "[shader][s5-d]") {
    const std::string_view selected[] = {"PREMULTIPLY"};
    const auto plain = cuexis::shader::ShaderCompiler::compile(spriteRequest());
    const auto variant = cuexis::shader::ShaderCompiler::compile(spriteRequest(selected));
    REQUIRE(plain.has_value());
    REQUIRE(variant.has_value());
    CHECK(plain->fragmentSpirv != variant->fragmentSpirv);
    CHECK(cuexis::shader::encodeCanonicalReflection(plain->reflection) ==
          cuexis::shader::encodeCanonicalReflection(variant->reflection));
}

TEST_CASE("S5-D rejects duplicate declared bindings", "[shader][s5-d]") {
    const cuexis::shader::ShaderDeclaredBinding bindings[] = {
        {.set = 0,
         .binding = 1,
         .type = cuexis::shader::ShaderParameterType::Texture2D,
         .name = "albedo"},
        {.set = 0,
         .binding = 1,
         .type = cuexis::shader::ShaderParameterType::Vec4,
         .name = "color"},
    };
    const auto compiled = cuexis::shader::ShaderCompiler::compile({
        .vertexSource = kPassthroughVertex,
        .fragmentSource = kPassthroughFragment,
        .declaredBindings = bindings,
    });
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == cuexis::shader::diagnosticSchemaInvalid);
}

TEST_CASE("S5-D rejects undeclared selected keywords", "[shader][s5-d]") {
    const std::string_view selected[] = {"NOT_DECLARED"};
    const auto compiled = cuexis::shader::ShaderCompiler::compile({
        .vertexSource = kPassthroughVertex,
        .fragmentSource = kPassthroughFragment,
        .selectedKeywords = selected,
    });
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == cuexis::shader::diagnosticKeywordInvalid);
}

TEST_CASE("S5-D rejects illegal #include before shaderc runs", "[shader][s5-d]") {
    const auto vertex = std::string{kPassthroughVertex} + "#include \"stolen.glsl\"\n";
    const auto compiled = cuexis::shader::ShaderCompiler::compile({
        .vertexSource = vertex,
        .fragmentSource = kPassthroughFragment,
    });
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == cuexis::shader::diagnosticSubsetInvalid);
}

TEST_CASE("S5-D rejects a user binding at the reserved CuexisObject slot", "[shader][s5-d]") {
    const cuexis::shader::ShaderDeclaredBinding bindings[] = {
        {.set = 0,
         .binding = 0,
         .type = cuexis::shader::ShaderParameterType::Vec4,
         .name = "stolen"},
    };
    const auto compiled = cuexis::shader::ShaderCompiler::compile({
        .vertexSource = kPassthroughVertex,
        .fragmentSource = kPassthroughFragment,
        .declaredBindings = bindings,
    });
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == cuexis::shader::diagnosticReservedBinding);
}

TEST_CASE("S5-D rejects missing CuexisObject as a reflection mismatch", "[shader][s5-d]") {
    constexpr std::string_view vertex = "#version 450\n"
                                        "layout(location = 0) in vec3 aPosition;\n"
                                        "void main() { gl_Position = vec4(aPosition, 1.0); }\n";
    constexpr std::string_view fragment = "#version 450\n"
                                          "layout(location = 0) out vec4 outColor;\n"
                                          "void main() { outColor = vec4(1.0); }\n";
    const auto compiled = cuexis::shader::ShaderCompiler::compile({
        .vertexSource = vertex,
        .fragmentSource = fragment,
    });
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == cuexis::shader::diagnosticReflectMismatch);
}

TEST_CASE("S5-D rejects undeclared sampled images as a reflection mismatch", "[shader][s5-d]") {
    const auto compiled = cuexis::shader::ShaderCompiler::compile({
        .vertexSource = kSpriteVertex,
        .fragmentSource = kSpriteFragment,
    });
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == cuexis::shader::diagnosticReflectMismatch);
}

TEST_CASE("S5-D reports shaderc failures with the frozen compile code", "[shader][s5-d]") {
    constexpr std::string_view vertex =
        "#version 450\n"
        "layout(std140, set = 0, binding = 0) uniform CuexisObject {\n"
        "    mat4 world;\n"
        "    mat4 viewProjection;\n"
        "    vec3 tint;\n"
        "    float opacity;\n"
        "} cuexisObject;\n"
        "not valid glsl\n";
    const auto compiled = cuexis::shader::ShaderCompiler::compile({
        .vertexSource = vertex,
        .fragmentSource = kPassthroughFragment,
    });
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == cuexis::shader::diagnosticCompileFailed);
    CHECK(compiled.error().message().find("S5-D") == std::string_view::npos);
}
