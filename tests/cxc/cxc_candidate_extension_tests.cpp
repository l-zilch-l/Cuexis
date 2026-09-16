// R0 characterization tests for the CXC candidate chart-entry mapping.
//
// Before this file, `validateCandidateChartExtension` had no test and no executable caller: the
// `cxc_candidate_extension.valid.json` / `.invalid.json` fixtures were documentation-only, and
// their placeholder identities can never satisfy the real validator. These cases build a real
// CXC package that carries real Packed playback bytes, so the mapping is exercised through the
// same implementation entry point the `cxc_validate` tool uses.
//
// Two structural findings shape this file:
//   * R0-A03: the CXC v1 project-declared closure admits no compiled Packed entry, so the
//     Spec-shaped `compiled/chart.packed` manifest entry is refused by CxcWriter/CxcPackageLoader
//     (`cxc.entry.unlisted`). To reach the validator at all, the reachable fixtures below declare
//     the Packed artifact through the Asset Index. That workaround is R0 evidence only; R1/R4 must
//     admit the registered candidate playback entry in the closure itself.
//   * R0-H01: the validator only checks the SHA-256 *shape* of compiledSemanticIdentity.
//
// Cases named R0-H01 record the pre-hardening defect and are flipped by R1.

#include "cxc_test_support.hpp"

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/cxc/cxc_writer.hpp>
#include <cuexis/tools/cxc_candidate.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// The path the Packed artifact must use to stay inside the Asset Index closure (R0-A03).
constexpr std::string_view reachablePackedPath{"assets/compiled/chart.packed"};
// The path the Foundation mapping specifies for a compiled playback entry.
constexpr std::string_view specifiedPackedPath{"compiled/chart.packed"};

[[nodiscard]] auto tapPackedBytes() -> std::vector<std::byte> {
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
        throw std::runtime_error{"Candidate Packed encode failed for the R0 CXC fixture"};
    }
    return *encoded;
}

// Declares the Packed artifact as an Asset Index source so that the CXC closure admits it.
[[nodiscard]] auto assetIndexWithPackedAsset() -> std::string {
    return R"({
  "format": "cuexis.asset-index",
  "version": 1,
  "assets": [
    {
      "id": "candidate.packed",
      "type": "mesh",
      "source": "compiled/chart.packed",
      "dependencies": []
    }
  ],
  "extensions": {}
}
)";
}

[[nodiscard]] auto candidateExtensionJson(std::string_view entryPath,
                                          std::string_view artifactIdentity,
                                          std::string_view compiledIdentity) -> std::string {
    std::ostringstream output;
    output << R"({"cuexis.chart-entry.v1":{"entries":[{"path":")" << entryPath
           << R"(","kind":"chart","encoding":"packed-chart","playback":true,)"
           << R"("compiledSemanticIdentity":")" << compiledIdentity << R"(","artifactIdentity":")"
           << artifactIdentity << R"(","compilerProfile":"candidate.static-tap-lanes4-v1",)"
           << R"("expandedEntityCount":1,"expandedRequirementCount":1}]}})";
    return output.str();
}

struct CandidateRequest final {
    cuexis::cxc::CxcWriteRequest request;
    std::string entryPath;
};

[[nodiscard]] auto makeCandidateRequest(std::string_view entryPath, bool insideAssetClosure,
                                        std::string_view artifactIdentity,
                                        std::string_view compiledIdentity) -> CandidateRequest {
    const auto packed = tapPackedBytes();
    auto request = cuexis::cxc::test::makeV4StaticRequest();
    if (insideAssetClosure) {
        for (auto& entry : request.entries) {
            if (entry.path == "assets/cuexis.asset-index.json") {
                entry.bytes = cuexis::cxc::test::bytesFromText(assetIndexWithPackedAsset());
            }
        }
    }
    request.entries.push_back(cuexis::cxc::test::binaryEntry(
        std::string{entryPath},
        std::string_view{reinterpret_cast<const char*>(packed.data()), packed.size()}));
    request.extensionsJson = candidateExtensionJson(entryPath, artifactIdentity, compiledIdentity);
    return CandidateRequest{std::move(request), std::string{entryPath}};
}

[[nodiscard]] auto writeCandidatePackage(std::string_view entryPath, bool insideAssetClosure,
                                         std::string_view artifactIdentity,
                                         std::string_view compiledIdentity)
    -> std::vector<std::byte> {
    return cuexis::cxc::test::writePackage(
        makeCandidateRequest(entryPath, insideAssetClosure, artifactIdentity, compiledIdentity)
            .request);
}

