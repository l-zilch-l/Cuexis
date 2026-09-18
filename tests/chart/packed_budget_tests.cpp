// R3 contract tests: the frozen Foundation Packed budgets and the checked arithmetic of the
// candidate reader and writer.
//
// R3 keeps the 16 MiB file gate and the 40,000 entity gate, makes every declared budget
// executable - including maxPackedSectionBytes, which no entry point read before this batch -
// and replaces silent uint32 narrowing and count-driven reservations with stable diagnostics.
//
// The negative examples are hand-patched artifacts the typed Writer cannot produce. Section and
// header CRCs are kept consistent so the expected failure order between the structural, budget,
// profile and identity checks is the only thing under test.

#include "packed_fixture_support.hpp"

#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/core/result.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

using cuexis::chart::CanonicalEntity;
using cuexis::chart::CanonicalFeature;
using cuexis::chart::CanonicalRequirement;
using cuexis::chart::CanonicalSemanticChart;
using cuexis::chart::ChartId;
using cuexis::chart::ChartObjectId;
using cuexis::chart::ExplicitEntityIdentity;
using cuexis::chart::GeneratedEntityIdentity;
using cuexis::chart::LaneConstraint;
using cuexis::chart::PackedChartLimits;
using cuexis::chart::PackedChartWriter;
using cuexis::chart::RationalBeat;
using cuexis::chart::SemanticIdentityStep;
using cuexis::chart::TypedReference;
using cuexis::chart::packed::test::appendSection;
using cuexis::chart::packed::test::findSection;
using cuexis::chart::packed::test::readU32;
using cuexis::chart::packed::test::readU8;
using cuexis::chart::packed::test::refreshCrcs;
using cuexis::chart::packed::test::refreshHeaderCrc;
using cuexis::chart::packed::test::renameSection;
using cuexis::chart::packed::test::replaceSection;
using cuexis::chart::packed::test::SectionRef;
using cuexis::chart::packed::test::writeU32;

constexpr std::string_view registeredFeature{"cuexis.gameplay.candidate.lanes4"};

// Spec 5.1 fixed header offsets (see Spec 5.1 field order).
constexpr std::size_t headerDirectoryCountOffset = 24U;
constexpr std::size_t headerDirectoryBytesOffset = 28U;
constexpr std::size_t headerEntityCountOffset = 64U;
constexpr std::size_t headerRequirementCountOffset = 68U;
constexpr std::size_t headerDecodedBytesOffset = 76U;
constexpr std::size_t headerStringCountOffset = 80U;
constexpr std::size_t headerReferenceCountOffset = 84U;

constexpr std::size_t mebibyte = 1024U * 1024U;
constexpr std::size_t fileBudget = 16U * mebibyte;

[[nodiscard]] auto requirement(std::string localId, std::uint32_t lane) -> CanonicalRequirement {
    CanonicalRequirement value;
    value.localId = std::move(localId);
    value.judgementDomain = TypedReference{"judgement-domain", "candidate.lanes4"};
    value.requiredAction = TypedReference{"action", "press"};
    value.constraints.emplace_back(LaneConstraint{lane});
    return value;
}

[[nodiscard]] auto tapChart() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.defaultCamera = cuexis::chart::CameraData{};
    chart.features.push_back(CanonicalFeature{std::string{registeredFeature}, 1});
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000010"}};
    entity.requirements.push_back(requirement("hit", 2U));
    chart.entities.push_back(std::move(entity));
    return chart;
}

// One entity with a generated identity whose only indexed step repeats twice. This is the only
// fixture where IDN0 carries an iteration index on the wire.
[[nodiscard]] auto generatedChart() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.defaultCamera = cuexis::chart::CameraData{};
    GeneratedEntityIdentity generated;
    generated.chartId = chart.chartId;
    generated.bindingId = "binding0";
    generated.moduleId = "module0";
    generated.exportId = "emit";
    generated.path.push_back(SemanticIdentityStep{"group", 2});
    generated.path.push_back(SemanticIdentityStep{"emit", 0});
    CanonicalEntity entity;
    entity.identity = std::move(generated);
    chart.entities.push_back(std::move(entity));
    return chart;
}

[[nodiscard]] auto encodedBytes(const CanonicalSemanticChart& chart) -> std::vector<std::byte> {
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    return *encoded;
}

