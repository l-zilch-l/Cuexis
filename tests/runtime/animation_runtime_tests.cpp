#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/property.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator = 1)
    -> cuexis::chart::RationalBeat {
    auto value = cuexis::chart::RationalBeat::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto runtimeChart() {
    cuexis::chart::ObjectComponents components;
    components.transform = cuexis::chart::TransformData{};
    components.camera = cuexis::chart::CameraComponentData{.fovY = 60.0};
    components.behavior = cuexis::chart::BehaviorReferenceData{{"move"}};
    cuexis::chart::BehaviorTrack position{
        .property = cuexis::chart::BehaviorProperty::TransformPositionX,
        .keys = {{.beat = beat(0), .value = 0.0}, {.beat = beat(1), .value = 10.0}}};
    cuexis::chart::ChartDocument document{
        .chartId = {"runtime.animation"},
        .timing = {.offsetMs = 0.0, .defaultBpm = 120.0},
        .camera = cuexis::chart::CameraData{},
        .behaviors = {{.id = {"move"},
                       .type = "behavior.transform.keyframe",
                       .version = 1,
                       .tracks = cuexis::chart::BehaviorTracks{{std::move(position)}}}},
        .objects = {{.id = {"object"}, .components = std::move(components)}},
    };
    auto compiled = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE(compiled.hasValue());
    return std::move(*compiled.runtime);
}

[[nodiscard]] auto constantPositionClip(double value) -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .tracks = {{
            .property = cuexis::chart::AnimationProperty::TransformPositionX,
            .segments = {{
                .startBeat = beat(0),
                .durationBeats = beat(4),
                .startValue = value,
                .endValue = value,
                .fieldPath = "$/tracks/0/segments/0",
            }},
            .fieldPath = "$/tracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] auto compilePositionProgram(double value, double layerWeight = 1.0)
    -> cuexis::animation::AnimationProgram {
    cuexis::chart::AnimationProgramInput input;
    cuexis::chart::AnimationClip clip = constantPositionClip(value);
    clip.id = "clip-id";
    input.clips.push_back({.identity = "clip", .clip = std::move(clip)});
    cuexis::chart::ResolvedClipInstance instance{
        .identity = "instance",
        .clipIdentity = "clip",
        .startBeat = beat(0),
        .durationScale = beat(1),
        .iterations = {.infinite = false, .count = 1},
        .fillMode = cuexis::chart::AnimationFillMode::Hold,
        .weight = 1.0,
        .propertyMask = {.properties = {"transform.position.x"}},
    };
    cuexis::chart::ResolvedBlendGroup group{
        .identity = "group",
        .mode = cuexis::chart::AnimationBlendMode::Override,
        .weight = 1.0,
        .instances = {std::move(instance)},
    };
    cuexis::chart::ResolvedAnimationLayer layer{
        .identity = "layer",
        .priority = 0,
        .weight = layerWeight,
        .propertyMask = {.properties = {"transform.position.x"}},
        .blendGroups = {std::move(group)},
    };
    cuexis::chart::ObjectAnimationProgram object{
        .objectId = {"object"},
        .layers = {std::move(layer)},
    };
    input.objects.push_back(std::move(object));
    auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    REQUIRE(compiled.hasValue());
    return std::move(*compiled.program);
}

[[nodiscard]] auto eventBaselineChart() {
    auto chart = runtimeChart();
    chart.behaviors[0].eventTracks.push_back(cuexis::chart::RuntimeEventTrack{
        .property = cuexis::chart::BehaviorProperty::TransformPositionY,
        .events = {{
            .startBeat = 1.0,
            .endBeat = 2.0,
            .startValue = 0.0,
            .endValue = 8.0,
        }},
    });
    return chart;
}

[[nodiscard]] auto objectPosition(cuexis::runtime::RuntimeSession& session) -> float {
    const auto object = session.findEntity({"object"});
    REQUIRE(object.has_value());
    REQUIRE(object->has_value());
    const auto position = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return registry.get<cuexis::world::TransformComponent>(**object).position.x;
        });
    });
    REQUIRE(position.has_value());
    return *position;
}

} // namespace

