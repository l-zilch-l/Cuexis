// R0 baseline characterization tests for the Chart Format Foundation hardening stage.
//
// Every case here records the *current*, pre-hardening behaviour of the candidate Packed Chart
// implementation so that R0 can prove a finding before anything is changed. Finding IDs follow
// the 2026-09-16 handoff review (H01-H04) plus the R0 section-registry findings (A01/A02).
//
// These cases deliberately assert the defect, not the contract. The fix batches flip each CHECK
// into the behaviour required by docs/formats/PACKED_CHART_FORMAT.md, so a flipped case that
// fails afterwards is the regression signal.

#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using cuexis::chart::CanonicalEntity;
using cuexis::chart::CanonicalFeature;
using cuexis::chart::CanonicalIntervalKind;
using cuexis::chart::CanonicalSemanticChart;
using cuexis::chart::ChartId;
using cuexis::chart::ChartObjectId;
using cuexis::chart::ExplicitEntityIdentity;
using cuexis::chart::GeneratedEntityIdentity;
using cuexis::chart::LaneConstraint;
using cuexis::chart::PackedChartLimits;
using cuexis::chart::RationalBeat;
using cuexis::chart::SemanticIdentityStep;

auto emptyChart() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.defaultCamera = cuexis::chart::CameraData{};
    return chart;
}

auto tapChart() -> CanonicalSemanticChart {
    auto chart = emptyChart();
    chart.features.push_back(CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    cuexis::chart::CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.judgementDomain = {"candidate.lanes4", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{2});
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000010"}};
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));
    return chart;
}

[[nodiscard]] auto readU32(std::span<const std::byte> bytes, std::size_t offset) -> std::uint32_t {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
                 << (8U * index);
    }
    return value;
}

void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (8U * index)) & 0xffU);
    }
}

void appendU32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (8U * index)) & 0xffU));
    }
}

struct SectionView final {
    std::size_t offset{};
    std::size_t size{};
};

[[nodiscard]] auto findSection(std::span<const std::byte> bytes, std::string_view code)
    -> SectionView {
    const auto count = readU32(bytes, 24U);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto base = 96U + 32U * static_cast<std::size_t>(index);
        const std::string_view name{reinterpret_cast<const char*>(bytes.data() + base), 4U};
        if (name == code) {
            return SectionView{readU32(bytes, base + 8U), readU32(bytes, base + 12U)};
        }
    }
    return {};
}

void refreshHeaderCrc(std::vector<std::byte>& bytes) {
    const auto crc = cuexis::chart::packed::crc32(std::span<const std::byte>{bytes.data(), 92U});
    writeU32(bytes, 92U, crc);
}

// Appends one registered inspection section to a valid Packed file. The helper only rewrites the
// directory, the header counters and the header CRC, so the result stays a structurally valid
// artifact and isolates the question under test (which sections a reader accepts).
[[nodiscard]] auto appendInspectionSection(std::span<const std::byte> input, std::string_view code,
                                           std::span<const std::byte> payload)
    -> std::vector<std::byte> {
    const auto directoryCount = readU32(input, 24U);
    const auto directoryBytes = readU32(input, 28U);
    const auto totalBytes = readU32(input, 16U);
    const auto decodedBytes = readU32(input, 76U);
    const auto payloadBytes = static_cast<std::uint32_t>(payload.size());
    std::vector<std::byte> output;
    output.reserve(totalBytes + 32U + payloadBytes);
    output.insert(output.end(), input.begin(), input.begin() + 96);
    for (std::uint32_t index = 0; index < directoryCount; ++index) {
        const auto base = 96U + 32U * static_cast<std::size_t>(index);
        output.insert(output.end(), input.begin() + static_cast<std::ptrdiff_t>(base),
                      input.begin() + static_cast<std::ptrdiff_t>(base + 32U));
        writeU32(output, output.size() - 32U + 8U, readU32(input, base + 8U) + 32U);
    }
    for (const auto character : code) {
        output.push_back(static_cast<std::byte>(character));
    }
    output.push_back(std::byte{0}); // codec: uncompressed
    output.push_back(std::byte{1}); // flags: inspection
    output.push_back(std::byte{0});
    output.push_back(std::byte{0});
    appendU32(output, totalBytes + 32U);
    appendU32(output, payloadBytes);
    appendU32(output, payloadBytes);
    appendU32(output, payloadBytes);
    appendU32(output, cuexis::chart::packed::crc32(payload));
    appendU32(output, 0U);
    output.insert(output.end(), input.begin() + static_cast<std::ptrdiff_t>(96U + directoryBytes),
                  input.end());
    output.insert(output.end(), payload.begin(), payload.end());
    writeU32(output, 16U, totalBytes + 32U + payloadBytes);
    writeU32(output, 24U, directoryCount + 1U);
    writeU32(output, 28U, directoryBytes + 32U);
    writeU32(output, 76U, decodedBytes + payloadBytes);
    refreshHeaderCrc(output);
    return output;
}

} // namespace

