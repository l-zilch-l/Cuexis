#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

static_assert(!std::is_move_constructible_v<cuexis::playback::PlaybackSession>);
static_assert(!std::is_move_assignable_v<cuexis::playback::PlaybackSession>);
static_assert(!std::is_copy_constructible_v<cuexis::playback::PreparedPlayback>);
static_assert(std::is_nothrow_move_constructible_v<cuexis::playback::PreparedPlayback>);

constexpr std::string_view chart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000201","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
            "pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
  "templates":[],
  "behaviors":[{
    "id":"move","type":"behavior.transform.keyframe","version":1,
    "tracks":[
      {"property":"transform.position.x","keys":[
        {"beat":{"numerator":0,"denominator":1},"value":0},
        {"beat":{"numerator":1,"denominator":1},"value":10,"easing":"linear"}]},
      {"property":"camera.fovY","keys":[
        {"beat":{"numerator":0,"denominator":1},"value":60},
        {"beat":{"numerator":1,"denominator":1},"value":90,"easing":"linear"}]}
    ]
  }],
  "objects":[{
    "id":"019b0000-0000-7abc-8def-000000000210","name":"camera","parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,-10],
                          "rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.camera":{"version":1,"type":"perspective","fovY":60,
                       "near":0.1,"far":1000},
      "cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"move"}}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open stage 1C chart: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    const auto text = readFile(path);
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const unsigned char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

TEST_CASE("PlaybackSession drives a headless owning snapshot", "[playback][headless]") {
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(chart).has_value());
    REQUIRE(session.state().has_value());
    REQUIRE(*session.state() == cuexis::playback::SessionState::Ready);
    REQUIRE(
        session
            .update({.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());

    auto frame = session.extractFrame({.width = 1280, .height = 720});
    REQUIRE(frame.has_value());
    REQUIRE(frame->objects.size() == 1);
    CHECK(frame->objects[0].id == "019b0000-0000-7abc-8def-000000000210");
    CHECK(frame->objects[0].hasTransform);
    CHECK(frame->objects[0].worldMatrix[12] == Catch::Approx(5.0F));
    CHECK(frame->camera.active);
    CHECK(frame->camera.fovY == Catch::Approx(75.0));
    const float wideProjectionX = frame->camera.projectionMatrix[0];

    auto square = session.extractFrame({.width = 720, .height = 720});
    REQUIRE(square.has_value());
    CHECK(square->camera.projectionMatrix[0] > wideProjectionX);

    cuexis::playback::FrameSnapshot reusable;
    REQUIRE(session.extractFrame({.width = 1280, .height = 720}, reusable).has_value());
    const auto* const objectBuffer = reusable.objects.data();
    const auto* const idBuffer = reusable.objects.front().id.data();
    REQUIRE(session.update({.chartTimeMs = 500.0, .simulationDeltaTimeMs = 250.0}).has_value());
    REQUIRE(session.extractFrame({.width = 1280, .height = 720}, reusable).has_value());
    CHECK(reusable.objects.data() == objectBuffer);
    CHECK(reusable.objects.front().id.data() == idBuffer);
    CHECK(reusable.objects.front().worldMatrix[12] == Catch::Approx(10.0F));

    const auto saved = *frame;
    REQUIRE(session.unload().has_value());
    CHECK(saved.objects[0].worldMatrix[12] == Catch::Approx(5.0F));
    CHECK(saved.camera.fovY == Catch::Approx(75.0));
}

TEST_CASE("Playback snapshot includes non-spatial objects with explicit transform presence",
          "[playback][snapshot]") {
    constexpr std::string_view nonSpatialChart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000301","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],"behaviors":[],
  "objects":[{
    "id":"019b0000-0000-7abc-8def-000000000310","parent":null,
    "components":{"cuexis.element":{"version":1}},"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(nonSpatialChart).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto info = session.chartInfo();
    const auto frame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(info.has_value());
    REQUIRE(frame.has_value());
    REQUIRE(frame->objects.size() == info->objectCount);
    REQUIRE(frame->objects.size() == 1);
    CHECK_FALSE(frame->objects[0].hasTransform);
    CHECK(frame->objects[0].worldMatrix[0] == Catch::Approx(1.0F));
    CHECK(frame->objects[0].worldMatrix[5] == Catch::Approx(1.0F));
    CHECK(frame->objects[0].worldMatrix[10] == Catch::Approx(1.0F));
    CHECK(frame->objects[0].worldMatrix[15] == Catch::Approx(1.0F));
}

TEST_CASE("PlaybackSession host-driven seek and explicit reload sample the target frame",
          "[playback][seek][reload]") {
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(chart).has_value());
    REQUIRE(
        session
            .update({.chartTimeMs = 400.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());
    REQUIRE(
        session
            .update({.chartTimeMs = 100.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1})
            .has_value());
    auto seekFrame = session.extractFrame({.width = 800, .height = 600});
    REQUIRE(seekFrame.has_value());
    CHECK(seekFrame->objects[0].worldMatrix[12] == Catch::Approx(2.0F));

    REQUIRE(
        session
            .reload(chart,
                    {.chartTimeMs = 375.0, .simulationDeltaTimeMs = 99.0, .timeDiscontinuityId = 2},
                    cuexis::playback::ReloadPolicy::KeepChartTime)
            .has_value());
    auto reloadFrame = session.extractFrame({.width = 800, .height = 600});
    REQUIRE(reloadFrame.has_value());
    CHECK(reloadFrame->objects[0].worldMatrix[12] == Catch::Approx(7.5F));
    CHECK(reloadFrame->camera.fovY == Catch::Approx(82.5));
}

TEST_CASE("PlaybackSession exposes complete diagnostics for a failed reload",
          "[playback][reload][diagnostics]") {
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(chart).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());

    auto invalid = std::string{chart};
    const auto fov = invalid.find("\"fovY\":60");
    REQUIRE(fov != std::string::npos);
    invalid.replace(fov, std::string_view{"\"fovY\":60"}.size(), "\"fovY\":0");
    const auto nearPlane = invalid.find("\"near\":0.1");
    REQUIRE(nearPlane != std::string::npos);
    invalid.replace(nearPlane, std::string_view{"\"near\":0.1"}.size(), "\"near\":2");

    const auto failed = session.reload(
        invalid, {.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1},
        cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE_FALSE(failed.has_value());
    REQUIRE(session.lastOperationDiagnostics().has_value());
    CHECK(session.lastOperationDiagnostics()->hasErrors());
    REQUIRE(session.state().has_value());
    CHECK(*session.state() == cuexis::playback::SessionState::Running);
}

TEST_CASE("PlaybackSession target-frame reload failure preserves active data and diagnostics",
          "[playback][reload][rollback][diagnostics]") {
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(chart).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());

    const auto frameBefore = session.extractFrame({.width = 800, .height = 600});
    const auto contentBefore = session.contentInfo();
    const auto diagnosticsBefore = session.diagnostics();
    REQUIRE(frameBefore.has_value());
    REQUIRE(contentBefore.has_value());
    REQUIRE(diagnosticsBefore.has_value());

    const auto failed =
        session.reload(chart, {.chartTimeMs = std::numeric_limits<double>::quiet_NaN()},
                       cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "playback.session.reload_sample_failed");
    REQUIRE(failed.error().context().size() == 1);
    CHECK(failed.error().context()[0].key == "diagnostic_code");
    CHECK(failed.error().context()[0].value == "runtime.frame.chart_time_non_finite");

    const auto operationDiagnostics = session.lastOperationDiagnostics();
    REQUIRE(operationDiagnostics.has_value());
    REQUIRE(operationDiagnostics->size() == 1);
    CHECK(operationDiagnostics->items().front().code() == "runtime.frame.chart_time_non_finite");

    const auto stateAfter = session.state();
    const auto frameAfter = session.extractFrame({.width = 800, .height = 600});
    const auto contentAfter = session.contentInfo();
    const auto diagnosticsAfter = session.diagnostics();
    REQUIRE(stateAfter.has_value());
    CHECK(*stateAfter == cuexis::playback::SessionState::Running);
    REQUIRE(frameAfter.has_value());
    CHECK(frameAfter->objects[0].worldMatrix[12] ==
          Catch::Approx(frameBefore->objects[0].worldMatrix[12]));
    CHECK(frameAfter->camera.fovY == Catch::Approx(frameBefore->camera.fovY));
    REQUIRE(contentAfter.has_value());
    CHECK(contentAfter->chartId == contentBefore->chartId);
    CHECK(contentAfter->mode == contentBefore->mode);
    REQUIRE(diagnosticsAfter.has_value());
    CHECK(diagnosticsAfter->size() == diagnosticsBefore->size());
}

TEST_CASE("PlaybackSession instances are independent and validate lifecycle errors",
          "[playback][lifecycle]") {
    cuexis::playback::PlaybackSession first;
    cuexis::playback::PlaybackSession second;
    REQUIRE_FALSE(first.extractFrame({.width = 1, .height = 1}).has_value());
    REQUIRE(first.loadChart(chart).has_value());
    REQUIRE(second.loadChart(chart).has_value());
    REQUIRE(first.update({.chartTimeMs = 0.0}).has_value());
    REQUIRE(second.update({.chartTimeMs = 500.0}).has_value());
    auto firstFrame = first.extractFrame({.width = 640, .height = 480});
    auto secondFrame = second.extractFrame({.width = 640, .height = 480});
    REQUIRE(firstFrame.has_value());
    REQUIRE(secondFrame.has_value());
    CHECK(firstFrame->objects[0].worldMatrix[12] == Catch::Approx(0.0F));
    CHECK(secondFrame->objects[0].worldMatrix[12] == Catch::Approx(10.0F));
    REQUIRE_FALSE(first.extractFrame({.width = 0, .height = 480}).has_value());
    REQUIRE_FALSE(first.loadChart(chart).has_value());
}

TEST_CASE("Stage 1C project samples all demo properties deterministically",
          "[playback][stage1c][seek]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                      "stage1c_project" / "assets" / "charts" / "stage1c_example.cuexis.chart.json";
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(readFile(path)).has_value());

    const auto info = session.chartInfo();
    REQUIRE(info.has_value());
    CHECK(info->objectCount == 3);
    CHECK(info->behaviorCount == 3);

    REQUIRE(
        session
            .update({.chartTimeMs = 500.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());
    const auto first = session.extractFrame({.width = 1280, .height = 720});
    REQUIRE(first.has_value());
    REQUIRE(first->objects.size() == 3);
    CHECK(first->objects.size() * 3 == 9);
    CHECK(first->camera.active);
    CHECK(first->camera.fovY == Catch::Approx(70.0));
    CHECK(first->objects[1].worldMatrix[12] == Catch::Approx(0.0F));
    CHECK(first->objects[2].worldMatrix[0] == Catch::Approx(1.375F));
    CHECK(first->objects[2].worldMatrix[5] == Catch::Approx(0.75F));
    CHECK(first->objects[2].worldMatrix[10] == Catch::Approx(1.125F));

    REQUIRE(
        session
            .update(
                {.chartTimeMs = 1000.0, .simulationDeltaTimeMs = 500.0, .timeDiscontinuityId = 0})
            .has_value());
    REQUIRE(
        session
            .update({.chartTimeMs = 500.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1})
            .has_value());
    const auto seek = session.extractFrame({.width = 1280, .height = 720});
    REQUIRE(seek.has_value());
    CHECK(seek->camera.fovY == Catch::Approx(first->camera.fovY));
    for (std::size_t objectIndex = 0; objectIndex < first->objects.size(); ++objectIndex) {
        CHECK(seek->objects[objectIndex].id == first->objects[objectIndex].id);
        for (std::size_t matrixIndex = 0; matrixIndex < 16; ++matrixIndex) {
            CHECK(seek->objects[objectIndex].worldMatrix[matrixIndex] ==
                  Catch::Approx(first->objects[objectIndex].worldMatrix[matrixIndex]));
        }
    }
}

TEST_CASE("PlaybackSession keeps the Stage 1B Renderable resource closure alive",
          "[playback][stage1b][resources]") {
    const auto projectRoot = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                             "stage1b_project" / "assets";
    auto source =
        cuexis::playback::PlaybackSource::fromFilesystemProject(projectRoot.parent_path());
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto info = session.chartInfo();
    REQUIRE(info.has_value());
    CHECK(info->objectCount == 4);
    CHECK(info->renderableCount == 3);
    CHECK(info->resourceCount == 3);

    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto snapshot = session.extractFrame({.width = 1280, .height = 720});
    REQUIRE(snapshot.has_value());
    CHECK(snapshot->objects.size() == 4);
    CHECK(snapshot->camera.active);
    REQUIRE(session.unload().has_value());
}

TEST_CASE("PlaybackSession prepares typed main music before an atomic commit",
          "[playback][audio][prepared]") {
    const auto assetsRoot = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                            "stage1d_project" / "assets";
    const auto chartText = readFile(assetsRoot / "charts" / "stage1d_example.cuexis.chart.json");
    auto source = cuexis::playback::PlaybackSource::fromFilesystemProject(assetsRoot.parent_path());
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::CuexisAudio);
    REQUIRE(prepared.has_value());
    REQUIRE(prepared->valid());
    REQUIRE(prepared->contentInfo() != nullptr);
    CHECK(prepared->contentInfo()->chartFormatVersion == 2);
    CHECK(prepared->contentInfo()->mode == cuexis::playback::PlaybackMode::CuexisAudio);
    REQUIRE(prepared->contentInfo()->mainMusicAssetId.has_value());
    CHECK(*prepared->contentInfo()->mainMusicAssetId == "audio.main");
    const auto mainMusic = prepared->mainMusicSource();
    REQUIRE(mainMusic.has_value());
    CHECK(mainMusic->assetId == "audio.main");
    CHECK(mainMusic->contentRevision != 0);
    CHECK(mainMusic->bytes.size() == 192044);
    REQUIRE(session.state().has_value());
    CHECK(*session.state() == cuexis::playback::SessionState::Empty);

    REQUIRE(session.commit(std::move(*prepared)).has_value());
    CHECK_FALSE(prepared->valid());
    const auto committed = session.contentInfo();
    REQUIRE(committed.has_value());
    CHECK(committed->mode == cuexis::playback::PlaybackMode::CuexisAudio);
}

TEST_CASE("PlaybackSession rejects host provider reentry without changing the active session",
          "[playback][content][reentry]") {
    const auto projectRoot =
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage1d_project";
    const auto chartText =
        readFile(projectRoot / "assets" / "charts" / "stage1d_example.cuexis.chart.json");
    const auto audioBytes = readBytes(projectRoot / "assets" / "audio" / "main.wav");

    auto initial = cuexis::playback::PlaybackSource::fromFilesystemProject(projectRoot);
    REQUIRE(initial.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*initial), cuexis::playback::PlaybackMode::HostClock).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    const auto before = session.contentInfo();
    REQUIRE(before.has_value());

    std::vector<std::string> reentryErrors;
    auto provider = cuexis::content::HostContentProvider::create(
        [&](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            const auto record = [&reentryErrors](const auto& result) {
                reentryErrors.emplace_back(result ? "unexpected_success"
                                                  : std::string{result.error().code()});
            };
            record(session.state());
            record(session.update({.chartTimeMs = 500.0}));
            record(session.commit(cuexis::playback::PreparedPlayback{}));
            record(session.unload());
            return cuexis::content::ContentBlob{.bytes = audioBytes, .revision = 2};
        });
    REQUIRE(provider.has_value());

    auto replacement = cuexis::playback::PlaybackSource::fromTypedProject(
        {.sourceId = "reentry-replacement",
         .chartJson = chartText,
         .assets = {{.id = "audio.main",
                     .type = cuexis::playback::PlaybackAssetType::Audio,
                     .rootId = "main",
                     .logicalSource = "audio/main.wav"}}},
        std::move(*provider));
    REQUIRE(replacement.has_value());

    auto prepared = session.prepareReload(
        std::move(*replacement),
        {.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1},
        cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE(prepared.has_value());
    REQUIRE(reentryErrors.size() == 4);
    for (const auto& error : reentryErrors) {
        CHECK(error == "playback.session.reentrant");
    }

    const auto afterState = session.state();
    REQUIRE(afterState.has_value());
    CHECK(*afterState == cuexis::playback::SessionState::Running);
    const auto after = session.contentInfo();
    REQUIRE(after.has_value());
    CHECK(after->chartId == before->chartId);
}

TEST_CASE("Prepared commit publishes both prebuilt diagnostic snapshots",
          "[playback][prepared][diagnostics]") {
    constexpr std::string_view chartWithWarning = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000299","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],"behaviors":[],"objects":[],"requiredExtensions":[],
  "extensions":{"org.example.optional":{"version":1,"data":{"x":1}}}
}
)json";

    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(chartWithWarning, cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());

    const auto active = session.diagnostics();
    const auto lastOperation = session.lastOperationDiagnostics();
    REQUIRE(active.has_value());
    REQUIRE(lastOperation.has_value());
    REQUIRE(active->size() == 1);
    REQUIRE(lastOperation->size() == 1);
    CHECK(active->items().front().code() == "chart.extension.optional_unknown");
    CHECK(lastOperation->items().front().code() == active->items().front().code());
}

TEST_CASE("PlaybackSession rejects mode mismatch and stale prepared tokens",
          "[playback][audio][prepared]") {
    const auto assetsRoot = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                            "stage1d_project" / "assets";
    auto makeSource = [&]() {
        return cuexis::playback::PlaybackSource::fromFilesystemProject(assetsRoot.parent_path());
    };
    auto mismatchSource = makeSource();
    REQUIRE(mismatchSource.has_value());
    cuexis::playback::PlaybackSession mismatch;
    const auto rejected = mismatch.prepareLoad(std::move(*mismatchSource),
                                               cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "playback.mode.content_mismatch");

    auto staleSource = makeSource();
    auto currentSource = makeSource();
    REQUIRE(staleSource.has_value());
    REQUIRE(currentSource.has_value());
    cuexis::playback::PlaybackSession session;
    auto stale =
        session.prepareLoad(std::move(*staleSource), cuexis::playback::PlaybackMode::HostClock);
    auto current =
        session.prepareLoad(std::move(*currentSource), cuexis::playback::PlaybackMode::HostClock);
    REQUIRE(stale.has_value());
    REQUIRE(current.has_value());
    REQUIRE(session.commit(std::move(*current)).has_value());
    const auto staleCommit = session.commit(std::move(*stale));
    REQUIRE_FALSE(staleCommit.has_value());
    CHECK(staleCommit.error().code() == "playback.prepared.stale");
}

TEST_CASE("PreparedPlayback expires after an active session update",
          "[playback][prepared][ownership]") {
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(chart).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());

    auto prepared = session.prepareReload(
        chart, {.chartTimeMs = 100.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1},
        cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE(prepared.has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0, .simulationDeltaTimeMs = 250.0}).has_value());

    const auto committed = session.commit(std::move(*prepared));
    REQUIRE_FALSE(committed.has_value());
    CHECK(committed.error().code() == "playback.prepared.stale");
}

