#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_migrator.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not read migration fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto fixture(std::string_view name) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / name;
}

[[nodiscard]] bool hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code) {
    return std::any_of(diagnostics.items().begin(), diagnostics.items().end(),
                       [code](const auto& item) { return item.code() == code; });
}

} // namespace

TEST_CASE("ChartMigrator converts keyframes, bindings, templates, and reports deterministically",
          "[chart][migration][stage2]") {
    const auto source = readFile(fixture("stage2_migration_v1.cuexis.chart.json"));
    const auto first = cuexis::chart::ChartMigrator::migrateToV3(source);
    const auto second = cuexis::chart::ChartMigrator::migrateToV3(source);
    REQUIRE(first.hasValue());
    REQUIRE(second.hasValue());
    CHECK(first.artifact->chartJson == second.artifact->chartJson);
    CHECK(first.artifact->reportJson == second.artifact->reportJson);

    const auto& artifact = *first.artifact;
    CHECK(artifact.document.version == 3);
    CHECK(artifact.document.templates.empty());
    CHECK(artifact.document.behaviors.size() == 2);
    CHECK(artifact.report.sourceVersion == 1);
    CHECK(artifact.report.targetVersion == 3);
    CHECK(artifact.report.convertedBehaviors == 3);
    CHECK(artifact.report.generatedEvents == 6);
    CHECK(artifact.report.rewrittenBindings == 9);
    CHECK(artifact.report.expandedTemplateObjects == 1);
    CHECK(artifact.report.unboundBehaviorIds == std::vector<std::string>{"unbound.motion"});

    const auto shared =
        std::find_if(artifact.document.behaviors.begin(), artifact.document.behaviors.end(),
                     [](const auto& behavior) { return behavior.id.value == "shared.motion"; });
    REQUIRE(shared != artifact.document.behaviors.end());
    REQUIRE(shared->events.size() == 5);
    const auto rotation =
        std::find_if(shared->events.begin(), shared->events.end(), [](const auto& event) {
            return event.property == cuexis::chart::BehaviorProperty::TransformRotation;
        });
    REQUIRE(rotation != shared->events.end());
    CHECK(rotation->durationBeats.toString() == "1/1");
    CHECK(rotation->startSlope == Catch::Approx(0.0));
    CHECK(rotation->endSlope == Catch::Approx(3.0));
    const auto& midpoint = std::get<cuexis::core::Quat>(rotation->endValue);
    CHECK(midpoint.z == Catch::Approx(0.70710677F));
    CHECK(midpoint.w == Catch::Approx(0.70710677F));

    for (const auto& object : artifact.document.objects) {
        CHECK_FALSE(object.sourceTemplate.has_value());
    }
    REQUIRE(artifact.document.objects[0].components.transform.has_value());
    CHECK(artifact.document.objects[0].components.transform->position.x == Catch::Approx(1.0F));
    CHECK(artifact.document.objects[0].components.transform->scale ==
          cuexis::core::Vec3{1.0F, 2.0F, 3.0F});
    REQUIRE(artifact.document.objects[2].components.transform.has_value());
    CHECK(artifact.document.objects[2].components.transform->position.x == Catch::Approx(7.0F));
    CHECK_FALSE(artifact.document.objects[2].components.behavior.has_value());

    const auto reloaded = cuexis::chart::ChartLoader::load(artifact.chartJson);
    REQUIRE(reloaded.hasValue());
    const auto compiled = cuexis::chart::ChartCompiler::compile(*reloaded.document);
    REQUIRE(compiled.hasValue());
}

TEST_CASE("ChartMigrator golden output and report remain byte-stable",
          "[chart][migration][stage2][golden]") {
    const auto source = readFile(fixture("stage2_migration_v1.cuexis.chart.json"));
    const auto migrated = cuexis::chart::ChartMigrator::migrateToV3(source);
    REQUIRE(migrated.hasValue());
    CHECK(migrated.artifact->chartJson ==
          readFile(fixture("stage2_migration_v3.golden.cuexis.chart.json")));
    CHECK(migrated.artifact->reportJson ==
          readFile(fixture("stage2_migration_report.golden.json")));
}

TEST_CASE("ChartMigrator rejects v3 sources without producing an artifact",
          "[chart][migration][stage2][failure]") {
    const auto source = readFile(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "charts" /
                                 "stage2_example.cuexis.chart.json");
    const auto migrated = cuexis::chart::ChartMigrator::migrateToV3(source);
    REQUIRE_FALSE(migrated.hasValue());
    CHECK_FALSE(migrated.artifact.has_value());
    CHECK(hasCode(migrated.diagnostics, "chart.migration.source_version_unsupported"));
}

