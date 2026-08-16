#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/limits.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"Could not read CFU-F4 Chart fixture: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto fixture(std::string_view relative) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           relative;
}

[[nodiscard]] auto importChart(std::string_view imports) -> std::string {
    auto result = std::string{R"({
      "format":"cuexis.chart","version":4,
      "chartId":"01a00000-0000-7abc-8def-000000000f40","metadata":{},
      "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
      "parameters":[],"templates":[],"behaviors":[],
      "animationTemplateImports":)"};
    result += imports;
    result += R"(,"animationClips":[],"objects":[],
      "requiredExtensions":[],"extensions":{}})";
    return result;
}

template <typename Result>
[[nodiscard]] auto hasDiagnostic(const Result& result, std::string_view code) -> bool {
    return std::ranges::any_of(result.diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

} // namespace

TEST_CASE("CFU-F4 Chart v4 import totals accept exact max and reject boundary plus one",
          "[chart][v4][cfu-f4][limits][cxt]") {
    constexpr std::string_view imports =
        R"([{"id":"motion.a","source":"templates/a.cxt"},{"id":"motion.b","source":"templates/b.cxt"}])";
    auto limits = cuexis::chart::ChartLimits{};
    limits.maxAnimationImports = 2;

    const auto exact = cuexis::chart::ChartV4Loader::load(importChart(imports), limits);
    REQUIRE(exact.hasValue());
    CHECK(exact.document->animationTemplateImports.size() == 2);

    limits.maxAnimationImports = 1;
    const auto over = cuexis::chart::ChartV4Loader::load(importChart(imports), limits);
    CHECK_FALSE(over.hasValue());
    CHECK(hasDiagnostic(over, "cxt.budget.exceeded"));
}

TEST_CASE("CFU-F4 resolved animation aggregate totals accept exact max and reject plus one",
          "[chart][v4][cfu-f4][limits][animation]") {
    const auto loaded =
        cuexis::chart::ChartV4Loader::load(readText(fixture("valid/chart_v4_animation.json")));
    REQUIRE(loaded.hasValue());

    std::size_t trackCount = 0;
    std::size_t segmentAndStepCount = 0;
    for (const auto& clip : loaded.document->animationClips) {
        trackCount += clip.tracks.size() + clip.stepTracks.size();
        for (const auto& track : clip.tracks) {
            segmentAndStepCount += track.segments.size();
        }
        for (const auto& track : clip.stepTracks) {
            segmentAndStepCount += track.steps.size();
        }
    }
    REQUIRE(trackCount > 0);
    REQUIRE(segmentAndStepCount > 0);

    auto limits = cuexis::chart::ChartLimits{};
    limits.maxAnimationTracks = trackCount;
    limits.maxAnimationSegmentsAndSteps = segmentAndStepCount;
    const auto exact =
        cuexis::chart::ChartV4Resolver::resolve(*loaded.document, {}, {}, {}, limits);
    REQUIRE(exact.hasValue());

    limits.maxAnimationTracks = trackCount - 1U;
    const auto overTracks =
        cuexis::chart::ChartV4Resolver::resolve(*loaded.document, {}, {}, {}, limits);
    CHECK_FALSE(overTracks.hasValue());
    CHECK(hasDiagnostic(overTracks, "chart.animation.generated_limit"));

    limits.maxAnimationTracks = trackCount;
    limits.maxAnimationSegmentsAndSteps = segmentAndStepCount - 1U;
    const auto overSegments =
        cuexis::chart::ChartV4Resolver::resolve(*loaded.document, {}, {}, {}, limits);
    CHECK_FALSE(overSegments.hasValue());
    CHECK(hasDiagnostic(overSegments, "chart.animation.generated_limit"));
}
