// R4 end-to-end CXC candidate package tests.
//
// Every case builds a real candidate package with the product CxcWriter, loads it with the
// product CxcPackageLoader, and then runs the product candidate validator. The package exercises
// the actual closure, manifest hashing and validation paths instead of parsing a sample JSON
// document; the Packed entry itself is produced by cuexis::chart::packed::encode.

#include "cxc_test_support.hpp"

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/cxc/cxc_writer.hpp>
#include <cuexis/tools/cxc_candidate.hpp>

#include "cxc_hash_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view packedEntryPath{"compiled/chart.packed"};
constexpr std::string_view profileId{"candidate.static-tap-lanes4-v1"};

struct PackedFixture final {
    std::vector<std::byte> bytes;
    std::string semanticIdentity;
    std::size_t entityCount{};
    std::size_t requirementCount{};
};

// A small candidate chart with a Transform component and one lane-2 tap requirement.
[[nodiscard]] auto packedFixture() -> PackedFixture {
    cuexis::chart::CanonicalSemanticChart chart;
    chart.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.features.push_back(
        cuexis::chart::CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    cuexis::chart::CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.interval.startBeat = cuexis::chart::RationalBeat::zero();
    requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(cuexis::chart::LaneConstraint{2});
    cuexis::chart::CanonicalEntity entity;
    entity.identity = cuexis::chart::ExplicitEntityIdentity{
        cuexis::chart::ChartObjectId{"019b0000-0000-7abc-8def-000000000010"}};
    entity.components.emplace_back(cuexis::chart::CanonicalTransform{});
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));

    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    const auto statistics = cuexis::chart::packed::inspect(*encoded);
    REQUIRE(statistics);
    const auto identity = cuexis::chart::packed::semanticIdentity(chart);
    REQUIRE(identity);
    return PackedFixture{*encoded, cuexis::chart::packed::semanticIdentityHex(*identity),
                         statistics->entityCount, statistics->requirementCount};
}

[[nodiscard]] auto extensionJson(std::string_view entryPath, std::string_view kind,
                                 std::string_view encoding, bool playback,
                                 std::string_view artifactIdentity,
                                 std::string_view compiledIdentity, std::size_t entityCount,
                                 std::size_t requirementCount, std::string_view sourcePath = {})
    -> std::string {
    std::ostringstream output;
    output << R"({"cuexis.chart-entry.v1":{"entries":[{"path":")" << entryPath << R"(","kind":")"
           << kind << R"(","encoding":")" << encoding << R"(","playback":)"
           << (playback ? "true" : "false");
    if (!sourcePath.empty()) {
        output << R"(,"sourcePath":")" << sourcePath << R"(")";
    }
    output << R"(,"compiledSemanticIdentity":")" << compiledIdentity << R"(","artifactIdentity":")"
           << artifactIdentity << R"(","compilerProfile":")" << profileId
           << R"(","expandedEntityCount":)" << entityCount << R"(,"expandedRequirementCount":)"
           << requirementCount << R"(}]}})";
    return output.str();
}

// Builds the Spec-shaped candidate request: the Packed entry is an opaque archive entry declared
// by the registered extension, and it is deliberately not part of the project asset closure.
[[nodiscard]] auto candidateRequest(const PackedFixture& fixture, std::string_view entryPath,
                                    bool addEntry, std::string_view kind, std::string_view encoding,
                                    bool playback, std::string_view artifactIdentity,
                                    std::string_view compiledIdentity, std::size_t entityCount,
                                    std::size_t requirementCount, std::string_view sourcePath = {})
    -> cuexis::cxc::CxcWriteRequest {
    auto request = cuexis::cxc::test::makeV4StaticRequest();
    if (addEntry) {
        request.entries.push_back(cuexis::cxc::test::binaryEntry(
            std::string{entryPath},
            std::string_view{reinterpret_cast<const char*>(fixture.bytes.data()),
                             fixture.bytes.size()}));
    }
    request.extensionsJson =
        extensionJson(entryPath, kind, encoding, playback, artifactIdentity, compiledIdentity,
                      entityCount, requirementCount, sourcePath);
    return request;
}

