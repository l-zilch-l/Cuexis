#include "cxc_test_support.hpp"

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/cxc/cxc_candidate.hpp>
#include <cuexis/cxc/cxc_manifest_loader.hpp>
#include <cuexis/cxc/cxc_writer.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "cxc_hash_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <string_view>
#include <utility>

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

TEST_CASE("CXC manifest and candidate identities require exactly 64 lowercase hex characters",
          "[cxc][manifest][candidate][sha256]") {
    const std::string valid = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const std::array cases{
        std::pair{valid, true},
        std::pair{std::string(64, '0'), true},
        std::pair{std::string(64, 'f'), true},
        std::pair{std::string{}, false},
        std::pair{valid.substr(0, 63), false},
        std::pair{valid + "0", false},
        std::pair{"A" + valid.substr(1), false},
        std::pair{valid.substr(0, 63) + "F", false},
        std::pair{valid.substr(0, 32) + "g" + valid.substr(33), false},
        std::pair{valid.substr(0, 32) + "-" + valid.substr(33), false},
        std::pair{valid.substr(0, 32) + " " + valid.substr(33), false},
    };

    for (const auto& [value, accepted] : cases) {
        INFO("SHA-256 text: " << value);
        const auto manifest = cuexis::cxc::CxcManifestLoader::load(
            R"({"format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
                "entries":[{"path":"cuexis.project.json","byteCount":1,"sha256":")" +
            value + R"("}],"requiredExtensions":[],"extensions":{}})");
        CHECK(manifest.hasValue() == accepted);
        CHECK(hasCode(manifest.diagnostics, "cxc.entry.hash_mismatch") == !accepted);
        if (accepted) {
            REQUIRE(manifest.document.has_value());
            REQUIRE(manifest.document->entries.size() == 1);
            CHECK(manifest.document->entries.front().sha256 == value);
        }

        for (const auto field :
             {std::string_view{"artifactIdentity"}, std::string_view{"compiledSemanticIdentity"},
              std::string_view{"sourceSemanticIdentity"}}) {
            INFO("Candidate field: " << field);
            auto entry = entryJson("compiled/chart.packed", true,
                                   field == "artifactIdentity" ? value : valid,
                                   field == "compiledSemanticIdentity" ? value : valid,
                                   "candidate.static-tap-lanes4-v1", 1, 1);
            if (field == "sourceSemanticIdentity") {
                entry.pop_back();
                entry += ",\"sourceSemanticIdentity\":\"" + value + "\"}";
            }
            const auto candidate = cuexis::cxc::parseCandidateChartEntryExtension(
                extensionJson(entry), "compiled/chart.packed");
            CHECK(candidate.hasValue() == accepted);
            CHECK(hasCode(candidate.diagnostics, "cxc.chart_entry.identity_invalid") == !accepted);
        }
    }
}

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

TEST_CASE("Package and selected entry validators share Behavior source validation",
          "[cxc][candidate][behavior-validation]") {
    using namespace cuexis;
    const auto fixture = std::filesystem::path{CUEXIS_SOURCE_DIR} /
                         "tests/fixtures/chart_format_update/behavior_candidate.v5.json";
    std::ifstream input{fixture, std::ios::binary};
    REQUIRE(input.good());
    std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto compiled = chart::compileBehaviorCandidate(source);
    REQUIRE(compiled);
    auto identity = chart::packed::semanticIdentityHex(compiled->metadata.semanticIdentity);
    auto entities = static_cast<std::uint64_t>(compiled->runtime.objects.size());
    std::uint64_t requirements = 0;
    std::string profile{chart::behaviorCandidateProfile};
    std::string encoding{"source-json"};
    bool badArtifact = false;
    bool accepted = false;
    std::string expectedCode;
    SECTION("mixed Packed and source-json entries are accepted") {
        accepted = true;
    }
    SECTION("semantic identity mismatch is rejected") {
        identity = std::string(64, '0');
        expectedCode = "cxc.candidate.compiled_identity_mismatch";
    }
    SECTION("object count mismatch is rejected") {
        ++entities;
        expectedCode = "cxc.candidate.count_mismatch";
    }
    SECTION("Behavior requirements must be zero") {
        requirements = 1;
        expectedCode = "cxc.candidate.count_mismatch";
    }
    SECTION("malformed JSON with matching byte identity is rejected") {
        source = "{";
        expectedCode = "cxc.candidate.source_invalid";
    }
    SECTION("byte identity mismatch is rejected") {
        badArtifact = true;
        expectedCode = "cxc.candidate.artifact_identity_mismatch";
    }
    SECTION("wrong encoding is rejected") {
        encoding = "packed-chart";
    }
    SECTION("unregistered profile is rejected") {
        profile = "candidate.unknown";
        encoding = "packed-chart";
        expectedCode = "cxc.candidate.profile_unsupported";
    }
    SECTION("Behavior input above 16 MiB is rejected before compilation") {
        source.resize(16U * 1024U * 1024U + 1U, ' ');
        expectedCode = "cxc.candidate.budget_exceeded";
    }
    auto request = cxc::test::makeV4StaticRequest();
    const auto packed = chart::packed::encode(tapChart());
    REQUIRE(packed);
    const auto packedIdentity = chart::packed::semanticIdentity(tapChart());
    REQUIRE(packedIdentity);
    const std::string packedPath{"compiled/a.packed"};
    const std::string sourcePath{"compiled/z.v5.json"};
    const auto sourceBytes = cxc::test::binaryEntry(sourcePath, source);
    const auto artifact =
        badArtifact ? std::string(64, '0') : cxc::detail::sha256Hex(sourceBytes.bytes);
    auto sourceEntry =
        entryJson(sourcePath, true, artifact, identity, profile, entities, requirements);
    const auto encodingAt = sourceEntry.find("packed-chart");
    REQUIRE(encodingAt != std::string::npos);
    sourceEntry.replace(encodingAt, std::string{"packed-chart"}.size(), encoding);
    request.entries.push_back(cxc::test::binaryEntry(
        packedPath, {reinterpret_cast<const char*>(packed->data()), packed->size()}));
    request.entries.push_back(sourceBytes);
    request.extensionsJson =
        extensionJson(entryJson(packedPath, true, cxc::detail::sha256Hex(*packed),
                                chart::packed::semanticIdentityHex(*packedIdentity),
                                "candidate.static-tap-lanes4-v1", 1, 1) +
                      "," + sourceEntry);
    const auto written = cxc::CxcWriter::write(std::move(request));
    REQUIRE(written.hasValue());
    const auto loaded = cxc::CxcPackageLoader::loadMemory(*written.bytes);
    REQUIRE(loaded.hasValue());
    const auto aggregate = cxc::validateCandidateChartExtension(*loaded.package);
    const auto selected = cxc::validateCandidateChartEntry(*loaded.package, sourcePath);
    CHECK(aggregate.hasErrors() == !accepted);
    CHECK(selected.hasValue() == accepted);
    if (!expectedCode.empty()) {
        CHECK(hasCode(aggregate, expectedCode));
        CHECK(hasCode(selected.diagnostics, expectedCode));
        for (const auto& diagnostic : aggregate.items()) {
            CHECK(std::string{diagnostic.fieldPath()}.starts_with(
                "$/extensions/cuexis.chart-entry.v1/entries/1"));
        }
    }
    if (accepted) {
        REQUIRE(selected.candidate->behavior);
        CHECK(selected.candidate->behavior->runtime.objects.size() == entities);
        CHECK(selected.candidate->bytes == sourceBytes.bytes);
    }
}
