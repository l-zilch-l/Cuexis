#pragma once

// Application command table for the reference Player. This internal support library must not
// include Playback, SDL, OpenGL, AudioSDL, or JSON DOM types; the Player control layer maps
// these commands onto PlaybackSession, IPresentationRenderer, and the audio seat.
//
// The table owns the application state only. The public playback::SessionState enum is
// unchanged and is not extended by this layer.

#include <cuexis/core/result.hpp>

#include <cstdint>
#include <string_view>

namespace cuexis::player_support {

// Application state owned by the Player. Loaded, Playing, Paused, and Stopped are not
// distinguishable in playback::SessionState.
enum class PlayerAppState : std::uint8_t {
    Empty = 1,
    Loaded = 2,
    Playing = 3,
    Paused = 4,
    Stopped = 5,
    Failed = 6,
};

// The formal user entry points. Load replaces the active bundle and re-resolves the mode from
// the configured source; Reload keeps the active PlaybackMode; Rebuild rebuilds the renderer
// and the audio device and is the only recovery from Failed.
enum class PlayerCommandKind : std::uint8_t {
    Load = 1,
    Play = 2,
    Pause = 3,
    Stop = 4,
    Seek = 5,
    Reload = 6,
    Rebuild = 7,
};

// Reload policy for the application layer. Mirrors playback::ReloadPolicy without depending on
// the Playback headers.
enum class PlayerReloadPolicy : std::uint8_t {
    KeepChartTime = 1,
    RestartAtZero = 2,
};

// Transport state observed from the audio device, folded into the application state. Kept
// neutral so the state machine stays free of the audio module.
enum class PlayerTransportObservation : std::uint8_t {
    None = 0,
    Playing = 1,
    Paused = 2,
    Stopped = 3,
    Ended = 4,
    Error = 5,
};

struct PlayerCommandContext final {
    PlayerAppState state{PlayerAppState::Empty};
    // True while a load, reload, or rebuild transaction owns the application. Stage 6 prepares
    // synchronously on the owner thread, so this flag rejects re-entrant commands only.
    bool transactionInProgress{};
};

struct PlayerCommandDecision final {
    PlayerAppState state{PlayerAppState::Empty};
    // True when the command repeats the state the application already holds. The command is
    // still rejected; callers use this to report a duplicate instead of an invalid transition.
    bool duplicate{};
};

[[nodiscard]] auto playerAppStateName(PlayerAppState state) noexcept -> std::string_view;
[[nodiscard]] auto playerCommandName(PlayerCommandKind kind) noexcept -> std::string_view;

// Pure transition table. Returns the application state to publish, or a stable rejection:
//   player.command.transaction_in_progress  a transaction already owns the application
//   player.command.not_loaded               the command needs committed content
//   player.command.not_playing              the command needs an active transport
//   player.command.failed                   only Rebuild or Load can leave Failed
//   player.command.duplicate                the command is already satisfied
[[nodiscard]] auto evaluatePlayerCommand(const PlayerCommandContext& context,
                                         PlayerCommandKind kind)
    -> core::Result<PlayerCommandDecision>;

// Folds an observed transport state into the application state. A transport that ended while the
// application was playing stops the application; an error fails it. Every other observation is
// ignored, so a ChartClock session without an audio track never stops for a missing length.
[[nodiscard]] auto observeTransportState(PlayerAppState current,
                                         PlayerTransportObservation observed) noexcept
    -> PlayerAppState;

// Seek target policy. The target must be finite and non-negative. A negative duration means the
// playable duration is unknown and only the lower bound applies; otherwise the target must not
// exceed it. The target is never clamped.
[[nodiscard]] auto validateSeekTargetMs(double targetMs, double playableDurationMs)
    -> core::Result<void>;

} // namespace cuexis::player_support
