#include <cuexis/chart/packed_chart_io.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>

namespace {

auto emptyChart() -> cuexis::chart::CanonicalSemanticChart {
    cuexis::chart::CanonicalSemanticChart chart;
    chart.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.defaultCamera = cuexis::chart::CameraData{};
    return chart;
}

auto tapChart() -> cuexis::chart::CanonicalSemanticChart {
    auto chart = emptyChart();
    chart.features.push_back({"cuexis.gameplay.candidate.lanes4", 1});
    cuexis::chart::CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.interval.startBeat = cuexis::chart::RationalBeat::zero();
    requirement.judgementDomain = {"candidate.lanes4", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(cuexis::chart::LaneConstraint{2});
    cuexis::chart::CanonicalEntity entity;
    entity.identity = cuexis::chart::ExplicitEntityIdentity{
        cuexis::chart::ChartObjectId{"019b0000-0000-7abc-8def-000000000010"}};
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));
    return chart;
}

} // namespace

TEST_CASE("Packed writer sizes and atomically replaces an output", "[chart][packed][f5]") {
    const auto target = std::filesystem::temp_directory_path() / "cuexis-packed-chart-test.bin";
    std::error_code ignored;
    std::filesystem::remove(target, ignored);

    const auto result = cuexis::chart::PackedChartWriter::writeAtomic(emptyChart(), target);
    REQUIRE(result);
    CHECK(result->statistics.packedBytes > 96U);
    CHECK(result->headerBytes == 96U);
    CHECK(std::filesystem::file_size(target) == result->statistics.packedBytes);

    const auto loaded = cuexis::chart::PackedChartReader::read(target);
    REQUIRE(loaded);
    CHECK(loaded->chartId.value == "019b0000-0000-7abc-8def-000000000001");
    std::filesystem::remove(target, ignored);
}

TEST_CASE("Packed reader rejects an input larger than the configured file budget",
          "[chart][packed][f6]") {
    const auto target = std::filesystem::temp_directory_path() / "cuexis-packed-chart-limit.bin";
    {
        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        output << "oversized";
    }
    auto limits = cuexis::chart::PackedChartLimits{};
    limits.maxPackedFileBytes = 1U;
    const auto loaded = cuexis::chart::PackedChartReader::read(target, limits);
    REQUIRE_FALSE(loaded);
    // R3 unified the file byte gate on one diagnostic across the reader, the writer and the byte
    // level validators, replacing the packed.io.file_limit alias.
    CHECK(loaded.error().code() == "packed.budget.file_bytes");
    std::error_code ignored;
    std::filesystem::remove(target, ignored);
}

TEST_CASE("Packed reader restores a static tap requirement", "[chart][packed][f6]") {
    const auto encoded = cuexis::chart::packed::encode(tapChart());
    REQUIRE(encoded);
    const auto decoded = cuexis::chart::PackedChartReader::decode(*encoded);
    REQUIRE(decoded);
    REQUIRE(decoded->entities.size() == 1U);
    REQUIRE(decoded->entities.front().requirements.size() == 1U);
    CHECK(decoded->entities.front().requirements.front().localId == "hit");
    CHECK(std::get<cuexis::chart::LaneConstraint>(
              decoded->entities.front().requirements.front().constraints.front())
              .lane == 2U);
}

TEST_CASE("Packed reader rejects truncated bytes before semantic publication",
          "[chart][packed][f6]") {
    const std::array<std::byte, 3> truncated{};
    const auto loaded = cuexis::chart::PackedChartReader::decode(truncated);
    REQUIRE_FALSE(loaded);
}