TEST_CASE("ChartMigrator preserves the v2 main-music contract", "[chart][migration][stage2][v2]") {
    const auto source =
        readFile(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                 "stage1d_project" / "assets" / "charts" / "stage1d_example.cuexis.chart.json");
    const auto migrated = cuexis::chart::ChartMigrator::migrateToV3(source);
    REQUIRE(migrated.hasValue());
    CHECK(migrated.artifact->report.sourceVersion == 2);
    REQUIRE(migrated.artifact->document.audio.has_value());
    CHECK(migrated.artifact->document.audio->mainMusic.value == "audio.main");

    const auto reloaded = cuexis::chart::ChartLoader::load(migrated.artifact->chartJson);
    REQUIRE(reloaded.hasValue());
    CHECK(reloaded.document->version == 3);
    REQUIRE(reloaded.document->audio.has_value());
    CHECK(reloaded.document->audio->mainMusic.value == "audio.main");
    CHECK(cuexis::chart::ChartCompiler::compile(*reloaded.document).hasValue());
}

TEST_CASE("ChartMigrator rejects an unbound single-key Track without dropping data",
          "[chart][migration][stage2][failure]") {
    constexpr std::string_view source = R"json({
  "format":"cuexis.chart",
  "version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000001",
  "metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],
  "behaviors":[{
    "id":"unbound.single",
    "type":"behavior.transform.keyframe",
    "version":1,
    "tracks":[{
      "property":"transform.position.x",
      "keys":[{"beat":{"numerator":0,"denominator":1},"value":4.0}]
    }]
  }],
  "objects":[],
  "requiredExtensions":[],
  "extensions":{}
})json";

    const auto migrated = cuexis::chart::ChartMigrator::migrateToV3(source);
    REQUIRE_FALSE(migrated.hasValue());
    CHECK_FALSE(migrated.artifact.has_value());
    CHECK(hasCode(migrated.diagnostics, "chart.migration.unbound_single_key_unrepresentable"));
}

TEST_CASE("ChartMigrator lifts a static v3 Chart to canonical empty-animation v4",
          "[chart][migration][cfu-d][v4]") {
    const auto source = readFile(fixture("chart_format_update/valid/chart_v3_static_migration.json"));
    const auto first = cuexis::chart::ChartMigrator::migrateToV4(source);
    const auto second = cuexis::chart::ChartMigrator::migrateToV4(source);
    REQUIRE(first.hasValue());
    REQUIRE(second.hasValue());
    CHECK(first.artifact->chartJson == second.artifact->chartJson);
    CHECK(first.artifact->reportJson == second.artifact->reportJson);

    const auto& artifact = *first.artifact;
    CHECK(artifact.document.version == 4);
    REQUIRE(artifact.v4Document.has_value());
    CHECK(artifact.v4Document->parameters.empty());
    CHECK(artifact.v4Document->animationTemplateImports.empty());
    CHECK(artifact.v4Document->animationClips.empty());
    CHECK(artifact.v4Document->animators.empty());
    CHECK(artifact.report.sourceVersion == 3);
    CHECK(artifact.report.targetVersion == 4);
    CHECK(artifact.report.convertedBehaviors == 0);
    CHECK(artifact.report.generatedEvents == 0);
    CHECK(artifact.report.rewrittenBindings == 0);
    CHECK(artifact.report.expandedTemplateObjects == 0);
    CHECK(artifact.report.unboundBehaviorIds.empty());
    CHECK(artifact.report.generatedClips == 0);
    CHECK(artifact.report.generatedBindings == 0);
    CHECK(artifact.report.generatedParameters == 0);
    CHECK(artifact.report.discardedFields.empty());
    REQUIRE(artifact.report.fieldCounts.has_value());
    CHECK(artifact.report.fieldCounts->animationClips == 0);
    CHECK(artifact.report.fieldCounts->animationTemplateImports == 0);
    CHECK(artifact.report.fieldCounts->parameters == 0);
    CHECK(artifact.report.fieldCounts->objects == 1);
    CHECK(artifact.report.sourceCanonicalIdentity.has_value());
    CHECK(artifact.report.targetCanonicalIdentity.has_value());
    CHECK(artifact.report.sourceCanonicalIdentity->size() == 64);
    CHECK(artifact.report.targetCanonicalIdentity->size() == 64);
    CHECK(*artifact.report.sourceCanonicalIdentity != *artifact.report.targetCanonicalIdentity);
    CHECK(artifact.report.warnings.empty());
    CHECK(artifact.report.diagnostics.empty());
    CHECK(artifact.chartJson ==
          readFile(fixture("chart_format_update/golden/chart_v4_static_migration.canonical.json")));

    const auto reloaded = cuexis::chart::ChartV4Loader::load(artifact.chartJson);
    REQUIRE(reloaded.hasValue());
    CHECK(reloaded.document->legacyProjection.version == 4);
}

