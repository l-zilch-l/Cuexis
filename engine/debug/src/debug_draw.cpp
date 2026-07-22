//  DebugDraw 实现 — XYZ 轴线调试绘制
//  读取 World 中每个有 TransformComponent 和 WorldTransformComponent 的 Entity
//  为其生成三条 RenderScene DebugLine（红 X、绿 Y、蓝 Z），线长可配置

#include <cuexis/debug/debug_draw.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/render/render_scene.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/world.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace cuexis::debug {

auto appendTransformAxes(const world::World& world, render::RenderScene& scene,
                         const TransformAxesConfig& config) -> core::Result<void> {
    if (!std::isfinite(config.axisLength) || config.axisLength <= 0.0F) {
        return core::unexpected(core::Error{"debug.draw.invalid_axis_length",
                                            "Transform axis length must be finite and positive"});
    }

    return world.withRegistry([&](const auto& registry) -> core::Result<void> {
        auto view = registry.template view<const world::WorldTransformComponent>();
        std::vector<entt::entity> entities;
        for (const auto entity : view) {
            entities.push_back(entity);
        }
        std::sort(entities.begin(), entities.end(), [](entt::entity left, entt::entity right) {
            return entt::to_integral(left) < entt::to_integral(right);
        });

        for (const auto entity : entities) {
            const auto& matrix =
                view.template get<const world::WorldTransformComponent>(entity).matrix;
            const core::Vec3 origin = core::transformPoint(matrix, {});
            const core::Vec3 xEnd = core::transformPoint(matrix, {config.axisLength, 0.0F, 0.0F});
            const core::Vec3 yEnd = core::transformPoint(matrix, {0.0F, config.axisLength, 0.0F});
            const core::Vec3 zEnd = core::transformPoint(matrix, {0.0F, 0.0F, config.axisLength});

            if (auto result = scene.addDebugLine(origin, xEnd, {1.0F, 0.2F, 0.2F, 1.0F}); !result) {
                return result;
            }
            if (auto result = scene.addDebugLine(origin, yEnd, {0.2F, 1.0F, 0.2F, 1.0F}); !result) {
                return result;
            }
            if (auto result = scene.addDebugLine(origin, zEnd, {0.2F, 0.45F, 1.0F, 1.0F});
                !result) {
                return result;
            }
        }
        return {};
    });
}

} // namespace cuexis::debug
