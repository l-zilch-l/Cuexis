#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/math.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/transform_system.hpp>
#include <cuexis/world/world.hpp>

#include <array>

TEST_CASE("World scopes entity creation and destruction through callbacks", "[world]") {
    cuexis::world::World world;

    const entt::entity entity =
        world.withRegistry([](entt::registry& registry) { return registry.create(); });
    const auto& constWorld = world;
    const bool validAfterCreation = constWorld.withRegistry(
        [entity](const entt::registry& registry) { return registry.valid(entity); });
    REQUIRE(validAfterCreation);

    world.withRegistry([entity](entt::registry& registry) { registry.destroy(entity); });
    const bool validAfterDestruction = constWorld.withRegistry(
        [entity](const entt::registry& registry) { return registry.valid(entity); });
    CHECK_FALSE(validAfterDestruction);
}

TEST_CASE("World transforms compose parent matrices before children", "[world][transform]") {
    cuexis::world::World world;
    std::array<entt::entity, 3> entities{};

    world.withRegistry([&](entt::registry& registry) {
        entities[1] = registry.create();
        entities[2] = registry.create();
        entities[0] = registry.create();

        registry.emplace<cuexis::world::TransformComponent>(
            entities[0], cuexis::core::Vec3{1.0F, 0.0F, 0.0F}, cuexis::core::Quat{},
            cuexis::core::Vec3{1.0F, 1.0F, 1.0F});
        registry.emplace<cuexis::world::TransformComponent>(
            entities[1], cuexis::core::Vec3{0.0F, 2.0F, 0.0F}, cuexis::core::Quat{},
            cuexis::core::Vec3{1.0F, 1.0F, 1.0F});
        registry.emplace<cuexis::world::TransformComponent>(
            entities[2], cuexis::core::Vec3{0.0F, 0.0F, 3.0F}, cuexis::core::Quat{},
            cuexis::core::Vec3{1.0F, 1.0F, 1.0F});
        registry.emplace<cuexis::world::HierarchyComponent>(entities[1], entities[0]);
        registry.emplace<cuexis::world::HierarchyComponent>(entities[2], entities[1]);
    });

    const auto updated = cuexis::world::updateWorldTransforms(world);
    REQUIRE(updated.has_value());

    const auto positions = world.withRegistry([&](const entt::registry& registry) {
        std::array<cuexis::core::Vec3, 3> result{};
        for (std::size_t index = 0; index < entities.size(); ++index) {
            result[index] = cuexis::core::transformPoint(
                registry.get<cuexis::world::WorldTransformComponent>(entities[index]).matrix, {});
        }
        return result;
    });

    CHECK(cuexis::core::nearlyEqual(positions[0], {1.0F, 0.0F, 0.0F}));
    CHECK(cuexis::core::nearlyEqual(positions[1], {1.0F, 2.0F, 0.0F}));
    CHECK(cuexis::core::nearlyEqual(positions[2], {1.0F, 2.0F, 3.0F}));
}

TEST_CASE("World transform validation preserves the previous complete result",
          "[world][transform][rollback]") {
    cuexis::world::World world;
    entt::entity root{entt::null};
    entt::entity child{entt::null};
    entt::entity staleParent{entt::null};

    world.withRegistry([&](entt::registry& registry) {
        root = registry.create();
        child = registry.create();
        staleParent = registry.create();
        registry.emplace<cuexis::world::TransformComponent>(root);
        registry.emplace<cuexis::world::TransformComponent>(
            child, cuexis::core::Vec3{2.0F, 0.0F, 0.0F}, cuexis::core::Quat{},
            cuexis::core::Vec3{1.0F, 1.0F, 1.0F});
        registry.emplace<cuexis::world::HierarchyComponent>(child, root);
        registry.destroy(staleParent);
    });

    REQUIRE(cuexis::world::updateWorldTransforms(world).has_value());
    const auto before = world.withRegistry([child](const entt::registry& registry) {
        return registry.get<cuexis::world::WorldTransformComponent>(child).matrix;
    });

    world.withRegistry([child, staleParent](entt::registry& registry) {
        registry.replace<cuexis::world::HierarchyComponent>(child, staleParent);
    });
    const auto failed = cuexis::world::updateWorldTransforms(world);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "world.transform.invalid_parent");

    const auto after = world.withRegistry([child](const entt::registry& registry) {
        return registry.get<cuexis::world::WorldTransformComponent>(child).matrix;
    });
    CHECK(cuexis::core::nearlyEqual(before, after));
}

TEST_CASE("World transform cycles fail without publishing partial matrices",
          "[world][transform][rollback]") {
    cuexis::world::World world;
    entt::entity first{entt::null};
    entt::entity second{entt::null};

    world.withRegistry([&](entt::registry& registry) {
        first = registry.create();
        second = registry.create();
        registry.emplace<cuexis::world::TransformComponent>(first);
        registry.emplace<cuexis::world::TransformComponent>(second);
        registry.emplace<cuexis::world::HierarchyComponent>(first, second);
        registry.emplace<cuexis::world::HierarchyComponent>(second, first);
    });

    const auto failed = cuexis::world::updateWorldTransforms(world);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "world.transform.hierarchy_cycle");

    const bool published = world.withRegistry([&](const entt::registry& registry) {
        return registry.any_of<cuexis::world::WorldTransformComponent>(first) ||
               registry.any_of<cuexis::world::WorldTransformComponent>(second);
    });
    CHECK_FALSE(published);
}
