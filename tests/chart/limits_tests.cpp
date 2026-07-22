#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/limits.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>

namespace {

bool hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code) {
    return std::any_of(
        diagnostics.items().begin(), diagnostics.items().end(),
        [code](const cuexis::core::Diagnostic& diagnostic) { return diagnostic.code() == code; });
}

} // namespace

TEST_CASE("Chart default budgets match the accepted stage 1A limits", "[chart][limits]") {
    const cuexis::chart::ChartLimits limits;
    CHECK(limits.maxObjects == 100000);
    CHECK(limits.maxTemplates == 10000);
    CHECK(limits.maxBehaviors == 10000);
    CHECK(limits.maxPatchesPerTemplate == 256);
    CHECK(limits.maxDiagnostics == 1024);
    CHECK(limits.maxStringBytes == 1024U * 1024U);
}

TEST_CASE("Canonical loader applies the configured JSON string byte budget",
          "[chart][limits][json]") {
    cuexis::chart::ChartLimits limits;
    limits.maxStringBytes = 11;
    const auto loaded =
        cuexis::chart::CanonicalChartLoader::load(R"({"format":"cuexis.chart"})", limits);

    REQUIRE_FALSE(loaded.hasValue());
    CHECK(hasCode(loaded.diagnostics, "json.parse.string_limit"));
}

TEST_CASE("Canonical loader rejects an object count over the configured budget",
          "[chart][limits]") {
    constexpr std::string_view chart = R"json(
{"format":"cuexis.chart","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
 "templates":[],"behaviors":[],
 "objects":[
   {"id":"019b0000-0000-7abc-8def-000000000010","parent":null,"components":{"cuexis.element":{"version":1}},"extensions":{}},
   {"id":"019b0000-0000-7abc-8def-000000000011","parent":null,"components":{"cuexis.element":{"version":1}},"extensions":{}}
 ],"requiredExtensions":[],"extensions":{}}
)json";
    cuexis::chart::ChartLimits limits;
    limits.maxObjects = 1;
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(chart, limits);
    REQUIRE_FALSE(loaded.hasValue());
    CHECK(hasCode(loaded.diagnostics, "chart.limit.objects"));
}

TEST_CASE("Chart diagnostics stop at the configured budget and append one stable sentinel",
          "[chart][limits][diagnostics]") {
    constexpr std::string_view invalid = R"json(
{"format":"cuexis.chart","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
 "templates":[],"behaviors":[],"objects":[],"requiredExtensions":[],"extensions":{},
 "unknown1":1,"unknown2":2,"unknown3":3,"unknown4":4,"unknown5":5}
)json";
    cuexis::chart::ChartLimits limits;
    limits.maxDiagnostics = 2;
    const auto loaded = cuexis::chart::CanonicalChartLoader::load(invalid, limits);
    REQUIRE_FALSE(loaded.hasValue());
    CHECK(loaded.diagnostics.limitReached());
    CHECK(loaded.diagnostics.size() == 2);
    CHECK(hasCode(loaded.diagnostics, "chart.diagnostics.limit_exceeded"));
}

TEST_CASE("A zero chart diagnostic budget is rejected before parsing", "[chart][limits]") {
    cuexis::chart::ChartLimits limits;
    limits.maxDiagnostics = 0;
    const auto loaded = cuexis::chart::CanonicalChartLoader::load("{}", limits);
    REQUIRE_FALSE(loaded.hasValue());
    REQUIRE(loaded.diagnostics.size() == 1);
    CHECK(hasCode(loaded.diagnostics, "chart.limits.invalid"));
}
