#include "../audio/fake_audio_transport.hpp"

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/runtime_timeline.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input.is_open());
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void checkFramesEqual(const cuexis::playback::RuntimeFrame& first,
                      const cuexis::playback::RuntimeFrame& second) {
    CHECK(first.chartTimeMs == second.chartTimeMs);
    CHECK(first.simulationDeltaTimeMs == second.simulationDeltaTimeMs);
    CHECK(first.timeDiscontinuityId == second.timeDiscontinuityId);
}

void checkSnapshotsEqual(const cuexis::playback::FrameSnapshot& first,
                         const cuexis::playback::FrameSnapshot& second) {
    REQUIRE(first.objects.size() == second.objects.size());
    for (std::size_t objectIndex = 0; objectIndex < first.objects.size(); ++objectIndex) {
        const auto& firstObject = first.objects[objectIndex];
        const auto& secondObject = second.objects[objectIndex];
        CHECK(firstObject.id == secondObject.id);
        CHECK(firstObject.hasTransform == secondObject.hasTransform);
        CHECK(firstObject.visible == secondObject.visible);
        for (std::size_t matrixIndex = 0; matrixIndex < 16; ++matrixIndex) {
            CHECK(firstObject.worldMatrix[matrixIndex] == secondObject.worldMatrix[matrixIndex]);
        }
    }

    CHECK(first.camera.active == second.camera.active);
    CHECK(first.camera.fovY == second.camera.fovY);
    CHECK(first.camera.nearPlane == second.camera.nearPlane);
    CHECK(first.camera.farPlane == second.camera.farPlane);
    CHECK(first.camera.pitch == second.camera.pitch);
    CHECK(first.camera.yaw == second.camera.yaw);
    CHECK(first.camera.roll == second.camera.roll);
    for (std::size_t matrixIndex = 0; matrixIndex < 16; ++matrixIndex) {
        CHECK(first.camera.viewMatrix[matrixIndex] == second.camera.viewMatrix[matrixIndex]);
        CHECK(first.camera.projectionMatrix[matrixIndex] ==
              second.camera.projectionMatrix[matrixIndex]);
    }
    CHECK(first.clearRed == second.clearRed);
    CHECK(first.clearGreen == second.clearGreen);
    CHECK(first.clearBlue == second.clearBlue);
    CHECK(first.clearAlpha == second.clearAlpha);
    CHECK(first.viewportWidth == second.viewportWidth);
    CHECK(first.viewportHeight == second.viewportHeight);
}

} // namespace

TEST_CASE("HostClock and CuexisAudio produce identical RuntimeFrame and FrameSnapshot traces",
          "[playback][timeline][parity]") {
    const auto assetsRoot = std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                            "stage1d_project" / "assets";
    const auto chartText = readFile(assetsRoot / "charts" / "stage1d_example.cuexis.chart.json");
    const cuexis::assets::AssetDatabaseInput databaseConfig{
        .roots = {{.root = {.id = "main", .path = assetsRoot},
                   .index = {.version = 2,
                             .assets = {{.id = {"audio.main"},
                                         .type = cuexis::assets::AssetType::Audio,
                                         .source = "audio/main.wav"}}}}},
    };
    auto hostDatabase = cuexis::assets::AssetDatabase::create(databaseConfig);
    auto audioDatabase = cuexis::assets::AssetDatabase::create(databaseConfig);
    REQUIRE(hostDatabase.has_value());
    REQUIRE(audioDatabase.has_value());

    cuexis::playback::PlaybackSession hostSession{std::move(*hostDatabase)};
    cuexis::playback::PlaybackSession audioSession{std::move(*audioDatabase)};
    auto hostPrepared =
        hostSession.prepareLoad(chartText, cuexis::playback::PlaybackMode::HostClock);
    auto audioPrepared =
        audioSession.prepareLoad(chartText, cuexis::playback::PlaybackMode::CuexisAudio);
    REQUIRE(hostPrepared.has_value());
    REQUIRE(audioPrepared.has_value());
    REQUIRE(hostSession.commit(std::move(*hostPrepared)).has_value());
    REQUIRE(audioSession.commit(std::move(*audioPrepared)).has_value());

    const auto hostContent = hostSession.contentInfo();
    const auto audioContent = audioSession.contentInfo();
    REQUIRE(hostContent.has_value());
    REQUIRE(audioContent.has_value());
    REQUIRE(hostContent->timingOffsetMs == audioContent->timingOffsetMs);
    auto hostTimeline = cuexis::playback::RuntimeTimeline::create(hostContent->timingOffsetMs);
    auto audioTimeline = cuexis::playback::RuntimeTimeline::create(audioContent->timingOffsetMs);
    REQUIRE(hostTimeline.has_value());
    REQUIRE(audioTimeline.has_value());

    cuexis::audio::AudioClipStore store;
    auto clip = cuexis::audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.25F));
    REQUIRE(clip.has_value());
    auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle.has_value());
    cuexis::test_support::FakeAudioTransport audioTransport{store};
    REQUIRE(audioTransport.load(*handle).has_value());
    cuexis::audio::HostClock hostClock;

    const std::vector<cuexis::audio::SourceClockSample> script{
        {0.0, cuexis::audio::PlaybackState::Stopped, 1},
        {0.0, cuexis::audio::PlaybackState::Playing, 1},
        {250.0, cuexis::audio::PlaybackState::Playing, 1},
        {250.0, cuexis::audio::PlaybackState::Paused, 1},
        {750.0, cuexis::audio::PlaybackState::Paused, 2},
        {750.0, cuexis::audio::PlaybackState::Playing, 2},
        {1000.0, cuexis::audio::PlaybackState::Playing, 2},
    };

    for (std::size_t index = 0; index < script.size(); ++index) {
        INFO("control sample " << index);
        REQUIRE(hostClock.submit(script[index]).has_value());
        REQUIRE(audioTransport.submit(script[index]).has_value());
        auto hostFrame = hostTimeline->advance(hostClock.snapshot());
        auto audioFrame = audioTimeline->advance(audioTransport.snapshot().source);
        REQUIRE(hostFrame.has_value());
        REQUIRE(audioFrame.has_value());
        checkFramesEqual(*hostFrame, *audioFrame);

        REQUIRE(hostSession.update(*hostFrame).has_value());
        REQUIRE(audioSession.update(*audioFrame).has_value());
        auto hostSnapshot = hostSession.extractFrame({.width = 1280, .height = 720});
        auto audioSnapshot = audioSession.extractFrame({.width = 1280, .height = 720});
        REQUIRE(hostSnapshot.has_value());
        REQUIRE(audioSnapshot.has_value());
        checkSnapshotsEqual(*hostSnapshot, *audioSnapshot);
    }
}
