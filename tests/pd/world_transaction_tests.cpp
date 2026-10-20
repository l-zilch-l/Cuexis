#include "transaction_allocations.hpp"
#include "transaction_state.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/world/transform_system.hpp>
#include <cuexis/world/transform_update.hpp>
#include <limits>

using namespace cuexis;
namespace {
struct Scene {
    world::World world;
    entt::entity parent{}, child{}, stale{};
    explicit Scene(bool matrices = true) {
        REQUIRE(world.withRegistry([&](entt::registry& r) {
            parent = r.create();
            child = r.create();
            stale = r.create();
            r.emplace<world::TransformComponent>(parent);
            r.emplace<world::TransformComponent>(child);
            r.emplace<world::HierarchyComponent>(child, parent);
        }));
        if (matrices)
            REQUIRE(world::updateWorldTransforms(world));
    }
    auto state() const -> std::vector<float> {
        auto values = world.withRegistry([&](const entt::registry& r) {
            std::vector<float> result;
            for (auto e : {parent, child, stale}) {
                if (const auto* t = r.try_get<world::TransformComponent>(e)) {
                    result.push_back(1);
                    result.push_back(t->position.x);
                    result.push_back(t->scale.x);
                } else
                    result.push_back(0);
                if (const auto* t = r.try_get<world::WorldTransformComponent>(e)) {
                    result.push_back(1);
                    result.insert(result.end(), t->matrix.values.begin(), t->matrix.values.end());
                } else
                    result.push_back(0);
            }
            return result;
        });
        REQUIRE(values);
        return *values;
    }
    auto writes() const -> std::array<world::detail::TransformWrite, 1> {
        world::TransformComponent local;
        local.position.x = 5;
        return {{{parent, local}}};
    }
};
struct Signals {
    unsigned called{};
    void receive(entt::registry&, entt::entity) {
        ++called;
    }
};
} // namespace

TEST_CASE("PD3a World prepares cold storage and installs without allocating or invoking signals",
          "[pd][transaction][world]") {
    for (bool initialized : {false, true}) {
        Scene scene(initialized);
        Signals signals;
        REQUIRE(scene.world.withRegistry([&](entt::registry& r) {
            r.on_update<world::WorldTransformComponent>().connect<&Signals::receive>(signals);
            if (initialized) {
                r.remove<world::WorldTransformComponent>(scene.child);
                r.emplace<world::WorldTransformComponent>(scene.stale);
            }
        }));
        auto before = scene.state();
        const auto cacheBefore = world::detail::WorldTestAccess::cache(scene.world);
        world::detail::TransformUpdate plan;
        REQUIRE(plan.prepare(scene.world, scene.writes()));
        CHECK_FALSE(scene.world.withRegistry([](auto&) {}));
        plan.cancel();
        CHECK(scene.state() == before);
        CHECK(world::detail::WorldTestAccess::cache(scene.world) == cacheBefore);
        REQUIRE(plan.prepare(scene.world, scene.writes()));
        pd_allocations::begin();
        plan.apply();
        plan.finish();
        const auto allocations = pd_allocations::end();
        CHECK(allocations == 0);
        CHECK(signals.called == 0);
        REQUIRE(scene.world.withRegistry([&](entt::registry& r) {
            CHECK(r.get<world::TransformComponent>(scene.parent).position.x == 5);
            CHECK(r.get<world::WorldTransformComponent>(scene.child).matrix.values[12] == 5);
            CHECK_FALSE(r.all_of<world::WorldTransformComponent>(scene.stale));
            r.patch<world::WorldTransformComponent>(scene.child);
        }));
        CHECK(signals.called == 1); // Signal objects survived the payload exchange.
    }
}

