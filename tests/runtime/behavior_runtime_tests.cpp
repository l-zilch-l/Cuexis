#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/world/components.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

namespace {

auto runtimeChart() {
    cuexis::chart::ObjectComponents components;
    components.transform = cuexis::chart::TransformData{};
    components.camera = cuexis::chart::CameraComponentData{.fovY = 60.0};
    components.behavior = cuexis::chart::BehaviorReferenceData{{"move"}};
    cuexis::chart::BehaviorTrack position{
        .property = cuexis::chart::BehaviorProperty::TransformPositionX,
        .keys = {{.beat = *cuexis::chart::RationalBeat::create(0, 1), .value = 0.0},
                 {.beat = *cuexis::chart::RationalBeat::create(1, 1), .value = 10.0}}};
    cuexis::chart::BehaviorTrack fov{
        .property = cuexis::chart::BehaviorProperty::CameraFovY,
        .keys = {{.beat = *cuexis::chart::RationalBeat::create(0, 1), .value = 60.0},
                 {.beat = *cuexis::chart::RationalBeat::create(1, 1), .value = 90.0}}};
    cuexis::chart::ChartDocument document{
        .chartId = {"runtime.behavior"},
        .timing = {.offsetMs = 0.0, .defaultBpm = 120.0},
        .camera = cuexis::chart::CameraData{},
        .behaviors = {{.id = {"move"},
                       .type = "behavior.transform.keyframe",
                       .version = 1,
                       .tracks =
                           cuexis::chart::BehaviorTracks{{std::move(position), std::move(fov)}}}},
        .objects = {{.id = {"object"}, .components = std::move(components)}},
    };
    return *cuexis::chart::ChartCompiler::compile(document).runtime;
}

auto beat(std::int64_t numerator, std::int64_t denominator = 1) {
    return *cuexis::chart::RationalBeat::create(numerator, denominator);
}

auto stage2RuntimeChart() {
    cuexis::chart::ObjectComponents components;
    components.transform = cuexis::chart::TransformData{};
    components.camera = cuexis::chart::CameraComponentData{.fovY = 60.0};
    components.behavior = cuexis::chart::BehaviorReferenceData{{"events"}};

    cuexis::chart::ChartBehavior behavior{
        .id = {"events"},
        .type = "behavior.event",
        .version = 1,
        .events =
            {
                {.property = cuexis::chart::BehaviorProperty::TransformPositionX,
                 .startBeat = beat(-1),
                 .durationBeats = beat(0),
                 .startValue = 2.0,
                 .endValue = 2.0},
                {.property = cuexis::chart::BehaviorProperty::TransformPositionX,
                 .startBeat = beat(0),
                 .durationBeats = beat(2),
                 .startValue = 4.0,
                 .endValue = 8.0,
                 .startSlope = 1.0,
                 .endSlope = 1.0},
                {.property = cuexis::chart::BehaviorProperty::TransformPositionX,
                 .startBeat = beat(2),
                 .durationBeats = beat(2),
                 .startValue = 10.0,
                 .endValue = 14.0,
                 .startSlope = 1.0,
                 .endSlope = 1.0},
                {.property = cuexis::chart::BehaviorProperty::TransformPositionY,
                 .startBeat = beat(1),
                 .durationBeats = beat(0),
                 .startValue = 3.0,
                 .endValue = 3.0},
                {.property = cuexis::chart::BehaviorProperty::TransformRotation,
                 .startBeat = beat(0),
                 .durationBeats = beat(2),
                 .startValue = cuexis::core::Quat{0.0F, 0.0F, 0.0F, 1.0F},
                 .endValue = cuexis::core::Quat{0.0F, 0.0F, -1.0F, 0.0F},
                 .startSlope = 1.0,
                 .endSlope = 1.0},
                {.property = cuexis::chart::BehaviorProperty::CameraFovY,
                 .startBeat = beat(0),
                 .durationBeats = beat(2),
                 .startValue = 60.0,
                 .endValue = 100.0,
                 .startSlope = 0.0,
                 .endSlope = 0.0},
            },
    };
    cuexis::chart::ChartDocument document{
        .chartId = {"runtime.stage2"},
        .timing = {.offsetMs = 0.0,
                   .defaultBpm = 120.0,
                   .tempoEvents = {},
                   .stops = {{.beat = beat(1), .durationMs = 250.0}}},
        .camera = cuexis::chart::CameraData{},
        .behaviors = {std::move(behavior)},
        .objects = {{.id = {"object"}, .components = std::move(components)}},
        .version = 3,
    };
    auto compiled = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE(compiled.hasValue());
    return std::move(*compiled.runtime);
}

struct Stage2State final {
    cuexis::world::TransformComponent transform;
    cuexis::render::CameraComponent camera;
};

auto stage2State(cuexis::runtime::RuntimeSession& session) {
    const auto object = session.findEntity({"object"});
    REQUIRE(object.has_value());
    REQUIRE(object->has_value());
    const auto state = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return Stage2State{
                .transform = registry.get<cuexis::world::TransformComponent>(**object),
                .camera = registry.get<cuexis::render::CameraComponent>(**object),
            };
        });
    });
    REQUIRE(state.has_value());
    return *state;
}

