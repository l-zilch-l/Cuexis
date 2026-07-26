#pragma once

//  RenderScene / RenderCommand - the minimal frame contract of the render front end
//  RenderSystem produces a RenderScene; RenderBackend consumes it to perform the draw
//  Phases 1A/1B support the DebugLine command only; the full RenderGraph is left to later
//  phases
//  Exposes neither OpenGL nor any other graphics backend type

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