// R0-H01 was flipped by R1: the writer now publishes the Spec 10.2 digest instead of zeros.
TEST_CASE("R1-H01 Packed writer publishes the recomputed semantic identity in the header",
          "[chart][packed][hardening][r1]") {
    const auto encoded = cuexis::chart::packed::encode(tapChart());
    REQUIRE(encoded);
    REQUIRE(encoded->size() > 96U);
    const auto allZero = std::all_of(encoded->begin() + 32, encoded->begin() + 64,
                                     [](std::byte value) { return value == std::byte{0}; });
    CHECK_FALSE(allZero);

    const auto identity = cuexis::chart::packed::semanticIdentity(tapChart());
    REQUIRE(identity);
    CHECK(std::equal(identity->begin(), identity->end(), encoded->begin() + 32,
                     [](std::uint8_t left, std::byte right) {
                         return left == std::to_integer<std::uint8_t>(right);
                     }));
}

// R0-H01 was flipped by R1: a body that contradicts the declared digest is rejected.
TEST_CASE("R1-H01 Packed reader rejects a tampered semanticIdentity even with a valid header CRC",
          "[chart][packed][hardening][r1]") {
    const auto encoded = cuexis::chart::packed::encode(tapChart());
    REQUIRE(encoded);
    auto tampered = *encoded;
    std::fill(tampered.begin() + 32, tampered.begin() + 64, std::byte{0xff});
    refreshHeaderCrc(tampered);

    // The artifact is structurally valid, so only the semantic identity comparison can fail.
    CHECK(cuexis::chart::packed::inspect(tampered));
    const auto decoded = cuexis::chart::packed::decode(tampered);
    REQUIRE_FALSE(decoded);
    CHECK(decoded.error().code() == "packed.identity.mismatch");
    const auto bridged = cuexis::chart::PackedChartReader::decode(tampered);
    REQUIRE_FALSE(bridged);
}

TEST_CASE("R0-H02 Range requirements outside the registered profile are encoded and decoded",
          "[chart][packed][hardening][r0]") {
    auto chart = tapChart();
    auto& requirement = chart.entities.front().requirements.front();
    const auto endBeat = RationalBeat::create(1, 1);
    REQUIRE(endBeat);
    requirement.interval.kind = CanonicalIntervalKind::HalfOpenRange;
    requirement.interval.endBeat = *endBeat;

    // Spec 7.6 registers kind=tap with interval=point (wire 0) only. The writer publishes the
    // out-of-profile range row instead of refusing it, and the reader accepts any interval <= 1.
    const auto encoded = cuexis::chart::packed::encode(chart);
    CHECK(encoded);
    if (encoded) {
        CHECK(cuexis::chart::packed::decode(*encoded));
    }
}

