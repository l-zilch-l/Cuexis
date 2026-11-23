#include <cuexis/chart/packed_chart_io.hpp>

#include <catch2/catch_test_macros.hpp>

#include "packed_fixture_support.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

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
    requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
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

// The card-level name is the CTest name, so the Catch2 case keeps exactly this identifier and the
// description lives in the tags and comments.
TEST_CASE("rr2_contract_packed", "[chart][packed][rr2][crc][codec]") {
    const auto encoded = cuexis::chart::packed::encode(tapChart());
    REQUIRE(encoded);
    const auto bytes = std::span<const std::byte>{encoded->data(), encoded->size()};

    // The same readable sample passes and its stored Header CRC is exactly CRC-32 over [0,92).
    REQUIRE(cuexis::chart::PackedChartReader::decode(*encoded));
    const auto storedCrc = cuexis::chart::packed::test::readU32(bytes, 92U);
    CHECK(storedCrc == cuexis::chart::packed::crc32(bytes.first(92U)));

    // The superseded wording hashed the complete 96-byte Header with the CRC field zeroed. A file
    // carrying that value in the CRC field must be rejected, not silently accepted as a valid
    // first-92-byte checksum.
    std::vector<std::byte> oldHeader(encoded->begin(), encoded->begin() + 96);
    cuexis::chart::packed::test::writeU32(oldHeader, 92U, 0U);
    const auto oldAlgorithmCrc =
        cuexis::chart::packed::crc32(std::span<const std::byte>{oldHeader});
    REQUIRE(oldAlgorithmCrc != storedCrc);
    auto oldCrcFile = *encoded;
    cuexis::chart::packed::test::writeU32(oldCrcFile, 92U, oldAlgorithmCrc);
    const auto rejectedCrc = cuexis::chart::PackedChartReader::decode(oldCrcFile);
    REQUIRE_FALSE(rejectedCrc);
    CHECK(rejectedCrc.error().code() == "packed.header.crc");

    // TIME starts with offsetMs, defaultBpm, tempoCount, stopCount and then three one-byte Beat
    // codec modes; mode 1 is GridDelta and is not in the Foundation temporal acceptance set.
    const auto time = cuexis::chart::packed::test::findSection(bytes, "TIME");
    REQUIRE(time);
    REQUIRE(time->offset + time->size <= bytes.size());
    auto timePayload = std::vector<std::byte>(
        bytes.begin() + static_cast<std::ptrdiff_t>(time->offset),
        bytes.begin() + static_cast<std::ptrdiff_t>(time->offset + time->size));
    REQUIRE(timePayload.size() > 24U);
    timePayload[24] = std::byte{1};
    auto timeMode = cuexis::chart::packed::test::replaceSection(bytes, time->index, timePayload);
    REQUIRE_FALSE(timeMode.empty());
    // The envelope, directory and every section CRC stay consistent; only the temporal mode fails.
    CHECK(cuexis::chart::packed::inspect(timeMode).has_value());
    const auto rejectedTime = cuexis::chart::PackedChartReader::decode(timeMode);
    REQUIRE_FALSE(rejectedTime);
    CHECK(rejectedTime.error().code() == "packed.time.invalid");

    // REQ0 starts with the startBeat/endBeat Beat descriptors; a non-zero mode is unsupported.
    const auto requirements = cuexis::chart::packed::test::findSection(bytes, "REQ0");
    REQUIRE(requirements);
    REQUIRE(requirements->offset + requirements->size <= bytes.size());
    auto requirementPayload = std::vector<std::byte>(
        bytes.begin() + static_cast<std::ptrdiff_t>(requirements->offset),
        bytes.begin() +
            static_cast<std::ptrdiff_t>(requirements->offset + requirements->size));
    REQUIRE_FALSE(requirementPayload.empty());
    requirementPayload[0] = std::byte{1};
    auto requirementMode =
        cuexis::chart::packed::test::replaceSection(bytes, requirements->index, requirementPayload);
    REQUIRE_FALSE(requirementMode.empty());
    CHECK(cuexis::chart::packed::inspect(requirementMode).has_value());
    const auto rejectedRequirements = cuexis::chart::PackedChartReader::decode(requirementMode);
    REQUIRE_FALSE(rejectedRequirements);
    CHECK(rejectedRequirements.error().code() == "packed.requirements.codec");
}
