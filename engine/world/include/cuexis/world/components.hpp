#pragma once

//  Base components of the World module - data only, no behavior
//  TransformComponent: local position/rotation/scale (right-handed, column vectors, meters)
//  HierarchyComponent: parent entity reference (null means a root node)
//  WorldTransformComponent: the cached world matrix (computed by TransformSystem)

#include <cuexis/core/math.hpp>

#include <entt/entity/entity.hpp>

namespace cuexis::world {

struct TransformComponent final {
    core::Vec3 position{};
    core::Quat rotation{};
    core::Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct HierarchyComponent final {
    entt::entity parent{entt::null};
};

struct WorldTransformComponent final {
    core::Mat4 matrix{};
};

} // namespace cuexis::world