TEST_CASE("PreparedPlayback rejects a same-address replacement owner",
          "[playback][prepared][ownership]") {
    std::optional<cuexis::playback::PlaybackSession> storage;
    storage.emplace();
    const auto* firstAddress = &*storage;
    auto prepared = storage->prepareLoad(chart, cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());

    storage.reset();
    storage.emplace();
    REQUIRE(&*storage == firstAddress);
    const auto committed = storage->commit(std::move(*prepared));
    REQUIRE_FALSE(committed.has_value());
    CHECK(committed.error().code() == "playback.prepared.wrong_session");
}

TEST_CASE("PlaybackSession rejects every stateful operation from a non-owner thread",
          "[playback][thread]") {
    cuexis::playback::PlaybackSession session;
    auto worker = std::async(std::launch::async, [&session] {
        std::vector<std::string> errors;
        cuexis::playback::FrameSnapshot destination;
        const auto record = [&errors](const auto& result) {
            if (!result) {
                errors.emplace_back(result.error().code());
            }
        };
        record(session.state());
        record(session.loadChart(chart));
        record(session.update({.chartTimeMs = 0.0}));
        record(session.extractFrame({.width = 1, .height = 1}));
        record(session.extractFrame({.width = 1, .height = 1}, destination));
        record(session.chartInfo());
        record(session.diagnostics());
        record(session.lastOperationDiagnostics());
        record(session.unload());
        return errors;
    });

    const auto errors = worker.get();
    REQUIRE(errors.size() == 9);
    for (const auto& error : errors) {
        CHECK(error == "playback.session.not_owner_thread");
    }
    REQUIRE(session.state().has_value());
    CHECK(*session.state() == cuexis::playback::SessionState::Empty);
    REQUIRE(session.loadChart(chart).has_value());
}

