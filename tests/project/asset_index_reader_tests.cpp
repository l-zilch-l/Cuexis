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
