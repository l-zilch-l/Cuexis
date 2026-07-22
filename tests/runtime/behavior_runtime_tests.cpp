#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/world/components.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
        .behaviors = {{.id = {"move"},
                       .type = "behavior.transform.keyframe",
                       .version = 1,
                       .tracks =
                           cuexis::chart::BehaviorTracks{{std::move(position), std::move(fov)}}}},
        .objects = {{.id = {"object"}, .components = std::move(components)}},
    };
    return *cuexis::chart::ChartCompiler::compile(document).runtime;
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

} // namespace
