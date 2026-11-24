// RR-F302 Chart boundary contract: the base Reader/CXT1/v4 Resolver, the codec Reader and the
// tools source/migration paths must all stay reachable while living in separate libraries.
// One CTest name covers the four boundary groups so a configuration without the candidate
// switch still proves the old Chart capabilities.

#include <cuexis/chart/animation_template_loader.hpp>
#include <cuexis/chart/candidate_assembler.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_migrator.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/cxt_v2_loader.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_reader.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/chart/packed_chart_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

namespace fs = std::filesystem;
using namespace cuexis::chart;

[[nodiscard]] auto readFixture(std::string_view relative) -> std::string {
    std::ifstream input{fs::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / relative,
                        std::ios::binary};
    REQUIRE(input);
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

[[nodiscard]] auto canonicalChartJson() -> std::string_view {
    return R"json(
{"format":"cuexis.chart","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
 "templates":[],"behaviors":[],
 "objects":[{"id":"019b0000-0000-7abc-8def-000000000010","parent":null,
             "components":{"cuexis.element":{"version":1}},"extensions":{}}],
 "requiredExtensions":[],"extensions":{}}
)json";
}

[[nodiscard]] auto emptyChart() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.defaultCamera = CameraData{};
    return chart;
}

[[nodiscard]] auto tapChart() -> CanonicalSemanticChart {
    auto chart = emptyChart();
    chart.features.push_back({"cuexis.gameplay.candidate.lanes4", 1});
    CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.interval.startBeat = RationalBeat::zero();
    requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{2});
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000010"}};
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));
    return chart;
}

} // namespace

TEST_CASE("rr_f302_chart_boundaries", "[chart][f302][boundaries]") {
    // Base Chart: the old Reader accepts the canonical v1 document without any candidate or
    // packed codec involvement.
    SECTION("base Reader") {
        const auto loaded = ChartLoader::load(canonicalChartJson());
        REQUIRE(loaded.hasValue());
        CHECK(loaded.document->version == 1);
        CHECK(loaded.document->chartId.value == "019b0000-0000-7abc-8def-000000000001");
        REQUIRE(loaded.document->objects.size() == 1);
    }

    // Base Chart: the frozen CXT1 animation-template reader still accepts the promoted fixture.
    SECTION("CXT1 reader") {
        const auto loaded = AnimationTemplateLoader::load(
            readFixture("chart_format_update/valid/templates/move-y.cxt"));
        REQUIRE(loaded.hasValue());
        CHECK(loaded.document->templateId == "motion.move-y");
        CHECK(loaded.document->clip.tracks.size() == 1);
    }

    // Base Chart: v4 loading and resolution keep their symbols in the base library.
    SECTION("v4 loader and resolver") {
        const auto loaded = ChartV4Loader::load(
            readFixture("chart_format_update/valid/chart_v4_parameterized_transform.json"));
        REQUIRE(loaded.hasValue());
        const auto resolved = ChartV4Resolver::resolve(*loaded.document);
        REQUIRE(resolved.hasValue());
        CHECK(resolved.artifact->document.chart.camera.fovY == 60.0);
    }

    // Codec: a typed Writer artifact round-trips through the Reader bridge, while a truncated
    // input keeps the structural diagnostic and produces no semantic chart.
    SECTION("Packed read and write") {
        const auto target = fs::temp_directory_path() / "cuexis-rr-f302-chart-boundaries.packed";
        std::error_code ignored;
        fs::remove(target, ignored);
        const auto written = PackedChartWriter::writeAtomic(tapChart(), target);
        REQUIRE(written.has_value());
        CHECK(written->headerBytes == 96U);
        const auto loaded = PackedChartReader::read(target);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->entities.size() == 1U);
        REQUIRE(loaded->entities.front().requirements.size() == 1U);
        CHECK(loaded->entities.front().requirements.front().localId == "hit");
        fs::remove(target, ignored);

        const std::array<std::byte, 3> truncated{};
        const auto rejected = PackedChartReader::decode(truncated);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "packed.header.invalid_size");
    }

    // Tools: CXT2 expansion and the candidate assembler remain available to tests and tools even
    // when the candidate Playback switch is OFF.
    SECTION("CXT2 and candidate assembly") {
        CxtV2Invocation invocation;
        invocation.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
        invocation.bindingId = "intro-stair";
        invocation.moduleId = "pattern.stair";
        invocation.exportId = "stair";
        const auto expanded = CxtV2Loader::expand(
            readFixture("chart_format_foundation/valid/pattern_stair.cxt"), invocation);
        REQUIRE(expanded.chart.has_value());
        CHECK(expanded.counts.entityCount == 16U);
        CHECK(expanded.counts.requirementCount == 16U);

        const std::array fragments{std::move(*expanded.chart)};
        const auto assembled = assembleCandidateChart(emptyChart(), fragments);
        REQUIRE(assembled.has_value());
        CHECK(assembled->entities.size() == 16U);
        REQUIRE(assembled->features.size() == 1U);
        CHECK(assembled->features.front().id == "cuexis.gameplay.candidate.lanes4");
        const auto encoded = packed::encode(*assembled);
        REQUIRE(encoded.has_value());
        CHECK(packed::decode(*encoded).has_value());
    }

    // Tools: migration stays the same algorithm and remains deterministic; a v3 result reloads
    // through the base Reader and a v4 migration exposes a typed v4 source document.
    SECTION("migration comparison") {
        const auto source = readFixture("stage2_migration_v1.cuexis.chart.json");
        const auto first = ChartMigrator::migrateToV3(source);
        const auto second = ChartMigrator::migrateToV3(source);
        REQUIRE(first.hasValue());
        REQUIRE(second.hasValue());
        CHECK(first.artifact->chartJson == second.artifact->chartJson);
        CHECK(first.artifact->reportJson == second.artifact->reportJson);
        CHECK(first.artifact->document.version == 3U);
        const auto reloaded = ChartLoader::load(first.artifact->chartJson);
        REQUIRE(reloaded.hasValue());
        CHECK(reloaded.document->version == 3U);

        const auto v4 = ChartMigrator::migrateToV4(source);
        REQUIRE(v4.hasValue());
        REQUIRE(v4.artifact->v4Document.has_value());
        CHECK(v4.artifact->v4Document->chartId.value == first.artifact->document.chartId.value);
        const auto resolved = ChartV4Resolver::resolve(*v4.artifact->v4Document);
        REQUIRE(resolved.hasValue());
        CHECK(resolved.artifact->document.chart.chartId.value ==
              first.artifact->document.chartId.value);
    }
}
