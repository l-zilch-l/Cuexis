#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/playback/presentation.hpp>

#include "s5h_shader_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using cuexis::playback::PlaybackSession;
using cuexis::test_support::s5h::kFragment;
using cuexis::test_support::s5h::kVertex;
using cuexis::test_support::s5h::makeParameterizedPayload;
using cuexis::test_support::s5h::makeShaderPayload;
using cuexis::test_support::s5h::makeSource;
using cuexis::test_support::s5h::MaterialLayout;
using cuexis::test_support::s5h::ShaderLayout;

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    const auto text = contents.str();
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

[[nodiscard]] auto triangleMesh() -> std::vector<std::byte> {
    return readBytes(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                     "stage3_project" / "assets" / "meshes" / "triangle.mesh.bin");
}

[[nodiscard]] auto prepareCode(std::vector<std::byte> shaderBytes,
                               std::vector<std::byte> materialBytes = {}) -> std::string {
    auto source = materialBytes.empty() ? makeSource(std::move(shaderBytes), triangleMesh())
                                        : makeSource(std::move(shaderBytes), triangleMesh(),
                                                     std::move(materialBytes));
    REQUIRE(source.has_value());
    PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    if (prepared) {
        return {};
    }
    return std::string{prepared.error().code()};
}

[[nodiscard]] auto errorContext(const cuexis::core::Error& error, std::string_view key)
    -> std::string {
    for (const auto& context : error.context()) {
        if (context.key == key) {
            return context.value;
        }
    }
    return {};
}

} // namespace

TEST_CASE("S5-H shader budget constants match the frozen spec",
          "[playback][presentation][s5-h][limits]") {
    CHECK(cuexis::playback::presentationMaxShaderSourceBytes == 262144);
    CHECK(cuexis::playback::presentationMaxSpirvBytes == 1048576);
    CHECK(cuexis::playback::presentationMaxVariantKeywords == 4);
    CHECK(cuexis::playback::presentationMaxVariantsPerShader == 16);
    CHECK(cuexis::playback::presentationMaxMaterialParameters == 32);
    CHECK(cuexis::playback::presentationMaxTextureBindings == 8);
}

TEST_CASE("S5-H rejects shader count budgets at plus one without extra records",
          "[playback][presentation][s5-h][limits]") {
    SECTION("keyword 4 + 1") {
        ShaderLayout layout;
        layout.keywordCount = 5;
        layout.writeKeywords = false;
        CHECK(prepareCode(makeShaderPayload(kVertex, kFragment, layout)) ==
              "playback.presentation.shader.keyword_invalid");
    }
    SECTION("parameter 32 + 1") {
        ShaderLayout layout;
        layout.parameterCount = 33;
        layout.writeParameters = false;
        CHECK(prepareCode(makeShaderPayload(kVertex, kFragment, layout)) ==
              "playback.presentation.shader.schema_invalid");
    }
    SECTION("binding 16 + 1") {
        ShaderLayout layout;
        layout.bindingCount = 17;
        layout.writeBindings = false;
        CHECK(prepareCode(makeShaderPayload(kVertex, kFragment, layout)) ==
              "playback.presentation.shader.schema_invalid");
    }
    SECTION("host extension 8 + 1") {
        ShaderLayout layout;
        layout.hostExtensionCount = 9;
        layout.writeHostExtensions = false;
        CHECK(prepareCode(makeShaderPayload(kVertex, kFragment, layout)) ==
              "playback.presentation.shader.schema_invalid");
    }
}

TEST_CASE("S5-H rejects claimed oversize shader source without allocating the claimed bytes",
          "[playback][presentation][s5-h][limits]") {
    SECTION("vertex 262144 + 1") {
        ShaderLayout layout;
        layout.claimedVertexSourceBytes = 262145;
        layout.writeSources = false;
        const auto bytes = makeShaderPayload(kVertex, kFragment, layout);
        CHECK(bytes.size() < 4096);
        CHECK(prepareCode(bytes) == "playback.presentation.shader.subset_invalid");
    }
    SECTION("fragment 262144 + 1") {
        ShaderLayout layout;
        layout.claimedFragmentSourceBytes = 262145;
        layout.writeSources = false;
        const auto bytes = makeShaderPayload(kVertex, kFragment, layout);
        CHECK(bytes.size() < 4096);
        CHECK(prepareCode(bytes) == "playback.presentation.shader.subset_invalid");
    }
}

TEST_CASE("S5-H rejects parameterized material count budgets at plus one without extra records",
          "[playback][presentation][s5-h][limits]") {
    SECTION("material keyword 4 + 1") {
        MaterialLayout layout;
        layout.keywordCount = 5;
        layout.writeKeywords = false;
        CHECK(prepareCode(makeShaderPayload(kVertex, kFragment, {}),
                          makeParameterizedPayload("shader.sprite", layout)) ==
              "playback.presentation.material.keyword_undeclared");
    }
    SECTION("material parameter 32 + 1") {
        MaterialLayout layout;
        layout.parameterCount = 33;
        layout.writeParameters = false;
        CHECK(prepareCode(makeShaderPayload(kVertex, kFragment, {}),
                          makeParameterizedPayload("shader.sprite", layout)) ==
              "playback.presentation.material.parameter_mismatch");
    }
}

