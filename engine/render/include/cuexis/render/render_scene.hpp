#pragma once

//  RenderScene / RenderCommand — 渲染前端的最小帧契约
//  RenderSystem 生成 RenderScene；RenderBackend 消费 RenderScene 执行绘制
//  阶段 1A/1B 仅支持 DebugLine 命令；完整 RenderGraph 留在后续阶段
//  不暴露 OpenGL 或任何图形后端类型

#include <cuexis/core/math.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <vector>

namespace cuexis::render {

struct Color final {
    float red{0.0F};
    float green{0.0F};
    float blue{0.0F};
    float alpha{1.0F};

    friend bool operator==(const Color&, const Color&) = default;
};

enum class RenderCommandType {
    DebugLine,
};

struct RenderCommand final {
    RenderCommandType type{RenderCommandType::DebugLine};
    core::Vec3 start{};
    core::Vec3 end{};
    Color color{};

    friend bool operator==(const RenderCommand&, const RenderCommand&) = default;
};

class RenderScene final {
  public:
    static constexpr std::size_t maxCommandCount = 1'000'000;

    [[nodiscard]] auto addDebugLine(core::Vec3 start, core::Vec3 end, Color color)
        -> core::Result<void>;
    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::vector<RenderCommand>& commands() const noexcept;

  private:
    std::vector<RenderCommand> commands_;
};

[[nodiscard]] bool isValidColor(const Color& color) noexcept;

} // namespace cuexis::render