TEST_CASE("RuntimeSession evaluates Behavior then Animation on the same frame",
          "[runtime][animation][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart(), compilePositionProgram(10.0, 0.5));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(
        session
            .update({.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());
    CHECK(objectPosition(session) == Catch::Approx(7.5F));
}

TEST_CASE("HostOverride restores the lower-layer result after release",
          "[runtime][override][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart(), compilePositionProgram(10.0, 0.5));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(7.5F));

    const auto token = session.acquireOverride(
        cuexis::world::OverrideKind::Host, "host", 1,
        cuexis::world::propertyBit(cuexis::world::PropertyId::TransformPositionX), {},
        std::array{cuexis::runtime::PropertyOverrideWrite{
            .objectId = {"object"},
            .property = cuexis::world::PropertyId::TransformPositionX,
            .value = 9.0,
        }});
    REQUIRE(token.has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(9.0F));
    REQUIRE(session.releaseOverride(*token).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(7.5F));
}

TEST_CASE("RemainingFrames override expires on the next frame", "[runtime][override][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());

    REQUIRE(session
                .acquireOverride(
                    cuexis::world::OverrideKind::Host, "host", 1,
                    cuexis::world::propertyBit(cuexis::world::PropertyId::TransformPositionX),
                    {.kind = cuexis::world::OverrideLifetimeKind::RemainingFrames,
                     .remainingFrames = 1},
                    std::array{cuexis::runtime::PropertyOverrideWrite{
                        .objectId = {"object"},
                        .property = cuexis::world::PropertyId::TransformPositionX,
                        .value = 4.0,
                    }})
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(4.0F));
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(0.0F));
}

TEST_CASE("UntilChartTimeMs override expires at the deadline", "[runtime][override][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());

    REQUIRE(session
                .acquireOverride(
                    cuexis::world::OverrideKind::Host, "host", 1,
                    cuexis::world::propertyBit(cuexis::world::PropertyId::TransformPositionX),
                    {.kind = cuexis::world::OverrideLifetimeKind::UntilChartTimeMs,
                     .remainingFrames = 0,
                     .untilChartTimeMs = 250.0},
                    std::array{cuexis::runtime::PropertyOverrideWrite{
                        .objectId = {"object"},
                        .property = cuexis::world::PropertyId::TransformPositionX,
                        .value = 4.0,
                    }})
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(4.0F));
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(5.0F));
}

TEST_CASE("BasePropertyCommand changes the Initial baseline and reevaluates the last frame",
          "[runtime][base][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart(), compilePositionProgram(10.0, 0.5));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(
        session
            .update({.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());
    CHECK(objectPosition(session) == Catch::Approx(7.5F));
    CHECK(session.baseRevision() == 0);
    REQUIRE(session
                .applyBaseProperty(cuexis::runtime::BasePropertyCommand{
                    .objectId = {"object"},
                    .property = cuexis::world::PropertyId::TransformPositionY,
                    .value = 2.0,
                })
                .has_value());
    CHECK(session.baseRevision() == 1);
    const auto object = session.findEntity({"object"});
    REQUIRE(object.has_value());
    REQUIRE(object->has_value());
    const auto position = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return registry.get<cuexis::world::TransformComponent>(**object).position;
        });
    });
    REQUIRE(position.has_value());
    CHECK(position->x == Catch::Approx(7.5F));
    CHECK(position->y == Catch::Approx(2.0F));
}

TEST_CASE("BasePropertyCommand updates Event baselines before the first event",
          "[runtime][base][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(eventBaselineChart());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    REQUIRE(session
                .applyBaseProperty(cuexis::runtime::BasePropertyCommand{
                    .objectId = {"object"},
                    .property = cuexis::world::PropertyId::TransformPositionY,
                    .value = 3.0,
                })
                .has_value());
    const auto object = session.findEntity({"object"});
    REQUIRE(object.has_value());
    REQUIRE(object->has_value());
    const auto position = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return registry.get<cuexis::world::TransformComponent>(**object).position;
        });
    });
    REQUIRE(position.has_value());
    CHECK(position->y == Catch::Approx(3.0F));
}

TEST_CASE("Playback RuntimeSession rejects StudioPreviewOverride", "[runtime][override][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart(), compilePositionProgram(10.0, 0.5));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(session.kind() == cuexis::runtime::RuntimeSessionKind::Playback);

    const auto token = session.acquireOverride(
        cuexis::world::OverrideKind::StudioPreview, "studio", 1,
        cuexis::world::propertyBit(cuexis::world::PropertyId::TransformPositionX), {},
        std::array{cuexis::runtime::PropertyOverrideWrite{
            .objectId = {"object"},
            .property = cuexis::world::PropertyId::TransformPositionX,
            .value = 1.0,
        }});
    REQUIRE_FALSE(token.has_value());
    CHECK(token.error().code() == "runtime.override.preview_session_required");
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(7.5F));
}

TEST_CASE("StudioPreviewOverride restores the lower-layer result after release",
          "[runtime][override][s4-d]") {
    cuexis::runtime::RuntimeSession session{cuexis::runtime::RuntimeSessionKind::StudioPreview};
    auto prepared = session.prepare(runtimeChart(), compilePositionProgram(10.0, 0.5));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(7.5F));

    const auto token = session.acquireOverride(
        cuexis::world::OverrideKind::StudioPreview, "studio", 1,
        cuexis::world::propertyBit(cuexis::world::PropertyId::TransformPositionX), {},
        std::array{cuexis::runtime::PropertyOverrideWrite{
            .objectId = {"object"},
            .property = cuexis::world::PropertyId::TransformPositionX,
            .value = 1.0,
        }});
    REQUIRE(token.has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(1.0F));
    REQUIRE(session.releaseOverride(*token).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(7.5F));
}