TEST_CASE("RuntimeSession absolute sampling drives Transform and camera FOV",
          "[runtime][behavior][camera]") {
    auto chart = runtimeChart();
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(std::move(chart));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(
        session
            .update({.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());

    const auto object = session.findEntity({"object"});
    REQUIRE(object.has_value());
    REQUIRE(object->has_value());
    const auto state = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return std::pair{registry.get<cuexis::world::TransformComponent>(**object),
                             registry.get<cuexis::render::CameraComponent>(**object)};
        });
    });
    REQUIRE(state.has_value());
    CHECK(state->first.position.x == Catch::Approx(5.0F));
    CHECK(state->second.fovY == Catch::Approx(75.0));

    REQUIRE(
        session.update({.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1})
            .has_value());
    const auto reset = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return std::pair{registry.get<cuexis::world::TransformComponent>(**object).position.x,
                             registry.get<cuexis::render::CameraComponent>(**object).fovY};
        });
    });
    REQUIRE(reset.has_value());
    CHECK(reset->first == Catch::Approx(0.0F));
    CHECK(reset->second == Catch::Approx(60.0));
}

TEST_CASE("RuntimeFrame requires zero delta on discontinuity and rejects undeclared seek",
          "[runtime][frame]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(
        session
            .update({.chartTimeMs = 100.0, .simulationDeltaTimeMs = 1.0, .timeDiscontinuityId = 0})
            .has_value());
    auto backward = session.update(
        {.chartTimeMs = 50.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0});
    REQUIRE_FALSE(backward.has_value());
    CHECK(backward.error().code() == "runtime.frame.backward_seek_undeclared");
    auto discontinuity = session.update(
        {.chartTimeMs = 50.0, .simulationDeltaTimeMs = 1.0, .timeDiscontinuityId = 1});
    REQUIRE_FALSE(discontinuity.has_value());
    CHECK(discontinuity.error().code() == "runtime.frame.discontinuity_delta_nonzero");
}

TEST_CASE("Stage 2 events honor negative, boundary, adjacency, zero-duration, and quaternion rules",
          "[runtime][behavior][stage2][boundary]") {
    auto chart = stage2RuntimeChart();
    const auto beforeZero = chart.timingMap.beatToChartTimeMs(-0.5);
    const auto atZero = chart.timingMap.beatToChartTimeMs(0.0);
    const auto atOne = chart.timingMap.beatToChartTimeMs(1.0);
    const auto atTwo = chart.timingMap.beatToChartTimeMs(2.0);
    REQUIRE(beforeZero.has_value());
    REQUIRE(atZero.has_value());
    REQUIRE(atOne.has_value());
    REQUIRE(atTwo.has_value());

    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(std::move(chart));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());

    REQUIRE(session.update({.chartTimeMs = *beforeZero}).has_value());
    CHECK(stage2State(session).transform.position.x == Catch::Approx(2.0F));

    REQUIRE(session.update({.chartTimeMs = *atZero, .timeDiscontinuityId = 1}).has_value());
    CHECK(stage2State(session).transform.position.x == Catch::Approx(4.0F));

    REQUIRE(session.update({.chartTimeMs = *atOne, .timeDiscontinuityId = 2}).has_value());
    const auto middle = stage2State(session);
    CHECK(middle.transform.position.x == Catch::Approx(6.0F));
    CHECK(middle.transform.position.y == Catch::Approx(3.0F));
    CHECK(middle.transform.rotation.z == Catch::Approx(-0.70710677F));
    CHECK(middle.transform.rotation.w == Catch::Approx(0.70710677F));
    CHECK(middle.camera.fovY == Catch::Approx(80.0));

    REQUIRE(session.update({.chartTimeMs = *atTwo, .timeDiscontinuityId = 3}).has_value());
    CHECK(stage2State(session).transform.position.x == Catch::Approx(10.0F));
}

