#include <cuexis/behavior/behavior_system.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace {

using cuexis::behavior::BehaviorBinding;
using cuexis::behavior::BehaviorDefinition;
using cuexis::behavior::BehaviorKey;
using cuexis::behavior::BehaviorProgram;
using cuexis::behavior::BehaviorSystem;
using cuexis::behavior::BehaviorTrack;
using cuexis::behavior::Easing;
using cuexis::behavior::RuntimeBehaviorIndex;
using cuexis::core::Quat;
using cuexis::core::Vec3;
using cuexis::world::PropertyId;
using cuexis::world::PropertyValue;

TEST_CASE("BehaviorSystem samples scalar tracks with target-key easing", "[behavior][sampling]") {
    BehaviorProgram program;
    BehaviorTrack track{.property = PropertyId::TransformPositionX, .keys = {}};
    track.keys.reserve(2);
    BehaviorKey firstKey{.chartTimeMs = 0.0, .value = {}, .easing = Easing::Linear};
    std::get<double>(firstKey.value) = 0.0;
    track.keys.push_back(std::move(firstKey));
    BehaviorKey secondKey{.chartTimeMs = 100.0, .value = {}, .easing = Easing::InCubic};
    std::get<double>(secondKey.value) = 10.0;
    track.keys.push_back(std::move(secondKey));
    BehaviorDefinition definition;
    definition.tracks.push_back(std::move(track));
    program.definitions.push_back(std::move(definition));
    program.bindings.push_back(
        BehaviorBinding{.entity = entt::entity{1}, .behavior = RuntimeBehaviorIndex{0}});

    cuexis::world::PropertyWriteBuffer writes;
    const auto firstSample = BehaviorSystem::evaluate(program, 50.0, writes);
    if (!firstSample) {
        UNSCOPED_INFO(std::string{firstSample.error().code()});
    }
    REQUIRE(firstSample.has_value());
    REQUIRE(writes.size() == 1);
    const auto* value = std::get_if<double>(&writes.writes()[0].value);
    REQUIRE(value != nullptr);
    CHECK(*value == Catch::Approx(1.25));

    REQUIRE(BehaviorSystem::evaluate(program, -10.0, writes).has_value());
    CHECK(*std::get_if<double>(&writes.writes()[0].value) == Catch::Approx(0.0));
    REQUIRE(BehaviorSystem::evaluate(program, 110.0, writes).has_value());
    CHECK(*std::get_if<double>(&writes.writes()[0].value) == Catch::Approx(10.0));
}

TEST_CASE("BehaviorSystem interpolates Vec3 and quaternion shortest path", "[behavior][sampling]") {
    BehaviorProgram program;
    BehaviorTrack scaleTrack{.property = PropertyId::TransformScale, .keys = {}};
    scaleTrack.keys.reserve(2);
    BehaviorKey firstScaleKey{.chartTimeMs = 0.0, .value = PropertyValue{Vec3{1.0F, 2.0F, 3.0F}}};
    scaleTrack.keys.push_back(std::move(firstScaleKey));
    BehaviorKey secondScaleKey{.chartTimeMs = 100.0,
                               .value = PropertyValue{Vec3{3.0F, 4.0F, 5.0F}},
                               .easing = Easing::OutCubic};
    scaleTrack.keys.push_back(std::move(secondScaleKey));

    BehaviorTrack rotationTrack{.property = PropertyId::TransformRotation, .keys = {}};
    rotationTrack.keys.reserve(2);
    BehaviorKey firstRotationKey{.chartTimeMs = 0.0,
                                 .value = PropertyValue{Quat{0.0F, 0.0F, 0.0F, 1.0F}}};
    rotationTrack.keys.push_back(std::move(firstRotationKey));
    BehaviorKey secondRotationKey{.chartTimeMs = 100.0,
                                  .value = PropertyValue{Quat{0.0F, 0.0F, 0.0F, -1.0F}}};
    rotationTrack.keys.push_back(std::move(secondRotationKey));

    BehaviorDefinition definition;
    definition.tracks.reserve(2);
    definition.tracks.push_back(std::move(scaleTrack));
    definition.tracks.push_back(std::move(rotationTrack));
    program.definitions.push_back(std::move(definition));
    program.bindings.push_back(
        BehaviorBinding{.entity = entt::entity{2}, .behavior = RuntimeBehaviorIndex{0}});

    cuexis::world::PropertyWriteBuffer writes;
    const auto midpoint = BehaviorSystem::evaluate(program, 50.0, writes);
    if (!midpoint) {
        UNSCOPED_INFO(std::string{midpoint.error().code()});
    }
    REQUIRE(midpoint.has_value());
    REQUIRE(writes.size() == 2);
    const auto* scale = std::get_if<Vec3>(&writes.writes()[0].value);
    REQUIRE(scale != nullptr);
    CHECK(scale->x == Catch::Approx(2.75F));
    CHECK(scale->y == Catch::Approx(3.75F));
    CHECK(scale->z == Catch::Approx(4.75F));
    const auto* rotation = std::get_if<Quat>(&writes.writes()[1].value);
    REQUIRE(rotation != nullptr);
    CHECK(rotation->w == Catch::Approx(1.0F));
    CHECK(std::abs(rotation->x) < 1.0e-5F);
}