TEST_CASE("Same-priority runtime overrides discard the conflict write",
          "[runtime][override][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart(), compilePositionProgram(10.0, 0.5));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    const auto mask = cuexis::world::propertyBit(cuexis::world::PropertyId::TransformPositionX);
    REQUIRE(session
                .acquireOverride(cuexis::world::OverrideKind::Host, "left", 4, mask, {},
                                 std::array{cuexis::runtime::PropertyOverrideWrite{
                                     .objectId = {"object"},
                                     .property = cuexis::world::PropertyId::TransformPositionX,
                                     .value = 8.0,
                                 }})
                .has_value());
    REQUIRE(session
                .acquireOverride(cuexis::world::OverrideKind::Host, "right", 4, mask, {},
                                 std::array{cuexis::runtime::PropertyOverrideWrite{
                                     .objectId = {"object"},
                                     .property = cuexis::world::PropertyId::TransformPositionX,
                                     .value = 9.0,
                                 }})
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    CHECK(objectPosition(session) == Catch::Approx(7.5F));
}

TEST_CASE("Runtime debug snapshots record animation and override sources",
          "[runtime][debug][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart(), compilePositionProgram(10.0, 1.0));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.configureDebug({.enabled = true, .capacity = 8}).has_value());
    REQUIRE(session
                .acquireOverride(
                    cuexis::world::OverrideKind::Host, "host", 1,
                    cuexis::world::propertyBit(cuexis::world::PropertyId::TransformPositionX), {},
                    std::array{cuexis::runtime::PropertyOverrideWrite{
                        .objectId = {"object"},
                        .property = cuexis::world::PropertyId::TransformPositionX,
                        .value = 9.0,
                    }})
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    const auto snapshot = session.debugSnapshot();
    REQUIRE(snapshot.has_value());
    REQUIRE_FALSE(snapshot->records.empty());
    const auto& record = snapshot->records.front();
    CHECK(record.property == cuexis::world::PropertyId::TransformPositionX);
    CHECK(std::get<double>(record.behaviorValue) == Catch::Approx(5.0));
    CHECK(std::get<double>(record.animationValue) == Catch::Approx(10.0));
    CHECK(std::get<double>(record.hostOverrideValue) == Catch::Approx(9.0));
    CHECK(std::get<double>(record.finalValue) == Catch::Approx(9.0));
    CHECK(record.sourceLayer == cuexis::world::PropertyLayer::HostOverride);
    REQUIRE(record.animationLayers.size() == 1);
    CHECK(record.animationLayers.front().identity == "layer");
    CHECK(record.animationLayers.front().weight == Catch::Approx(1.0));
    REQUIRE_FALSE(record.animationLayers.front().mask.empty());
    CHECK(record.animationLayers.front().mask.front() == "transform.position.x");
    CHECK(std::get<double>(record.animationLayers.front().value) == Catch::Approx(10.0));
}

TEST_CASE("Runtime reload publishes the candidate debug snapshot atomically",
          "[runtime][debug][reload]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart(), compilePositionProgram(10.0));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.configureDebug({.enabled = true, .capacity = 8}).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());

    const auto before = session.debugSnapshot();
    REQUIRE(before.has_value());
    REQUIRE_FALSE(before->records.empty());
    CHECK(before->records.front().objectId.value == "object");

    const auto reload = session.reload(runtimeChart(), {.chartTimeMs = 500.0},
                                       cuexis::runtime::ReloadPolicy::KeepChartTime);
    REQUIRE(reload.reloaded);
    CHECK_FALSE(reload.diagnostics.hasErrors());
    CHECK(objectPosition(session) == Catch::Approx(10.0F));

    const auto after = session.debugSnapshot();
    REQUIRE(after.has_value());
    REQUIRE_FALSE(after->records.empty());
    CHECK_FALSE(after->truncated);
    CHECK(after->records.front().objectId.value == "object");
    CHECK(std::get<double>(after->records.front().finalValue) == Catch::Approx(10.0));
}

TEST_CASE("BasePropertyCommand refreshes a bounded debug snapshot",
          "[runtime][debug][base][s4-d]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(runtimeChart());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    REQUIRE(session.configureDebug({.enabled = true, .capacity = 8}).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    REQUIRE(session
                .applyBaseProperty(cuexis::runtime::BasePropertyCommand{
                    .objectId = {"object"},
                    .property = cuexis::world::PropertyId::TransformPositionY,
                    .value = 2.0,
                })
                .has_value());
    const auto snapshot = session.debugSnapshot();
    REQUIRE(snapshot.has_value());
    REQUIRE_FALSE(snapshot->truncated);
    REQUIRE_FALSE(snapshot->records.empty());
}
