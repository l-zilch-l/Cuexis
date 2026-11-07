#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cuexis/behavior/behavior_component.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/component.hpp>

namespace {
constexpr std::size_t componentPageSize = 128;
constexpr std::size_t pageBoundaryCount = componentPageSize + 1;

static_assert(entt::component_traits<cuexis::world::TransformComponent>::page_size ==
              componentPageSize);
static_assert(entt::component_traits<cuexis::world::HierarchyComponent>::page_size ==
              componentPageSize);
static_assert(entt::component_traits<cuexis::world::WorldTransformComponent>::page_size ==
              componentPageSize);
static_assert(entt::component_traits<cuexis::behavior::BehaviorComponent>::page_size ==
              componentPageSize);
static_assert(entt::component_traits<cuexis::render::RenderableComponent>::page_size ==
              componentPageSize);
static_assert(entt::component_traits<cuexis::render::AppearanceComponent>::page_size ==
              componentPageSize);
static_assert(entt::component_traits<cuexis::render::CameraComponent>::page_size ==
              componentPageSize);

template <typename Component>
void checkPageBoundary(entt::registry& registry,
                       const std::array<entt::entity, pageBoundaryCount>& entities) {
    auto& storage = registry.storage<Component>();
    CHECK(storage.capacity() == 0);

    for (std::size_t index = 0; index < componentPageSize; ++index)
        registry.emplace<Component>(entities[index]);

    CHECK(storage.size() == componentPageSize);
    CHECK(storage.capacity() == componentPageSize);

    registry.emplace<Component>(entities.back());
    CHECK(storage.size() == pageBoundaryCount);
    CHECK(storage.capacity() == componentPageSize * 2);

    registry.remove<Component>(entities[componentPageSize / 2]);
    CHECK(storage.size() == componentPageSize);
    CHECK(storage.capacity() == componentPageSize * 2);
    CHECK_FALSE(registry.all_of<Component>(entities[componentPageSize / 2]));
    CHECK(registry.all_of<Component>(entities.back()));
}
} // namespace

TEST_CASE("World component pools grow at the declared page boundary",
          "[world][components][page-size]") {
    cuexis::world::World world;
    std::array<entt::entity, pageBoundaryCount> entities{};
    REQUIRE(world.withRegistry([&](entt::registry& registry) {
        for (auto& entity : entities)
            entity = registry.create();
    }));

    REQUIRE(world.withRegistry([&](entt::registry& registry) {
        checkPageBoundary<cuexis::world::TransformComponent>(registry, entities);
        checkPageBoundary<cuexis::world::HierarchyComponent>(registry, entities);
        checkPageBoundary<cuexis::world::WorldTransformComponent>(registry, entities);
        checkPageBoundary<cuexis::behavior::BehaviorComponent>(registry, entities);
        checkPageBoundary<cuexis::render::RenderableComponent>(registry, entities);
        checkPageBoundary<cuexis::render::AppearanceComponent>(registry, entities);
        checkPageBoundary<cuexis::render::CameraComponent>(registry, entities);
    }));
}
