#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_migrator.hpp>
#include <cuexis/chart/chart_runtime.hpp>

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
