#include <cuexis/chart/chart_loader.hpp>

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

TEST_CASE("ChartLoader explicitly routes canonical and simple formats", "[chart][loader]") {
    constexpr std::string_view canonical = R"json(
{"format":"cuexis.chart","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
 "templates":[],"behaviors":[],
 "objects":[{"id":"019b0000-0000-7abc-8def-000000000010","parent":null,
             "components":{"cuexis.element":{"version":1}},"extensions":{}}],
 "requiredExtensions":[],"extensions":{}}
)json";
    constexpr std::string_view simple = R"json(
{"format":"cuexis.chart.simple","version":1,
 "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
 "timing":{"offsetMs":0,"bpm":120},"templates":{},"behaviors":{},
 "objects":{"lane":{"kind":"element"}},"extensions":{}}
)json";

    const auto canonicalResult = cuexis::chart::ChartLoader::load(canonical);
    const auto simpleResult = cuexis::chart::ChartLoader::load(simple);
    CHECK(canonicalResult.hasValue());
    CHECK(simpleResult.hasValue());
}

TEST_CASE("ChartLoader rejects missing typed and unknown format discriminators",
          "[chart][loader][diagnostics]") {
    const auto missing = cuexis::chart::ChartLoader::load("{}");
    REQUIRE_FALSE(missing.hasValue());
    CHECK(hasCode(missing.diagnostics, "json.field.missing"));

    const auto typed = cuexis::chart::ChartLoader::load(R"json({"format":42})json");
    REQUIRE_FALSE(typed.hasValue());
    CHECK(hasCode(typed.diagnostics, "json.type.mismatch"));

    const auto unknown =
        cuexis::chart::ChartLoader::load(R"json({"format":"cuexis.chart.future"})json");
    REQUIRE_FALSE(unknown.hasValue());
    CHECK(hasCode(unknown.diagnostics, "chart.format.unsupported"));
}
