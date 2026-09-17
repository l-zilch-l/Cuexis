// R2 preparation tests: the two independent negative-example groups and the expected failure
// order between structural, profile and semantic-identity checks.
//
// These cases record the *current* behaviour so R2 can flip them when the registered Foundation
// profile becomes a hard rejection on every entry point. Group A builds artifacts through the
// typed Writer from an out-of-profile model; group B patches bytes the Writer cannot produce.
// Every group B case asserts that the failure is the structural/profile diagnostic and NOT
// packed.identity.mismatch, because a profile rejection must not be masked by the identity check.

#include "packed_fixture_support.hpp"

#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using cuexis::chart::CanonicalEntity;
using cuexis::chart::CanonicalFeature;
using cuexis::chart::CanonicalRequirement;
using cuexis::chart::CanonicalSemanticChart;
using cuexis::chart::CanonicalTransform;
using cuexis::chart::ChartId;
using cuexis::chart::ChartObjectId;
using cuexis::chart::ExplicitEntityIdentity;
using cuexis::chart::LaneConstraint;
using cuexis::chart::RationalBeat;
using cuexis::chart::packed::test::findConstraintRow;
using cuexis::chart::packed::test::findReferenceRow;
using cuexis::chart::packed::test::findRequirementRow;
using cuexis::chart::packed::test::findSection;
using cuexis::chart::packed::test::refreshCrcs;
using cuexis::chart::packed::test::refreshHeaderCrc;
using cuexis::chart::packed::test::SectionRef;
using cuexis::chart::packed::test::writeU32;
using cuexis::chart::packed::test::writeU8;

[[nodiscard]] auto tapChart() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.defaultCamera = cuexis::chart::CameraData{};
    chart.features.push_back(CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{2});
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000010"}};
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));
    return chart;
}

[[nodiscard]] auto transformChart() -> CanonicalSemanticChart {
    auto chart = tapChart();
    chart.entities.front().components.emplace_back(CanonicalTransform{});
    return chart;
}

// Current decode diagnostic for an artifact the typed Writer accepted.
[[nodiscard]] auto decodeCode(const CanonicalSemanticChart& chart) -> std::string {
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    const auto decoded = cuexis::chart::packed::decode(*encoded);
    if (decoded) {
        return "accepted";
    }
    return std::string{decoded.error().code()};
}

[[nodiscard]] auto encodedBytes(const CanonicalSemanticChart& chart) -> std::vector<std::byte> {
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    return *encoded;
}

[[nodiscard]] auto decodeCode(const std::vector<std::byte>& bytes) -> std::string {
    const auto decoded = cuexis::chart::packed::decode(bytes);
    if (decoded) {
        return "accepted";
    }
    return std::string{decoded.error().code()};
}

} // namespace

TEST_CASE("R2P-A the Writer currently publishes out-of-profile requirements the Reader refuses",
          "[chart][packed][r2prep]") {
    SECTION("lane outside the registered [0,3] range") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().constraints.front() = LaneConstraint{7};
        // The Writer maps the lane into CNS0 without a range check; the Reader refuses it.
        CHECK(decodeCode(chart) == "packed.constraints.invalid");
    }
    SECTION("action other than press") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().requiredAction = {"action", "release"};
        CHECK(decodeCode(chart) == "packed.requirements.profile");
    }
    SECTION("judgement domain other than candidate.lanes4") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().judgementDomain = {"judgement-domain",
                                                                       "candidate.lanes5"};
        CHECK(decodeCode(chart) == "packed.requirements.profile");
    }
    SECTION("half-open range interval") {
        auto chart = tapChart();
        auto& requirement = chart.entities.front().requirements.front();
        const auto end = RationalBeat::create(1, 1);
        REQUIRE(end);
        requirement.interval.kind = cuexis::chart::CanonicalIntervalKind::HalfOpenRange;
        requirement.interval.endBeat = *end;
        // Range is expressible on the wire and accepted on decode although Spec 7.6 registers
        // point only.
        CHECK(decodeCode(chart) == "accepted");
    }
}

