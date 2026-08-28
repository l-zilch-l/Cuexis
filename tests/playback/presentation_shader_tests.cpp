#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/playback/presentation.hpp>

#include "validation_sink.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using cuexis::playback::PlaybackAssetDescriptor;
using cuexis::playback::PlaybackAssetType;
using cuexis::playback::PlaybackCapabilitySet;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PlaybackSource;
using cuexis::playback::PortableParameterizedMaterial;
using cuexis::playback::PortableShader;
using cuexis::playback::PresentationCapabilities;
using cuexis::playback::PresentationRequest;
using cuexis::playback::PresentationResourceType;

void appendU32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xFFU));
    }
}

void appendU64(std::vector<std::byte>& bytes, std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xFFU));
    }
}

void appendRaw(std::vector<std::byte>& bytes, std::string_view text) {
    for (const unsigned char character : text) {
        bytes.push_back(static_cast<std::byte>(character));
    }
}

[[nodiscard]] auto wrapPayload(std::uint32_t kind, const std::vector<std::byte>& body)
    -> std::vector<std::byte> {
    std::vector<std::byte> bytes;
    appendRaw(bytes, "CXPRES01");
    appendU32(bytes, kind);
    appendU32(bytes, 1);
    appendU64(bytes, 24ULL + body.size());
    bytes.insert(bytes.end(), body.begin(), body.end());
    return bytes;
}

[[nodiscard]] auto
makeShaderPayload(std::string_view vertexSource, std::string_view fragmentSource,
                  std::string_view textureParameter = {},
                  std::string_view profile = cuexis::playback::rendererProfileBuiltInV1,
                  std::string_view hostExtension = {}) -> std::vector<std::byte> {
    const bool textured = !textureParameter.empty();
    const bool hasHostExtension = !hostExtension.empty();
    std::vector<std::byte> body;
    appendU32(body, 0);
    appendU32(body, 4);
    appendU32(body, 4);
    appendU32(body, 0);
    appendU32(body, textured ? 1U : 0U);
    appendU32(body, textured ? 1U : 0U);
    appendU32(body, hasHostExtension ? 1U : 0U);
    appendU32(body, 1);
    appendU32(body, 0);
    appendU32(body, static_cast<std::uint32_t>(profile.size()));
    appendU32(body, static_cast<std::uint32_t>(vertexSource.size()));
    appendU32(body, static_cast<std::uint32_t>(fragmentSource.size()));
    appendRaw(body, "main");
    appendRaw(body, "main");
    appendRaw(body, profile);
    if (textured) {
        appendU32(body, static_cast<std::uint32_t>(textureParameter.size()));
        appendRaw(body, textureParameter);
        appendU32(body, 7);
        appendU32(body, 0);
        appendU32(body, 1);
        appendU32(body, 0);
        appendU32(body, 0);
        appendU32(body, 0);
        appendU32(body, 0);
        appendU32(body, 0);
        appendU32(body, 0);
        appendU32(body, 0);
        appendU32(body, 1);
        appendU32(body, 7);
        appendU32(body, static_cast<std::uint32_t>(textureParameter.size()));
        appendRaw(body, textureParameter);
    }
    if (hasHostExtension) {
        appendU32(body, static_cast<std::uint32_t>(hostExtension.size()));
        appendRaw(body, hostExtension);
    }
    appendRaw(body, vertexSource);
    appendRaw(body, fragmentSource);
    return wrapPayload(4, body);
}

[[nodiscard]] auto makeParameterizedPayload(std::string_view shaderAssetId,
                                            std::string_view textureAssetId = {})
    -> std::vector<std::byte> {
    const bool textured = !textureAssetId.empty();
    std::vector<std::byte> body;
    appendU32(body, 1);
    appendU32(body, 0);
    appendU32(body, 0);
    appendU32(body, textured ? 1U : 0U);
    appendU32(body, static_cast<std::uint32_t>(shaderAssetId.size()));
    appendU32(body, 0);
    appendRaw(body, shaderAssetId);
    if (textured) {
        appendU32(body, static_cast<std::uint32_t>(textureAssetId.size()));
        appendRaw(body, textureAssetId);
    }
    return wrapPayload(5, body);
}

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