TEST_CASE("ChartMigrator reuses v3 migration when lifting v1 to v4",
          "[chart][migration][cfu-d][v4][v1]") {
    const auto source = readFile(fixture("stage2_migration_v1.cuexis.chart.json"));
    const auto first = cuexis::chart::ChartMigrator::migrateToV4(source);
    const auto second = cuexis::chart::ChartMigrator::migrateToV4(source);
    REQUIRE(first.hasValue());
    REQUIRE(second.hasValue());
    CHECK(first.artifact->chartJson == second.artifact->chartJson);
    CHECK(first.artifact->reportJson == second.artifact->reportJson);

    const auto& artifact = *first.artifact;
    CHECK(artifact.document.version == 4);
    CHECK(artifact.report.sourceVersion == 1);
    CHECK(artifact.report.targetVersion == 4);
    CHECK(artifact.report.convertedBehaviors == 3);
    CHECK(artifact.report.generatedEvents == 6);
    CHECK(artifact.report.rewrittenBindings == 9);
    CHECK(artifact.report.expandedTemplateObjects == 1);
    CHECK(artifact.report.unboundBehaviorIds == std::vector<std::string>{"unbound.motion"});
    CHECK(artifact.report.generatedClips == 0);
    CHECK(artifact.report.generatedBindings == 0);
    CHECK(artifact.report.generatedParameters == 0);
    REQUIRE(artifact.v4Document.has_value());
    CHECK(artifact.v4Document->parameters.empty());
    CHECK(artifact.v4Document->animationTemplateImports.empty());
    CHECK(artifact.v4Document->animationClips.empty());
    CHECK(artifact.report.sourceCanonicalIdentity.has_value());
    CHECK(artifact.report.targetCanonicalIdentity.has_value());
    CHECK(artifact.report.sourceCanonicalIdentity->size() == 64);
    CHECK(artifact.report.fieldCounts.has_value());
    CHECK(artifact.report.fieldCounts->behaviors == 2);
    CHECK(artifact.report.fieldCounts->objects == 3);
    CHECK(artifact.report.fieldCounts->parameters == 0);
    CHECK(artifact.chartJson ==
          readFile(fixture("chart_format_update/golden/chart_v1_to_v4.cuexis.chart.json")));

    const auto reloaded = cuexis::chart::ChartV4Loader::load(artifact.chartJson);
    REQUIRE(reloaded.hasValue());
}

TEST_CASE("ChartMigrator preserves v3 tempoEvents and templates when lifting to v4",
          "[chart][migration][cfu-d][v4][tempo]") {
    const auto source = readFile(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "charts" /
                                 "stage2_example.cuexis.chart.json");
    const auto migrated = cuexis::chart::ChartMigrator::migrateToV4(source);
    REQUIRE(migrated.hasValue());
    CHECK(migrated.artifact->document.version == 4);
    REQUIRE(migrated.artifact->document.timing.tempoEvents.size() == 2);
    REQUIRE(migrated.artifact->document.timing.stops.size() == 1);
    CHECK(migrated.artifact->chartJson.find("\"tempoEvents\"") != std::string::npos);
    CHECK(migrated.artifact->chartJson.find("\"startBpm\": 90.0") != std::string::npos);
    CHECK(migrated.artifact->chartJson.find("\"durationMs\": 250.0") != std::string::npos);
    CHECK(migrated.artifact->v4Document->parameters.empty());
    CHECK(migrated.artifact->v4Document->animationClips.empty());

    constexpr std::string_view templated = R"json({
  "format":"cuexis.chart",
  "version":3,
  "chartId":"019b0000-0000-7abc-8def-000000000001",
  "metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "templates":[{
    "id":"019b0000-0000-7abc-8def-000000000020",
    "name":"note",
    "extends":null,
    "prototype":{"components":{"cuexis.element":{"version":1}}},
    "extensions":{}
  }],
  "behaviors":[],
  "objects":[],
  "requiredExtensions":[],
  "extensions":{}
})json";
    const auto templatedResult = cuexis::chart::ChartMigrator::migrateToV4(templated);
    REQUIRE(templatedResult.hasValue());
    CHECK(templatedResult.artifact->document.templates.size() == 1);
    CHECK(templatedResult.artifact->chartJson.find("019b0000-0000-7abc-8def-000000000020") !=
          std::string::npos);
    CHECK(templatedResult.artifact->v4Document->parameters.empty());
}

TEST_CASE("ChartMigrator rejects v4 sources and invalid v3 without producing an artifact",
          "[chart][migration][cfu-d][failure]") {
    const auto v4 = readFile(fixture("chart_format_update/valid/chart_v4_static_migration.json"));
    const auto rejectedV4 = cuexis::chart::ChartMigrator::migrateToV4(v4);
    REQUIRE_FALSE(rejectedV4.hasValue());
    CHECK_FALSE(rejectedV4.artifact.has_value());
    CHECK(hasCode(rejectedV4.diagnostics, "chart.migration.source_version_unsupported"));

    constexpr std::string_view invalidV3 = R"json({
  "format":"cuexis.chart",
  "version":3,
  "chartId":"not-a-uuid",
  "metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "templates":[],
  "behaviors":[],
  "objects":[],
  "requiredExtensions":[],
  "extensions":{}
})json";
    const auto rejectedInvalid = cuexis::chart::ChartMigrator::migrateToV4(invalidV3);
    REQUIRE_FALSE(rejectedInvalid.hasValue());
    CHECK_FALSE(rejectedInvalid.artifact.has_value());
}