TEST_CASE("R2P-A the semantic preimage already refuses two out-of-profile model shapes",
          "[chart][packed][r2prep]") {
    SECTION("constraint count is not exactly one") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().constraints.clear();
        const auto encoded = cuexis::chart::packed::encode(chart);
        REQUIRE_FALSE(encoded);
        CHECK(encoded.error().code() == "packed.identity.constraints");
    }
    SECTION("non-empty effect set") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().effects.push_back(
            cuexis::chart::TypedReference{"effect", "flash"});
        const auto encoded = cuexis::chart::packed::encode(chart);
        REQUIRE_FALSE(encoded);
        CHECK(encoded.error().code() == "packed.identity.effects");
    }
}

TEST_CASE("R2P-B patched REQ0 fields fail on the profile check, not on the identity check",
          "[chart][packed][r2prep]") {
    const auto baseline = encodedBytes(tapChart());
    const auto reqSection = findSection(baseline, "REQ0");
    REQUIRE(reqSection);
    const auto payload =
        std::span<const std::byte>{baseline.data() + reqSection->offset, reqSection->size};

    SECTION("unknown requirement kind") {
        const auto row = findRequirementRow(payload, 0U);
        REQUIRE(row);
        auto patched = baseline;
        writeU8(patched, reqSection->offset + row->kindOffset, 2U);
        refreshCrcs(patched, *reqSection);
        CHECK(decodeCode(patched) == "packed.requirements.profile");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("unknown interval kind") {
        const auto row = findRequirementRow(payload, 0U);
        REQUIRE(row);
        auto patched = baseline;
        writeU8(patched, reqSection->offset + row->intervalOffset, 2U);
        refreshCrcs(patched, *reqSection);
        CHECK(decodeCode(patched) == "packed.requirements.profile");
    }
    SECTION("non-empty effect set on the wire") {
        const auto row = findRequirementRow(payload, 0U);
        REQUIRE(row);
        auto patched = baseline;
        writeU8(patched, reqSection->offset + row->effectOffset, 1U);
        refreshCrcs(patched, *reqSection);
        CHECK(decodeCode(patched) == "packed.requirements.profile");
    }
    SECTION("constraint index zero") {
        const auto row = findRequirementRow(payload, 0U);
        REQUIRE(row);
        auto patched = baseline;
        writeU8(patched, reqSection->offset + row->constraintOffset, 0U);
        refreshCrcs(patched, *reqSection);
        CHECK(decodeCode(patched) == "packed.requirements.profile");
    }
    SECTION("domain reference points at an action-kind row") {
        const auto row = findRequirementRow(payload, 0U);
        REQUIRE(row);
        const auto refSection = findSection(baseline, "REF0");
        REQUIRE(refSection);
        const auto actionRow = findReferenceRow(
            std::span<const std::byte>{baseline.data() + refSection->offset, refSection->size}, 5U);
        REQUIRE(actionRow);
        auto patched = baseline;
        writeU8(patched, reqSection->offset + row->domainOffset,
                static_cast<std::uint8_t>(*actionRow));
        refreshCrcs(patched, *reqSection);
        CHECK(decodeCode(patched) == "packed.requirements.profile");
    }
}

TEST_CASE("R2P-B patched CNS0 fields are refused before the identity check",
          "[chart][packed][r2prep]") {
    const auto baseline = encodedBytes(tapChart());
    const auto cnsSection = findSection(baseline, "CNS0");
    REQUIRE(cnsSection);
    const auto payload =
        std::span<const std::byte>{baseline.data() + cnsSection->offset, cnsSection->size};
    const auto row = findConstraintRow(payload, 0U);
    REQUIRE(row);

    SECTION("unknown constraint kind") {
        auto patched = baseline;
        writeU8(patched, cnsSection->offset + row->kindOffset, 2U);
        refreshCrcs(patched, *cnsSection);
        CHECK(decodeCode(patched) == "packed.constraints.invalid");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("lane above the registered maximum") {
        auto patched = baseline;
        writeU8(patched, cnsSection->offset + row->laneOffset, 4U);
        refreshCrcs(patched, *cnsSection);
        CHECK(decodeCode(patched) == "packed.constraints.invalid");
    }
}

TEST_CASE("R2P-B unknown header values are refused", "[chart][packed][r2prep]") {
    using cuexis::chart::packed::test::candidateRevisionOffset;
    using cuexis::chart::packed::test::headerFlagsOffset;
    using cuexis::chart::packed::test::packedVersionOffset;

    SECTION("packedVersion") {
        auto patched = encodedBytes(tapChart());
        writeU32(patched, packedVersionOffset, 2U);
        refreshHeaderCrc(patched);
        CHECK(decodeCode(patched) == "packed.header.invalid");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("header flags bit 1") {
        auto patched = encodedBytes(tapChart());
        writeU32(patched, headerFlagsOffset, 3U);
        refreshHeaderCrc(patched);
        CHECK(decodeCode(patched) == "packed.header.invalid");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("candidateRevision") {
        auto patched = encodedBytes(tapChart());
        writeU32(patched, candidateRevisionOffset, 2U);
        refreshHeaderCrc(patched);
        CHECK(decodeCode(patched) == "packed.header.invalid");
    }
}

TEST_CASE("R2P-B section registry: unknown semantic, unregistered inspection and DBG0",
          "[chart][packed][r2prep]") {
    using cuexis::chart::packed::test::appendSection;
    using cuexis::chart::packed::test::renameSection;

    SECTION("an unregistered semantic section code is refused") {
        // TRN0 exists because the chart carries a Transform and is optional, so renaming it only
        // exercises the section registry.
        auto patched = encodedBytes(transformChart());
        const auto transformStream = findSection(patched, "TRN0");
        REQUIRE(transformStream);
        renameSection(patched, *transformStream, "ZZZZ");
        refreshHeaderCrc(patched);
        // The unknown-section rejection comes from the structural inspection layer
        // (packed.directory.invalid); the table decoder's own packed.directory.unsupported branch
        // is unreachable for unknown codes because inspect runs first.
        CHECK(decodeCode(patched) == "packed.directory.invalid");
    }
    SECTION("the registered DBG0 inspection section is currently refused") {
        const auto baseline = encodedBytes(tapChart());
        const std::vector<std::byte> payload{std::byte{'d'}, std::byte{'b'}, std::byte{'g'}};
        const auto patched = appendSection(baseline, "DBG0", 1U, 3U, payload);
        // Spec 5.3 registers DBG0 as an optional inspection section; R2 accepts and ignores it.
        CHECK(decodeCode(patched) == "packed.directory.invalid");
    }
    SECTION("an unregistered inspection section is currently refused") {
        const auto baseline = encodedBytes(tapChart());
        const std::vector<std::byte> payload{std::byte{'z'}, std::byte{'z'}};
        const auto patched = appendSection(baseline, "ZZZZ", 1U, 2U, payload);
        // Spec 5.2 allows ignoring unknown inspection sections after length and CRC checks.
        CHECK(decodeCode(patched) == "packed.directory.invalid");
    }
}

TEST_CASE("R2P-B a semantic section carrying the inspection flag splits the entry points",
          "[chart][packed][r2prep]") {
    using cuexis::chart::packed::test::headerSize;
    auto patched = encodedBytes(tapChart());
    const auto meta = findSection(patched, "META");
    REQUIRE(meta);
    // Directory entry layout: type(0..3), codec(4), flags(5). Spec 5.3 requires flags=0 for every
    // section except DBG0.
    writeU8(patched, headerSize + 32U * meta->index + 5U, 1U);
    refreshHeaderCrc(patched);

    // The structural layer and the table decoder accept the flagged semantic section; only the IO
    // bridge rejects it, and it reports the misleading "unknown_section" code instead of a flags
    // diagnostic. R2 has to make the entry points and the diagnostic agree.
    CHECK(cuexis::chart::packed::inspect(patched));
    CHECK(decodeCode(patched) == "accepted");
    const auto bridged = cuexis::chart::PackedChartReader::decode(patched);
    REQUIRE_FALSE(bridged);
    CHECK(bridged.error().code() == "packed.io.unknown_section");
}

TEST_CASE("R2P inspect stays structural while decode refuses the same artifact",
          "[chart][packed][r2prep]") {
    auto chart = tapChart();
    chart.entities.front().requirements.front().constraints.front() = LaneConstraint{7};
    const auto bytes = encodedBytes(chart);

    // Structural inspection accepts the artifact; only decode enforces the profile. Any semantic
    // consumer must therefore call decode, which R2 has to keep true for every new check.
    CHECK(cuexis::chart::packed::inspect(bytes));
    CHECK(decodeCode(bytes) == "packed.constraints.invalid");
    CHECK_FALSE(cuexis::chart::PackedChartReader::decode(bytes));
}