template <typename T>
[[nodiscard]] auto resultCode(const cuexis::core::Result<T>& result) -> std::string {
    return result ? std::string{"accepted"} : std::string{result.error().code()};
}

[[nodiscard]] auto encodeCode(const CanonicalSemanticChart& chart) -> std::string {
    return resultCode(cuexis::chart::packed::encode(chart));
}

[[nodiscard]] auto encodeCode(const CanonicalSemanticChart& chart, const PackedChartLimits& limits)
    -> std::string {
    return resultCode(cuexis::chart::packed::encode(chart, {}, limits));
}

[[nodiscard]] auto encodedBytes(const CanonicalSemanticChart& chart,
                                const PackedChartLimits& limits) -> std::vector<std::byte> {
    const auto encoded = cuexis::chart::packed::encode(chart, {}, limits);
    REQUIRE(encoded);
    return *encoded;
}

[[nodiscard]] auto inspectCode(std::span<const std::byte> bytes, const PackedChartLimits& limits)
    -> std::string {
    return resultCode(cuexis::chart::packed::inspect(bytes, limits));
}

[[nodiscard]] auto decodeCode(std::span<const std::byte> bytes, const PackedChartLimits& limits)
    -> std::string {
    return resultCode(cuexis::chart::packed::decode(bytes, limits));
}

[[nodiscard]] auto bridgeCode(std::span<const std::byte> bytes, const PackedChartLimits& limits)
    -> std::string {
    return resultCode(cuexis::chart::PackedChartReader::decode(bytes, limits));
}

[[nodiscard]] auto sizeCode(const CanonicalSemanticChart& chart, const PackedChartLimits& limits)
    -> std::string {
    return resultCode(PackedChartWriter::size(chart, {}, limits));
}

[[nodiscard]] auto atomicCode(const CanonicalSemanticChart& chart, const fs::path& target,
                              const PackedChartLimits& limits) -> std::string {
    return resultCode(PackedChartWriter::writeAtomic(chart, target, {.limits = limits}));
}

[[nodiscard]] auto readCode(const fs::path& source, const PackedChartLimits& limits)
    -> std::string {
    return resultCode(cuexis::chart::PackedChartReader::read(source, limits));
}

[[nodiscard]] auto largestSectionBytes(const CanonicalSemanticChart& chart) -> std::size_t {
    const auto sizing = PackedChartWriter::size(chart);
    REQUIRE(sizing);
    std::size_t largest = 0U;
    for (const auto& section : sizing->sections) {
        largest = std::max(largest, section.encodedBytes);
    }
    REQUIRE(largest > 0U);
    return largest;
}

[[nodiscard]] auto sectionPayload(const std::vector<std::byte>& bytes, const SectionRef& section)
    -> std::span<const std::byte> {
    return std::span<const std::byte>{bytes.data() + section.offset, section.size};
}

void appendLeb128(std::vector<std::byte>& bytes, std::uint64_t value) {
    do {
        auto byte = static_cast<std::uint8_t>(value & 0x7fU);
        value >>= 7U;
        if (value != 0U) {
            byte |= 0x80U;
        }
        bytes.push_back(static_cast<std::byte>(byte));
    } while (value != 0U);
}

struct TemporaryFile final {
    fs::path path;
    TemporaryFile() : path(fs::temp_directory_path() / "cuexis-r3-budget.bin") {
        std::error_code ignored;
        fs::remove(path, ignored);
    }
    ~TemporaryFile() {
        std::error_code ignored;
        fs::remove(path, ignored);
    }
    TemporaryFile(const TemporaryFile&) = delete;
    auto operator=(const TemporaryFile&) -> TemporaryFile& = delete;
    TemporaryFile(TemporaryFile&&) = delete;
    auto operator=(TemporaryFile&&) -> TemporaryFile& = delete;
};

} // namespace

