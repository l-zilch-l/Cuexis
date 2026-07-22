#pragma once

//  World 模块基础 Component — 仅保存数据，不包含行为
//  TransformComponent: 局部位置/旋转/缩放（右手坐标系，列向量，米制单位）
//  HierarchyComponent: 父 Entity 引用（null 表示根节点）
//  WorldTransformComponent: 缓存的世界矩阵（由 TransformSystem 计算）

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
