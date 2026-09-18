// R2 contract tests: the Foundation registered subset, the section registry and the canonical
// internal ordering are enforced by every supported entry point.
//
// R2 replaced the two negative-example preparation groups with contract assertions:
//   * group A (typed Writer) - an out-of-profile model is refused by packed::encode before any
//     artifact or identity exists;
//   * group B (external bytes) - a hand-patched artifact that the Writer cannot produce is
//     refused by packed::decode with the same diagnostic, and the profile/structural rejection
//     always precedes the semantic identity comparison (never packed.identity.mismatch).

#include "packed_fixture_support.hpp"

#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>

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
using cuexis::chart::TypedReference;
using cuexis::chart::packed::test::archetypeRowRanges;
using cuexis::chart::packed::test::buildReferences;
using cuexis::chart::packed::test::buildStrings;
using cuexis::chart::packed::test::constraintRowRanges;
using cuexis::chart::packed::test::explicitIdentityRecordRanges;
using cuexis::chart::packed::test::findSection;
using cuexis::chart::packed::test::readReferences;
using cuexis::chart::packed::test::readStrings;
using cuexis::chart::packed::test::readU32;
using cuexis::chart::packed::test::readU64;
using cuexis::chart::packed::test::readU8;
using cuexis::chart::packed::test::refreshCrcs;
using cuexis::chart::packed::test::refreshHeaderCrc;
using cuexis::chart::packed::test::reorderRows;
using cuexis::chart::packed::test::replaceSection;
using cuexis::chart::packed::test::requirementRowRanges;
using cuexis::chart::packed::test::SectionRef;
using cuexis::chart::packed::test::writeU32;
using cuexis::chart::packed::test::writeU64;
using cuexis::chart::packed::test::writeU8;

constexpr std::string_view registeredFeature{"cuexis.gameplay.candidate.lanes4"};

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

// An empty second entity, used to give IDN0 two explicit identity records.
[[nodiscard]] auto withSecondEntity(CanonicalSemanticChart chart) -> CanonicalSemanticChart {
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000020"}};
    chart.entities.push_back(std::move(entity));
    return chart;
}

[[nodiscard]] auto encodedBytes(const CanonicalSemanticChart& chart) -> std::vector<std::byte> {
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    return *encoded;
}

[[nodiscard]] auto encodeCode(const CanonicalSemanticChart& chart) -> std::string {
    const auto encoded = cuexis::chart::packed::encode(chart);
    if (encoded) {
        return "accepted";
    }
    return std::string{encoded.error().code()};
}

[[nodiscard]] auto decodeCode(const std::vector<std::byte>& bytes) -> std::string {
    const auto decoded = cuexis::chart::packed::decode(bytes);
    if (decoded) {
        return "accepted";
    }
    return std::string{decoded.error().code()};
}

// The decoded semantics of an artifact, used to prove an ignored inspection payload changes
// nothing. CanonicalSemanticChart has no equality operator, so the semantic identity stands in.
[[nodiscard]] auto semanticHex(const std::vector<std::byte>& bytes) -> std::string {
    const auto decoded = cuexis::chart::packed::decode(bytes);
    REQUIRE(decoded);
    const auto identity = cuexis::chart::packed::semanticIdentity(*decoded);
    REQUIRE(identity);
    return cuexis::chart::packed::semanticIdentityHex(*identity);
}

// packed::decode and the file bridge always agree; inspect is structural only and may accept an
// artifact that the semantic gate refuses (see the dedicated inspect/decode test below).
[[nodiscard]] auto entryPointCode(const std::vector<std::byte>& bytes) -> std::string {
    const auto decoded = cuexis::chart::packed::decode(bytes);
    const auto bridged = cuexis::chart::PackedChartReader::decode(bytes);
    if (decoded) {
        REQUIRE(bridged);
        return "accepted";
    }
    const std::string decodedCode{decoded.error().code()};
    REQUIRE_FALSE(bridged);
    CHECK(std::string{bridged.error().code()} == decodedCode);
    return decodedCode;
}

[[nodiscard]] auto sectionPayload(const std::vector<std::byte>& bytes, const SectionRef& section)
    -> std::span<const std::byte> {
    return std::span<const std::byte>{bytes.data() + section.offset, section.size};
}

