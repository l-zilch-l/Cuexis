// R1 contract tests for the CXC candidate chart-entry mapping.
//
// R0 recorded that `validateCandidateChartExtension` had no executable caller and that the CXC
// v1 project-declared closure refused the Spec-shaped `compiled/chart.packed` playback entry.
// R1 admits the entry declared by the registered `cuexis.chart-entry.v1` extension, so these
// cases build the Spec-shaped package directly and verify the compiled semantic identity against
// the identity recomputed from the decoded Packed artifact.

#include "cxc_test_support.hpp"

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/cxc/cxc_writer.hpp>
#include <cuexis/tools/cxc_candidate.hpp>

#include "cxc_hash_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view packedEntryPath{"compiled/chart.packed"};

struct PackedFixture final {
    std::vector<std::byte> bytes;
    std::string semanticIdentity;
};

// The Spec 10.2 one-tap-lane2 chart, encoded as the candidate playback artifact.
[[nodiscard]] auto tapPackedFixture() -> PackedFixture {
    cuexis::chart::CanonicalSemanticChart chart;
    chart.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.features.push_back(
        cuexis::chart::CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    cuexis::chart::CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.judgementDomain = {"candidate.lanes4", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(cuexis::chart::LaneConstraint{2});
    cuexis::chart::CanonicalEntity entity;
    entity.identity = cuexis::chart::ExplicitEntityIdentity{
        cuexis::chart::ChartObjectId{"019b0000-0000-7abc-8def-000000000010"}};
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));

    const auto encoded = cuexis::chart::packed::encode(chart);
    if (!encoded) {
        throw std::runtime_error{"Candidate Packed encode failed for the R1 CXC fixture"};
    }
    const auto identity = cuexis::chart::packed::semanticIdentity(chart);
    if (!identity) {
        throw std::runtime_error{"Candidate semantic identity failed for the R1 CXC fixture"};
    }
    return PackedFixture{*encoded, cuexis::chart::packed::semanticIdentityHex(*identity)};
}

[[nodiscard]] auto candidateExtensionJson(std::string_view entryPath, bool playback,
                                          std::string_view artifactIdentity,
                                          std::string_view compiledIdentity) -> std::string {
    std::ostringstream output;
    output << R"({"cuexis.chart-entry.v1":{"entries":[{"path":")" << entryPath
           << R"(","kind":"chart","encoding":"packed-chart","playback":)"
           << (playback ? "true" : "false") << R"(,"compiledSemanticIdentity":")"
           << compiledIdentity << R"(","artifactIdentity":")" << artifactIdentity
           << R"(","compilerProfile":"candidate.static-tap-lanes4-v1",)"
           << R"("expandedEntityCount":1,"expandedRequirementCount":1}]}})";
    return output.str();
}

// Builds the Spec-shaped candidate package: the Packed playback entry is declared by the
// registered extension and is not part of the project asset closure.
[[nodiscard]] auto makeCandidateRequest(bool playback, std::string_view compiledIdentity,
                                        bool useRealArtifactIdentity)
    -> cuexis::cxc::CxcWriteRequest {
    const auto fixture = tapPackedFixture();
    const auto artifactIdentity = useRealArtifactIdentity
                                      ? cuexis::cxc::detail::sha256Hex(fixture.bytes)
                                      : std::string(64, 'f');
    auto request = cuexis::cxc::test::makeV4StaticRequest();
    request.entries.push_back(cuexis::cxc::test::binaryEntry(
        std::string{packedEntryPath},
        std::string_view{reinterpret_cast<const char*>(fixture.bytes.data()),
                         fixture.bytes.size()}));
    request.extensionsJson =
        candidateExtensionJson(packedEntryPath, playback, artifactIdentity, compiledIdentity);
    return request;
}

[[nodiscard]] auto loadCandidatePackage(const std::vector<std::byte>& bytes)
    -> cuexis::cxc::CxcPackage {
    const auto loaded = cuexis::cxc::CxcPackageLoader::loadMemory(bytes);
    if (!loaded.hasValue()) {
        throw std::runtime_error{"R1 CXC fixture package did not load:\n" +
                                 cuexis::cxc::test::diagnosticsText(loaded.diagnostics)};
    }
    return *loaded.package;
}

} // namespace

TEST_CASE("R1-A03 the Spec-shaped compiled playback entry is admitted by the CXC closure",
          "[cxc][hardening][r1]") {
    const auto fixture = tapPackedFixture();
    const auto bytes =
        cuexis::cxc::test::writePackage(makeCandidateRequest(true, fixture.semanticIdentity, true));
    const auto package = loadCandidatePackage(bytes);

    const auto entry = package.entryBytes(packedEntryPath);
    REQUIRE(entry);
    CHECK(std::equal(entry->begin(), entry->end(), fixture.bytes.begin(), fixture.bytes.end()));
    // The extension declares the entry, so it is reachable without an Asset Index workaround.
    CHECK(cuexis::chart::packed::decode(*entry));
}

TEST_CASE("R1-H01 CXC validation rejects a compiledSemanticIdentity that does not match",
          "[cxc][hardening][r1]") {
    const auto bytes =
        cuexis::cxc::test::writePackage(makeCandidateRequest(true, std::string(64, '0'), true));
    const auto package = loadCandidatePackage(bytes);

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK(
        cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.compiled_identity_mismatch"));
}

TEST_CASE("R1 CXC validation accepts the identity recomputed from the decoded artifact",
          "[cxc][hardening][r1]") {
    const auto fixture = tapPackedFixture();
    const auto bytes =
        cuexis::cxc::test::writePackage(makeCandidateRequest(true, fixture.semanticIdentity, true));
    const auto package = loadCandidatePackage(bytes);

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK_FALSE(diagnostics.hasErrors());
}

TEST_CASE("R1 CXC validation rejects an artifactIdentity that does not match the entry bytes",
          "[cxc][hardening][r1]") {
    const auto bytes =
        cuexis::cxc::test::writePackage(makeCandidateRequest(true, std::string(64, '0'), false));
    const auto package = loadCandidatePackage(bytes);

    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK(
        cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.artifact_identity_mismatch"));
}

TEST_CASE("R1-A03 only extension-declared playback entries join the CXC closure",
          "[cxc][hardening][r1]") {
    SECTION("an extension entry without playback is still outside the closure") {
        const auto fixture = tapPackedFixture();
        const auto written = cuexis::cxc::CxcWriter::write(
            makeCandidateRequest(false, fixture.semanticIdentity, true));
        REQUIRE_FALSE(written.hasValue());
        CHECK(cuexis::cxc::test::hasDiagnostic(written.diagnostics, "cxc.entry.unlisted"));
    }
    SECTION("an undeclared extra entry stays outside the closure") {
        auto request = cuexis::cxc::test::makeV4StaticRequest();
        request.entries.push_back(cuexis::cxc::test::binaryEntry("compiled/extra.bin", "extra"));
        const auto written = cuexis::cxc::CxcWriter::write(std::move(request));
        REQUIRE_FALSE(written.hasValue());
        CHECK(cuexis::cxc::test::hasDiagnostic(written.diagnostics, "cxc.entry.unlisted"));
    }
}