TEST_CASE("R3 a custom maxPackedSectionBytes is enforced by every entry point",
          "[chart][packed][budget][r3]") {
    const auto chart = tapChart();
    const auto artifact = encodedBytes(chart);
    const auto largest = largestSectionBytes(chart);

    // Boundary success: a ceiling that equals the largest emitted section still accepts.
    auto exact = PackedChartLimits{};
    exact.maxPackedSectionBytes = largest;
    CHECK(inspectCode(artifact, exact) == "accepted");
    CHECK(decodeCode(artifact, exact) == "accepted");
    CHECK(bridgeCode(artifact, exact) == "accepted");
    CHECK(sizeCode(chart, exact) == "accepted");

    // One byte below the largest section: every entry point reports the section budget.
    auto tight = PackedChartLimits{};
    tight.maxPackedSectionBytes = largest - 1U;
    CHECK(inspectCode(artifact, tight) == "packed.budget.section_bytes");
    CHECK(decodeCode(artifact, tight) == "packed.budget.section_bytes");
    CHECK(bridgeCode(artifact, tight) == "packed.budget.section_bytes");
    CHECK(sizeCode(chart, tight) == "packed.budget.section_bytes");

    TemporaryFile file;
    const auto valid = PackedChartWriter::writeAtomic(chart, file.path);
    REQUIRE(valid);
    CHECK(readCode(file.path, tight) == "packed.budget.section_bytes");
    CHECK(readCode(file.path, exact) == "accepted");
    CHECK(atomicCode(chart, file.path, tight) == "packed.budget.section_bytes");
    // A refused artifact is never written, so the previous valid bytes survive.
    CHECK(fs::file_size(file.path) == valid->statistics.packedBytes);
}

TEST_CASE("R3 every declared budget is reported by its own diagnostic",
          "[chart][packed][budget][r3]") {
    const auto chart = tapChart();
    const auto artifact = encodedBytes(chart);
    const auto statistics = cuexis::chart::packed::inspect(artifact);
    REQUIRE(statistics);
    REQUIRE(statistics->entityCount > 0U);
    REQUIRE(statistics->requirementCount > 0U);
    REQUIRE(statistics->stringCount > 0U);
    REQUIRE(statistics->referenceCount > 0U);
    REQUIRE(statistics->decodedBytes > 0U);

    const auto checkBudget = [&](const std::string& expected, const PackedChartLimits& tight) {
        CHECK(inspectCode(artifact, tight) == expected);
        CHECK(decodeCode(artifact, tight) == expected);
        CHECK(bridgeCode(artifact, tight) == expected);
    };

    SECTION("entityCount") {
        auto exact = PackedChartLimits{};
        exact.maxPackedEntities = statistics->entityCount;
        CHECK(inspectCode(artifact, exact) == "accepted");
        auto tight = exact;
        tight.maxPackedEntities = statistics->entityCount - 1U;
        checkBudget("packed.budget.entities", tight);
    }
    SECTION("requirementCount") {
        auto exact = PackedChartLimits{};
        exact.maxPackedRequirements = statistics->requirementCount;
        CHECK(inspectCode(artifact, exact) == "accepted");
        auto tight = exact;
        tight.maxPackedRequirements = statistics->requirementCount - 1U;
        checkBudget("packed.budget.requirements", tight);
    }
    SECTION("stringCount") {
        auto exact = PackedChartLimits{};
        exact.maxPackedStrings = statistics->stringCount;
        CHECK(inspectCode(artifact, exact) == "accepted");
        auto tight = exact;
        tight.maxPackedStrings = statistics->stringCount - 1U;
        checkBudget("packed.budget.strings", tight);
    }
    SECTION("referenceCount") {
        auto exact = PackedChartLimits{};
        exact.maxPackedReferences = statistics->referenceCount;
        CHECK(inspectCode(artifact, exact) == "accepted");
        auto tight = exact;
        tight.maxPackedReferences = statistics->referenceCount - 1U;
        checkBudget("packed.budget.references", tight);
    }
    SECTION("decodedBytes") {
        auto exact = PackedChartLimits{};
        exact.maxPackedDecodedBytes = statistics->decodedBytes;
        CHECK(inspectCode(artifact, exact) == "accepted");
        auto tight = exact;
        tight.maxPackedDecodedBytes = statistics->decodedBytes - 1U;
        checkBudget("packed.budget.decoded_bytes", tight);
    }
    SECTION("packedBytes") {
        auto exact = PackedChartLimits{};
        exact.maxPackedFileBytes = artifact.size();
        CHECK(inspectCode(artifact, exact) == "accepted");
        auto tight = exact;
        tight.maxPackedFileBytes = artifact.size() - 1U;
        checkBudget("packed.budget.file_bytes", tight);
        CHECK(sizeCode(chart, tight) == "packed.budget.file_bytes");
    }
}