TEST_CASE("Stage 2 events freeze in Stops and direct seek matches playback and reload",
          "[runtime][behavior][stage2][stop][seek][reload]") {
    auto chart = stage2RuntimeChart();
    const auto stopStart = chart.timingMap.beatToChartTimeMs(1.0);
    const auto targetTime = chart.timingMap.beatToChartTimeMs(2.5);
    REQUIRE(stopStart.has_value());
    REQUIRE(targetTime.has_value());

    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(std::move(chart));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.update({.chartTimeMs = *stopStart + 125.0}).has_value());
    const auto stopped = stage2State(session);
    CHECK(stopped.transform.position.x == Catch::Approx(6.0F));
    CHECK(stopped.transform.position.y == Catch::Approx(3.0F));

    REQUIRE(session.update({.chartTimeMs = *targetTime}).has_value());
    const auto played = stage2State(session);
    CHECK(played.transform.position.x == Catch::Approx(11.0F));

    cuexis::runtime::RuntimeSession direct;
    auto directPrepared = direct.prepare(stage2RuntimeChart());
    REQUIRE(directPrepared.hasValue());
    REQUIRE(direct.commit(std::move(*directPrepared.prepared)).has_value());
    REQUIRE(direct.update({.chartTimeMs = *targetTime, .timeDiscontinuityId = 1}).has_value());
    CHECK(stage2State(direct).transform.position.x == Catch::Approx(played.transform.position.x));

    const auto reloaded =
        direct.reload(stage2RuntimeChart(), {.chartTimeMs = *targetTime, .timeDiscontinuityId = 2},
                      cuexis::runtime::ReloadPolicy::KeepChartTime);
    REQUIRE(reloaded.reloaded);
    CHECK(stage2State(direct).transform.position.x == Catch::Approx(played.transform.position.x));
}

TEST_CASE("Runtime debug snapshots are explicit, bounded, and explain Event resolution",
          "[runtime][behavior][stage2][debug]") {
    auto chart = stage2RuntimeChart();
    auto& positionTrack = *std::find_if(
        chart.behaviors[0].eventTracks.begin(), chart.behaviors[0].eventTracks.end(),
        [](const cuexis::chart::RuntimeEventTrack& track) {
            return track.property == cuexis::chart::BehaviorProperty::TransformPositionX;
        });
    positionTrack.events[1].startValue = 0.1;
    positionTrack.events[1].endValue = 0.1;
    const auto atOne = chart.timingMap.beatToChartTimeMs(1.0);
    REQUIRE(atOne.has_value());
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(std::move(chart));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());

    REQUIRE(session.update({.chartTimeMs = *atOne}).has_value());
    const auto disabled = session.debugSnapshot();
    REQUIRE(disabled.has_value());
    CHECK(disabled->records.empty());
    CHECK_FALSE(disabled->truncated);

    REQUIRE(session.configureDebug({.enabled = true, .capacity = 2}).has_value());
    REQUIRE(session.update({.chartTimeMs = *atOne}).has_value());
    const auto bounded = session.debugSnapshot();
    REQUIRE(bounded.has_value());
    REQUIRE(bounded->records.size() == 2);
    CHECK(bounded->truncated);
    const auto& position = bounded->records[0];
    CHECK(position.objectId.value == "object");
    CHECK(position.property == cuexis::world::PropertyId::TransformPositionX);
    REQUIRE(position.eventIndex.has_value());
    CHECK(*position.eventIndex == 1);
    CHECK(position.normalizedProgress == Catch::Approx(0.5));
    CHECK(std::get<double>(position.initialValue) == Catch::Approx(0.0));
    const auto behaviorValue = std::get<double>(position.behaviorValue);
    const auto finalValue = std::get<double>(position.finalValue);
    CHECK(behaviorValue == Catch::Approx(0.1));
    CHECK(finalValue == static_cast<double>(static_cast<float>(behaviorValue)));
    CHECK(finalValue != behaviorValue);

    REQUIRE(session.configureDebug({.enabled = true, .capacity = 8}).has_value());
    REQUIRE(session.update({.chartTimeMs = *atOne}).has_value());
    const auto complete = session.debugSnapshot();
    REQUIRE(complete.has_value());
    CHECK(complete->records.size() == 4);
    CHECK_FALSE(complete->truncated);

    REQUIRE(session.configureDebug({.enabled = false}).has_value());
    REQUIRE(session.update({.chartTimeMs = *atOne}).has_value());
    CHECK(session.debugSnapshot()->records.empty());
    const auto invalid = session.configureDebug(
        {.enabled = true, .capacity = cuexis::runtime::maxRuntimeDebugRecords + 1});
    REQUIRE_FALSE(invalid.has_value());
    CHECK(invalid.error().code() == "runtime.debug.capacity_invalid");
}

} // namespace
