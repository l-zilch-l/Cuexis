#pragma once

// Playback prepare and the formal frame loop. This file has no SDL or OpenGL types.

#include "player_audio_seat.hpp"
#include "player_options.hpp"
#include "player_surface.hpp"

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/runtime_timeline.hpp>
#include <cuexis/player_support/resolved_config.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace cuexis::player {

class FrameDiagnostics;
class PlayerLogger;

struct PreparedPlayerContent final {
    playback::PreparedPlayback prepared;
    playback::RuntimeTimeline timeline;
    playback::ChartClock chartClock;
    playback::PlaybackMode mode{playback::PlaybackMode::ChartClock};
    player_support::ResolvedSessionConfig sessionConfig{};
    double timingOffsetMs{};
};

struct PlayerClockContext final {
    std::uint32_t renderedFrames{};
    playback::PlaybackSession& session;
    playback::RuntimeTimeline& timeline;
    playback::ChartClock& chartClock;
    presentation_renderer::IPresentationRenderer& renderer;
    PlayerAudioSeat* audio{};
    audio::AudioClipStore& audioStore;
    std::optional<audio::AudioClipHandle>& activeAudioHandle;
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
    playback::PlaybackSession& session;
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
    playback::PlaybackSession& session;
    playback::RuntimeTimeline& timeline;
    playback::ChartClock& chartClock;
    presentation_renderer::IPresentationRenderer& renderer;
    PlayerSurface& surface;
    PlayerAudioSeat* audio{};
    audio::AudioClipStore& audioStore;
    std::optional<audio::AudioClipHandle>& activeAudioHandle;
    player_support::ResolvedSessionConfig sessionConfig{};
    bool smokeTest{};
    bool audioSmokeTest{};
    FrameDiagnostics* diagnostics{};
    PlayerHooks hooks{};
    PlayerLogger& logger;
};

[[nodiscard]] auto openConfiguredPlaybackSource(const PlayerOptions& options)
    -> core::Result<playback::PlaybackSource>;

[[nodiscard]] auto preparePlayerContent(playback::PlaybackSession& session,
                                        const PlayerOptions& options,
                                        std::int64_t profileCorrectionUs, PlayerLogger& logger)
    -> core::Result<PreparedPlayerContent>;

[[nodiscard]] auto activatePreparedPlayback(playback::PlaybackSession& session,
                                            playback::PreparedPlayback& prepared,
                                            presentation_renderer::IPresentationRenderer& renderer,
                                            PlayerLogger& logger) -> core::Result<void>;

[[nodiscard]] auto runPlayerFrameLoop(PlayerFrameLoop& loop) -> core::Result<void>;

} // namespace cuexis::player