TEST_CASE("PlaybackSession capability preflight is explicit, deterministic, and pre-resource",
          "[playback][capability][stage2]") {
    const auto stage2Path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "charts" /
                            "stage2_example.cuexis.chart.json";
    const auto stage2Chart = readFile(stage2Path);

    cuexis::playback::PlaybackSession defaultSession;
    const auto defaultCapabilities = defaultSession.capabilities();
    REQUIRE(defaultCapabilities.has_value());
    CHECK(defaultCapabilities->version == 1);
    CHECK(defaultCapabilities->ids ==
          std::vector<std::string>{"cuexis.behavior.event.v1", "cuexis.chart.v3", "cuexis.chart.v4",
                                   "cuexis.material.snapshot.v1", "cuexis.render.visibility.v1",
                                   "cuexis.source.cxc.v1", "cuexis.source.cxt.v1"});
    REQUIRE(defaultSession.loadChart(stage2Chart).has_value());

    cuexis::playback::PlaybackSession unsupported{
        cuexis::playback::PlaybackCapabilitySet{.version = 1, .ids = {}}};
    const auto rejected =
        unsupported.prepareLoad(stage2Chart, cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "playback.capability.preflight_failed");
    const auto diagnostics = unsupported.lastOperationDiagnostics();
    REQUIRE(diagnostics.has_value());
    REQUIRE(diagnostics->size() == 2);
    CHECK(diagnostics->items()[0].code() == "playback.capability.unsupported");
    CHECK(diagnostics->items()[0].fieldPath() == "$/behaviors");
    CHECK(diagnostics->items()[0].context()[0].value == "cuexis.behavior.event.v1");
    CHECK(diagnostics->items()[1].fieldPath() == "$/version");
    CHECK(diagnostics->items()[1].context()[0].value == "cuexis.chart.v3");

    constexpr std::string_view renderableV3 = R"json(
{
  "format":"cuexis.chart","version":3,
  "chartId":"019c0000-0000-7abc-8def-000000000099","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "templates":[],"behaviors":[],
  "objects":[{
    "id":"019c0000-0000-7abc-8def-000000000010","parent":null,
    "components":{
      "cuexis.renderable":{"version":1,
        "mesh":{"domain":"asset","id":"mesh.stage2"},
        "material":{"domain":"asset","id":"material.stage2"}}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";
    std::size_t providerReads = 0;
    auto provider = cuexis::content::HostContentProvider::create(
        [&providerReads](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            ++providerReads;
            return cuexis::content::ContentBlob{};
        });
    REQUIRE(provider.has_value());
    auto source = cuexis::playback::PlaybackSource::fromTypedProject(
        {.sourceId = "capability-preflight",
         .chartJson = std::string{renderableV3},
         .assets = {{.id = "mesh.stage2",
                     .type = cuexis::playback::PlaybackAssetType::Mesh,
                     .rootId = "memory",
                     .logicalSource = "mesh.bin"},
                    {.id = "material.stage2",
                     .type = cuexis::playback::PlaybackAssetType::Material,
                     .rootId = "memory",
                     .logicalSource = "material.bin"}}},
        std::move(*provider));
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession chartOnly{cuexis::playback::PlaybackCapabilitySet{
        .version = 1, .ids = {std::string{cuexis::playback::capabilityChartV3}}}};
    const auto resourceRejected =
        chartOnly.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(resourceRejected.has_value());
    CHECK(providerReads == 0);
    CHECK(resourceRejected.error().code() == "playback.capability.preflight_failed");
}

} // namespace