[[nodiscard]] auto fixtureBytes(std::string_view relative) -> std::vector<std::byte> {
    return readBytes(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                     "stage3_project" / "assets" / relative);
}

[[nodiscard]] auto triangleMesh() -> std::vector<std::byte> {
    return fixtureBytes("meshes/triangle.mesh.bin");
}

[[nodiscard]] auto checkerTexture() -> std::vector<std::byte> {
    return fixtureBytes("textures/checker.texture.bin");
}

[[nodiscard]] auto parameterizedChart() -> std::string {
    return R"json({
  "format": "cuexis.chart",
  "version": 3,
  "chartId": "019f0000-0000-7abc-8def-000000000501",
  "metadata": { "title": "S5-C Parameterized", "artist": "Cuexis" },
  "timing": {
    "offsetMs": 0.0,
    "defaultBpm": 120.0,
    "tempoEvents": [],
    "stops": []
  },
  "camera": {
    "type": "perspective",
    "fovY": 60.0,
    "near": 0.1,
    "far": 1000.0,
    "defaultTransform": { "position": [0.0, 0.0, 5.0] }
  },
  "templates": [],
  "behaviors": [],
  "objects": [
    {
      "id": "019f0000-0000-7abc-8def-000000000510",
      "name": "sprite",
      "parent": null,
      "components": {
        "cuexis.transform": {
          "version": 1,
          "position": [0.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "cuexis.renderable": {
          "version": 1,
          "mesh": { "domain": "asset", "id": "mesh.triangle" },
          "material": { "domain": "asset", "id": "material.sprite" }
        }
      },
      "extensions": {}
    },
    {
      "id": "019f0000-0000-7abc-8def-000000000520",
      "name": "camera_main",
      "parent": null,
      "components": {
        "cuexis.transform": {
          "version": 1,
          "position": [0.0, 0.0, 5.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "cuexis.camera": {
          "version": 1,
          "type": "perspective",
          "fovY": 60.0,
          "near": 0.1,
          "far": 1000.0
        }
      },
      "extensions": {}
    }
  ],
  "requiredExtensions": [],
  "extensions": {}
})json";
}

constexpr std::string_view kVertex =
    "#version 450\nvoid main() {\n    gl_Position = vec4(0.0);\n}\n";
constexpr std::string_view kFragment = "#version 450\nlayout(location=0) out vec4 outColor;\nvoid "
                                       "main() {\n    outColor = vec4(1.0);\n}\n";

[[nodiscard]] auto descriptors(bool textured) -> std::vector<PlaybackAssetDescriptor> {
    std::vector<PlaybackAssetDescriptor> assets{
        {.id = "shader.sprite",
         .type = PlaybackAssetType::Shader,
         .rootId = "main",
         .logicalSource = "shaders/sprite.shader.bin",
         .dependencies = {}},
        {.id = "material.sprite",
         .type = PlaybackAssetType::Material,
         .rootId = "main",
         .logicalSource = "materials/sprite.material.bin",
         .dependencies = textured ? std::vector<std::string>{"shader.sprite", "texture.checker"}
                                  : std::vector<std::string>{"shader.sprite"}},
        {.id = "mesh.triangle",
         .type = PlaybackAssetType::Mesh,
         .rootId = "main",
         .logicalSource = "meshes/triangle.mesh.bin",
         .dependencies = {}},
    };
    if (textured) {
        assets.push_back({.id = "texture.checker",
                          .type = PlaybackAssetType::Texture,
                          .rootId = "main",
                          .logicalSource = "textures/checker.texture.bin",
                          .dependencies = {}});
    }
    return assets;
}

[[nodiscard]] auto makeSource(std::string_view vertexSource, std::string_view fragmentSource,
                              std::vector<std::byte> textureBytes = {},
                              std::string_view profile = cuexis::playback::rendererProfileBuiltInV1,
                              std::string_view hostExtension = {})
    -> cuexis::core::Result<PlaybackSource> {
    const bool textured = !textureBytes.empty();
    std::vector<cuexis::content::MemoryContentEntry> entries{
        {.rootId = "main",
         .source = "shaders/sprite.shader.bin",
         .bytes = makeShaderPayload(vertexSource, fragmentSource, textured ? "albedo" : "", profile,
                                    hostExtension),
         .revision = 1},
        {.rootId = "main",
         .source = "materials/sprite.material.bin",
         .bytes = makeParameterizedPayload("shader.sprite", textured ? "texture.checker" : ""),
         .revision = 1},
        {.rootId = "main",
         .source = "meshes/triangle.mesh.bin",
         .bytes = triangleMesh(),
         .revision = 1},
    };
    if (textured) {
        entries.push_back({.rootId = "main",
                           .source = "textures/checker.texture.bin",
                           .bytes = std::move(textureBytes),
                           .revision = 1});
    }
    auto provider = cuexis::content::MemoryContentProvider::create(std::move(entries));
    if (!provider) {
        return cuexis::core::unexpected(std::move(provider.error()));
    }
    cuexis::playback::TypedPlaybackProject project{
        .sourceId = "s5c-parameterized",
        .chartJson = parameterizedChart(),
        .assets = descriptors(textured),
    };
    return PlaybackSource::fromTypedProject(std::move(project), std::move(*provider));
}

[[nodiscard]] auto defaultCapabilities() -> PlaybackCapabilitySet {
    PlaybackSession session;
    auto capabilities = session.capabilities();
    REQUIRE(capabilities.has_value());
    return *capabilities;
}

[[nodiscard]] auto optInSession() -> PlaybackSession {
    auto capabilities = defaultCapabilities();
    CHECK(std::find(capabilities.ids.begin(), capabilities.ids.end(),
                    std::string{cuexis::playback::capabilityShaderAssetV1}) !=
          capabilities.ids.end());
    CHECK(std::find(capabilities.ids.begin(), capabilities.ids.end(),
                    std::string{cuexis::playback::capabilityMaterialParameterizedV1}) !=
          capabilities.ids.end());
    return PlaybackSession{std::move(capabilities)};
}

[[nodiscard]] auto trimmedSession() -> PlaybackSession {
    auto capabilities = defaultCapabilities();
    capabilities.ids.erase(
        std::remove_if(capabilities.ids.begin(), capabilities.ids.end(),
                       [](const std::string& id) {
                           return id == cuexis::playback::capabilityShaderAssetV1 ||
                                  id == cuexis::playback::capabilityMaterialParameterizedV1;
                       }),
        capabilities.ids.end());
    return PlaybackSession{std::move(capabilities)};
}

[[nodiscard]] auto hasDiagnostic(const cuexis::core::Diagnostics& diagnostics,
                                 std::string_view code) -> bool {
    return std::any_of(diagnostics.items().begin(), diagnostics.items().end(),
                       [&](const auto& item) { return item.code() == code; });
}

[[nodiscard]] auto diagnosticContext(const cuexis::core::Diagnostics& diagnostics,
                                     std::string_view code, std::string_view key,
                                     std::string_view value) -> bool {
    for (const auto& item : diagnostics.items()) {
        if (item.code() != code) {
            continue;
        }
        if (std::any_of(item.context().begin(), item.context().end(), [&](const auto& context) {
                return context.key == key && context.value == value;
            })) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto unlitPresentationCapabilities() -> PresentationCapabilities {
    return PresentationCapabilities{
        .opaquePass = true,
        .transparentPass = true,
        .linearTexture = true,
        .srgbTexture = true,
        .straightAlphaBlend = true,
        .backFaceCulling = true,
        .doubleSided = true,
        .debugPass = true,
        .maxResourceBytes = 64ULL * 1024ULL * 1024ULL,
        .maxTotalDecodedBytes = 512ULL * 1024ULL * 1024ULL,
        .maxTextureDimension = 8192,
        .maxMeshVertices = 1'048'576,
        .maxMeshIndices = 3'145'728,
    };
}

[[nodiscard]] auto parameterizedPresentationCapabilities() -> PresentationCapabilities {
    auto capabilities = unlitPresentationCapabilities();
    capabilities.version = 2;
    capabilities.parameterizedMaterial = true;
    capabilities.maxShaderSourceBytes = cuexis::playback::presentationMaxShaderSourceBytes;
    capabilities.maxSpirvBytes = cuexis::playback::presentationMaxSpirvBytes;
    capabilities.maxVariantKeywords = cuexis::playback::presentationMaxVariantKeywords;
    capabilities.maxVariantsPerShader = cuexis::playback::presentationMaxVariantsPerShader;
    capabilities.maxMaterialParameters = cuexis::playback::presentationMaxMaterialParameters;
    capabilities.maxTextureBindings = cuexis::playback::presentationMaxTextureBindings;
    return capabilities;
}

} // namespace

TEST_CASE("Default session accepts parameterized presentation content",
          "[playback][presentation][s5-g][capability]") {
    PlaybackSession session;
    auto source = makeSource(kVertex, kFragment);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto* manifest = prepared->presentationManifest();
    REQUIRE(manifest != nullptr);
    CHECK(std::any_of(manifest->entries.begin(), manifest->entries.end(), [](const auto& entry) {
        return entry.reference.type == PresentationResourceType::ParameterizedMaterial;
    }));
}

TEST_CASE("Trimmed session rejects parameterized content and keeps active Unlit",
          "[playback][presentation][s5-g][capability]") {
    auto session = trimmedSession();
    auto unlitSource = cuexis::playback::PlaybackSource::fromFilesystemProject(
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project");
    REQUIRE(unlitSource.has_value());
    REQUIRE(session.load(std::move(*unlitSource), cuexis::playback::PlaybackMode::ChartClock)
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto oldFrame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(oldFrame.has_value());
    REQUIRE_FALSE(oldFrame->objects.empty());
    const auto oldMaterial = oldFrame->objects.front().materialAssetId;

    auto parameterized = makeSource(kVertex, kFragment);
    REQUIRE(parameterized.has_value());
    const auto failed = session.prepareReload(std::move(*parameterized), {.chartTimeMs = 0.0},
                                              cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "playback.capability.preflight_failed");
    const auto diagnostics = session.lastOperationDiagnostics();
    REQUIRE(diagnostics.has_value());
    CHECK(
        std::any_of(diagnostics->items().begin(), diagnostics->items().end(), [](const auto& item) {
            return item.code() == "playback.capability.unsupported";
        }));

    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto after = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(after.has_value());
    REQUIRE_FALSE(after->objects.empty());
    CHECK(after->objects.front().materialAssetId == oldMaterial);
}

TEST_CASE("Opt-in session parses kind 4/5 and hashes shader source into material identity",
          "[playback][presentation][s5-c][identity]") {
    auto session = optInSession();
    auto source = makeSource(kVertex, kFragment);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto* manifest = prepared->presentationManifest();
    REQUIRE(manifest != nullptr);
    REQUIRE(manifest->entries.size() == 3);

    const auto shaderEntry =
        std::find_if(manifest->entries.begin(), manifest->entries.end(), [](const auto& entry) {
            return entry.reference.assetId == "shader.sprite" &&
                   entry.reference.type == PresentationResourceType::Shader;
        });
    const auto materialEntry =
        std::find_if(manifest->entries.begin(), manifest->entries.end(), [](const auto& entry) {
            return entry.reference.assetId == "material.sprite" &&
                   entry.reference.type == PresentationResourceType::ParameterizedMaterial;
        });
    REQUIRE(shaderEntry != manifest->entries.end());
    REQUIRE(materialEntry != manifest->entries.end());
    REQUIRE(materialEntry->dependencies.size() == 1);
    CHECK(materialEntry->dependencies.front().assetId == "shader.sprite");
    CHECK(materialEntry->dependencies.front().type == PresentationResourceType::Shader);

    auto shaderResource = prepared->acquirePresentationResource(shaderEntry->reference);
    REQUIRE(shaderResource.has_value());
    const auto* shader = std::get_if<PortableShader>(&(*shaderResource)->value);
    REQUIRE(shader != nullptr);
    CHECK(shader->vertexEntry == "main");
    CHECK(shader->requiredRendererProfile == cuexis::playback::rendererProfileBuiltInV1);

    auto materialResource = prepared->acquirePresentationResource(materialEntry->reference);
    REQUIRE(materialResource.has_value());
    const auto* material = std::get_if<PortableParameterizedMaterial>(&(*materialResource)->value);
    REQUIRE(material != nullptr);
    CHECK(material->shader.assetId == "shader.sprite");
    CHECK(material->shader.identity == shaderEntry->reference.identity);
    CHECK(material->shader.type == PresentationResourceType::Shader);

    const auto originalShaderIdentity = shaderEntry->reference.identity;
    const auto originalMaterialIdentity = materialEntry->reference.identity;

    auto changed = optInSession();
    auto changedSource =
        makeSource("#version 450\nvoid main() {\n    gl_Position = vec4(1.0);\n}\n", kFragment);
    REQUIRE(changedSource.has_value());
    auto changedPrepared =
        changed.prepareLoad(std::move(*changedSource), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(changedPrepared.has_value());
    const auto* changedManifest = changedPrepared->presentationManifest();
    REQUIRE(changedManifest != nullptr);
    const auto changedShader =
        std::find_if(changedManifest->entries.begin(), changedManifest->entries.end(),
                     [](const auto& entry) { return entry.reference.assetId == "shader.sprite"; });
    const auto changedMaterial = std::find_if(
        changedManifest->entries.begin(), changedManifest->entries.end(),
        [](const auto& entry) { return entry.reference.assetId == "material.sprite"; });
    REQUIRE(changedShader != changedManifest->entries.end());
    REQUIRE(changedMaterial != changedManifest->entries.end());
    CHECK(changedShader->reference.identity != originalShaderIdentity);
    CHECK(changedMaterial->reference.identity != originalMaterialIdentity);
}

TEST_CASE("Parameterized material identity changes when a texture parameter changes",
          "[playback][presentation][s5-c][identity]") {
    auto texture = checkerTexture();
    auto session = optInSession();
    auto source = makeSource(kVertex, kFragment, texture);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto* manifest = prepared->presentationManifest();
    REQUIRE(manifest != nullptr);
    const auto materialEntry =
        std::find_if(manifest->entries.begin(), manifest->entries.end(), [](const auto& entry) {
            return entry.reference.assetId == "material.sprite";
        });
    REQUIRE(materialEntry != manifest->entries.end());
    REQUIRE(materialEntry->dependencies.size() == 2);
    CHECK(materialEntry->dependencies[0].assetId == "shader.sprite");
    CHECK(materialEntry->dependencies[1].assetId == "texture.checker");
    const auto originalMaterialIdentity = materialEntry->reference.identity;

    texture.back() = static_cast<std::byte>(std::to_integer<unsigned char>(texture.back()) ^ 0xFFU);
    auto changed = optInSession();
    auto changedSource = makeSource(kVertex, kFragment, std::move(texture));
    REQUIRE(changedSource.has_value());
    auto changedPrepared =
        changed.prepareLoad(std::move(*changedSource), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(changedPrepared.has_value());
    const auto* changedManifest = changedPrepared->presentationManifest();
    REQUIRE(changedManifest != nullptr);
    const auto changedMaterial = std::find_if(
        changedManifest->entries.begin(), changedManifest->entries.end(),
        [](const auto& entry) { return entry.reference.assetId == "material.sprite"; });
    REQUIRE(changedMaterial != changedManifest->entries.end());
    CHECK(changedMaterial->reference.identity != originalMaterialIdentity);
}

TEST_CASE("Kind 4 shader source with CR is rejected without a compiler",
          "[playback][presentation][s5-c][encoding]") {
    auto session = optInSession();
    auto source = makeSource("#version 450\r\nvoid main() {}\n", kFragment);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(prepared.has_value());
    CHECK(prepared.error().code() == "playback.presentation.shader.source_encoding_invalid");
}

TEST_CASE("Frozen toolchain profile IDs match the Stage 5 contract",
          "[playback][presentation][s5-e][profile]") {
    CHECK(cuexis::playback::importerProfileShaderV1 == "cuexis.importer.shader.v1");
    CHECK(cuexis::playback::targetProfileSpirvV1 == "cuexis.target.spirv.v1");
    CHECK(cuexis::playback::targetProfileGlsl330V1 == "cuexis.target.glsl330.v1");
    CHECK(cuexis::playback::targetProfileGlslEs300V1 == "cuexis.target.glsles300.v1");
    CHECK(cuexis::playback::rendererProfilePortableV1 == "cuexis.renderer.portable.v1");
    CHECK(cuexis::playback::rendererProfileBuiltInV1 == "cuexis.renderer.builtin.v1");
}

TEST_CASE("Parameterized preflight order is parse, playback capability, then presentation",
          "[playback][presentation][s5-e][capability]") {
    {
        auto session = optInSession();
        auto source = makeSource("#version 450\r\nvoid main() {}\n", kFragment);
        REQUIRE(source.has_value());
        auto prepared =
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
        REQUIRE_FALSE(prepared.has_value());
        CHECK(prepared.error().code() == "playback.presentation.shader.source_encoding_invalid");
    }

    {
        auto session = trimmedSession();
        auto source = makeSource(kVertex, kFragment);
        REQUIRE(source.has_value());
        auto prepared =
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
        REQUIRE_FALSE(prepared.has_value());
        CHECK(prepared.error().code() == "playback.capability.preflight_failed");
    }

    auto session = optInSession();
    auto source = makeSource(kVertex, kFragment);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());

    const auto version1 = prepared->validatePresentation(unlitPresentationCapabilities(), {});
    CHECK_FALSE(version1.hasValue());
    CHECK(diagnosticContext(version1.diagnostics,
                            "playback.presentation.capability.required_missing", "capability",
                            "parameterized_material"));

    auto openglLike = unlitPresentationCapabilities();
    openglLike.version = 2;
    openglLike.builtInRendererProfileVersion = 1;
    const auto missingParameterized = prepared->validatePresentation(openglLike, {});
    CHECK_FALSE(missingParameterized.hasValue());
    CHECK(diagnosticContext(missingParameterized.diagnostics,
                            "playback.presentation.capability.required_missing", "capability",
                            "parameterized_material"));

    auto lowLimits = parameterizedPresentationCapabilities();
    lowLimits.maxShaderSourceBytes = 1;
    const auto insufficient = prepared->validatePresentation(lowLimits, {});
    CHECK_FALSE(insufficient.hasValue());
    CHECK(diagnosticContext(insufficient.diagnostics,
                            "playback.presentation.capability.limit_insufficient", "capability",
                            "max_shader_source_bytes"));

    auto headless = parameterizedPresentationCapabilities();
    CHECK(headless.builtInRendererProfileVersion == 0);
    const auto accepted = prepared->validatePresentation(headless, {});
    REQUIRE(accepted.hasValue());
    CHECK(accepted.settings->version == 1);
    CHECK(accepted.diagnostics.empty());

    const auto compileRequest = prepared->validatePresentation(
        headless, PresentationRequest{.version = 2, .enableShaderCompile = true});
    CHECK_FALSE(compileRequest.hasValue());
    CHECK(diagnosticContext(compileRequest.diagnostics,
                            "playback.presentation.capability.required_missing", "capability",
                            "shader_compile"));

    const auto hotReloadRequest = prepared->validatePresentation(
        headless, PresentationRequest{.version = 2, .enableShaderHotReload = true});
    REQUIRE(hotReloadRequest.hasValue());
    CHECK(hotReloadRequest.settings->version == 2);
    CHECK_FALSE(hotReloadRequest.settings->shaderCompileEnabled);
    CHECK_FALSE(hotReloadRequest.settings->shaderHotReloadEnabled);
    CHECK(hasDiagnostic(hotReloadRequest.diagnostics,
                        "playback.presentation.shader_hot_reload_unavailable"));

    auto compiling = parameterizedPresentationCapabilities();
    compiling.shaderGlsl450Source = true;
    compiling.shaderSpirv = true;
    compiling.shaderGlsl330 = true;
    const auto compileEnabled = prepared->validatePresentation(
        compiling, PresentationRequest{.version = 2, .enableShaderCompile = true});
    REQUIRE(compileEnabled.hasValue());
    CHECK(compileEnabled.settings->shaderCompileEnabled);
    CHECK_FALSE(compileEnabled.settings->shaderHotReloadEnabled);
}

TEST_CASE("Host extension IDs must be advertised by presentation capabilities",
          "[playback][presentation][s5-e][host-extension]") {
    constexpr std::string_view hostExtension = "hostExt";
    auto session = optInSession();
    auto source = makeSource(kVertex, kFragment, {}, hostExtension, hostExtension);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());

    auto missing = parameterizedPresentationCapabilities();
    const auto rejected = prepared->validatePresentation(missing, {});
    CHECK_FALSE(rejected.hasValue());
    CHECK(diagnosticContext(rejected.diagnostics,
                            "playback.presentation.capability.required_missing", "capability",
                            hostExtension));

    auto covered = parameterizedPresentationCapabilities();
    covered.hostExtensionIds.emplace_back(hostExtension);
    const auto accepted = prepared->validatePresentation(covered, {});
    REQUIRE(accepted.hasValue());
    CHECK(accepted.diagnostics.empty());
}

TEST_CASE("Validation Sink accepts kind 4/5 identity and schema",
          "[playback][presentation][s5-g][sink]") {
    PlaybackSession session;
    auto source = makeSource(kVertex, kFragment);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto* manifest = prepared->presentationManifest();
    REQUIRE(manifest != nullptr);

    const auto portableOnly = prepared->validatePresentation(unlitPresentationCapabilities(), {});
    CHECK_FALSE(portableOnly.hasValue());
    CHECK(diagnosticContext(portableOnly.diagnostics,
                            "playback.presentation.capability.required_missing", "capability",
                            "parameterized_material"));

    auto candidate = cuexis::test_support::prepareValidationCandidate(
        *prepared, parameterizedPresentationCapabilities(), {});
    REQUIRE(candidate.hasValue());
    REQUIRE(candidate.candidate.has_value());
    for (const auto& resource : candidate.candidate->resources()) {
        REQUIRE(resource);
        CHECK(cuexis::test_support::computePresentationIdentity(resource->value) ==
              resource->reference.identity);
    }
}

TEST_CASE("HostOverride MaterialTint updates parameterized sink effectiveColor",
          "[playback][presentation][s5-g][preview]") {
    constexpr std::string_view objectId = "019f0000-0000-7abc-8def-000000000510";
    PlaybackSession session;
    auto source = makeSource(kVertex, kFragment);
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    auto candidate = cuexis::test_support::prepareValidationCandidate(
        *prepared, parameterizedPresentationCapabilities(), {});
    REQUIRE(candidate.hasValue());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    cuexis::test_support::ValidationSink sink;
    sink.activate(std::move(*candidate.candidate));

    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto baseline = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(baseline.has_value());
    cuexis::test_support::ValidationSummary summary;
    REQUIRE(sink.validateFrame(*baseline, summary).has_value());
    REQUIRE((!summary.opaque.empty() || !summary.transparent.empty()));
    const auto& first =
        summary.opaque.empty() ? summary.transparent.front() : summary.opaque.front();
    CHECK(first.effectiveColor[0] == Catch::Approx(1.0));
    CHECK(first.effectiveColor[1] == Catch::Approx(1.0));
    CHECK(first.effectiveColor[2] == Catch::Approx(1.0));

    const auto mask =
        cuexis::playback::hostPropertyBit(cuexis::playback::HostPropertyId::MaterialTint);
    const auto token =
        session.acquireHostOverride("preview", 1, mask, {},
                                    std::array{cuexis::playback::HostOverrideWrite{
                                        .objectId = std::string{objectId},
                                        .property = cuexis::playback::HostPropertyId::MaterialTint,
                                        .value = cuexis::core::Vec3{0.25F, 0.50F, 0.75F},
                                    }});
    REQUIRE(token.has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto overridden = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(overridden.has_value());
    cuexis::test_support::ValidationSummary tinted;
    REQUIRE(sink.validateFrame(*overridden, tinted).has_value());
    REQUIRE((!tinted.opaque.empty() || !tinted.transparent.empty()));
    const auto& tintedCommand =
        tinted.opaque.empty() ? tinted.transparent.front() : tinted.opaque.front();
    CHECK(tintedCommand.effectiveColor[0] == Catch::Approx(0.25));
    CHECK(tintedCommand.effectiveColor[1] == Catch::Approx(0.50));
    CHECK(tintedCommand.effectiveColor[2] == Catch::Approx(0.75));
}
