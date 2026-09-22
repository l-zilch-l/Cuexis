#include "playback_candidate_internal.hpp"

#include <cuexis/chart/candidate_lowering.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/cxc/cxc_writer.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis_internal/sha256.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

#ifdef CUEXIS_ENABLE_CHART_V5_CANDIDATE

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream stream{path, std::ios::binary};
    REQUIRE(stream.good());
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto textBytes(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

auto writeText(const std::filesystem::path& path, std::string_view text) -> void {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream{path, std::ios::binary};
    REQUIRE(stream.good());
    stream << text;
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

struct PackedCandidate final {
    std::vector<std::byte> bytes;
    std::string semanticIdentity;
    std::string artifactIdentity;
};

[[nodiscard]] auto packedCandidate() -> PackedCandidate {
    const auto chart = tapChart();
    auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded.has_value());
    auto identity = cuexis::chart::packed::semanticIdentity(chart);
    REQUIRE(identity.has_value());
    const auto artifactIdentity = cuexis::core::detail::sha256Hex(*encoded);
    return PackedCandidate{
        std::move(*encoded),
        cuexis::chart::packed::semanticIdentityHex(*identity),
        artifactIdentity,
    };
}

[[nodiscard]] auto extensionJson(const PackedCandidate& candidate, std::string_view path)
    -> std::string {
    return std::string{"{\"cuexis.chart-entry.v1\":{\"entries\":[{\"path\":\""} +
           std::string{path} +
           "\",\"kind\":\"chart\",\"encoding\":\"packed-chart\",\"playback\":true,"
           "\"compiledSemanticIdentity\":\"" +
           candidate.semanticIdentity + "\",\"artifactIdentity\":\"" + candidate.artifactIdentity +
           "\",\"compilerProfile\":\"candidate.static-tap-lanes4-v1\","
           "\"expandedEntityCount\":1,\"expandedRequirementCount\":1}]}}";
}

[[nodiscard]] auto candidatePackage(const PackedCandidate& candidate) -> std::vector<std::byte> {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "static_project";
    cuexis::cxc::CxcWriteRequest request;
    request.entries = {
        cuexis::cxc::CxcWriteEntry{"cuexis.project.json",
                                   textBytes(readText(root / "cuexis.project.json"))},
        cuexis::cxc::CxcWriteEntry{
            "assets/cuexis.asset-index.json",
            textBytes(readText(root / "assets" / "cuexis.asset-index.json"))},
        cuexis::cxc::CxcWriteEntry{
            "assets/charts/main.cuexis.chart.json",
            textBytes(readText(root / "assets" / "charts" / "main.cuexis.chart.json"))},
        cuexis::cxc::CxcWriteEntry{"compiled/chart.packed", candidate.bytes},
    };
    request.extensionsJson = extensionJson(candidate, "compiled/chart.packed");
    auto written = cuexis::cxc::CxcWriter::write(std::move(request));
    REQUIRE(written.hasValue());
    return std::move(*written.bytes);
}

class TemporaryDirectory final {
  public:
    TemporaryDirectory() {
        static unsigned sequence = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("cuexis-candidate-" + std::to_string(++sequence));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    auto operator=(const TemporaryDirectory&) -> TemporaryDirectory& = delete;

    [[nodiscard]] auto path() const -> const std::filesystem::path& {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

#endif

} // namespace

TEST_CASE("Candidate factories stay disabled and do not read their inputs",
          "[playback][candidate][source]") {
#ifndef CUEXIS_ENABLE_CHART_V5_CANDIDATE
    const auto missing = std::filesystem::path{"cuexis-candidate-input-must-not-be-read"};
    const auto project =
        cuexis::playback::PlaybackSource::fromFilesystemProjectEntry(missing, "entry.packed");
    REQUIRE_FALSE(project.has_value());
    CHECK(project.error().code() == "playback.candidate.disabled");

    const auto file = cuexis::playback::PlaybackSource::fromCxcFileEntry(missing, "entry.packed");
    REQUIRE_FALSE(file.has_value());
    CHECK(file.error().code() == "playback.candidate.disabled");

    const auto memory =
        cuexis::playback::PlaybackSource::fromCxcMemoryEntry({std::byte{0x00}}, "entry.packed");
    REQUIRE_FALSE(memory.has_value());
    CHECK(memory.error().code() == "playback.candidate.disabled");
#else
    SUCCEED("Candidate factories are enabled in this build");
#endif
}

#ifdef CUEXIS_ENABLE_CHART_V5_CANDIDATE

TEST_CASE("Explicit candidate file and memory sources prepare the same typed chart",
          "[playback][candidate][prepare]") {
    const auto candidate = packedCandidate();
    const auto bytes = candidatePackage(candidate);
    auto memory =
        cuexis::playback::PlaybackSource::fromCxcMemoryEntry(bytes, "compiled/chart.packed");
    REQUIRE(memory.has_value());

    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*memory), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto* metadata = cuexis::playback::detail::CandidateMetadataAccess::prepared(*prepared);
    REQUIRE(metadata != nullptr);
    REQUIRE(metadata->objects.size() == 1);
    CHECK(metadata->objects[0].executionId == "019b0000-0000-7abc-8def-000000000010");
    CHECK(metadata->objects[0].requirements.size() == 1);
    CHECK(metadata->semanticIdentity ==
          cuexis::chart::packed::semanticIdentity(tapChart()).value());
    CHECK(metadata->resourceClosure.resources.empty());

    const auto expected =
        cuexis::chart::assembleCandidatePreparedSemanticIdentity(metadata->semanticIdentity, {});
    REQUIRE(expected.has_value());
    REQUIRE(prepared->semanticIdentity().has_value());
    CHECK(prepared->semanticIdentity()->sha256 == expected->sha256);
    REQUIRE(prepared->contentInfo() != nullptr);
    CHECK(prepared->contentInfo()->chartFormatVersion == 5);
    CHECK(prepared->contentInfo()->chartId == "019b0000-0000-7abc-8def-000000000001");

    REQUIRE(session.commit(std::move(*prepared)).has_value());
    const auto* active = cuexis::playback::detail::CandidateMetadataAccess::active(session);
    REQUIRE(active != nullptr);
    CHECK(active->objects[0].executionId == "019b0000-0000-7abc-8def-000000000010");
    REQUIRE(
        session.update({.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());
    auto frame = session.extractFrame({.width = 16, .height = 16});
    REQUIRE(frame.has_value());
    REQUIRE(frame->objects.size() == 1);
    CHECK(frame->objects[0].id == "019b0000-0000-7abc-8def-000000000010");

    TemporaryDirectory directory;
    const auto packagePath = directory.path() / "candidate.cxc";
    {
        std::ofstream stream{packagePath, std::ios::binary};
        REQUIRE(stream.good());
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    auto file =
        cuexis::playback::PlaybackSource::fromCxcFileEntry(packagePath, "compiled/chart.packed");
    REQUIRE(file.has_value());
    cuexis::playback::PlaybackSession fileSession;
    REQUIRE(
        fileSession.load(std::move(*file), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto memoryIdentity = session.semanticIdentity();
    const auto fileIdentity = fileSession.semanticIdentity();
    REQUIRE(memoryIdentity.has_value());
    REQUIRE(fileIdentity.has_value());
    CHECK(memoryIdentity->sha256 == fileIdentity->sha256);
}

TEST_CASE("Explicit project candidate entry prepares the packed chart rather than v4",
          "[playback][candidate][project]") {
    const auto candidate = packedCandidate();
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "static_project";
    TemporaryDirectory directory;
    const auto chartText = readText(root / "assets" / "charts" / "main.cuexis.chart.json");
    writeText(directory.path() / "assets" / "cuexis.asset-index.json",
              readText(root / "assets" / "cuexis.asset-index.json"));
    writeText(directory.path() / "assets" / "charts" / "main.cuexis.chart.json", chartText);
    writeText(directory.path() / "compiled" / "chart.packed",
              std::string_view{reinterpret_cast<const char*>(candidate.bytes.data()),
                               candidate.bytes.size()});
    auto project = readText(root / "cuexis.project.json");
    const std::string extension = "\"extensions\": {}";
    const auto position = project.find(extension);
    REQUIRE(position != std::string::npos);
    project.replace(position, extension.size(),
                    "\"extensions\": " + extensionJson(candidate, "compiled/chart.packed"));
    writeText(directory.path() / "cuexis.project.json", project);

    auto source = cuexis::playback::PlaybackSource::fromFilesystemProjectEntry(
        directory.path() / "cuexis.project.json", "compiled/chart.packed");
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    REQUIRE(
        session.update({.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());
    auto frame = session.extractFrame({.width = 16, .height = 16});
    REQUIRE(frame.has_value());
    REQUIRE(frame->objects.size() == 1);
    CHECK(frame->objects[0].id == "019b0000-0000-7abc-8def-000000000010");

    auto fallback = cuexis::playback::PlaybackSource::fromFilesystemProject(directory.path() /
                                                                            "cuexis.project.json");
    REQUIRE(fallback.has_value());
    cuexis::playback::PlaybackSession v4Session;
    REQUIRE(v4Session.load(std::move(*fallback), cuexis::playback::PlaybackMode::ChartClock)
                .has_value());
    REQUIRE(
        v4Session
            .update({.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());
    auto v4Frame = v4Session.extractFrame({.width = 16, .height = 16});
    REQUIRE(v4Frame.has_value());
    CHECK(v4Frame->objects[0].id != frame->objects[0].id);
    CHECK(cuexis::playback::detail::CandidateMetadataAccess::active(v4Session) == nullptr);
}

TEST_CASE("Candidate prepare failure leaves the active session unchanged",
          "[playback][candidate][failure]") {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "static_project";
    const auto chartText = readText(root / "assets" / "charts" / "main.cuexis.chart.json");
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(chartText).has_value());
    const auto identity = session.semanticIdentity();
    REQUIRE(identity.has_value());
    const auto info = session.contentInfo();
    REQUIRE(info.has_value());

    auto tampered = packedCandidate();
    tampered.artifactIdentity = std::string(64, 'a');
    const auto badPackage = candidatePackage(tampered);
    auto rejected =
        cuexis::playback::PlaybackSource::fromCxcMemoryEntry(badPackage, "compiled/chart.packed");
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "playback.candidate.invalid");
    CHECK(session.semanticIdentity()->sha256 == identity->sha256);
    CHECK(session.contentInfo()->chartId == info->chartId);

    const auto good = candidatePackage(packedCandidate());
    auto source =
        cuexis::playback::PlaybackSource::fromCxcMemoryEntry(good, "compiled/chart.packed");
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackPrepareOptions options;
    options.parameters.values.push_back(
        cuexis::playback::ChartParameter{"unused", cuexis::playback::ChartParameterNumber{1.0}});
    const auto reload =
        session.prepareReload(std::move(*source), cuexis::playback::RuntimeFrame{},
                              cuexis::playback::ReloadPolicy::KeepChartTime, options);
    REQUIRE_FALSE(reload.has_value());
    CHECK(session.state().value() == cuexis::playback::SessionState::Ready);
    CHECK(session.semanticIdentity()->sha256 == identity->sha256);
    CHECK(session.contentInfo()->chartFormatVersion == info->chartFormatVersion);
    CHECK(cuexis::playback::detail::CandidateMetadataAccess::active(session) == nullptr);
}

#endif
