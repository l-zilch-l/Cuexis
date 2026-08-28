#include <cuexis/project/asset_index_reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

std::string readText(const std::filesystem::path& file) {
    std::ifstream stream{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

bool hasDiagnostic(const cuexis::core::Diagnostics& diagnostics, std::string_view code,
                   std::string_view path = {}) {
    for (const auto& diagnostic : diagnostics.items()) {
        if (diagnostic.code() == code && (path.empty() || diagnostic.fieldPath() == path)) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("Asset Index Reader returns typed records without exposing a JSON DOM",
          "[project][asset-index]") {
    const auto fixture = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                         "stage1b_project" / "assets" / "cuexis.asset-index.json";
    const auto result = cuexis::project::AssetIndexReader::read(readText(fixture));

    REQUIRE(result.hasValue());
    REQUIRE(result.document->assets.size() == 3);
    CHECK(result.document->assets[0].id == "material.basic");
    CHECK(result.document->assets[0].type == cuexis::project::AssetType::Material);
    REQUIRE(result.document->assets[0].dependencies.size() == 1);
    CHECK(result.document->assets[0].dependencies[0] == "texture.white");
    CHECK(result.document->extensions.canonicalText == "{}");
}

TEST_CASE("Asset Index Reader rejects duplicate IDs dependencies and unsafe sources",
          "[project][asset-index][diagnostics]") {
    constexpr std::string_view invalid = R"json(
{
  "format": "cuexis.asset-index",
  "version": 1,
  "assets": [
    {"id":"mesh.note","type":"mesh","source":"../note.bin","dependencies":["texture.a","texture.a"]},
    {"id":"mesh.note","type":"shader","source":"note.bin","dependencies":[]}
  ],
  "extensions": {}
}
)json";
    const auto result = cuexis::project::AssetIndexReader::read(invalid);

    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result.diagnostics, "project.path.dot_segment", "$/assets/0/source"));
    CHECK(hasDiagnostic(result.diagnostics, "asset_index.dependency.duplicate",
                        "$/assets/0/dependencies/1"));
    CHECK(hasDiagnostic(result.diagnostics, "asset_index.id.duplicate", "$/assets/1/id"));
    CHECK(hasDiagnostic(result.diagnostics, "asset_index.type.unsupported", "$/assets/1/type"));
}

TEST_CASE("Asset Index Reader rejects unknown core fields and preserves opaque extensions",
          "[project][asset-index][extensions]") {
    constexpr std::string_view input = R"json(
{
  "format":"cuexis.asset-index",
  "version":1,
  "assets":[],
  "extensions":{"org.example":{"enabled":true}},
  "futureCore":true
}
)json";
    const auto result = cuexis::project::AssetIndexReader::read(input);

    CHECK_FALSE(result.hasValue());
    CHECK(result.diagnostics.hasWarnings());
    CHECK(hasDiagnostic(result.diagnostics, "json.field.unknown", "$/futureCore"));
}

TEST_CASE("Asset Index Reader routes v1 and v2 audio semantics", "[project][asset-index][v2]") {
    constexpr std::string_view v2 = R"json(
{
  "format":"cuexis.asset-index",
  "version":2,
  "assets":[
    {"id":"audio.main","type":"audio","source":"audio/main.wav","dependencies":[]},
    {"id":"mesh.note","type":"mesh","source":"mesh.bin","dependencies":[]}
  ],
  "extensions":{}
}
)json";
    const auto accepted = cuexis::project::AssetIndexReader::read(v2);
    REQUIRE(accepted.hasValue());
    REQUIRE(accepted.document->version == 2);
    REQUIRE(accepted.document->assets.size() == 2);
    CHECK(accepted.document->assets[0].type == cuexis::project::AssetType::Audio);

    auto v1 = std::string{v2};
    const auto version = v1.find("\"version\":2");
    REQUIRE(version != std::string::npos);
    v1.replace(version, std::string_view{"\"version\":2"}.size(), "\"version\":1");
    const auto rejected = cuexis::project::AssetIndexReader::read(v1);
    REQUIRE_FALSE(rejected.hasValue());
    CHECK(hasDiagnostic(rejected.diagnostics, "asset_index.type.unsupported", "$/assets/0/type"));
}