[[nodiscard]] auto loadPackage(const std::vector<std::byte>& bytes) -> cuexis::cxc::CxcPackage {
    auto loaded = cuexis::cxc::CxcPackageLoader::loadMemory(bytes);
    if (!loaded.hasValue()) {
        throw std::runtime_error{"R4 candidate package did not load:\n" +
                                 cuexis::cxc::test::diagnosticsText(loaded.diagnostics)};
    }
    return *loaded.package;
}

// Flips one byte inside a section payload without repairing any CRC, so the entry is structurally
// corrupt rather than out of profile.
void corruptSectionCrc(std::vector<std::byte>& bytes) {
    REQUIRE(bytes.size() > 200U);
    bytes[bytes.size() - 1U] =
        static_cast<std::byte>(std::to_integer<unsigned>(bytes.back()) ^ 0x5aU);
}

} // namespace

TEST_CASE("R4 a real candidate package validates end to end", "[cxc][hardening][r4]") {
    const auto fixture = packedFixture();
    const auto artifactIdentity = cuexis::cxc::detail::sha256Hex(fixture.bytes);
    const auto request = candidateRequest(fixture, packedEntryPath, true, "chart", "packed-chart",
                                          true, artifactIdentity, fixture.semanticIdentity,
                                          fixture.entityCount, fixture.requirementCount);
    const auto bytes = cuexis::cxc::test::writePackage(std::move(request));
    const auto package = loadPackage(bytes);

    // The entry bytes and the artifact identity come from the Packed encoder, not from a fixture.
    const auto entry = package.entryBytes(packedEntryPath);
    REQUIRE(entry.has_value());
    CHECK(std::vector<std::byte>{entry->begin(), entry->end()} == fixture.bytes);
    const auto decoded = cuexis::chart::packed::decode(*entry);
    REQUIRE(decoded);
    CHECK(decoded->entities.size() == fixture.entityCount);

    // The playback entry is not registered as a project document, so it stays outside the project
    // asset closure while still being admitted by the extension.
    const auto documents = package.projectDocuments();
    CHECK(std::none_of(documents.begin(), documents.end(),
                       [](const auto& document) { return document.path == packedEntryPath; }));

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK_FALSE(diagnostics.hasErrors());
}

TEST_CASE("R4 a declared playback entry missing from the archive is refused",
          "[cxc][hardening][r4]") {
    const auto fixture = packedFixture();
    const auto artifactIdentity = cuexis::cxc::detail::sha256Hex(fixture.bytes);
    const auto bytes = cuexis::cxc::test::writePackage(candidateRequest(
        fixture, packedEntryPath, false, "chart", "packed-chart", true, artifactIdentity,
        fixture.semanticIdentity, fixture.entityCount, fixture.requirementCount));
    const auto package = loadPackage(bytes);

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.entry_missing"));
}

