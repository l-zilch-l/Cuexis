#include <cuexis/debug/debug_draw.hpp>

#include <cuexis/render/render_scene.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/world.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DebugDraw emits three world-space axes per transform", "[debug][draw]") {
    cuexis::world::World world;
    REQUIRE(world
                .withRegistry([](entt::registry& registry) {
                    const auto first = registry.create();
                    registry.template emplace<cuexis::world::WorldTransformComponent>(first);

                    const auto second = registry.create();
                    cuexis::world::WorldTransformComponent translated;
                    translated.matrix.element(0, 3) = 0.5F;
                    registry.template emplace<cuexis::world::WorldTransformComponent>(second,
                                                                                      translated);
                })
                .has_value());

    cuexis::render::RenderScene scene;
    REQUIRE(cuexis::debug::appendTransformAxes(world, scene));

    REQUIRE(scene.size() == 6);
    CHECK(scene.commands()[0].start == cuexis::core::Vec3{});
    CHECK(scene.commands()[3].start == cuexis::core::Vec3{0.5F, 0.0F, 0.0F});
}

TEST_CASE("DebugDraw rejects invalid axis lengths without mutating the scene", "[debug][draw]") {
    cuexis::world::World world;
    cuexis::render::RenderScene scene;

    const auto result = cuexis::debug::appendTransformAxes(
        world, scene, cuexis::debug::TransformAxesConfig{.axisLength = 0.0F});

    REQUIRE_FALSE(result);
    CHECK(result.error().code() == "debug.draw.invalid_axis_length");
    CHECK(scene.empty());
}
