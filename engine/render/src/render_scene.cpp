//  RenderScene 实现 — 渲染前端的最小帧描述
//  有界命令列表（最大 100 万条），目前仅支持 DebugLine 命令
//  isValidColor(): 验证 RGBA 分量均为有限值且在 [0, 1] 范围内

#include <cuexis/render/render_scene.hpp>

#include <cuexis/core/error.hpp>

#include <cmath>

namespace cuexis::render {

namespace {

[[nodiscard]] bool isValidColorComponent(float component) noexcept {
    return std::isfinite(component) && component >= 0.0F && component <= 1.0F;
}

} // namespace

auto RenderScene::addDebugLine(core::Vec3 start, core::Vec3 end, Color color)
    -> core::Result<void> {
    if (!core::isFinite(start) || !core::isFinite(end)) {
        return core::unexpected(
            core::Error{"render.scene.invalid_position", "Debug line positions must be finite"});
    }
    if (!isValidColor(color)) {
        return core::unexpected(
            core::Error{"render.scene.invalid_color",
                        "Debug line color components must be finite values in the range [0, 1]"});
    }
    if (commands_.size() >= maxCommandCount) {
        return core::unexpected(core::Error{"render.scene.command_limit_exceeded",
                                            "RenderScene command limit was exceeded"});
    }

    commands_.push_back(RenderCommand{
        .type = RenderCommandType::DebugLine,
        .start = start,
        .end = end,
        .color = color,
    });
    return {};
}

void RenderScene::clear() noexcept {
    commands_.clear();
}

bool RenderScene::empty() const noexcept {
    return commands_.empty();
}

std::size_t RenderScene::size() const noexcept {
    return commands_.size();
}

const std::vector<RenderCommand>& RenderScene::commands() const noexcept {
    return commands_;
}

bool isValidColor(const Color& color) noexcept {
    return isValidColorComponent(color.red) && isValidColorComponent(color.green) &&
           isValidColorComponent(color.blue) && isValidColorComponent(color.alpha);
}

} // namespace cuexis::render