[[nodiscard]] auto loadCandidatePackage(std::span<const std::byte> bytes,
                                        std::string_view entryPath) -> cuexis::cxc::CxcPackage {
    const auto loaded = cuexis::cxc::CxcPackageLoader::loadMemory(bytes);
    if (!loaded.hasValue()) {
        throw std::runtime_error{"R0 CXC fixture package did not load:\n" +
                                 cuexis::cxc::test::diagnosticsText(loaded.diagnostics)};
    }
    return *loaded.package;
}

[[nodiscard]] auto entryIdentity(const cuexis::cxc::CxcPackage& package, std::string_view path)
    -> std::string {
    for (const auto& entry : package.entries()) {
        if (entry.path == path) {
            return entry.sha256;
        }
    }
    return {};
}

} // namespace

TEST_CASE("R0-A03 A compiled playback entry outside the asset closure is refused at pack time",
          "[cxc][hardening][r0]") {
    const auto placeholder = std::string(64, '0');
    const auto candidate =
        makeCandidateRequest(specifiedPackedPath, false, placeholder, placeholder);
    const auto written = cuexis::cxc::CxcWriter::write(candidate.request);

    // The Foundation mapping in docs/formats/CXC_FORMAT.md places the compiled artifact at
    // `compiled/chart.packed`, but the CXC v1 project-declared closure only admits the project
    // document, Asset Index documents, asset sources and the entry Chart. A real candidate
    // package therefore cannot be produced, let alone validated, without a closure decision.
    REQUIRE_FALSE(written.hasValue());
    CHECK(cuexis::cxc::test::hasDiagnostic(written.diagnostics, "cxc.entry.unlisted"));
}

TEST_CASE("R0 CXC candidate fixture carries real Packed playback bytes", "[cxc][hardening][r0]") {
    const auto placeholder = std::string(64, '0');
    const auto package = loadCandidatePackage(
        writeCandidatePackage(reachablePackedPath, true, placeholder, placeholder),
        reachablePackedPath);
    const auto bytes = package.entryBytes(reachablePackedPath);
    REQUIRE(bytes);
    // The entry is a real Foundation Packed artifact, not a JSON example, and the archive
    // identity is computed from the exact entry bytes.
    CHECK(cuexis::chart::packed::decode(*bytes));
    CHECK(entryIdentity(package, reachablePackedPath).size() == 64U);
    CHECK(entryIdentity(package, reachablePackedPath) != placeholder);
}

TEST_CASE("R0-H01 CXC validation accepts a compiledSemanticIdentity that does not match",
          "[cxc][hardening][r0]") {
    const auto placeholder = std::string(64, '0');
    const auto provisional = loadCandidatePackage(
        writeCandidatePackage(reachablePackedPath, true, placeholder, placeholder),
        reachablePackedPath);
    const auto artifactIdentity = entryIdentity(provisional, reachablePackedPath);
    REQUIRE(artifactIdentity.size() == 64U);

    // artifactIdentity matches the exact entry bytes; compiledSemanticIdentity is well-formed
    // but unrelated to the Packed artifact.
    const auto package = loadCandidatePackage(
        writeCandidatePackage(reachablePackedPath, true, artifactIdentity, placeholder),
        reachablePackedPath);
    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);

    // cxc_candidate.cpp only checks the SHA-256 shape of compiledSemanticIdentity. Because the
    // Packed header identity is itself always zero (H01), no comparison is even possible today.
    // R1 flips this to an explicit compiled-identity comparison against the decoded header.
    CHECK_FALSE(diagnostics.hasErrors());
    CHECK_FALSE(cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.identity_invalid"));
}

TEST_CASE("R0 CXC validation rejects an artifactIdentity that does not match the entry bytes",
          "[cxc][hardening][r0]") {
    const auto package =
        loadCandidatePackage(writeCandidatePackage(reachablePackedPath, true, std::string(64, 'f'),
                                                   std::string(64, '0')),
                             reachablePackedPath);
    const auto diagnostics = cuexis::tools::validateCandidateChartExtension(package);
    CHECK(
        cuexis::cxc::test::hasDiagnostic(diagnostics, "cxc.candidate.artifact_identity_mismatch"));
}