// Rebuilds STR0 with one string replaced, so a REF0 row can carry an unregistered text.
[[nodiscard]] auto withString(const std::vector<std::byte>& artifact, std::string_view from,
                              std::string_view to) -> std::vector<std::byte> {
    const auto section = findSection(artifact, "STR0");
    REQUIRE(section);
    auto values = readStrings(sectionPayload(artifact, *section));
    REQUIRE_FALSE(values.empty());
    const auto found = std::find(values.begin(), values.end(), std::string{from});
    REQUIRE(found != values.end());
    *found = std::string{to};
    return replaceSection(artifact, section->index, buildStrings(values));
}

} // namespace

TEST_CASE("R2 the Writer and the Reader refuse the same out-of-profile requirements",
          "[chart][packed][profile][r2]") {
    SECTION("lane outside the registered [0,3] range") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().constraints.front() = LaneConstraint{7};
        CHECK(encodeCode(chart) == "packed.profile.lane");

        auto patched = encodedBytes(tapChart());
        const auto cns = findSection(patched, "CNS0");
        REQUIRE(cns);
        const auto rows = constraintRowRanges(sectionPayload(patched, *cns));
        REQUIRE(rows.size() == 1U);
        // CNS0 row: kind(1) then lane(uv32). The Writer emitted lane 2.
        CHECK(readU8(patched, cns->offset + rows[0].first + 1U) == 2U);
        writeU8(patched, cns->offset + rows[0].first + 1U, 7U);
        refreshCrcs(patched, *cns);
        CHECK(decodeCode(patched) == "packed.profile.lane");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("action other than press") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().requiredAction =
            TypedReference{"action", "release"};
        CHECK(encodeCode(chart) == "packed.profile.action");

        const auto patched = withString(encodedBytes(tapChart()), "press", "zzz");
        CHECK(decodeCode(patched) == "packed.profile.action");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("judgement domain other than candidate.lanes4") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().judgementDomain =
            TypedReference{"judgement-domain", "candidate.lanes5"};
        CHECK(encodeCode(chart) == "packed.profile.domain");

        const auto patched = withString(encodedBytes(tapChart()), "candidate.lanes4", "aa");
        CHECK(decodeCode(patched) == "packed.profile.domain");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("half-open range interval") {
        auto chart = tapChart();
        auto& target = chart.entities.front().requirements.front();
        const auto end = RationalBeat::create(1, 1);
        REQUIRE(end);
        target.interval.kind = cuexis::chart::CanonicalIntervalKind::HalfOpenRange;
        target.interval.endBeat = *end;
        CHECK(encodeCode(chart) == "packed.profile.interval");

        // A well-formed range row (both Beat atoms present, references intact) is still outside
        // the registered profile, so the Reader must refuse it for that reason.
        auto baseline = encodedBytes(tapChart());
        const auto req = findSection(baseline, "REQ0");
        const auto str = findSection(baseline, "STR0");
        const auto ref = findSection(baseline, "REF0");
        REQUIRE(req);
        REQUIRE(str);
        REQUIRE(ref);
        const auto strings = readStrings(sectionPayload(baseline, *str));
        const auto references = readReferences(sectionPayload(baseline, *ref));
        const auto localIndex = std::find(strings.begin(), strings.end(), std::string{"hit"});
        REQUIRE(localIndex != strings.end());
        const auto domainRow = std::find_if(references.begin(), references.end(),
                                            [](const auto& row) { return row.kind == 4U; });
        const auto actionRow = std::find_if(references.begin(), references.end(),
                                            [](const auto& row) { return row.kind == 5U; });
        REQUIRE(domainRow != references.end());
        REQUIRE(actionRow != references.end());
        cuexis::chart::packed::ByteWriter rows;
        rows.writeU8(0);             // startBeat codec
        rows.writeU8(0);             // endBeat codec
        rows.writeUnsignedLeb128(0); // first ordinal delta
        rows.writeUnsignedLeb128(
            static_cast<std::uint32_t>(std::distance(strings.begin(), localIndex)));
        rows.writeU8(1); // requirementKind: tap
        rows.writeU8(1); // intervalKind: half-open range
        const auto start = RationalBeat::zero();
        REQUIRE(cuexis::chart::packed::writeRationalBeatAtom(rows, start));
        REQUIRE(cuexis::chart::packed::writeRationalBeatAtom(rows, *end));
        rows.writeUnsignedLeb128(
            static_cast<std::uint32_t>(std::distance(references.begin(), domainRow)));
        rows.writeUnsignedLeb128(
            static_cast<std::uint32_t>(std::distance(references.begin(), actionRow)));
        rows.writeUnsignedLeb128(1); // constraintSetIndexPlusOne
        rows.writeUnsignedLeb128(0); // effectSetIndexPlusOne
        const auto patched = replaceSection(baseline, req->index, std::move(rows).takeBytes());
        CHECK(decodeCode(patched) == "packed.profile.interval");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("constraint count is not exactly one") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().constraints.clear();
        CHECK(encodeCode(chart) == "packed.profile.constraints");

        auto patched = encodedBytes(tapChart());
        const auto req = findSection(patched, "REQ0");
        REQUIRE(req);
        const auto rows = requirementRowRanges(sectionPayload(patched, *req));
        REQUIRE(rows.size() == 1U);
        // constraintSetIndexPlusOne is the second uv32 before the effect index.
        const auto rowBegin = req->offset + rows[0].first;
        const auto rowEnd = req->offset + rows[0].second;
        writeU8(patched, rowEnd - 2U, 0U);
        refreshCrcs(patched, *req);
        CHECK(decodeCode(patched) == "packed.profile.constraints");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
        CHECK(rowBegin < rowEnd);
    }
    SECTION("non-empty effect set") {
        auto chart = tapChart();
        chart.entities.front().requirements.front().effects.push_back(
            TypedReference{"effect", "flash"});
        CHECK(encodeCode(chart) == "packed.profile.effects");

        auto patched = encodedBytes(tapChart());
        const auto req = findSection(patched, "REQ0");
        REQUIRE(req);
        const auto rows = requirementRowRanges(sectionPayload(patched, *req));
        REQUIRE(rows.size() == 1U);
        writeU8(patched, req->offset + rows[0].second - 1U, 1U);
        refreshCrcs(patched, *req);
        CHECK(decodeCode(patched) == "packed.profile.effects");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("unknown requirement kind") {
        auto patched = encodedBytes(tapChart());
        const auto req = findSection(patched, "REQ0");
        REQUIRE(req);
        const auto rows = requirementRowRanges(sectionPayload(patched, *req));
        REQUIRE(rows.size() == 1U);
        const auto row =
            cuexis::chart::packed::test::findRequirementRow(sectionPayload(patched, *req), 0U);
        REQUIRE(row);
        writeU8(patched, req->offset + row->kindOffset, 2U);
        refreshCrcs(patched, *req);
        CHECK(decodeCode(patched) == "packed.profile.requirement");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
}

TEST_CASE("R2 requirements require the registered feature declaration",
          "[chart][packed][profile][r2]") {
    SECTION("missing feature declaration") {
        auto chart = tapChart();
        chart.features.clear();
        CHECK(encodeCode(chart) == "packed.profile.feature");
    }
    SECTION("unregistered feature version") {
        auto chart = tapChart();
        chart.features = {CanonicalFeature{std::string{registeredFeature}, 2}};
        CHECK(encodeCode(chart) == "packed.profile.feature");
    }
    SECTION("unregistered feature id") {
        auto chart = tapChart();
        chart.features = {CanonicalFeature{"cuexis.gameplay.candidate.other", 1}};
        CHECK(encodeCode(chart) == "packed.profile.feature");
    }
    SECTION("the same rule applies to a patched META feature string") {
        const auto patched = withString(encodedBytes(tapChart()), registeredFeature,
                                        "cuexis.gameplay.candidate.lanes5");
        CHECK(decodeCode(patched) == "packed.profile.feature");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("a chart without requirements may omit the feature") {
        auto chart = tapChart();
        chart.features.clear();
        chart.entities.front().requirements.clear();
        CHECK(encodeCode(chart) == "accepted");
    }
}

TEST_CASE("R2 the section registry accepts DBG0 and refuses later-contract sections",
          "[chart][packed][profile][r2]") {
    using cuexis::chart::packed::test::appendSection;

    const auto baseline = encodedBytes(tapChart());
    const std::vector<std::byte> debugPayload{std::byte{'d'}, std::byte{'b'}, std::byte{'g'}};
    const std::vector<std::byte> unknownPayload{std::byte{'z'}, std::byte{'z'}};

    SECTION("the registered DBG0 inspection section is accepted and ignored") {
        const auto patched = appendSection(baseline, "DBG0", 1U, 3U, debugPayload);
        CHECK(entryPointCode(patched) == "accepted");
        CHECK(cuexis::chart::packed::inspect(patched));
        CHECK(semanticHex(patched) == semanticHex(baseline));
    }
    SECTION("DBG0 must carry the inspection flag") {
        const auto patched = appendSection(baseline, "DBG0", 0U, 3U, debugPayload);
        CHECK(entryPointCode(patched) == "packed.directory.flags");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("DBG0 still requires a matching section CRC") {
        auto patched = appendSection(baseline, "DBG0", 1U, 3U, debugPayload);
        writeU8(patched, patched.size() - 1U, static_cast<std::uint8_t>('x'));
        refreshHeaderCrc(patched);
        CHECK(decodeCode(patched) == "packed.section.crc");
    }
    SECTION("an unknown inspection section is ignored after length and CRC checks") {
        const auto patched = appendSection(baseline, "ZZZZ", 1U, 2U, unknownPayload);
        CHECK(entryPointCode(patched) == "accepted");
        CHECK(cuexis::chart::packed::inspect(patched));
        CHECK(semanticHex(patched) == semanticHex(baseline));
    }
    SECTION("an unknown semantic section is refused") {
        const auto patched = appendSection(baseline, "ZZZZ", 0U, 2U, unknownPayload);
        CHECK(entryPointCode(patched) == "packed.directory.invalid");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("later-contract sections are refused even when empty") {
        const auto patched = appendSection(baseline, "BEH0", 0U, 0U, {});
        CHECK(entryPointCode(patched) == "packed.directory.refused");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("a semantic section carrying the inspection flag is refused") {
        auto patched = encodedBytes(tapChart());
        const auto meta = findSection(patched, "META");
        REQUIRE(meta);
        // Directory entry: type(0..3), codec(4), flags(5).
        writeU8(patched, cuexis::chart::packed::test::headerSize + 32U * meta->index + 5U, 1U);
        refreshHeaderCrc(patched);
        CHECK(entryPointCode(patched) == "packed.directory.flags");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("a section reusing another section code is refused") {
        using cuexis::chart::packed::test::renameSection;
        auto patched = encodedBytes(tapChart());
        const auto req = findSection(patched, "REQ0");
        REQUIRE(req);
        renameSection(patched, *req, "META");
        refreshHeaderCrc(patched);
        CHECK(entryPointCode(patched) == "packed.directory.duplicate");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
}

TEST_CASE("R2 unknown header declarations are refused instead of ignored",
          "[chart][packed][profile][r2]") {
    using cuexis::chart::packed::test::candidateRevisionOffset;
    using cuexis::chart::packed::test::eventCountOffset;
    using cuexis::chart::packed::test::headerFlagsOffset;
    using cuexis::chart::packed::test::packedVersionOffset;

    SECTION("packedVersion") {
        auto patched = encodedBytes(tapChart());
        writeU32(patched, packedVersionOffset, 2U);
        refreshHeaderCrc(patched);
        CHECK(entryPointCode(patched) == "packed.header.invalid");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("header flags bit 1") {
        auto patched = encodedBytes(tapChart());
        writeU32(patched, headerFlagsOffset, 3U);
        refreshHeaderCrc(patched);
        CHECK(entryPointCode(patched) == "packed.header.invalid");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("candidateRevision") {
        auto patched = encodedBytes(tapChart());
        writeU32(patched, candidateRevisionOffset, 2U);
        refreshHeaderCrc(patched);
        CHECK(entryPointCode(patched) == "packed.header.unsupported_revision");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
    SECTION("a declared schedule event count") {
        auto patched = encodedBytes(tapChart());
        writeU32(patched, eventCountOffset, 1U);
        refreshHeaderCrc(patched);
        CHECK(entryPointCode(patched) == "packed.header.events");
        CHECK_FALSE(cuexis::chart::packed::inspect(patched));
    }
}

TEST_CASE("R2 ARCH and component-stream masks only use registered bits",
          "[chart][packed][profile][r2]") {
    SECTION("an unregistered ARCH component bit") {
        auto chart = tapChart();
        chart.entities.front().components.emplace_back(CanonicalTransform{});
        auto patched = encodedBytes(chart);
        const auto arch = findSection(patched, "ARCH");
        REQUIRE(arch);
        // Component mask: bit 0 (Transform) and bit 2 (requirement presence).
        CHECK(readU64(patched, arch->offset) == 5U);
        writeU64(patched, arch->offset, 0x9U); // bit 3 belongs to a later contract
        refreshCrcs(patched, *arch);
        CHECK(decodeCode(patched) == "packed.arch.mask");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
    SECTION("an undefined component-stream change bit") {
        auto chart = tapChart();
        chart.entities.front().components.emplace_back(CanonicalTransform{});
        auto patched = encodedBytes(chart);
        const auto trn = findSection(patched, "TRN0");
        REQUIRE(trn);
        // One entity sharing the archetype default: ordinal 0, change mask 0.
        REQUIRE(trn->size == 2U);
        CHECK(readU8(patched, trn->offset) == 0U);
        CHECK(readU8(patched, trn->offset + 1U) == 0U);
        writeU8(patched, trn->offset + 1U, 2U); // bit 1 has no field atom
        refreshCrcs(patched, *trn);
        CHECK(decodeCode(patched) == "packed.stream.mask");
        CHECK(decodeCode(patched) != "packed.identity.mismatch");
    }
}

TEST_CASE("R2 the Reader enforces the canonical internal ordering of every table",
          "[chart][packed][profile][r2]") {
    SECTION("STR0 strings") {
        auto patched = encodedBytes(tapChart());
        const auto str = findSection(patched, "STR0");
        REQUIRE(str);
        auto values = readStrings(sectionPayload(patched, *str));
        REQUIRE(values.size() > 1U);
        std::reverse(values.begin(), values.end());
        const auto reordered = replaceSection(patched, str->index, buildStrings(values));
        CHECK(decodeCode(reordered) == "packed.strings.order");
        CHECK(decodeCode(reordered) != "packed.identity.mismatch");
    }
    SECTION("REF0 rows") {
        auto patched = encodedBytes(tapChart());
        const auto ref = findSection(patched, "REF0");
        REQUIRE(ref);
        auto rows = readReferences(sectionPayload(patched, *ref));
        REQUIRE(rows.size() > 1U);
        std::reverse(rows.begin(), rows.end());
        const auto reordered = replaceSection(patched, ref->index, buildReferences(rows));
        CHECK(decodeCode(reordered) == "packed.references.order");
        CHECK(decodeCode(reordered) != "packed.identity.mismatch");
    }
    SECTION("IDN0 identity records") {
        auto patched = encodedBytes(withSecondEntity(tapChart()));
        const auto idn = findSection(patched, "IDN0");
        REQUIRE(idn);
        const auto rows = explicitIdentityRecordRanges(sectionPayload(patched, *idn));
        REQUIRE(rows.size() == 2U);
        const auto reordered = replaceSection(
            patched, idn->index, reorderRows(sectionPayload(patched, *idn), rows, {1U, 0U}, 12U));
        CHECK(decodeCode(reordered) == "packed.identity.order");
        CHECK(decodeCode(reordered) != "packed.identity.mismatch");
    }
    SECTION("REQ0 rows") {
        auto chart = tapChart();
        chart.entities.front().requirements.push_back(requirement("jump", 2U));
        auto patched = encodedBytes(chart);
        const auto req = findSection(patched, "REQ0");
        REQUIRE(req);
        const auto rows = requirementRowRanges(sectionPayload(patched, *req));
        REQUIRE(rows.size() == 2U);
        const auto reordered = replaceSection(
            patched, req->index, reorderRows(sectionPayload(patched, *req), rows, {1U, 0U}, 2U));
        CHECK(decodeCode(reordered) == "packed.requirements.order");
        CHECK(decodeCode(reordered) != "packed.identity.mismatch");
    }
    SECTION("CNS0 sets") {
        auto chart = withSecondEntity(tapChart());
        chart.entities.back().requirements.push_back(requirement("hit", 0U));
        auto patched = encodedBytes(chart);
        const auto cns = findSection(patched, "CNS0");
        REQUIRE(cns);
        const auto rows = constraintRowRanges(sectionPayload(patched, *cns));
        REQUIRE(rows.size() == 2U);
        const auto reordered = replaceSection(
            patched, cns->index, reorderRows(sectionPayload(patched, *cns), rows, {1U, 0U}, 0U));
        CHECK(decodeCode(reordered) == "packed.constraints.order");
        CHECK(decodeCode(reordered) != "packed.identity.mismatch");
    }
    SECTION("ARCH archetypes") {
        auto chart = withSecondEntity(tapChart());
        chart.entities.front().components.emplace_back(CanonicalTransform{});
        // Archetypes: mask 0 for the empty entity, mask 5 (Transform and requirement presence).
        auto patched = encodedBytes(chart);
        const auto arch = findSection(patched, "ARCH");
        REQUIRE(arch);
        const auto rows = archetypeRowRanges(sectionPayload(patched, *arch));
        REQUIRE(rows.size() == 2U);
        const auto reordered = replaceSection(
            patched, arch->index, reorderRows(sectionPayload(patched, *arch), rows, {1U, 0U}, 0U));
        CHECK(decodeCode(reordered) == "packed.arch.order");
        CHECK(decodeCode(reordered) != "packed.identity.mismatch");
    }
}

TEST_CASE("R2 the Writer emits the canonical order regardless of the model order",
          "[chart][packed][profile][r2]") {
    SECTION("REQ0 requirements are ordered by localId") {
        auto sorted = tapChart();
        sorted.entities.front().requirements.push_back(requirement("jump", 2U));
        auto shuffled = tapChart();
        shuffled.entities.front().requirements.insert(
            shuffled.entities.front().requirements.begin(), requirement("jump", 2U));
        // "jump" appears first in the model but second on the wire.
        CHECK(encodedBytes(sorted) == encodedBytes(shuffled));
    }
    SECTION("CNS0 sets are ordered by lane") {
        auto ascending = withSecondEntity(tapChart());
        ascending.entities.back().requirements.push_back(requirement("hit", 0U));
        auto descending = withSecondEntity(tapChart());
        descending.entities.back().requirements.push_back(requirement("hit", 3U));
        auto reordered = descending;
        reordered.entities.back().requirements.front().constraints.front() = LaneConstraint{0};
        CHECK(encodedBytes(ascending) == encodedBytes(reordered));
        CHECK(encodedBytes(ascending) != encodedBytes(descending));
    }
    SECTION("ARCH archetypes are ordered by component mask") {
        auto chart = withSecondEntity(tapChart());
        chart.entities.front().components.emplace_back(CanonicalTransform{});
        auto reversed = chart;
        std::swap(reversed.entities.front(), reversed.entities.back());
        // Entity order on the wire follows the identity bytes, and archetypes follow the mask,
        // so the model order of the entity list cannot change the artifact.
        CHECK(encodedBytes(chart) == encodedBytes(reversed));
    }
}

TEST_CASE("R2 inspect stays structural while decode enforces the profile",
          "[chart][packed][profile][r2]") {
    auto chart = tapChart();
    chart.entities.front().requirements.front().constraints.front() = LaneConstraint{7};
    // The Writer refuses the model, so the artifact is crafted from a valid one.
    auto patched = encodedBytes(tapChart());
    const auto cns = findSection(patched, "CNS0");
    REQUIRE(cns);
    const auto rows = constraintRowRanges(sectionPayload(patched, *cns));
    REQUIRE(rows.size() == 1U);
    writeU8(patched, cns->offset + rows[0].first + 1U, 7U);
    refreshCrcs(patched, *cns);

    CHECK(cuexis::chart::packed::inspect(patched));
    CHECK(decodeCode(patched) == "packed.profile.lane");
    CHECK_FALSE(cuexis::chart::PackedChartReader::decode(patched));
}

TEST_CASE("R2 the file Writer bridge inherits the profile refusal",
          "[chart][packed][profile][r2]") {
    auto chart = tapChart();
    chart.entities.front().requirements.front().constraints.front() = LaneConstraint{7};

    const auto sizing = cuexis::chart::PackedChartWriter::size(chart);
    REQUIRE_FALSE(sizing);
    CHECK(sizing.error().code() == "packed.profile.lane");

    const auto target = std::filesystem::temp_directory_path() / "cuexis-r2-profile-refusal.bin";
    std::error_code ignored;
    std::filesystem::remove(target, ignored);
    const auto written = cuexis::chart::PackedChartWriter::writeAtomic(chart, target);
    REQUIRE_FALSE(written);
    CHECK(written.error().code() == "packed.profile.lane");
    // A refused model must not leave a partial artifact behind.
    CHECK_FALSE(std::filesystem::exists(target));
}