TEST_CASE("R4 a declared count that does not match the artifact is refused",
          "[cxc][hardening][r4]") {
    const auto fixture = packedFixture();
    const auto artifactIdentity = cuexis::cxc::detail::sha256Hex(fixture.bytes);
    const auto bytes = cuexis::cxc::test::writePackage(candidateRequest(
        fixture, packedEntryPath, true, "chart", "packed-chart", true, artifactIdentity,
        fixture.semanticIdentity, fixture.entityCount + 1U, fixture.requirementCount));
    const auto package = loadPackage(bytes);

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.count_mismatch"));
    CHECK_FALSE(
        cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.compiled_identity_mismatch"));
}

TEST_CASE("R4 a structurally corrupt Packed entry is refused", "[cxc][hardening][r4]") {
    auto fixture = packedFixture();
    corruptSectionCrc(fixture.bytes);
    // The declared artifact identity follows the corrupted bytes, so the only defect under test is
    // the section CRC that packed::inspect detects.
    const auto artifactIdentity = cuexis::cxc::detail::sha256Hex(fixture.bytes);
    const auto bytes = cuexis::cxc::test::writePackage(candidateRequest(
        fixture, packedEntryPath, true, "chart", "packed-chart", true, artifactIdentity,
        fixture.semanticIdentity, fixture.entityCount, fixture.requirementCount));
    const auto package = loadPackage(bytes);

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.packed_invalid"));
    CHECK_FALSE(
        cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.artifact_identity_mismatch"));
}

TEST_CASE("R4 a Packed entry above the candidate file budget is refused", "[cxc][hardening][r4]") {
    // The budget gate runs before any Packed parsing, so a 16 MiB + 1 placeholder entry is enough
    // to exercise it without building a legal over-sized artifact.
    const auto oversized = std::vector<std::byte>(16U * 1024U * 1024U + 1U, std::byte{0});
    PackedFixture fixture;
    fixture.bytes = oversized;
    fixture.semanticIdentity = std::string(64, '0');
    fixture.entityCount = 1U;
    fixture.requirementCount = 1U;
    const auto artifactIdentity = cuexis::cxc::detail::sha256Hex(fixture.bytes);
    const auto bytes = cuexis::cxc::test::writePackage(candidateRequest(
        fixture, packedEntryPath, true, "chart", "packed-chart", true, artifactIdentity,
        fixture.semanticIdentity, fixture.entityCount, fixture.requirementCount));
    const auto package = loadPackage(bytes);

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.budget.exceeded"));
}

TEST_CASE("R4 a source-only entry cannot serve as the playback entry", "[cxc][hardening][r4]") {
    // The source chart is reachable as a project document, so the package loads; declaring it as a
    // non-playback chart entry must still leave the package without a compiled playback entry.
    const auto fixture = packedFixture();
    const auto sourcePath = "assets/charts/main.cuexis.chart.json";
    const auto request = cuexis::cxc::test::makeV4StaticRequest();
    const auto sourceEntry =
        std::find_if(request.entries.begin(), request.entries.end(),
                     [&](const auto& entry) { return entry.path == sourcePath; });
    REQUIRE(sourceEntry != request.entries.end());
    const auto artifactIdentity = cuexis::cxc::detail::sha256Hex(sourceEntry->bytes);

    auto candidate = candidateRequest(fixture, sourcePath, false, "chart", "packed-chart", false,
                                      artifactIdentity, fixture.semanticIdentity,
                                      fixture.entityCount, fixture.requirementCount, sourcePath);
    const auto bytes = cuexis::cxc::test::writePackage(std::move(candidate));
    const auto package = loadPackage(bytes);

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.playback_missing"));
    CHECK_FALSE(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.packed_invalid"));
}

TEST_CASE("R4 an entry with an unregistered kind or encoding is refused", "[cxc][hardening][r4]") {
    const auto fixture = packedFixture();
    const auto artifactIdentity = cuexis::cxc::detail::sha256Hex(fixture.bytes);
    SECTION("encoding that is not packed-chart") {
        const auto bytes = cuexis::cxc::test::writePackage(candidateRequest(
            fixture, packedEntryPath, true, "chart", "source-cxt", true, artifactIdentity,
            fixture.semanticIdentity, fixture.entityCount, fixture.requirementCount));
        const auto package = loadPackage(bytes);
        const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
        CHECK(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.entry_unsupported"));
    }
    SECTION("kind that is not chart") {
        const auto bytes = cuexis::cxc::test::writePackage(candidateRequest(
            fixture, packedEntryPath, true, "cxt", "packed-chart", true, artifactIdentity,
            fixture.semanticIdentity, fixture.entityCount, fixture.requirementCount));
        const auto package = loadPackage(bytes);
        const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
        CHECK(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.entry_unsupported"));
    }
}
