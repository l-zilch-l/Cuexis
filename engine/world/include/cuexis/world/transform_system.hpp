#pragma once

//  TransformSystem — 世界矩阵更新
//  按父级优先顺序自顶向下计算，脏标记避免无变化层级重复计算
//  验证失败时保持原子性：失败后不部分更新 World 状态

#include <cuexis/core/result.hpp>

namespace cuexis::world {

class World;

// 原子地重新计算所有世界矩阵；验证失败时不修改任何状态
[[nodiscard]] auto updateWorldTransforms(World& world) -> core::Result<void>;

} // namespace cuexis::world
