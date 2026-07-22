#include <cuexis/playback/playback_session.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

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

TEST_CASE("PlaybackSession drives a headless owning snapshot", "[playback][headless]") {
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(chart).has_value());
    REQUIRE(session.state() == cuexis::playback::SessionState::Ready);
    REQUIRE(
        session
            .update({.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());

    auto frame = session.extractFrame({.width = 1280, .height = 720});
    REQUIRE(frame.has_value());
    REQUIRE(frame->objects.size() == 1);
    CHECK(frame->objects[0].id == "019b0000-0000-7abc-8def-000000000210");
    CHECK(frame->objects[0].worldMatrix[12] == Catch::Approx(5.0F));
    CHECK(frame->camera.active);
    CHECK(frame->camera.fovY == Catch::Approx(75.0));
    const float wideProjectionX = frame->camera.projectionMatrix[0];

    auto square = session.extractFrame({.width = 720, .height = 720});
    REQUIRE(square.has_value());
    CHECK(square->camera.projectionMatrix[0] > wideProjectionX);

    const auto saved = *frame;
    REQUIRE(session.unload().has_value());
    CHECK(saved.objects[0].worldMatrix[12] == Catch::Approx(5.0F));
    CHECK(saved.camera.fovY == Catch::Approx(75.0));
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

} // namespace