TEST_CASE("PD3a World allocation failures and derived overflow preserve active state",
          "[pd][transaction][world][failure]") {
    std::size_t count{};
    {
        Scene scene(false);
        world::detail::TransformUpdate plan;
        auto writes = scene.writes();
        pd_allocations::begin();
        auto prepared = plan.prepare(scene.world, writes);
        count = pd_allocations::end();
        REQUIRE(prepared);
        plan.cancel();
    }
    REQUIRE(count > 0);
    for (std::size_t index = 1; index <= count; ++index) {
        CAPTURE(index, count);
        Scene scene(false);
        const auto before = scene.state();
        const auto cacheBefore = world::detail::WorldTestAccess::cache(scene.world);
        world::detail::TransformUpdate plan;
        auto writes = scene.writes();
        bool rejected{};
        pd_allocations::begin(index);
        try {
            auto p = plan.prepare(scene.world, writes);
            rejected = !p;
        } catch (const std::bad_alloc&) {
            rejected = true;
        }
        pd_allocations::end();
        plan.cancel();
        CHECK(pd_allocations::failed);
        CHECK(rejected);
        CHECK(scene.state() == before);
        CHECK(world::detail::WorldTestAccess::cache(scene.world) == cacheBefore);
        REQUIRE(plan.prepare(scene.world, writes));
        plan.apply();
        plan.finish();
        REQUIRE(world::updateWorldTransforms(scene.world));
    }
    for (bool initialized : {false, true}) {
        Scene scene(initialized);
        REQUIRE(scene.world.withRegistry([&](entt::registry& r) {
            r.get<world::TransformComponent>(scene.child).scale = {1e30F, 1e30F, 1e30F};
        }));
        const auto before = scene.state();
        const auto cacheBefore = world::detail::WorldTestAccess::cache(scene.world);
        world::TransformComponent local;
        local.scale = {1e30F, 1e30F, 1e30F};
        const std::array writes{world::detail::TransformWrite{scene.parent, local}};
        world::detail::TransformUpdate plan;
        auto failed = plan.prepare(scene.world, writes);
        REQUIRE_FALSE(failed);
        CHECK(failed.error().code() == "world.transform.invalid_world_transform");
        CHECK(scene.state() == before);
        CHECK(world::detail::WorldTestAccess::cache(scene.world) == cacheBefore);
    }
}

TEST_CASE("PD3a World refuses observed structural replacement without invalidating groups",
          "[pd][transaction][world][observer]") {
    Scene scene(false);
    Signals signals;
    REQUIRE(scene.world.withRegistry([&](entt::registry& r) {
        r.on_construct<world::WorldTransformComponent>().connect<&Signals::receive>(signals);
    }));
    const auto before = scene.state();
    world::detail::TransformUpdate plan;
    CHECK_FALSE(plan.prepare(scene.world, scene.writes()));
    CHECK(scene.state() == before);
    CHECK(signals.called == 0);
    REQUIRE(world::updateWorldTransforms(scene.world)); // Legacy publishing is still explicit.
    CHECK(signals.called == 2);
}

TEST_CASE("PD3a standalone World reuses warmed transform preparation", "[pd][transaction][world]") {
    Scene scene;
    pd_allocations::begin();
    auto unchanged = world::updateWorldTransforms(scene.world);
    const auto unchangedAllocations = pd_allocations::end();
    REQUIRE(unchanged);
    CHECK(unchangedAllocations == 0);
    REQUIRE(scene.world.withRegistry(
        [&](entt::registry& r) { r.get<world::TransformComponent>(scene.child).position.x = 7; }));
    pd_allocations::begin();
    auto changed = world::updateWorldTransforms(scene.world);
    const auto changedAllocations = pd_allocations::end();
    REQUIRE(changed);
    CHECK(changedAllocations == 0);
}

TEST_CASE("PD3a cold storage refuses a live EnTT group and preserves its membership",
          "[pd][transaction][world][observer]") {
    Scene scene(false);
    REQUIRE(scene.world.withRegistry([&](entt::registry& r) {
        auto group = r.group<world::WorldTransformComponent>(entt::get<world::TransformComponent>);
        CHECK(group.empty());
    }));
    world::detail::TransformUpdate plan;
    const auto before = scene.state();
    CHECK_FALSE(plan.prepare(scene.world, scene.writes()));
    CHECK(scene.state() == before);
    REQUIRE(world::updateWorldTransforms(scene.world));
    REQUIRE(scene.world.withRegistry([&](entt::registry& r) {
        auto group = r.group<world::WorldTransformComponent>(entt::get<world::TransformComponent>);
        CHECK(group.size() == 2);
        CHECK(group.contains(scene.parent));
        CHECK(group.contains(scene.child));
    }));
}