TEST_CASE("R3 zero budget values are literal ceilings, never unlimited",
          "[chart][packed][budget][r3]") {
    const auto chart = tapChart();
    const auto artifact = encodedBytes(chart);

    const auto zero = [](auto setter) {
        auto limits = PackedChartLimits{};
        setter(limits);
        return limits;
    };

    CHECK(inspectCode(artifact, zero([](auto& l) { l.maxPackedEntities = 0U; })) ==
          "packed.budget.entities");
    CHECK(inspectCode(artifact, zero([](auto& l) { l.maxPackedRequirements = 0U; })) ==
          "packed.budget.requirements");
    CHECK(inspectCode(artifact, zero([](auto& l) { l.maxPackedStrings = 0U; })) ==
          "packed.budget.strings");
    CHECK(inspectCode(artifact, zero([](auto& l) { l.maxPackedReferences = 0U; })) ==
          "packed.budget.references");
    CHECK(inspectCode(artifact, zero([](auto& l) { l.maxPackedDecodedBytes = 0U; })) ==
          "packed.budget.decoded_bytes");
    CHECK(inspectCode(artifact, zero([](auto& l) { l.maxPackedSectionBytes = 0U; })) ==
          "packed.budget.section_bytes");
    CHECK(inspectCode(artifact, zero([](auto& l) { l.maxPackedFileBytes = 0U; })) ==
          "packed.budget.file_bytes");
    CHECK(sizeCode(chart, zero([](auto& l) { l.maxPackedFileBytes = 0U; })) ==
          "packed.budget.file_bytes");
}

TEST_CASE("R3 a caller cannot relax the frozen Foundation budgets", "[chart][packed][budget][r3]") {
    SECTION("a declared entity count above 40000 is not unlocked by a larger limit") {
        auto patched = encodedBytes(tapChart());
        writeU32(patched, headerEntityCountOffset, 50000U);
        refreshHeaderCrc(patched);

        auto loosened = PackedChartLimits{};
        loosened.maxPackedEntities = 100000U;
        CHECK(inspectCode(patched, loosened) == "packed.budget.entities");
        CHECK(decodeCode(patched, loosened) == "packed.budget.entities");
    }
    SECTION("an artifact above 16 MiB is not unlocked by larger file and section limits") {
        const auto base = encodedBytes(tapChart());
        const std::vector<std::byte> filler(fileBudget, std::byte{0});
        const auto oversized = appendSection(base, "DBG0", 1U, 1U, filler);
        REQUIRE(oversized.size() > fileBudget);

        // Every declared budget except the file byte gate is loosened far beyond what the
        // artifact needs, so the file gate is the only budget that can refuse it.
        auto loosened = PackedChartLimits{};
        loosened.maxPackedFileBytes = 64U * mebibyte;
        loosened.maxPackedSectionBytes = 64U * mebibyte;
        loosened.maxPackedDecodedBytes = 64U * mebibyte;
        loosened.maxPackedEntities = 100000U;
        loosened.maxPackedRequirements = 100000U;
        loosened.maxPackedStrings = 1000000U;
        loosened.maxPackedReferences = 1000000U;
        CHECK(readU32(oversized, headerEntityCountOffset) <= loosened.maxPackedEntities);
        CHECK(readU32(oversized, headerRequirementCountOffset) <= loosened.maxPackedRequirements);
        CHECK(readU32(oversized, headerDecodedBytesOffset) <= loosened.maxPackedDecodedBytes);
        CHECK(readU32(oversized, headerStringCountOffset) <= loosened.maxPackedStrings);
        CHECK(readU32(oversized, headerReferenceCountOffset) <= loosened.maxPackedReferences);
        CHECK(oversized.size() < loosened.maxPackedFileBytes);

        // A 16 MiB + 1 artifact is still refused: the frozen ceiling wins over the request.
        CHECK(inspectCode(oversized, loosened) == "packed.budget.file_bytes");
        CHECK(decodeCode(oversized, loosened) == "packed.budget.file_bytes");
        CHECK(bridgeCode(oversized, loosened) == "packed.budget.file_bytes");
    }
}

