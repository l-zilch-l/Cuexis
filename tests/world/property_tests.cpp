#include <cuexis/world/property.hpp>
#include <cuexis/world/world.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

namespace {

using cuexis::core::Quat;
using cuexis::core::Vec3;
using cuexis::world::PropertyId;
using cuexis::world::PropertyWrite;
using cuexis::world::TransformComponent;
using cuexis::world::TransformPropertyResolver;
using cuexis::world::World;

TEST_CASE("Transform resolver rebuilds every frame from the captured baseline",
          "[world][property][determinism]") {
    World world;
    entt::entity entity{entt::null};
    world.withRegistry([&](entt::registry& registry) {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity, Vec3{1.0F, 2.0F, 3.0F},
                                             Quat{0.0F, 0.0F, 0.0F, 1.0F}, Vec3{1.0F, 1.0F, 1.0F});
    });

    auto resolverResult = TransformPropertyResolver::capture(world);
    REQUIRE(resolverResult.has_value());
    auto resolver = std::move(*resolverResult);

    cuexis::world::PropertyWriteBuffer writes;
    REQUIRE(writes.push(PropertyWrite{entity, PropertyId::TransformPositionX, 9.0}).has_value());
    REQUIRE(resolver.prepare(writes.writes()).has_value());
    REQUIRE(resolver.commit(world).has_value());
    auto first = world.withRegistry([&](const entt::registry& registry) {
        return registry.get<TransformComponent>(entity).position;
    });
    CHECK(first.x == Catch::Approx(9.0F));

    writes.clear();
    REQUIRE(writes.push(PropertyWrite{entity, PropertyId::TransformPositionY, 8.0}).has_value());
    REQUIRE(resolver.prepare(writes.writes()).has_value());
    REQUIRE(resolver.commit(world).has_value());
    auto second = world.withRegistry([&](const entt::registry& registry) {
        return registry.get<TransformComponent>(entity).position;
    });
    CHECK(second.x == Catch::Approx(1.0F));
    CHECK(second.y == Catch::Approx(8.0F));
    CHECK(second.z == Catch::Approx(3.0F));
}

TEST_CASE("Transform resolver validates all writes before commit", "[world][property][rollback]") {
    World world;
    entt::entity entity{entt::null};
    world.withRegistry([&](entt::registry& registry) {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity);
    });
    auto resolverResult = TransformPropertyResolver::capture(world);
    REQUIRE(resolverResult.has_value());
    auto resolver = std::move(*resolverResult);
    cuexis::world::PropertyWriteBuffer writes;
    REQUIRE(writes.push(PropertyWrite{entity, PropertyId::TransformPositionX, 4.0}).has_value());
    REQUIRE(writes
                .push(PropertyWrite{entity, PropertyId::TransformRotation,
                                    Quat{0.0F, 0.0F, 0.0F, 2.0F}})
                .has_value());
    auto result = resolver.prepare(writes.writes());
    REQUIRE_FALSE(result.has_value());
    const auto transform = world.withRegistry(
        [&](const entt::registry& registry) { return registry.get<TransformComponent>(entity); });
    CHECK(transform.position == Vec3{});
    CHECK(transform.rotation == Quat{});
}

TEST_CASE("PropertyWriteBuffer enforces its configured budget", "[world][property][limits]") {
    cuexis::world::PropertyWriteBuffer writes{1};
    REQUIRE(writes.push(PropertyWrite{entt::entity{1}, PropertyId::TransformPositionX, 1.0})
                .has_value());
    const auto result =
        writes.push(PropertyWrite{entt::entity{1}, PropertyId::TransformPositionY, 2.0});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "world.property.write_limit");
}

TEST_CASE("Transform resolver rollback only restores entities written in the current frame",
          "[world][property][sparse]") {
    World world;
    entt::entity first{entt::null};
    entt::entity second{entt::null};
    world.withRegistry([&](entt::registry& registry) {
        first = registry.create();
        second = registry.create();
        registry.emplace<TransformComponent>(first);
        registry.emplace<TransformComponent>(second);
    });
    auto captured = TransformPropertyResolver::capture(world);
    REQUIRE(captured.has_value());
    auto resolver = std::move(*captured);

    const PropertyWrite firstWrite{first, PropertyId::TransformPositionX, 3.0};
    REQUIRE(resolver.prepare(std::span{&firstWrite, 1}).has_value());
    REQUIRE(resolver.commit(world).has_value());

    const PropertyWrite secondWrite{second, PropertyId::TransformPositionY, 4.0};
    REQUIRE(resolver.prepare(std::span{&secondWrite, 1}).has_value());
    REQUIRE(resolver.commit(world).has_value());
    resolver.rollback(world);

    const auto positions = world.withRegistry([&](const entt::registry& registry) {
        return std::pair{registry.get<TransformComponent>(first).position,
                         registry.get<TransformComponent>(second).position};
    });
    CHECK(positions.first.x == Catch::Approx(3.0F));
    CHECK(positions.second == Vec3{});
}

} // namespace
