#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/cxc/cxc_candidate.hpp>

#include "cxc_hash_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code)
    -> bool {
    for (const auto& item : diagnostics.items()) {
        if (item.code() == code) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto tapChart() -> cuexis::chart::CanonicalSemanticChart {
    cuexis::chart::CanonicalSemanticChart chart;
    chart.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.features.push_back(
        cuexis::chart::CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    cuexis::chart::CanonicalRequirement requirement;
    requirement.localId = "hit";
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

[[nodiscard]] auto extensionJson(std::string_view entries) -> std::string {
    return std::string{"{\"cuexis.chart-entry.v1\":{\"entries\":["} + std::string{entries} + "]}}";
}

[[nodiscard]] auto entryJson(std::string_view path, bool playback, std::string_view artifact,
                             std::string_view compiled, std::string_view profile,
                             std::uint64_t entities, std::uint64_t requirements) -> std::string {
    std::string json = std::string{"{\"path\":\""} + std::string{path} +
                       "\",\"kind\":\"chart\",\"encoding\":\"packed-chart\",\"playback\":";
    json += playback ? "true" : "false";
    json += std::string{",\"compiledSemanticIdentity\":\""} + std::string{compiled} +
            "\",\"artifactIdentity\":\"" + std::string{artifact} + "\",\"compilerProfile\":\"" +
            std::string{profile} + "\",\"expandedEntityCount\":" + std::to_string(entities) +
            ",\"expandedRequirementCount\":" + std::to_string(requirements) + "}";
    return json;
}

} // namespace

TEST_CASE("Candidate bridge selects one playback entry and decodes it once",
          "[cxc][candidate][bridge]") {
    const auto chart = tapChart();
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded.has_value());
    const auto identity = cuexis::chart::packed::semanticIdentity(chart);
    REQUIRE(identity.has_value());
    const auto identityHex = cuexis::chart::packed::semanticIdentityHex(*identity);
    const auto artifact = std::string(64, 'a');
    auto selected = cuexis::cxc::parseCandidateChartEntryExtension(
        extensionJson(entryJson("compiled/chart.packed", true, artifact, identityHex,
                                "candidate.static-tap-lanes4-v1", 1, 1)),
        "compiled/chart.packed");
    REQUIRE(selected.hasValue());

    auto mismatched = cuexis::cxc::validateCandidateChartBytes(*selected.entry, *encoded);
    CHECK_FALSE(mismatched.hasValue());
    CHECK_FALSE(mismatched.candidate.has_value());
    CHECK(hasCode(mismatched.diagnostics, "cxc.candidate.artifact_identity_mismatch"));

    selected.entry->artifactIdentity = cuexis::cxc::detail::sha256Hex(*encoded);
    const auto validated = cuexis::cxc::validateCandidateChartBytes(*selected.entry, *encoded);
    REQUIRE(validated.hasValue());
    CHECK(validated.candidate->semantic.entities.size() == 1);
    CHECK(validated.candidate->bytes.size() == encoded->size());
    CHECK(validated.candidate->artifactIdentity == *selected.entry->artifactIdentity);
}

TEST_CASE("Candidate bridge rejects profile, revision, count, order and path conflicts",
          "[cxc][candidate][bridge]") {
    const auto chart = tapChart();
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded.has_value());
    const auto identityHex =
        cuexis::chart::packed::semanticIdentityHex(*cuexis::chart::packed::semanticIdentity(chart));
    const auto artifact = cuexis::cxc::detail::sha256Hex(*encoded);
    auto entry = cuexis::cxc::parseCandidateChartEntryExtension(
        extensionJson(entryJson("compiled/chart.packed", true, artifact, identityHex,
                                "candidate.static-tap-lanes4-v1", 1, 1)),
        "compiled/chart.packed");
    REQUIRE(entry.hasValue());

    entry.entry->compilerProfile = "candidate.other";
    auto rejected = cuexis::cxc::validateCandidateChartBytes(*entry.entry, *encoded);
    CHECK_FALSE(rejected.candidate.has_value());
    CHECK(hasCode(rejected.diagnostics, "cxc.candidate.profile_unsupported"));

    entry.entry->compilerProfile = "candidate.static-tap-lanes4-v1";
    entry.entry->expandedEntityCount = 2;
    rejected = cuexis::cxc::validateCandidateChartBytes(*entry.entry, *encoded);
    CHECK_FALSE(rejected.candidate.has_value());
    CHECK(hasCode(rejected.diagnostics, "cxc.candidate.count_mismatch"));

    const auto garbage = std::vector<std::byte>{std::byte{1}, std::byte{2}, std::byte{3}};
    entry.entry->artifactIdentity = cuexis::cxc::detail::sha256Hex(garbage);
    entry.entry->expandedEntityCount = 1;
    rejected = cuexis::cxc::validateCandidateChartBytes(*entry.entry, garbage);
    CHECK_FALSE(rejected.candidate.has_value());
    const bool packedOrRevision =
        hasCode(rejected.diagnostics, "cxc.candidate.packed_invalid") ||
        hasCode(rejected.diagnostics, "cxc.candidate.revision_unsupported");
    CHECK(packedOrRevision);

    const auto duplicate = cuexis::cxc::parseCandidateChartEntryExtension(
        extensionJson(entryJson("compiled/a.packed", true, artifact, identityHex,
                                "candidate.static-tap-lanes4-v1", 1, 1) +
                      "," +
                      entryJson("compiled/a.packed", true, artifact, identityHex,
                                "candidate.static-tap-lanes4-v1", 1, 1)),
        "compiled/a.packed");
    CHECK_FALSE(duplicate.hasValue());
    CHECK(hasCode(duplicate.diagnostics, "cxc.chart_entry.duplicate_path"));

    const auto unordered = cuexis::cxc::parseCandidateChartEntryExtension(
        extensionJson(entryJson("compiled/b.packed", true, artifact, identityHex,
                                "candidate.static-tap-lanes4-v1", 1, 1) +
                      "," +
                      entryJson("compiled/a.packed", true, artifact, identityHex,
                                "candidate.static-tap-lanes4-v1", 1, 1)),
        "compiled/a.packed");
    CHECK_FALSE(unordered.hasValue());
    CHECK(hasCode(unordered.diagnostics, "cxc.chart_entry.order_invalid"));

    const auto nested = cuexis::cxc::parseCandidateChartEntryExtension(
        extensionJson(entryJson("compiled/chart.packed", true, artifact, identityHex,
                                "candidate.static-tap-lanes4-v1", 1, 1) +
                      "," +
                      entryJson("compiled/chart.packed/extra.packed", true, artifact, identityHex,
                                "candidate.static-tap-lanes4-v1", 1, 1)),
        "compiled/chart.packed");
    CHECK_FALSE(nested.hasValue());
    CHECK(hasCode(nested.diagnostics, "cxc.chart_entry.duplicate_path"));
}
