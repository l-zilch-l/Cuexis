#include <cuexis/behavior/behavior_system.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

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
    program.definitions.push_back(
        BehaviorDefinition{.tracks = {
                               BehaviorTrack{.property = PropertyId::TransformPositionX,
                                             .keys = {BehaviorKey{.chartTimeMs = 0.0,
                                                                  .value = PropertyValue{0.0},
                                                                  .easing = Easing::Linear},
                                                      BehaviorKey{.chartTimeMs = 100.0,
                                                                  .value = PropertyValue{10.0},
                                                                  .easing = Easing::InCubic}}},
                           }});
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
    program.definitions.push_back(BehaviorDefinition{
        .tracks = {
            BehaviorTrack{.property = PropertyId::TransformScale,
                          .keys = {BehaviorKey{.chartTimeMs = 0.0,
                                               .value = PropertyValue{Vec3{1.0F, 2.0F, 3.0F}}},
                                   BehaviorKey{.chartTimeMs = 100.0,
                                               .value = PropertyValue{Vec3{3.0F, 4.0F, 5.0F}},
                                               .easing = Easing::OutCubic}}},
            BehaviorTrack{
                .property = PropertyId::TransformRotation,
                .keys = {BehaviorKey{.chartTimeMs = 0.0,
                                     .value = PropertyValue{Quat{0.0F, 0.0F, 0.0F, 1.0F}}},
                         BehaviorKey{.chartTimeMs = 100.0,
                                     .value = PropertyValue{Quat{0.0F, 0.0F, 0.0F, -1.0F}}}}},
        }});
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

} // namespace