TEST_CASE("BehaviorSystem rejects invalid program and non-finite time", "[behavior][errors]") {
    BehaviorProgram invalid;
    invalid.bindings.push_back(
        BehaviorBinding{.entity = entt::entity{1}, .behavior = RuntimeBehaviorIndex{0}});
    cuexis::world::PropertyWriteBuffer writes;
    auto result = BehaviorSystem::evaluate(invalid, 0.0, writes);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "behavior.program.binding_invalid");

    BehaviorProgram empty;
    REQUIRE_FALSE(BehaviorSystem::evaluate(empty, std::numeric_limits<double>::infinity(), writes)
                      .has_value());
}

TEST_CASE("BehaviorSystem event and step sampling distinguishes the Beat 0 legacy overload",
          "[behavior][sampling][rr-f133]") {
    using cuexis::behavior::BehaviorEvent;
    using cuexis::behavior::BehaviorEventTrack;
    using cuexis::behavior::BehaviorSample;
    using cuexis::behavior::BehaviorStepEvent;
    using cuexis::behavior::BehaviorStepTrack;
    using cuexis::behavior::PropertyBaseline;

    BehaviorDefinition definition;
    BehaviorEventTrack events;
    events.property = PropertyId::TransformPositionX;
    events.events.push_back(BehaviorEvent{.startBeat = 2.0,
                                          .endBeat = 4.0,
                                          .startValue = PropertyValue{10.0},
                                          .endValue = PropertyValue{20.0},
                                          .startSlope = 0.0,
                                          .endSlope = 0.0,
                                          .instantaneous = false});
    events.events.push_back(BehaviorEvent{.startBeat = 4.0,
                                          .endBeat = 4.0,
                                          .startValue = PropertyValue{30.0},
                                          .endValue = PropertyValue{30.0},
                                          .startSlope = 0.0,
                                          .endSlope = 0.0,
                                          .instantaneous = true});
    definition.eventTracks.push_back(std::move(events));

    BehaviorStepTrack steps;
    steps.property = PropertyId::TransformPositionY;
    steps.events.push_back(BehaviorStepEvent{.beat = -1.0, .value = PropertyValue{5.0}});
    steps.events.push_back(BehaviorStepEvent{.beat = 1.0, .value = PropertyValue{7.0}});
    steps.events.push_back(BehaviorStepEvent{.beat = 3.0, .value = PropertyValue{9.0}});
    definition.stepTracks.push_back(std::move(steps));

    BehaviorProgram program;
    program.definitions.push_back(std::move(definition));
    BehaviorBinding binding{.entity = entt::entity{3}, .behavior = RuntimeBehaviorIndex{0}};
    binding.baselines.push_back(
        PropertyBaseline{.property = PropertyId::TransformPositionX, .value = PropertyValue{0.0}});
    binding.baselines.push_back(
        PropertyBaseline{.property = PropertyId::TransformPositionY, .value = PropertyValue{1.0}});
    program.bindings.push_back(std::move(binding));

    cuexis::world::PropertyWriteBuffer writes;
    REQUIRE(BehaviorSystem::evaluate(program, 1000.0, writes).has_value());
    REQUIRE(writes.size() == 2);
    //  The legacy overload has no Beat input: 1000 ms is Beat 2 at 120 BPM, but the Event track is
    //  evaluated at Beat 0 (its baseline) and the Step track takes the last step at or before 0.
    CHECK(*std::get_if<double>(&writes.writes()[0].value) == Catch::Approx(0.0));
    CHECK(*std::get_if<double>(&writes.writes()[1].value) == Catch::Approx(5.0));

    //  The full sample evaluates Event and Step tracks at the actual Beat: the first Event starts
    //  above Beat 0, Beat 3 is strictly inside it, and Beat 4 is the instantaneous boundary.
    REQUIRE(BehaviorSystem::evaluate(program, BehaviorSample{1000.0, 2.0, false, 0.0}, writes)
                .has_value());
    CHECK(*std::get_if<double>(&writes.writes()[0].value) == Catch::Approx(10.0));
    CHECK(*std::get_if<double>(&writes.writes()[1].value) == Catch::Approx(7.0));

    REQUIRE(BehaviorSystem::evaluate(program, BehaviorSample{1500.0, 3.0, false, 0.0}, writes)
                .has_value());
    CHECK(*std::get_if<double>(&writes.writes()[0].value) == Catch::Approx(15.0));
    CHECK(*std::get_if<double>(&writes.writes()[1].value) == Catch::Approx(9.0));

    REQUIRE(BehaviorSystem::evaluate(program, BehaviorSample{2000.0, 4.0, false, 0.0}, writes)
                .has_value());
    CHECK(*std::get_if<double>(&writes.writes()[0].value) == Catch::Approx(30.0));
    CHECK(*std::get_if<double>(&writes.writes()[1].value) == Catch::Approx(9.0));

    //  A negative Beat before the first Event still yields the captured baseline, and the Step
    //  track takes the declared negative-beat step.
    REQUIRE(BehaviorSystem::evaluate(program, BehaviorSample{-250.0, -0.5, true, 0.25}, writes)
                .has_value());
    CHECK(*std::get_if<double>(&writes.writes()[0].value) == Catch::Approx(0.0));
    CHECK(*std::get_if<double>(&writes.writes()[1].value) == Catch::Approx(5.0));
}

} // namespace
