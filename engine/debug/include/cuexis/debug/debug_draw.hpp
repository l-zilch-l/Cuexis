#pragma once

//  DebugDraw — 调试绘制，为每个有 Transform 的 Entity 渲染 XYZ 轴线
//  读取 World 中的 TransformComponent 和 WorldTransformComponent
//  通过 RenderScene.addDebugLine() 输出，不直接调用 OpenGL
//  阶段 1A/1B 用此替代真实 Mesh GPU 绘制作为可视化输出

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