TEST_CASE("R0-H02 Requirements are accepted without the registered feature declaration",
          "[chart][packed][hardening][r0]") {
    SECTION("feature table is empty while REQ0 is present") {
        auto chart = tapChart();
        chart.features.clear();
        const auto encoded = cuexis::chart::packed::encode(chart);
        CHECK(encoded);
        if (encoded) {
            CHECK(cuexis::chart::packed::decode(*encoded));
        }
    }
    SECTION("feature version is not the registered version") {
        auto chart = tapChart();
        chart.features = {CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 2}};
        const auto encoded = cuexis::chart::packed::encode(chart);
        CHECK(encoded);
        if (encoded) {
            CHECK(cuexis::chart::packed::decode(*encoded));
        }
    }
    SECTION("feature id is not the registered id") {
        auto chart = tapChart();
        chart.features = {CanonicalFeature{"cuexis.gameplay.candidate.other", 1}};
        const auto encoded = cuexis::chart::packed::encode(chart);
        CHECK(encoded);
        if (encoded) {
            CHECK(cuexis::chart::packed::decode(*encoded));
        }
    }
}

TEST_CASE("R0-H04 A custom maxPackedSectionBytes does not constrain any entry point",
          "[chart][packed][hardening][r0]") {
    const auto chart = tapChart();
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    auto limits = PackedChartLimits{};
    limits.maxPackedSectionBytes = 1U; // Far below every emitted section.

    // The declared per-section ceiling is never read, so a limit that is orders of magnitude
    // smaller than the artifact is silently ignored by inspect, decode and the writer bridge.
    CHECK(cuexis::chart::packed::inspect(*encoded, limits));
    CHECK(cuexis::chart::packed::decode(*encoded, limits));
    CHECK(cuexis::chart::PackedChartWriter::size(chart, {}, limits));
}

TEST_CASE("R0-A01 A registered DBG0 inspection section is rejected by the table reader",
          "[chart][packed][hardening][r0]") {
    const auto encoded = cuexis::chart::packed::encode(tapChart());
    REQUIRE(encoded);
    const std::array<std::byte, 4> payload{std::byte{'d'}, std::byte{'b'}, std::byte{'g'},
                                           std::byte{'0'}};
    const auto withDebug = appendInspectionSection(*encoded, "DBG0", payload);

    // Spec 5.3 registers DBG0 as an optional inspection section with flags=1, and the IO bridge
    // lists DBG0 as known before delegating. The table reader's registry omits DBG0, so a
    // Spec-registered artifact is refused as an unknown semantic section.
    CHECK_FALSE(cuexis::chart::packed::inspect(withDebug));
    CHECK_FALSE(cuexis::chart::packed::decode(withDebug));
    CHECK_FALSE(cuexis::chart::PackedChartReader::decode(withDebug));
}

// R0-A02 was flipped by R1: IDN0 now stores the Spec 6.5 scope/path tables.
TEST_CASE("R1-A02 IDN0 stores the Spec 6.5 scope and path tables",
          "[chart][packed][hardening][r1]") {
    auto chart = tapChart();
    GeneratedEntityIdentity identity;
    identity.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    identity.bindingId = "binding0";
    identity.moduleId = "module0";
    identity.exportId = "emit";
    identity.path.push_back(SemanticIdentityStep{"step", 0});
    CanonicalEntity generated;
    generated.identity = std::move(identity);
    chart.entities.push_back(std::move(generated));

    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    const auto section = findSection(*encoded, "IDN0");
    REQUIRE(section.size > 30U);
    // Spec 6.5: u32 scopeCount, scopes[], u32 pathCount, paths[], u32 identityCount.
    // This chart has nine strings, so every string index is a one-byte LEB128: scopeCount at 0,
    // one scope of 16 + 3 bytes, pathCount at 23, one path of 1 + (1 + 1) bytes, identityCount
    // at 30.
    CHECK(readU32(*encoded, section.offset) == 1U);
    CHECK(readU32(*encoded, section.offset + 23U) == 1U);
    CHECK(readU32(*encoded, section.offset + 30U) == 2U);
    CHECK(cuexis::chart::packed::decode(*encoded));
}
