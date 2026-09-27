#pragma once

// Application command layer, content transactions, and the formal frame loop.
// This file has no SDL or OpenGL types: the control layer drives PlaybackSession,
// IPresentationRenderer, and the backend-neutral audio seat only.

#include "player_audio_seat.hpp"
#include "player_options.hpp"
#include "player_surface.hpp"

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/runtime_timeline.hpp>
#include <cuexis/player_support/player_command.hpp>
#include <cuexis/player_support/resolved_config.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace cuexis::player {

class FrameDiagnostics;
class PlayerLogger;

// One application command. Every control surface, including the smoke scripts, produces this
// value and hands it to PlayerController::apply. Every member carries a default initializer so a
// designated initializer that names only the interesting members stays warning-free under GCC's
// -Wmissing-field-initializers.
struct PlayerCommand final {
    player_support::PlayerCommandKind kind{player_support::PlayerCommandKind::Load};
    double seekTargetMs{};
    player_support::PlayerReloadPolicy reloadPolicy{
        player_support::PlayerReloadPolicy::KeepChartTime};
    // Explicit replacement source. When empty the controller asks its configured source port.
    std::optional<playback::PlaybackSource> source{};
    // Explicit target mode for Load. When empty the controller resolves the mode from the source
    // instead of guessing from a failed load.
    std::optional<playback::PlaybackMode> mode{};
};

// Operations the control layer cannot implement itself. The assembly layer supplies them.
struct PlayerControlPorts final {
    std::function<core::Result<playback::PlaybackSource>()> makeSource;
    std::function<core::Result<audio::AudioClipHandle>(playback::PreparedPlayback&,
                                                       audio::AudioClipStore&)>
        prepareClip;
    PlayerAudioOpener openAudio;
};

// Distance one relative seek key moves the chart time. The controller clamps the result at zero
// because a negative chart time is never a legal target.
inline constexpr double playerSeekStepMs = 5000.0;

// Stable name for a user action, used by the input trace and by tests.
[[nodiscard]] auto playerInputActionName(PlayerInputAction action) -> std::string_view;

// Translates one user action into the command that carries it. The action is state independent, so
// PlayPause resolves against the current state and every other action produces the same command in
// every state. The command table, not this function, decides whether the command is allowed.
[[nodiscard]] auto playerCommandForInput(PlayerInputAction action,
                                         player_support::PlayerAppState state,
                                         double currentChartTimeMs) -> PlayerCommand;

// Owns the active content bundle, the application state, and the fixed transaction order for
// load, reload, and rebuild. Every command runs on the owner thread and at most one transaction
// owns the application at a time.
class PlayerController final {
  public:
    PlayerController(presentation_renderer::IPresentationRenderer& renderer,
                     audio::AudioClipStore& audioStore, double gain,
                     std::int64_t profileCorrectionUs, PlayerControlPorts ports,
                     PlayerLogger& logger);
    ~PlayerController();

    PlayerController(const PlayerController&) = delete;
    auto operator=(const PlayerController&) -> PlayerController& = delete;
    PlayerController(PlayerController&&) = delete;
    auto operator=(PlayerController&&) -> PlayerController& = delete;

    // The single command entry for every control surface. The command is taken by value because an
    // explicit source is move-only.
    [[nodiscard]] auto apply(PlayerCommand command) -> core::Result<void>;

    // Folds an observed audio transport state into the application state.
    void observeTransport(const audio::SourceClockSample& sample) noexcept;
    // Records the frame the active bundle last produced; a reload uses it as its target.
    void noteRuntimeFrame(const playback::RuntimeFrame& frame) noexcept;
    // Publishes a failure raised outside a transaction, such as a present error after a commit.
    void markFailed(std::string_view reason);

    [[nodiscard]] auto state() const noexcept -> player_support::PlayerAppState;
    // Valid only while state() is not Empty, which is the precondition for running the frame loop.
    [[nodiscard]] auto session() -> playback::PlaybackSession&;
    [[nodiscard]] auto timeline() -> playback::RuntimeTimeline&;
    [[nodiscard]] auto chartClock() -> playback::ChartClock&;
    [[nodiscard]] auto mode() const noexcept -> playback::PlaybackMode;
    [[nodiscard]] auto sessionConfig() const noexcept
        -> const player_support::ResolvedSessionConfig&;
    [[nodiscard]] auto timingOffsetMs() const noexcept -> double;
    [[nodiscard]] auto audio() noexcept -> PlayerAudioSeat*;
    [[nodiscard]] auto activeAudioHandle() const noexcept -> std::optional<audio::AudioClipHandle>;
    [[nodiscard]] auto lastRuntimeFrame() const noexcept -> const playback::RuntimeFrame&;

    // A chart-clock seek target waiting for the next frame. Empty when there is nothing to apply.
    [[nodiscard]] auto consumeChartSeekTarget() noexcept -> std::optional<double>;
    // True once after a transaction replaced the active bundle, so the loop re-samples the clock
    // and re-advances before extracting the frame.
    [[nodiscard]] auto consumeBundleReplaced() noexcept -> bool;