TEST_CASE("R3 the directory count product is checked before any reservation",
          "[chart][packed][budget][r3]") {
    auto patched = encodedBytes(tapChart());
    const auto directoryBytes = readU32(patched, headerDirectoryBytesOffset);
    const auto directoryCount = readU32(patched, headerDirectoryCountOffset);
    REQUIRE(directoryBytes == directoryCount * 32U);
    // Adding 2^27 to the entry count wraps the uint32 product back onto the real byte count, so
    // the header stays self-consistent while declaring 134,217,740 directory entries.
    writeU32(patched, headerDirectoryCountOffset,
             static_cast<std::uint32_t>(directoryCount + (1U << 27U)));
    refreshHeaderCrc(patched);

    CHECK(inspectCode(patched, {}) == "packed.header.invalid");
    CHECK(decodeCode(patched, {}) == "packed.header.invalid");
}

TEST_CASE("R3 CNS0 lane values outside the uint32 wire range are refused",
          "[chart][packed][budget][r3]") {
    const auto base = encodedBytes(tapChart());
    const auto cns = findSection(base, "CNS0");
    REQUIRE(cns);

    // lane = 2^32 + 2 truncates to the registered lane 2, so the artifact stays semantically
    // identical to the valid one: only a range check on the wire value can refuse it.
    std::vector<std::byte> payload;
    payload.push_back(std::byte{1});
    appendLeb128(payload, 0x100000002ULL);
    const auto patched = replaceSection(base, cns->index, payload);

    CHECK(decodeCode(patched, {}) == "packed.constraints.invalid");
    CHECK(bridgeCode(patched, {}) == "packed.constraints.invalid");
}

TEST_CASE("R3 IDN0 declared counts are bounded by the payload before reservation",
          "[chart][packed][budget][r3]") {
    const auto base = encodedBytes(tapChart());
    const auto idn = findSection(base, "IDN0");
    REQUIRE(idn);

    const auto replaceIdentity = [&](const std::vector<std::byte>& payload) {
        return replaceSection(base, idn->index, payload);
    };

    SECTION("scope count") {
        cuexis::chart::packed::ByteWriter payload;
        payload.writeU32(0xffffffffU);
        payload.writeU32(0U);
        payload.writeU32(0U);
        const auto patched = replaceIdentity(std::move(payload).takeBytes());
        CHECK(decodeCode(patched, {}) == "packed.identity.invalid");
    }
    SECTION("path count") {
        cuexis::chart::packed::ByteWriter payload;
        payload.writeU32(0U);
        payload.writeU32(0xffffffffU);
        payload.writeU32(0U);
        const auto patched = replaceIdentity(std::move(payload).takeBytes());
        CHECK(decodeCode(patched, {}) == "packed.identity.invalid");
    }
    SECTION("step count") {
        cuexis::chart::packed::ByteWriter payload;
        payload.writeU32(0U);
        payload.writeU32(1U);
        payload.writeUnsignedLeb128(0xffffffffffffffffULL);
        const auto patched = replaceIdentity(std::move(payload).takeBytes());
        CHECK(decodeCode(patched, {}) == "packed.identity.invalid");
    }
}

TEST_CASE("R3 IDN0 iteration indices outside the uint32 wire range are refused",
          "[chart][packed][budget][r3]") {
    const auto base = encodedBytes(generatedChart());
    const auto idn = findSection(base, "IDN0");
    REQUIRE(idn);

    std::vector<std::byte> payload{sectionPayload(base, *idn).begin(),
                                   sectionPayload(base, *idn).end()};
    REQUIRE(payload.size() > 4U);
    // The generated identity is the only identity record, so its last field is the repeat index.
    REQUIRE(readU8(payload, payload.size() - 1U) == 2U);
    payload.pop_back();
    appendLeb128(payload, 0x100000002ULL);
    const auto patched = replaceSection(base, idn->index, payload);

    // 2^32 + 2 truncates to the repeat index the valid artifact already carries.
    CHECK(decodeCode(patched, {}) == "packed.identity.path");
    CHECK(bridgeCode(patched, {}) == "packed.identity.path");
}