TEST_CASE("Asset Index v2 enforces audio leaf boundaries", "[project][asset-index][v2]") {
    constexpr std::string_view invalid = R"json(
{
  "format":"cuexis.asset-index",
  "version":2,
  "assets":[
    {"id":"audio.main","type":"audio","source":"audio/main.wav","dependencies":["mesh.note"]},
    {"id":"mesh.note","type":"mesh","source":"mesh.bin","dependencies":["audio.main"]}
  ],
  "extensions":{}
}
)json";
    const auto result = cuexis::project::AssetIndexReader::read(invalid);
    REQUIRE_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result.diagnostics, "asset_index.audio.dependencies_not_empty"));
    CHECK(hasDiagnostic(result.diagnostics, "asset_index.audio.dependency_forbidden"));
}

TEST_CASE("Asset Index Reader routes v3 shader and keeps v1/v2 rejection",
          "[project][asset-index][v3]") {
    constexpr std::string_view v3 = R"json(
{
  "format":"cuexis.asset-index",
  "version":3,
  "assets":[
    {"id":"shader.sprite","type":"shader","source":"shaders/sprite.shader.bin","dependencies":[]},
    {"id":"material.sprite","type":"material","source":"materials/sprite.material.bin","dependencies":["shader.sprite"]},
    {"id":"mesh.note","type":"mesh","source":"mesh.bin","dependencies":[]}
  ],
  "extensions":{}
}
)json";
    const auto accepted = cuexis::project::AssetIndexReader::read(v3);
    REQUIRE(accepted.hasValue());
    REQUIRE(accepted.document->version == 3);
    REQUIRE(accepted.document->assets.size() == 3);
    CHECK(accepted.document->assets[0].type == cuexis::project::AssetType::Shader);

    auto v1 = std::string{v3};
    const auto version = v1.find("\"version\":3");
    REQUIRE(version != std::string::npos);
    v1.replace(version, std::string_view{"\"version\":3"}.size(), "\"version\":1");
    const auto rejectedV1 = cuexis::project::AssetIndexReader::read(v1);
    REQUIRE_FALSE(rejectedV1.hasValue());
    CHECK(hasDiagnostic(rejectedV1.diagnostics, "asset_index.type.unsupported", "$/assets/0/type"));

    auto v2 = std::string{v3};
    v2.replace(v2.find("\"version\":3"), std::string_view{"\"version\":3"}.size(), "\"version\":2");
    const auto rejectedV2 = cuexis::project::AssetIndexReader::read(v2);
    REQUIRE_FALSE(rejectedV2.hasValue());
    CHECK(hasDiagnostic(rejectedV2.diagnostics, "asset_index.type.unsupported", "$/assets/0/type"));
}

TEST_CASE("Asset Index v3 enforces shader leaf boundaries", "[project][asset-index][v3]") {
    constexpr std::string_view invalid = R"json(
{
  "format":"cuexis.asset-index",
  "version":3,
  "assets":[
    {"id":"shader.sprite","type":"shader","source":"shaders/sprite.shader.bin","dependencies":["mesh.note"]},
    {"id":"mesh.note","type":"mesh","source":"mesh.bin","dependencies":["shader.sprite"]}
  ],
  "extensions":{}
}
)json";
    const auto result = cuexis::project::AssetIndexReader::read(invalid);
    REQUIRE_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result.diagnostics, "asset_index.shader.dependencies_not_empty"));
    CHECK(hasDiagnostic(result.diagnostics, "asset_index.shader.dependency_forbidden"));
}