TEST_CASE("S5-H rejects a ninth texture parameter without a 64 MiB fixture",
          "[playback][presentation][s5-h][limits]") {
    ShaderLayout shaderLayout;
    shaderLayout.parameterCount = 9;
    shaderLayout.bindingCount = 9;
    shaderLayout.textureParameters = true;
    MaterialLayout materialLayout;
    materialLayout.parameterCount = 9;
    materialLayout.textureCount = 9;
    const auto shaderBytes = makeShaderPayload(kVertex, kFragment, shaderLayout);
    const auto materialBytes = makeParameterizedPayload("shader.sprite", materialLayout);
    CHECK(shaderBytes.size() < 4096);
    CHECK(materialBytes.size() < 4096);
    CHECK(prepareCode(shaderBytes, materialBytes) ==
          "playback.presentation.resource.budget_exceeded");
}

TEST_CASE("S5-H accepts exact shader keyword, parameter, and binding maxima",
          "[playback][presentation][s5-h][limits]") {
    SECTION("keywords 4") {
        ShaderLayout layout;
        layout.keywordCount = 4;
        CHECK(prepareCode(makeShaderPayload(kVertex, kFragment, layout)).empty());
    }
    SECTION("bindings 16") {
        ShaderLayout layout;
        layout.bindingCount = 16;
        CHECK(prepareCode(makeShaderPayload(kVertex, kFragment, layout)).empty());
    }
    SECTION("parameters 32 with 16 bindings") {
        ShaderLayout layout;
        layout.parameterCount = 32;
        layout.bindingCount = 16;
        MaterialLayout material;
        material.parameterCount = 32;
        const auto code = prepareCode(makeShaderPayload(kVertex, kFragment, layout),
                                      makeParameterizedPayload("shader.sprite", material));
        INFO(code);
        CHECK(code.empty());
    }
    SECTION("keywords 4, parameters 32, bindings 16") {
        ShaderLayout layout;
        layout.keywordCount = 4;
        layout.parameterCount = 32;
        layout.bindingCount = 16;
        MaterialLayout material;
        material.parameterCount = 32;
        const auto code = prepareCode(makeShaderPayload(kVertex, kFragment, layout),
                                      makeParameterizedPayload("shader.sprite", material));
        INFO(code);
        CHECK(code.empty());
    }
}

TEST_CASE("S5-H maps an oversized encoded shader provider to budget_exceeded without a giant blob",
          "[playback][presentation][s5-h][limits]") {
    auto legal = makeShaderPayload(kVertex, kFragment, {});
    auto provider = cuexis::content::HostContentProvider::create(
        [legal = std::move(legal)](const cuexis::content::ContentRequest& request)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            if (request.source == "shaders/sprite.shader.bin") {
                return cuexis::core::unexpected(cuexis::core::Error{
                    "content.provider.too_large", "S5-H simulates an encoded shader over 64 MiB"});
            }
            if (request.source == "materials/sprite.material.bin") {
                return cuexis::content::ContentBlob{
                    .bytes = cuexis::test_support::s5h::makeParameterizedPayload("shader.sprite"),
                    .revision = 1};
            }
            if (request.source == "meshes/triangle.mesh.bin") {
                return cuexis::content::ContentBlob{.bytes = triangleMesh(), .revision = 1};
            }
            return cuexis::core::unexpected(
                cuexis::core::Error{"test.content.source_missing", "unexpected source"});
        });
    REQUIRE(provider.has_value());
    cuexis::playback::TypedPlaybackProject project{
        .sourceId = "s5h-encoded-budget",
        .chartJson = std::string{cuexis::test_support::s5h::kChart},
        .assets = {{.id = "shader.sprite",
                    .type = cuexis::playback::PlaybackAssetType::Shader,
                    .rootId = "main",
                    .logicalSource = "shaders/sprite.shader.bin"},
                   {.id = "material.sprite",
                    .type = cuexis::playback::PlaybackAssetType::Material,
                    .rootId = "main",
                    .logicalSource = "materials/sprite.material.bin",
                    .dependencies = {"shader.sprite"}},
                   {.id = "mesh.triangle",
                    .type = cuexis::playback::PlaybackAssetType::Mesh,
                    .rootId = "main",
                    .logicalSource = "meshes/triangle.mesh.bin"}},
    };
    auto source = cuexis::playback::PlaybackSource::fromTypedProject(std::move(project),
                                                                     std::move(*provider));
    REQUIRE(source.has_value());
    PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(prepared.has_value());
    CHECK(prepared.error().code() == "playback.presentation.resource.budget_exceeded");
    CHECK(errorContext(prepared.error(), "asset_id") == "shader.sprite");
    CHECK(errorContext(prepared.error(), "resource_type") == "shader");
    CHECK(errorContext(prepared.error(), "limit") == "67108864");
    CHECK(errorContext(prepared.error(), "actual") == "greater_than_limit");
}

TEST_CASE("S5-H truncated envelope with a huge declared size does not allocate the declared size",
          "[playback][presentation][s5-h][limits]") {
    auto bytes = makeShaderPayload(kVertex, kFragment, {});
    REQUIRE(bytes.size() >= 24);
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[16 + index] = static_cast<std::byte>(0xFF);
    }
    CHECK(bytes.size() < 4096);
    CHECK(prepareCode(std::move(bytes)) == "playback.presentation.payload.truncated");
}
