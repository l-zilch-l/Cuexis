#include <cuexis/project/project_loader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

bool hasDiagnostic(const cuexis::core::Diagnostics& diagnostics, std::string_view code,
                   std::string_view path = {}) {
    for (const auto& diagnostic : diagnostics.items()) {
        if (diagnostic.code() == code && (path.empty() || diagnostic.fieldPath() == path)) {
            return true;
        }
    }
    return false;
}

std::filesystem::path fixtureRoot() {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "stage1b_project";
}

class TemporaryDirectory final {
  public:
    TemporaryDirectory() {
        const auto token = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cuexis-project-tests-" + std::to_string(token));
        std::filesystem::copy(fixtureRoot(), path_, std::filesystem::copy_options::recursive);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

constexpr std::string_view minimalProject = R"json(
{
  "format":"cuexis.project",
  "version":1,
  "projectId":"019b0000-0000-7abc-8def-000000000100",
  "assetRoots":[{"id":"main","path":"assets"}],
  "entry":{"chart":{"root":"main","path":"charts/stage1b_example.cuexis.chart.json"}},
  "extensions":{}
}
)json";

} // namespace

TEST_CASE("ProjectConfig Reader returns a typed configuration", "[project][reader]") {
    const auto result = cuexis::project::ProjectConfigReader::read(minimalProject);

    REQUIRE(result.hasValue());
    CHECK(result.config->projectId == "019b0000-0000-7abc-8def-000000000100");
    REQUIRE(result.config->assetRoots.size() == 1);
    CHECK(result.config->entry.chart.root == "main");
    CHECK(result.config->extensions.canonicalText == "{}");
}

TEST_CASE("ProjectConfig Reader rejects non-portable paths and invalid UUIDs",
          "[project][reader][security]") {
    constexpr std::string_view invalid = R"json(
{
  "format":"cuexis.project",
  "version":1,
  "projectId":"019B0000-0000-7ABC-8DEF-000000000100",
  "assetRoots":[{"id":"main","path":"assets\\content"}],
  "entry":{"chart":{"root":"main","path":"../outside.chart.json"}},
  "extensions":{}
}
)json";
    const auto result = cuexis::project::ProjectConfigReader::read(invalid);

    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result.diagnostics, "project.id.invalid", "$/projectId"));
    CHECK(hasDiagnostic(result.diagnostics, "project.path.backslash", "$/assetRoots/0/path"));
    CHECK(hasDiagnostic(result.diagnostics, "project.path.dot_segment", "$/entry/chart/path"));
}

TEST_CASE("Project Loader accepts a directory or exact fixed file locator", "[project][loader]") {
    const auto fromDirectory = cuexis::project::ProjectLoader::load(fixtureRoot());
    REQUIRE(fromDirectory.hasValue());
    CHECK(fromDirectory.project->assetRoots.size() == 1);
    CHECK(std::filesystem::is_regular_file(fromDirectory.project->chartFile));
    CHECK(fromDirectory.project->findAssetRoot("main") != nullptr);
    CHECK(fromDirectory.project->findAssetRoot("missing") == nullptr);

    const auto fromFile = cuexis::project::ProjectLoader::load(
        fixtureRoot() / std::string{cuexis::project::projectFileName});
    REQUIRE(fromFile.hasValue());
    CHECK(fromFile.project->projectRoot == fromDirectory.project->projectRoot);

    const auto wrongFile =
        cuexis::project::ProjectLoader::load(fixtureRoot() / "assets" / "cuexis.asset-index.json");
    CHECK_FALSE(wrongFile.hasValue());
    CHECK(hasDiagnostic(wrongFile.diagnostics, "project.locator.invalid", "$"));
}

TEST_CASE("Project Loader rejects physically overlapping roots and bounded input overflow",
          "[project][loader][security]") {
    constexpr std::string_view overlapping = R"json(
{
  "format":"cuexis.project",
  "version":1,
  "projectId":"019b0000-0000-7abc-8def-000000000100",
  "assetRoots":[
    {"id":"main","path":"assets"},
    {"id":"charts","path":"assets/charts"}
  ],
  "entry":{"chart":{"root":"main","path":"charts/stage1b_example.cuexis.chart.json"}},
  "extensions":{}
}
)json";
    const auto overlap = cuexis::project::ProjectLoader::loadText(overlapping, fixtureRoot());
    CHECK_FALSE(overlap.hasValue());
    CHECK(hasDiagnostic(overlap.diagnostics, "project.asset_root.overlap", "$/assetRoots/1/path"));

    cuexis::project::ProjectLimits limits;
    limits.maxInputBytes = 16;
    const auto tooLarge = cuexis::project::ProjectLoader::load(fixtureRoot(), limits);
    CHECK_FALSE(tooLarge.hasValue());
    CHECK(hasDiagnostic(tooLarge.diagnostics, "project.file.size_limit", "$"));
}

TEST_CASE("Project Loader atomically saves and reloads a complete configuration",
          "[project][save]") {
    TemporaryDirectory temporary;
    auto loaded = cuexis::project::ProjectLoader::load(temporary.path());
    REQUIRE(loaded.hasValue());

    auto changed = loaded.project->config;
    changed.projectId = "019b0000-0000-7abc-8def-000000000101";
    const auto saved = cuexis::project::ProjectLoader::saveAtomic(changed, temporary.path());
    REQUIRE(saved.has_value());

    const auto reloaded = cuexis::project::ProjectLoader::load(temporary.path());
    REQUIRE(reloaded.hasValue());
    CHECK(reloaded.project->config.projectId == changed.projectId);

    changed.projectId = "invalid";
    const auto rejected = cuexis::project::ProjectLoader::saveAtomic(changed, temporary.path());
    CHECK_FALSE(rejected.has_value());
    const auto retained = cuexis::project::ProjectLoader::load(temporary.path());
    REQUIRE(retained.hasValue());
    CHECK(retained.project->config.projectId == "019b0000-0000-7abc-8def-000000000101");
}
