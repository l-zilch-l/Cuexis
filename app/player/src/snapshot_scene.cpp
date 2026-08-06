#include "snapshot_scene.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>

#include <algorithm>
#include <iterator>

namespace cuexis::player {
namespace {

[[nodiscard]] auto matrixFrom(const float (&values)[16]) noexcept -> core::Mat4 {
    core::Mat4 matrix;
    std::copy(std::begin(values), std::end(values), matrix.values.begin());
    return matrix;
}

} // namespace

auto appendSnapshotAxes(const playback::FrameSnapshot& snapshot, render::RenderScene& scene)
    -> core::Result<void> {
    constexpr float axisLength = 0.15F;
    for (const auto& object : snapshot.objects) {
        if (!object.visible) {
            continue;
        }
        const auto matrix = matrixFrom(object.worldMatrix);
        if (!core::isFinite(matrix)) {
            return core::unexpected(
                core::Error{"player.snapshot.matrix_invalid", "Snapshot matrix is not finite"}
                    .withContext("object_id", object.id));
        }

        const core::Vec3 origin = core::transformPoint(matrix, {});
        const core::Vec3 xEnd = core::transformPoint(matrix, {axisLength, 0.0F, 0.0F});
        const core::Vec3 yEnd = core::transformPoint(matrix, {0.0F, axisLength, 0.0F});
        const core::Vec3 zEnd = core::transformPoint(matrix, {0.0F, 0.0F, axisLength});
        if (auto result = scene.addDebugLine(origin, xEnd, {1.0F, 0.2F, 0.2F, 1.0F}); !result) {
            return result;
        }
        if (auto result = scene.addDebugLine(origin, yEnd, {0.2F, 1.0F, 0.2F, 1.0F}); !result) {
            return result;
        }
        if (auto result = scene.addDebugLine(origin, zEnd, {0.2F, 0.45F, 1.0F, 1.0F}); !result) {
            return result;
        }
    }
    return {};
}

} // namespace cuexis::player
