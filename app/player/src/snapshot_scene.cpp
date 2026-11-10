#include "snapshot_scene.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <string>

namespace cuexis::player {
namespace {

[[nodiscard]] auto matrixFrom(const float (&values)[16]) noexcept -> core::Mat4 {
    core::Mat4 matrix;
    std::copy(std::begin(values), std::end(values), matrix.values.begin());
    return matrix;
}

} // namespace

void PlayerFpsMeter::notePresented() noexcept {
    ++frames_;
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - started_).count();
    if (seconds >= 0.5) {
        reading_ = static_cast<unsigned>(
            std::clamp(std::round(static_cast<double>(frames_) / seconds), 0.0, 99999.0));
        frames_ = 0;
        started_ = now;
    }
}

auto appendFpsOverlay(const playback::FrameSnapshot& snapshot, std::optional<unsigned> fps,
                      render::RenderScene& scene) -> core::Result<void> {
    if (!snapshot.camera.active || snapshot.viewportWidth < 180 || snapshot.viewportHeight < 48)
        return {};
    auto inverse = core::inverse(core::multiply(matrixFrom(snapshot.camera.projectionMatrix),
                                                matrixFrom(snapshot.camera.viewMatrix)));
    if (!inverse)
        return core::unexpected(std::move(inverse.error()));
    const auto text = std::string{"FPS "} + (fps ? std::to_string(*fps) : "--");
    const float left = static_cast<float>(snapshot.viewportWidth) - 12.0F -
                       static_cast<float>(text.size()) * 18.0F;
    const auto point = [&](float x, float y, float z) {
        const core::Vec3 ndc{2.0F * x / static_cast<float>(snapshot.viewportWidth) - 1.0F,
                             1.0F - 2.0F * y / static_cast<float>(snapshot.viewportHeight), z};
        const auto& m = *inverse;
        const auto w = m.element(3, 0) * ndc.x + m.element(3, 1) * ndc.y + m.element(3, 2) * ndc.z +
                       m.element(3, 3);
        const auto p = core::transformPoint(m, ndc);
        return core::Vec3{p.x / w, p.y / w, p.z / w};
    };
    const auto glyph = [](char c) -> std::array<unsigned, 7> {
        constexpr std::array<std::array<unsigned, 7>, 10> digits{{{14, 17, 19, 21, 25, 17, 14},
                                                                  {4, 12, 4, 4, 4, 4, 14},
                                                                  {14, 17, 1, 2, 4, 8, 31},
                                                                  {30, 1, 1, 14, 1, 1, 30},
                                                                  {2, 6, 10, 18, 31, 2, 2},
                                                                  {31, 16, 16, 30, 1, 1, 30},
                                                                  {14, 16, 16, 30, 17, 17, 14},
                                                                  {31, 1, 2, 4, 8, 8, 8},
                                                                  {14, 17, 17, 14, 17, 17, 14},
                                                                  {14, 17, 17, 15, 1, 1, 14}}};
        if (c >= '0' && c <= '9')
            return digits[static_cast<unsigned>(c - '0')];
        if (c == 'F')
            return {31, 16, 16, 30, 16, 16, 16};
        if (c == 'P')
            return {30, 17, 17, 30, 16, 16, 16};
        if (c == 'S')
            return {15, 16, 16, 14, 1, 1, 30};
        if (c == '-')
            return {0, 0, 0, 31, 0, 0, 0};
        return {};
    };
    // Pixel-aligned HUD through the existing neutral DebugLine pass. Keep it inside
    // the near plane so it follows the viewport instead of chart objects or camera motion.
    for (unsigned pass = 0; pass < 2; ++pass) {
        const float offset = pass == 0 ? 1.0F : 0.0F;
        const float depth = pass == 0 ? -0.998F : -0.999F;
        const render::Color color =
            pass == 0 ? render::Color{0, 0, 0, 1} : render::Color{0.85F, 1.0F, 0.85F, 1};
        for (std::size_t i = 0; i < text.size(); ++i) {
            const auto rows = glyph(text[i]);
            for (unsigned row = 0; row < 7; ++row)
                for (unsigned column = 0; column < 5; ++column)
                    if (rows[row] & (1U << (4 - column)))
                        for (unsigned stroke = 0; stroke < 3; ++stroke) {
                            const float x = left + static_cast<float>(i) * 18.0F +
                                            static_cast<float>(column) * 3.0F + offset;
                            const float y = 12.0F + static_cast<float>(row) * 3.0F +
                                            static_cast<float>(stroke) + offset;
                            if (auto drawn = scene.addDebugLine(point(x, y, depth),
                                                                point(x + 2.5F, y, depth), color);
                                !drawn)
                                return drawn;
                        }
        }
    }
    return {};
}

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
