#include <cuexis/render/render_scene.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>

TEST_CASE("RenderScene accepts finite debug lines in insertion order", "[render][scene]") {
    cuexis::render::RenderScene scene;

    REQUIRE(scene.addDebugLine({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F, 1.0F}));
    REQUIRE(scene.addDebugLine({0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F, 1.0F}));

    REQUIRE(scene.size() == 2);
    CHECK(scene.commands()[0].end == cuexis::core::Vec3{1.0F, 0.0F, 0.0F});
    CHECK(scene.commands()[1].end == cuexis::core::Vec3{0.0F, 1.0F, 0.0F});
}

TEST_CASE("RenderScene rejects invalid positions and colors", "[render][scene]") {
    cuexis::render::RenderScene scene;
    const float infinity = std::numeric_limits<float>::infinity();

    const auto invalidPosition = scene.addDebugLine({infinity, 0.0F, 0.0F}, {}, {});
    REQUIRE_FALSE(invalidPosition);
    CHECK(invalidPosition.error().code() == "render.scene.invalid_position");

    const auto invalidColor = scene.addDebugLine({}, {1.0F, 0.0F, 0.0F}, {1.1F, 0.0F, 0.0F, 1.0F});
    REQUIRE_FALSE(invalidColor);
    CHECK(invalidColor.error().code() == "render.scene.invalid_color");
    CHECK(scene.empty());
}

TEST_CASE("RenderScene can be cleared without changing its contract", "[render][scene]") {
    cuexis::render::RenderScene scene;
    REQUIRE(scene.addDebugLine({}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}));

    scene.clear();

    CHECK(scene.empty());
    CHECK(scene.commands().empty());
}
