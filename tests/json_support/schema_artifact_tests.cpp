#include <cuexis/json/parse.hpp>
#include <cuexis/json/schema.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open JSON artifact: " + path.string()};
    }

    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto parseArtifact(const std::filesystem::path& path) -> cuexis::json::Value {
    const auto text = readFile(path);
    auto parsed =
        cuexis::json::parse(text, cuexis::json::ParseLimits{2U * 1024U * 1024U, 64, 16384});
    REQUIRE(parsed.has_value());
    return std::move(*parsed);
}

void requireValid(const cuexis::json::Value& schema, const cuexis::json::Value& instance) {
    cuexis::core::Diagnostics diagnostics;
    const auto result = cuexis::json::validateAgainstSchema(instance, schema, diagnostics);
    REQUIRE(result.has_value());
    CHECK_FALSE(diagnostics.hasErrors());
}

void requireInvalid(const cuexis::json::Value& schema, std::string_view instanceText) {
    const auto instance =
        cuexis::json::parse(instanceText, cuexis::json::ParseLimits{1024U * 1024U, 32, 16384});
    REQUIRE(instance.has_value());

    cuexis::core::Diagnostics diagnostics;
    const auto result = cuexis::json::validateAgainstSchema(*instance, schema, diagnostics);
    REQUIRE(result.has_value());
    CHECK(diagnostics.hasErrors());
}

} // namespace

TEST_CASE("Stage 1B project and asset index schemas accept the shipped fixture",
          "[json][schema][artifact]") {
    const auto source = std::filesystem::path{CUEXIS_SOURCE_DIR};
    const auto projectSchema = parseArtifact(source / "schemas" / "cuexis.project.v1.schema.json");
    const auto indexSchema =
        parseArtifact(source / "schemas" / "cuexis.asset-index.v1.schema.json");
    const auto fixture = source / "tests" / "fixtures" / "stage1b_project";

    requireValid(projectSchema, parseArtifact(fixture / "cuexis.project.json"));
    requireValid(indexSchema, parseArtifact(fixture / "assets" / "cuexis.asset-index.json"));
}

TEST_CASE("Stage 1B schemas reject unknown core fields and unsupported asset types",
          "[json][schema][artifact]") {
    const auto source = std::filesystem::path{CUEXIS_SOURCE_DIR};
    const auto projectSchema = parseArtifact(source / "schemas" / "cuexis.project.v1.schema.json");
    const auto indexSchema =
        parseArtifact(source / "schemas" / "cuexis.asset-index.v1.schema.json");

    requireInvalid(projectSchema, R"({
        "format":"cuexis.project",
        "version":1,
        "projectId":"019b0000-0000-7abc-8def-000000000100",
        "assetRoots":[{"id":"main","path":"assets"}],
        "entry":{"chart":{"root":"main","path":"charts/example.json"}},
        "extensions":{},
        "displayName":"not-a-v1-field"
    })");

    requireInvalid(indexSchema, R"({
        "format":"cuexis.asset-index",
        "version":1,
        "assets":[{
            "id":"audio.theme",
            "type":"audio",
            "source":"audio/theme.bin",
            "dependencies":[]
        }],
        "extensions":{}
    })");
}
