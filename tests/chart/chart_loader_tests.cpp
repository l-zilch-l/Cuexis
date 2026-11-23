#include <cuexis/chart/chart_loader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

bool hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code) {
    return std::any_of(
        diagnostics.items().begin(), diagnostics.items().end(),
        [code](const cuexis::core::Diagnostic& diagnostic) { return diagnostic.code() == code; });
}

} // namespace

TEST_CASE("ChartLoader routes canonical charts and rejects the retired Simple format",
          "[chart][loader]") {
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
    REQUIRE_FALSE(simpleResult.hasValue());
    CHECK(hasCode(simpleResult.diagnostics, "chart.format.unsupported"));
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

namespace {

// A v1/v2/v3 canonical chart whose only Beat is the cuexis.note component. The timing field name
// changes at v3; everything else is deliberately valid so a limit rejection is the only failure.
auto chartWithNoteBeat(int version, std::string_view numerator, std::string_view denominator)
    -> std::string {
    const auto timing =
        version == 3
            ? R"("timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]})"
            : R"("timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]})";
    return std::string{"{\"format\":\"cuexis.chart\",\"version\":"} + std::to_string(version) +
           R"(,"chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},)" + timing +
           R"(,"templates":[],"behaviors":[],"objects":[)"
           R"({"id":"019b0000-0000-7abc-8def-000000000010","parent":null,)"
           R"("components":{"cuexis.note":{"version":1,"beat":{"numerator":)" +
           std::string{numerator} + R"(,"denominator":)" + std::string{denominator} +
           R"(}}},"extensions":{}}],"requiredExtensions":[],"extensions":{}})";
}

} // namespace

TEST_CASE("rr2_contract_beat", "[chart][loader][beat][rr2][limits]") {
    const cuexis::chart::ChartLimits limits{};
    const auto maximumNumerator = limits.maxBeatNumeratorMagnitude;
    const auto maximumDenominator = limits.maxBeatDenominator;
    REQUIRE(maximumNumerator > 0);
    REQUIRE(maximumDenominator > 0);

    for (const int version : {1, 2, 3}) {
        INFO("chart version " << version);
        // v1-v3 的最小合法外形已由 candidate-debug 的 rr2_contract_beat 实测覆盖（Test #170）：
        // 空 tempoEvents/behaviors 不引入其它诊断，边界与越界都由 chart.beat.limit_exceeded 判定。

        // Simplest rationals at the authoritative bound load successfully.
        CHECK(cuexis::chart::ChartLoader::load(
                  chartWithNoteBeat(version, std::to_string(maximumNumerator), "1"))
                  .hasValue());
        CHECK(cuexis::chart::ChartLoader::load(
                  chartWithNoteBeat(version, std::to_string(-maximumNumerator), "1"))
                  .hasValue());
        CHECK(cuexis::chart::ChartLoader::load(
                  chartWithNoteBeat(version, "1", std::to_string(maximumDenominator)))
                  .hasValue());

        // One past the bound is a stable limit rejection, on both numerator signs and on the
        // denominator. The check runs after reduction, so each case spells a reduced rational.
        const auto positiveOver = cuexis::chart::ChartLoader::load(
            chartWithNoteBeat(version, std::to_string(maximumNumerator + 1), "1"));
        REQUIRE_FALSE(positiveOver.hasValue());
        CHECK(hasCode(positiveOver.diagnostics, "chart.beat.limit_exceeded"));

        const auto negativeOver = cuexis::chart::ChartLoader::load(
            chartWithNoteBeat(version, std::to_string(-maximumNumerator - 1), "1"));
        REQUIRE_FALSE(negativeOver.hasValue());
        CHECK(hasCode(negativeOver.diagnostics, "chart.beat.limit_exceeded"));

        const auto denominatorOver = cuexis::chart::ChartLoader::load(
            chartWithNoteBeat(version, "1", std::to_string(maximumDenominator + 1)));
        REQUIRE_FALSE(denominatorOver.hasValue());
        CHECK(hasCode(denominatorOver.diagnostics, "chart.beat.limit_exceeded"));
    }
}
