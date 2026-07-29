#pragma once

//  PlaybackSession - the single playback facade for host integration
//  Wraps the internal RuntimeSession lifecycle, hiding World, EnTT and backend specifics
//  The host owns the main loop, the window and the render backend; PlaybackSession only
//  consumes RuntimeFrame and emits FrameSnapshot
//  Judgement and replay interfaces arrive in phase 11 via cuexis_judgement; PlaybackSession
//  reserves the extension points for them
//  This public header exposes no entt, SDL, OpenGL, JSON DOM or internal Runtime types

#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::playback {

CUEXIS_ABI_WARNING_PUSH

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

enum class PlaybackMode {
    ChartClock,
    HostClock,
    CuexisAudio,
};

struct PlaybackContentInfo final {
    std::string chartId;
    std::uint32_t chartFormatVersion{1};
    double timingOffsetMs{};
    PlaybackMode mode{PlaybackMode::ChartClock};
    std::optional<std::string> mainMusicAssetId;
};

struct MainMusicSourceView final {
    std::string_view assetId;
    double timingOffsetMs{};
    std::uint64_t contentRevision{};
    std::span<const std::byte> bytes;
};

struct FrameSnapshot final {
    struct ObjectSnapshot final {
        std::string id;
        float worldMatrix[16]{}; // column-major Mat4 flattened
        bool hasTransform{};
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
    std::size_t renderableCount{};
    std::size_t resourceCount{};
};

enum class SessionState {
    Empty,
    Ready,
    Running,
    Failed,
};

class PlaybackSession;

class CUEXIS_PLAYBACK_API PreparedPlayback final {
  public:
    PreparedPlayback() noexcept;
    ~PreparedPlayback();

    PreparedPlayback(const PreparedPlayback&) = delete;
    auto operator=(const PreparedPlayback&) -> PreparedPlayback& = delete;
    PreparedPlayback(PreparedPlayback&& other) noexcept;
    auto operator=(PreparedPlayback&& other) noexcept -> PreparedPlayback&;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] const PlaybackContentInfo* contentInfo() const noexcept;
    [[nodiscard]] std::optional<MainMusicSourceView> mainMusicSource() const noexcept;

  private:
    friend class PlaybackSession;
    struct State;
    explicit PreparedPlayback(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> state_;
};

class CUEXIS_PLAYBACK_API PlaybackSession final {
  public:
    PlaybackSession() noexcept;
    ~PlaybackSession();

    PlaybackSession(const PlaybackSession&) = delete;
    auto operator=(const PlaybackSession&) -> PlaybackSession& = delete;
    PlaybackSession(PlaybackSession&&) = delete;
    auto operator=(PlaybackSession&&) -> PlaybackSession& = delete;

    [[nodiscard]] auto state() const -> core::Result<SessionState>;

    [[nodiscard]] auto prepareLoad(std::string_view jsonText, PlaybackMode mode)
        -> core::Result<PreparedPlayback>;
    [[nodiscard]] auto prepareLoad(PlaybackSource&& source, PlaybackMode mode)
        -> core::Result<PreparedPlayback>;
    [[nodiscard]] auto prepareReload(std::string_view replacementJson,
                                     const RuntimeFrame& targetFrame, ReloadPolicy policy)
        -> core::Result<PreparedPlayback>;
    [[nodiscard]] auto prepareReload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                                     ReloadPolicy policy) -> core::Result<PreparedPlayback>;
    [[nodiscard]] auto commit(PreparedPlayback&& prepared) -> core::Result<void>;

    [[nodiscard]] auto load(PlaybackSource&& source, PlaybackMode mode) -> core::Result<void>;
    [[nodiscard]] auto loadChart(std::string_view jsonText) -> core::Result<void>;

    [[nodiscard]] auto update(const RuntimeFrame& frame) -> core::Result<void>;
    [[nodiscard]] auto extractFrame(const FrameViewport& viewport) const
        -> core::Result<FrameSnapshot>;
    [[nodiscard]] auto extractFrame(const FrameViewport& viewport, FrameSnapshot& destination) const
        -> core::Result<void>;

    [[nodiscard]] auto reload(std::string_view replacementJson, const RuntimeFrame& targetFrame,
                              ReloadPolicy policy) -> core::Result<void>;
    [[nodiscard]] auto reload(PlaybackSource&& replacement, const RuntimeFrame& targetFrame,
                              ReloadPolicy policy) -> core::Result<void>;
    [[nodiscard]] auto unload() -> core::Result<void>;

    [[nodiscard]] auto chartInfo() const -> core::Result<ChartInfo>;
    [[nodiscard]] auto contentInfo() const -> core::Result<PlaybackContentInfo>;
    [[nodiscard]] auto diagnostics() const -> core::Result<core::Diagnostics>;
    [[nodiscard]] auto lastOperationDiagnostics() const -> core::Result<core::Diagnostics>;

  private:
    [[nodiscard]] auto prepare(PlaybackSource&& source, PlaybackMode mode,
                               const RuntimeFrame* targetFrame, ReloadPolicy policy,
                               bool replacement) -> core::Result<PreparedPlayback>;

    struct State;
    std::unique_ptr<State> state_;
};

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::playback
