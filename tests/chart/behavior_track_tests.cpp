#include <cuexis/chart/chart_runtime.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

namespace {

auto beat(std::int64_t numerator, std::int64_t denominator = 1) {
    return *cuexis::chart::RationalBeat::create(numerator, denominator);
}

auto documentWithBehavior(std::vector<cuexis::chart::BehaviorTrack> tracks) {
    cuexis::chart::ObjectComponents components;
    components.transform = cuexis::chart::TransformData{};
    components.camera = cuexis::chart::CameraComponentData{};
    components.behavior = cuexis::chart::BehaviorReferenceData{{"move"}};
    return cuexis::chart::ChartDocument{
        .chartId = {"chart.behavior"},
        .timing = {.offsetMs = 250.0, .defaultBpm = 120.0},
        .behaviors = {{.id = {"move"},
                       .type = "behavior.transform.keyframe",
                       .version = 1,
                       .tracks = cuexis::chart::BehaviorTracks{std::move(tracks)}}},
        .objects = {{.id = {"object"}, .components = std::move(components)}},
    };
}

TEST_CASE("ChartCompiler sorts typed keys and compiles absolute chart time", "[chart][behavior]") {
    cuexis::chart::BehaviorTrack track{
        .property = cuexis::chart::BehaviorProperty::TransformPositionX,
        .keys = {{.beat = beat(2), .value = 2.0}, {.beat = beat(0), .value = 0.0}}};
    auto result = cuexis::chart::ChartCompiler::compile(documentWithBehavior({std::move(track)}));
    REQUIRE(result.hasValue());
    REQUIRE(result.runtime->behaviors[0].tracks.size() == 1);
    const auto& keys = result.runtime->behaviors[0].tracks[0].keys;
    REQUIRE(keys.size() == 2);
    CHECK(keys[0].chartTimeMs == Catch::Approx(0.0));
    CHECK(keys[1].chartTimeMs == Catch::Approx(1000.0));
}

TEST_CASE("ChartCompiler rejects duplicate beats, duplicate properties and invalid FOV",
          "[chart][behavior][errors]") {
    const auto duplicateBeat = documentWithBehavior({cuexis::chart::BehaviorTrack{
        .property = cuexis::chart::BehaviorProperty::TransformPositionX,
        .keys = {{.beat = beat(0), .value = 0.0}, {.beat = beat(0), .value = 1.0}}}});
    auto result = cuexis::chart::ChartCompiler::compile(duplicateBeat);
    REQUIRE_FALSE(result.hasValue());
    CHECK(std::any_of(result.diagnostics.items().begin(), result.diagnostics.items().end(),
                      [](const auto& diagnostic) {
                          return diagnostic.code() == "chart.behavior.beat_duplicate";
                      }));

    auto invalidFov = documentWithBehavior(
        {cuexis::chart::BehaviorTrack{.property = cuexis::chart::BehaviorProperty::CameraFovY,
                                      .keys = {{.beat = beat(0), .value = 179.0}}}});
    result = cuexis::chart::ChartCompiler::compile(invalidFov);
    REQUIRE_FALSE(result.hasValue());
    CHECK(std::any_of(result.diagnostics.items().begin(), result.diagnostics.items().end(),
                      [](const auto& diagnostic) {
                          return diagnostic.code() == "chart.behavior.value_invalid";
                      }));
}

} // namespace
