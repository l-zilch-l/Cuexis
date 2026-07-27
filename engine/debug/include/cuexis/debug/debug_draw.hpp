#pragma once

// DebugDraw appends XYZ axes for entities with Transform components.
// It reads TransformComponent and WorldTransformComponent from World.
// Output goes through RenderScene::addDebugLine() without direct OpenGL calls.
// Stages 1A and 1B use this as visible output before real Mesh GPU rendering.

#include <cuexis/core/result.hpp>

namespace cuexis::render {
class RenderScene;
}

namespace cuexis::world {
class World;
}

namespace cuexis::debug {

struct TransformAxesConfig final {
    float axisLength{0.15F};
};

[[nodiscard]] auto appendTransformAxes(const world::World& world, render::RenderScene& scene,
                                       const TransformAxesConfig& config = {})
    -> core::Result<void>;

} // namespace cuexis::debug