TEST_CASE("R3 budget diagnostics stay ordered against structural and registry checks",
          "[chart][packed][budget][r3]") {
    SECTION("the section budget precedes the section CRC") {
        auto patched = encodedBytes(tapChart());
        const auto req = findSection(patched, "REQ0");
        REQUIRE(req);
        patched[req->offset] = std::byte{0x7f};

        auto tight = PackedChartLimits{};
        tight.maxPackedSectionBytes = 1U;
        CHECK(inspectCode(patched, tight) == "packed.budget.section_bytes");
        CHECK(inspectCode(patched, {}) == "packed.section.crc");
    }
    SECTION("the section registry precedes the section budget") {
        auto patched = encodedBytes(tapChart());
        const auto arch = findSection(patched, "ARCH");
        REQUIRE(arch);
        renameSection(patched, *arch, "BEH0");

        auto tight = PackedChartLimits{};
        tight.maxPackedSectionBytes = 1U;
        CHECK(inspectCode(patched, tight) == "packed.directory.refused");
    }
}

TEST_CASE("R3 budget rejection precedes every semantic and profile diagnostic",
          "[chart][packed][budget][r3]") {
    auto chart = tapChart();
    chart.entities.front().requirements.front().constraints.front() = LaneConstraint{7};
    auto patched = encodedBytes(tapChart());
    const auto cns = findSection(patched, "CNS0");
    REQUIRE(cns);
    std::vector<std::byte> payload;
    payload.push_back(std::byte{1});
    appendLeb128(payload, 7U);
    patched = replaceSection(patched, cns->index, payload);

    // The artifact is out of profile; a section budget that refuses it first still wins, and it
    // is never reported as an identity mismatch.
    auto tight = PackedChartLimits{};
    tight.maxPackedSectionBytes = 1U;
    CHECK(decodeCode(patched, tight) == "packed.budget.section_bytes");
    CHECK(decodeCode(patched, {}) == "packed.profile.lane");
    CHECK(decodeCode(patched, {}) != "packed.identity.mismatch");
    CHECK(encodeCode(chart) == "packed.profile.lane");
}

TEST_CASE("R3 the Writer budget gates run before any artifact or file output",
          "[chart][packed][budget][r3]") {
    const auto chart = tapChart();
    const auto artifact = encodedBytes(chart);
    const auto statistics = cuexis::chart::packed::inspect(artifact);
    REQUIRE(statistics);
    const auto largest = largestSectionBytes(chart);

    SECTION("count budgets") {
        auto entities = PackedChartLimits{};
        entities.maxPackedEntities = 0U;
        CHECK(encodeCode(chart, entities) == "packed.budget.entities");
        auto requirements = PackedChartLimits{};
        requirements.maxPackedRequirements = 0U;
        CHECK(encodeCode(chart, requirements) == "packed.budget.requirements");
        auto strings = PackedChartLimits{};
        strings.maxPackedStrings = 0U;
        CHECK(encodeCode(chart, strings) == "packed.budget.strings");
        auto references = PackedChartLimits{};
        references.maxPackedReferences = 0U;
        CHECK(encodeCode(chart, references) == "packed.budget.references");
    }
    SECTION("section and decoded byte budgets") {
        auto tight = PackedChartLimits{};
        tight.maxPackedSectionBytes = largest - 1U;
        CHECK(encodeCode(chart, tight) == "packed.budget.section_bytes");

        auto decoded = PackedChartLimits{};
        decoded.maxPackedDecodedBytes = statistics->decodedBytes - 1U;
        CHECK(encodeCode(chart, decoded) == "packed.budget.decoded_bytes");
    }
    SECTION("the file envelope is exact and budget-independent") {
        auto exact = PackedChartLimits{};
        exact.maxPackedFileBytes = artifact.size();
        exact.maxPackedSectionBytes = largest;
        CHECK(encodeCode(chart, exact) == "accepted");
        // Enforcing the budgets never changes the emitted bytes.
        CHECK(encodedBytes(chart, exact) == artifact);

        auto oneLess = exact;
        oneLess.maxPackedFileBytes = artifact.size() - 1U;
        CHECK(encodeCode(chart, oneLess) == "packed.budget.file_bytes");
    }
    SECTION("a refused model never reaches the filesystem") {
        auto tight = PackedChartLimits{};
        tight.maxPackedSectionBytes = largest - 1U;
        TemporaryFile file;
        CHECK(atomicCode(chart, file.path, tight) == "packed.budget.section_bytes");
        CHECK_FALSE(fs::exists(file.path));
    }
}
