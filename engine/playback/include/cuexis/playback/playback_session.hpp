#pragma once

//  PlaybackSession — 宿主集成的单一播放门面
//  包装 RuntimeSession 的内部生命周期，隐藏 World、EnTT 和后端具体实现
//  宿主拥有主循环、窗口和渲染后端；PlaybackSession 只消费 RuntimeFrame 并输出 FrameSnapshot
//  判定与回放接口在阶段 11 由 cuexis_judgement 提供，PlaybackSession 为其预留扩展点
//  公共头不暴露 entt、SDL、OpenGL、JSON DOM 或内部 Runtime 类型

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::playback {

struct RuntimeFrame final {
    double chartTimeMs{};
    double simulationDeltaTimeMs{};
    std::uint64_t timeDiscontinuityId{};
};

struct FrameViewport final {
    std::uint32_t width{};
    std::uint32_t height{};
};

enum class ReloadPolicy {
    KeepChartTime,
    RestartAtZero,
};

struct FrameSnapshot final {
    struct ObjectSnapshot final {
        std::string id;
        float worldMatrix[16]{}; // column-major Mat4 flattened
        bool visible{true};
    };

    struct CameraSnapshot final {
        bool active{};
        float viewMatrix[16]{};       // world-to-view transform of camera entity
        float projectionMatrix[16]{}; // perspective projection from fovY/near/far/aspect
        double fovY{60.0};
        double nearPlane{0.1};
        double farPlane{1000.0};
        double pitch{0.0};
        double yaw{0.0};
        double roll{0.0};
    };

    std::vector<ObjectSnapshot> objects;
    CameraSnapshot camera;
    float clearRed{0.055F};
    float clearGreen{0.063F};
    float clearBlue{0.071F};
    float clearAlpha{1.0F};
    std::uint32_t viewportWidth{};
    std::uint32_t viewportHeight{};
};

struct ChartInfo final {
    std::size_t objectCount{};
    std::size_t behaviorCount{};
};

enum class SessionState {
    Empty,
    Ready,
    Running,
    Paused,
    Failed,
};

class PlaybackSession final {
  public:
    PlaybackSession() noexcept;
    ~PlaybackSession();

    PlaybackSession(const PlaybackSession&) = delete;
    auto operator=(const PlaybackSession&) -> PlaybackSession& = delete;
    PlaybackSession(PlaybackSession&&) noexcept;
    auto operator=(PlaybackSession&&) noexcept -> PlaybackSession&;

    [[nodiscard]] auto state() const noexcept -> SessionState;

    [[nodiscard]] auto loadChart(std::string_view jsonText) -> core::Result<void>;

    [[nodiscard]] auto update(const RuntimeFrame& frame) -> core::Result<void>;
    [[nodiscard]] auto extractFrame(const FrameViewport& viewport) const
        -> core::Result<FrameSnapshot>;

    [[nodiscard]] auto reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                              ReloadPolicy policy) -> core::Result<void>;
    [[nodiscard]] auto unload() -> core::Result<void>;

    [[nodiscard]] auto chartInfo() const noexcept -> core::Result<ChartInfo>;
    [[nodiscard]] auto diagnostics() const noexcept -> const core::Diagnostics&;

  private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace cuexis::playback