    // Unloads audio, the active clip, and the session in the documented close order.
    [[nodiscard]] auto shutdown() -> core::Result<void>;

  private:
    [[nodiscard]] auto runLoad(PlayerCommand& command) -> core::Result<void>;
    [[nodiscard]] auto runReload(PlayerCommand& command) -> core::Result<void>;
    [[nodiscard]] auto runRebuild(PlayerCommand& command) -> core::Result<void>;
    [[nodiscard]] auto runPlay() -> core::Result<void>;
    [[nodiscard]] auto runPause() -> core::Result<void>;
    [[nodiscard]] auto runStop() -> core::Result<void>;
    [[nodiscard]] auto runSeek(double targetMs) -> core::Result<void>;

    [[nodiscard]] auto sourceFor(PlayerCommand& command) -> core::Result<playback::PlaybackSource>;
    [[nodiscard]] auto freezeConfig(playback::PlaybackMode mode) const
        -> player_support::ResolvedSessionConfig;
    // Steps 1 to 6 of the fixed transaction. `session` becomes the active bundle on success.
    [[nodiscard]] auto commitBundle(playback::PlaybackSession& session,
                                    std::unique_ptr<playback::PlaybackSession> adopt,
                                    playback::PreparedPlayback&& prepared,
                                    playback::PlaybackMode mode,
                                    player_support::ResolvedSessionConfig config,
                                    bool restartTimeline) -> core::Result<void>;
    void discardClip(const std::optional<audio::AudioClipHandle>& clip);
    void publishFailure(std::string_view reason) noexcept;
    [[nodiscard]] auto seekChartClock(double targetMs) -> core::Result<void>;
    [[nodiscard]] auto seekAudio(double targetMs) -> core::Result<void>;

    presentation_renderer::IPresentationRenderer& renderer_;
    audio::AudioClipStore& audioStore_;
    double gain_{1.0};
    std::int64_t profileCorrectionUs_{};
    PlayerControlPorts ports_;
    PlayerLogger& logger_;

    player_support::PlayerAppState state_{player_support::PlayerAppState::Empty};
    bool transactionInProgress_{};
    bool bundleReplaced_{};
    playback::PlaybackMode activeMode_{playback::PlaybackMode::ChartClock};
    player_support::ResolvedSessionConfig sessionConfig_{};
    double timingOffsetMs_{};
    std::unique_ptr<playback::PlaybackSession> session_;
    std::optional<playback::RuntimeTimeline> timeline_;
    std::optional<playback::ChartClock> chartClock_;
    std::unique_ptr<PlayerAudioSeat> audioSeat_;
    std::optional<audio::AudioClipHandle> activeClip_;
    playback::RuntimeFrame lastFrame_{};
    double lastSourcePositionMs_{};
    // Chart time a later Play resumes from. Pause freezes it, Stop rewinds it to zero, and Seek
    // sets it to the requested target.
    double chartResumeMs_{};
    std::optional<double> pendingChartSeekMs_;
};

struct PlayerClockContext final {
    std::uint32_t renderedFrames{};
    PlayerController& controller;
    presentation_renderer::IPresentationRenderer& renderer;
    core::Result<playback::RuntimeFrame>& runtimeFrame;
    audio::AudioClockSnapshot& audioClock;
    PlayerLogger& logger;
};

struct PlayerPresentedFrame final {
    std::uint32_t renderedFrames{};
    const playback::FrameSnapshot& snapshot;
    const presentation_renderer::DrawSummary& summary;
    double renderMicroseconds{};
    presentation_renderer::IPresentationRenderer& renderer;
    PlayerController& controller;
};

struct PlayerHooks final {
    std::function<core::Result<void>(std::uint32_t renderedFrames)> beforeAudioService;
    std::function<core::Result<void>(PlayerClockContext& context)> afterTimelineAdvance;
    std::function<core::Result<void>(const playback::FrameSnapshot& snapshot,
                                     std::uint32_t renderedFrames)>
        beforeSubmit;
    std::function<core::Result<void>(const PlayerPresentedFrame& presented)> notePresented;
    std::function<core::Result<void>(const PlayerPresentedFrame& presented)> validatePresented;
};

struct PlayerFrameLoop final {
    PlayerController& controller;
    presentation_renderer::IPresentationRenderer& renderer;
    PlayerSurface& surface;
    bool smokeTest{};
    bool audioSmokeTest{};
    FrameDiagnostics* diagnostics{};
    PlayerHooks hooks{};
    PlayerLogger& logger;
};

[[nodiscard]] auto openConfiguredPlaybackSource(const PlayerOptions& options)
    -> core::Result<playback::PlaybackSource>;

[[nodiscard]] auto runPlayerFrameLoop(PlayerFrameLoop& loop) -> core::Result<void>;

} // namespace cuexis::player
