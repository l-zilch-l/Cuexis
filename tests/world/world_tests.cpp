#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/math.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/transform_system.hpp>
#include <cuexis/world/world.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace {

struct TransformUpdateCounter final {
    std::vector<entt::entity> entities;

    void receive(entt::registry&, entt::entity entity) {
        entities.push_back(entity);
    }
};

struct TransformUpdateCount final {
    std::size_t value{};

    void receive(entt::registry&, entt::entity) noexcept {
        ++value;
    }
};

} // namespace

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

TEST_CASE("World transform cache publishes only a changed hierarchy subtree",
          "[world][transform][sparse]") {
    cuexis::world::World world;
    entt::entity parent{entt::null};
    entt::entity child{entt::null};
    entt::entity unrelated{entt::null};
    TransformUpdateCounter updates;
    world.withRegistry([&](entt::registry& registry) {
        parent = registry.create();
        child = registry.create();
        unrelated = registry.create();
        registry.emplace<cuexis::world::TransformComponent>(parent);
        registry.emplace<cuexis::world::TransformComponent>(child);
        registry.emplace<cuexis::world::TransformComponent>(unrelated);
        registry.emplace<cuexis::world::HierarchyComponent>(child, parent);
        registry.on_update<cuexis::world::WorldTransformComponent>()
            .connect<&TransformUpdateCounter::receive>(updates);
    });

    REQUIRE(cuexis::world::updateWorldTransforms(world).has_value());
    REQUIRE(cuexis::world::updateWorldTransforms(world).has_value());
    CHECK(updates.entities.empty());

    world.withRegistry([&](entt::registry& registry) {
        auto transform = registry.get<cuexis::world::TransformComponent>(parent);
        transform.position.x = 2.0F;
        registry.replace<cuexis::world::TransformComponent>(parent, transform);
    });
    REQUIRE(cuexis::world::updateWorldTransforms(world).has_value());
    REQUIRE(updates.entities.size() == 2);
    CHECK(std::find(updates.entities.begin(), updates.entities.end(), parent) !=
          updates.entities.end());
    CHECK(std::find(updates.entities.begin(), updates.entities.end(), child) !=
          updates.entities.end());
    CHECK(std::find(updates.entities.begin(), updates.entities.end(), unrelated) ==
          updates.entities.end());
}

TEST_CASE("World transform cache preserves sparse work through the default object budget",
          "[world][transform][scale]") {
    constexpr std::array<std::size_t, 3> objectCounts{1'000, 10'000, 100'000};
    for (const auto objectCount : objectCounts) {
        CAPTURE(objectCount);
        cuexis::world::World world;
        entt::entity root{entt::null};
        entt::entity leaf{entt::null};
        world.withRegistry([&](entt::registry& registry) {
            entt::entity parent{entt::null};
            for (std::size_t index = 0; index < objectCount; ++index) {
                const auto entity = registry.create();
                registry.emplace<cuexis::world::TransformComponent>(entity);
                if (parent != entt::null) {
                    registry.emplace<cuexis::world::HierarchyComponent>(entity, parent);
                } else {
                    root = entity;
                }
                parent = entity;
                leaf = entity;
            }
        });

        REQUIRE(cuexis::world::updateWorldTransforms(world).has_value());
        TransformUpdateCount updates;
        world.withRegistry([&](entt::registry& registry) {
            registry.on_update<cuexis::world::WorldTransformComponent>()
                .connect<&TransformUpdateCount::receive>(updates);
        });

        REQUIRE(cuexis::world::updateWorldTransforms(world).has_value());
        CHECK(updates.value == 0);

        world.withRegistry([&](entt::registry& registry) {
            auto transform = registry.get<cuexis::world::TransformComponent>(leaf);
            transform.position.x = 1.0F;
            registry.replace<cuexis::world::TransformComponent>(leaf, transform);
        });
        REQUIRE(cuexis::world::updateWorldTransforms(world).has_value());
        CHECK(updates.value == 1);

        updates.value = 0;
        world.withRegistry([&](entt::registry& registry) {
            auto transform = registry.get<cuexis::world::TransformComponent>(root);
            transform.position.y = 1.0F;
            registry.replace<cuexis::world::TransformComponent>(root, transform);
        });
        REQUIRE(cuexis::world::updateWorldTransforms(world).has_value());
        CHECK(updates.value == objectCount);
    }
}
