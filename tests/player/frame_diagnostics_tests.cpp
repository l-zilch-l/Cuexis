#include "frame_diagnostics.hpp"
#include "snapshot_scene.hpp"

#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/frame_digest.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

class TemporaryTrace final {
  public:
    TemporaryTrace() {
        static std::atomic<unsigned int> next{1};
        directory_ = std::filesystem::temp_directory_path() /
                     ("cuexis-frame-trace-" + std::to_string(next.fetch_add(1)));
        prefix_ = directory_ / "trace";
    }

    ~TemporaryTrace() {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    [[nodiscard]] const std::filesystem::path& prefix() const noexcept {
        return prefix_;
    }

    [[nodiscard]] std::string read(std::string_view suffix) const {
        auto path = prefix_;
        path += suffix;
        std::ifstream input{path, std::ios::binary};
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    }

  private:
    std::filesystem::path directory_;
    std::filesystem::path prefix_;
};

cuexis::playback::FrameSnapshot snapshot() {
    cuexis::playback::FrameSnapshot result;
    result.viewportWidth = 1280;
    result.viewportHeight = 720;
    result.objects.push_back({.id = "object.a", .hasTransform = true});
    result.objects[0].worldMatrix[0] = 1.0F;
    result.objects[0].worldMatrix[5] = 1.0F;
    result.objects[0].worldMatrix[10] = 1.0F;
    result.objects[0].worldMatrix[15] = 1.0F;
    return result;
}

constexpr std::string_view traceChart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000501","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],"behaviors":[],
  "objects":[{
    "id":"019b0000-0000-7abc-8def-000000000510","parent":null,
    "components":{"cuexis.element":{"version":1}},"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

constexpr std::string_view visibilityChart = R"json(
{
  "format":"cuexis.chart","version":3,
  "chartId":"019c0000-0000-7abc-8def-000000000401","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000},
  "templates":[],
  "behaviors":[{
    "id":"visibility.steps","type":"behavior.event","version":1,"events":[],
    "stepEvents":[
      {"property":"render.visible","beat":{"numerator":0,"denominator":1},"value":false},
      {"property":"render.visible","beat":{"numerator":1,"denominator":1},"value":true},
      {"property":"render.visible","beat":{"numerator":2,"denominator":1},"value":false}
    ]
  }],
  "objects":[{
    "id":"019c0000-0000-7abc-8def-000000000410","parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.renderable":{"version":1,
        "mesh":{"domain":"asset","id":"mesh.visibility"},
        "material":{"domain":"asset","id":"material.visibility"}},
      "cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"visibility.steps"}}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

} // namespace

TEST_CASE("Frame diagnostics hash only stable deterministic fields", "[player][frame-stats]") {
    const cuexis::playback::RuntimeFrame frame{100.0, 16.0, 3};
    auto first = snapshot();
    const auto digest = cuexis::playback::computeFrameDigest(frame, first);
    REQUIRE(digest.has_value());
    CHECK(digest->algorithmVersion == 3);
    CHECK(digest->value == 9292624206614054870ULL);
    const auto hash = digest->value;
    CHECK(cuexis::playback::computeFrameDigest(frame, first)->value == hash);

    first.objects[0].worldMatrix[12] = 2.0F;
    CHECK(cuexis::playback::computeFrameDigest(frame, first)->value != hash);
    CHECK(cuexis::playback::computeFrameDigest({100.0, 16.0, 4}, snapshot())->value != hash);

    first = snapshot();
    first.objects[0].visible = false;
    CHECK(cuexis::playback::computeFrameDigest(frame, first)->value != hash);
    first = snapshot();
    first.objects[0].materialAssetId = "material.stage2";
    CHECK(cuexis::playback::computeFrameDigest(frame, first)->value != hash);
    first = snapshot();
    first.objects[0].materialOpacity = 0.5;
    CHECK(cuexis::playback::computeFrameDigest(frame, first)->value != hash);
    first = snapshot();
    first.objects[0].materialTint[1] = 0.25F;
    CHECK(cuexis::playback::computeFrameDigest(frame, first)->value != hash);

    first = snapshot();
    first.objects[0].worldMatrix[12] = std::numeric_limits<float>::infinity();
    const auto invalid = cuexis::playback::computeFrameDigest(frame, first);
    REQUIRE_FALSE(invalid.has_value());
    CHECK(invalid.error().code() == "playback.frame_digest.non_finite");
}

TEST_CASE("Frame diagnostics exports stable prefixes and truncation metadata",
          "[player][frame-stats]") {
    TemporaryTrace trace;
    cuexis::player::FrameDiagnostics diagnostics{trace.prefix(), {.maxRows = 2, .maxBytes = 4096}};
    const auto frameSnapshot = snapshot();
    for (std::uint64_t index = 0; index < 3; ++index) {
        diagnostics.captureFrame(index, {static_cast<double>(index), 0.0, index}, frameSnapshot);
        diagnostics.captureAudio(
            index, static_cast<double>(index),
            {.source = {static_cast<double>(index), cuexis::audio::PlaybackState::Playing, index},
             .presentedFrame = static_cast<std::int64_t>(index),
             .sampleRate = 48000,
             .estimatedOutputLatencyMs = 10.0},
            {.queuedFrames = 100, .underrunCount = index, .serviceCount = index});
    }

    const auto exported = diagnostics.exportArtifacts(cuexis::playback::PlaybackMode::CuexisAudio);
    REQUIRE_FALSE(exported.has_value());
    CHECK(exported.error().code() == "player.frame_stats.truncated");
    CHECK(diagnostics.capturedFrameRows() == 2);
    CHECK(diagnostics.capturedAudioRows() == 2);
    CHECK(diagnostics.droppedFrameRows() == 1);
    CHECK(diagnostics.droppedAudioRows() == 1);

    const auto frames = trace.read(".frames.csv");
    const auto audio = trace.read(".audio.csv");
    const auto meta = trace.read(".meta.json");
    CHECK(frames.starts_with(
        "frameIndex,chartTimeMs,simulationDeltaTimeMs,discontinuityId,frameHash\r\n"));
    CHECK(audio.starts_with("frameIndex,wallClockMs,sourcePositionMs,"
                            "estimatedOutputLatencyMs,queuedFrames,underrunCount,"
                            "transportState\r\n"));
    CHECK(meta.find("\"sdkApiVersion\": \"0.7.0\"") != std::string::npos);
    CHECK(meta.find("\"mode\": \"cuexis_audio\"") != std::string::npos);
    CHECK(meta.find("\"droppedRows\": 1") != std::string::npos);
    CHECK(meta.find("\"truncated\": true") != std::string::npos);
}

TEST_CASE("Frame diagnostics do not change the RuntimeFrame or snapshot trace",
          "[player][frame-stats][parity]") {
    struct Trace final {
        std::vector<cuexis::playback::RuntimeFrame> frames;
        std::vector<std::uint64_t> hashes;
    };

    const auto runTrace = [](const std::optional<std::filesystem::path>& prefix) {
        cuexis::playback::PlaybackSession session;
        REQUIRE(session.loadChart(traceChart).has_value());
        std::optional<cuexis::player::FrameDiagnostics> diagnostics;
        if (prefix) {
            diagnostics.emplace(*prefix);
        }

        constexpr std::array frames{
            cuexis::playback::RuntimeFrame{0.0, 0.0, 0},
            cuexis::playback::RuntimeFrame{16.0, 16.0, 0},
            cuexis::playback::RuntimeFrame{33.0, 17.0, 0},
        };
        Trace trace;
        for (std::size_t index = 0; index < frames.size(); ++index) {
            REQUIRE(session.update(frames[index]).has_value());
            const auto frameSnapshot = session.extractFrame({.width = 1280, .height = 720});
            REQUIRE(frameSnapshot.has_value());
            if (diagnostics) {
                diagnostics->captureFrame(index, frames[index], *frameSnapshot);
            }
            trace.frames.push_back(frames[index]);
            const auto digest = cuexis::playback::computeFrameDigest(frames[index], *frameSnapshot);
            REQUIRE(digest.has_value());
            trace.hashes.push_back(digest->value);
        }
        if (diagnostics) {
            REQUIRE(diagnostics->exportArtifacts(cuexis::playback::PlaybackMode::ChartClock)
                        .has_value());
        }
        return trace;
    };

    TemporaryTrace disabled;
    TemporaryTrace enabled;
    const auto withoutDiagnostics = runTrace(std::nullopt);
    const auto withDiagnostics = runTrace(enabled.prefix());

    REQUIRE(withoutDiagnostics.frames.size() == withDiagnostics.frames.size());
    REQUIRE(withoutDiagnostics.hashes == withDiagnostics.hashes);
    for (std::size_t index = 0; index < withoutDiagnostics.frames.size(); ++index) {
        CHECK(withoutDiagnostics.frames[index].chartTimeMs ==
              withDiagnostics.frames[index].chartTimeMs);
        CHECK(withoutDiagnostics.frames[index].simulationDeltaTimeMs ==
              withDiagnostics.frames[index].simulationDeltaTimeMs);
        CHECK(withoutDiagnostics.frames[index].timeDiscontinuityId ==
              withDiagnostics.frames[index].timeDiscontinuityId);
    }

    auto disabledFramesPath = disabled.prefix();
    disabledFramesPath += ".frames.csv";
    auto disabledAudioPath = disabled.prefix();
    disabledAudioPath += ".audio.csv";
    auto disabledMetaPath = disabled.prefix();
    disabledMetaPath += ".meta.json";
    CHECK_FALSE(std::filesystem::exists(disabledFramesPath));
    CHECK_FALSE(std::filesystem::exists(disabledAudioPath));
    CHECK_FALSE(std::filesystem::exists(disabledMetaPath));
}

TEST_CASE("Player scene adapter accepts invisible frames, visibility changes, and seek",
          "[player][scene][visibility][seek]") {
    auto provider = cuexis::content::HostContentProvider::create(
        [](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            return cuexis::content::ContentBlob{.bytes = {std::byte{0x42}}, .revision = 1};
        });
    REQUIRE(provider.has_value());
    auto source = cuexis::playback::PlaybackSource::fromTypedProject(
        {.sourceId = "player-visibility",
         .chartJson = std::string{visibilityChart},
         .assets = {{.id = "mesh.visibility",
                     .type = cuexis::playback::PlaybackAssetType::Mesh,
                     .rootId = "memory",
                     .logicalSource = "mesh.bin"},
                    {.id = "material.visibility",
                     .type = cuexis::playback::PlaybackAssetType::Material,
                     .rootId = "memory",
                     .logicalSource = "material.bin"}}},
        std::move(*provider));
    REQUIRE(source.has_value());

    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto sceneAt = [&](const cuexis::playback::RuntimeFrame& frame) {
        REQUIRE(session.update(frame).has_value());
        const auto frameSnapshot = session.extractFrame({.width = 1280, .height = 720});
        REQUIRE(frameSnapshot.has_value());
        cuexis::render::RenderScene scene;
        REQUIRE(cuexis::player::appendSnapshotAxes(*frameSnapshot, scene).has_value());
        return scene;
    };

    CHECK(sceneAt({.chartTimeMs = 0.0}).empty());
    CHECK(sceneAt({.chartTimeMs = 500.0, .simulationDeltaTimeMs = 500.0}).size() == 3);
    CHECK(sceneAt({.chartTimeMs = 1000.0, .simulationDeltaTimeMs = 500.0}).empty());
    CHECK(sceneAt({.chartTimeMs = 500.0, .timeDiscontinuityId = 1}).size() == 3);
}
