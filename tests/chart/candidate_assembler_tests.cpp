#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cuexis/chart/candidate_assembler.hpp>
#include <cuexis/chart/cxt_v2_loader.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
using namespace cuexis::chart;
auto metadata() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId.value = "019b0000-0000-7abc-8def-000000000001";
    return chart;
}
auto expand(const char* name) -> CanonicalSemanticChart {
    std::ifstream input{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                        "tests/fixtures/chart_format_foundation/valid" / name};
    REQUIRE(input);
    std::ostringstream text;
    text << input.rdbuf();
    CxtV2Invocation invocation;
    invocation.chartId = metadata().chartId;
    invocation.bindingId = "intro-stair";
    invocation.moduleId = "pattern.stair";
    invocation.exportId = "stair";
    auto result = CxtV2Loader::expand(text.str(), invocation);
    REQUIRE(result.chart);
    return std::move(*result.chart);
}
} // namespace

TEST_CASE("Candidate assembler derives actual requirements and canonical bytes",
          "[chart][candidate]") {
    const auto base = metadata();
    const std::array left{expand("pattern_stair.cxt")};
    const std::array right{expand("pattern_stair_reordered.cxt")};
    const auto assembled = assembleCandidateChart(base, left);
    const auto reordered = assembleCandidateChart(base, right);
    REQUIRE(assembled);
    REQUIRE(reordered);
    CHECK(assembled->entities.size() == 16);
    REQUIRE(assembled->features.size() == 1);
    CHECK(assembled->features.front().id == "cuexis.gameplay.candidate.lanes4");
    CHECK(packed::encode(*assembled).value() == packed::encode(*reordered).value());
    auto invalid = left;
    std::get<LaneConstraint>(invalid[0].entities[0].requirements[0].constraints[0]).lane = 4;
    CHECK_FALSE(assembleCandidateChart(base, invalid));
    auto duplicate = left;
    duplicate[0].entities.push_back(duplicate[0].entities[0]);
    CHECK_FALSE(assembleCandidateChart(base, duplicate));
    auto limited = PackedChartLimits{};
    limited.maxPackedEntities = 15;
    CHECK_FALSE(assembleCandidateChart(base, left, limited));
    auto foreign = left;
    foreign[0].chartId.value = "019b0000-0000-7abc-8def-000000000002";
    CHECK_FALSE(assembleCandidateChart(base, foreign));
}

TEST_CASE("Candidate CLI input is a real canonical Packed metadata artifact",
          "[chart][candidate-cli-input]") {
    const auto encoded = packed::encode(metadata());
    REQUIRE(encoded);
    const auto directory = std::filesystem::path{CUEXIS_BINARY_DIR} / "candidate-cli";
    std::filesystem::create_directories(directory);
    std::ofstream output{directory / "metadata.packed", std::ios::binary};
    output.write(reinterpret_cast<const char*>(encoded->data()),
                 static_cast<std::streamsize>(encoded->size()));
    REQUIRE(output.good());
    auto missingResource = metadata();
    missingResource.mainMusic = AssetId{"missing.music"};
    missingResource.resourceClosure.resources = {
        {*missingResource.mainMusic, CanonicalResourceUseKind::MainMusic}};
    const auto resourceBytes = packed::encode(missingResource);
    REQUIRE(resourceBytes);
    std::ofstream bad{directory / "missing-resource.packed", std::ios::binary};
    bad.write(reinterpret_cast<const char*>(resourceBytes->data()),
              static_cast<std::streamsize>(resourceBytes->size()));
    REQUIRE(bad.good());
}
