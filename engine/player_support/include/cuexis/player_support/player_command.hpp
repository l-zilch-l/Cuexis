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

// The formal user entry points. Load is only the initial installation from a configured source;
// Reload keeps the active PlaybackMode and starts paused at zero; Retry reuses the prepared active
// content from zero; Rebuild repairs presentation and device state without re-reading content.
enum class PlayerCommandKind : std::uint8_t {
    Load = 1,
    Play = 2,
    Pause = 3,
    Stop = 4,
    Seek = 5,
    Reload = 6,
    Rebuild = 7,
    Retry = 8,
};

// What the owner must do with an accepted command. Execute performs the command's own work;
// RegisterPause records a pause source before any freeze receipt; AlreadySatisfied means the
// requested pause source is already held and no second freeze may be started.
enum class PlayerCommandEffect : std::uint8_t {
    Execute = 1,
    RegisterPause = 2,
    AlreadySatisfied = 3,
};

// How the owner derives the published success state from the latest facts at commit time. Only
// Fixed publishes PlayerCommandDecision::state as soon as the command's own work succeeds.
enum class PlayerSuccessStatePolicy : std::uint8_t {
    Fixed = 1,
    RetryAfterDevices = 2,
    ReloadPaused = 3,
    ResumeAfterReasons = 4,
    RebuildAfterFacts = 5,
};

// Classification of a Pause request supplied by the owner. None means the request carries no
// classified pause source. CurrentRun is a new source for the active run, PendingNewRun is a
// source accepted while an accepted Retry or Reload transaction owns the application, and
// AlreadyHeld is a source that is already registered and confirmed for the current run.
enum class PlayerPauseAdmission : std::uint8_t {
    None = 0,
    CurrentRun = 1,
    PendingNewRun = 2,
    AlreadyHeld = 3,
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
    // True while a load, reload, rebuild, or retry transaction owns the application. A qualified
    // independent Pause is the only command admitted while this flag is set.
    bool transactionInProgress{};
    // True when the application holds prepared active content (A). Failed with and without A
    // admit different commands.
    bool hasActiveContent{};
    // True when the process holds a locked source for the initial Load, Reload, or Rebuild.
    bool hasConfiguredSource{};
    // True until the first Load is accepted. A rejected initial Load does not consume it.
    bool initialLoad{};
    // Real-time gameplay violates the ordinary Seek domain even when a seek position exists.
    bool realtimeGameplay{};
    // Owner classification of a Pause request; meaningless for every other command.
    PlayerPauseAdmission pauseAdmission{PlayerPauseAdmission::None};
};

struct PlayerCommandDecision final {
    PlayerAppState state{PlayerAppState::Empty};
    // True when the command repeats the state or pause source the application already holds. A
    // rejected command reports this through its "duplicate" error context instead.
    bool duplicate{};
    // Owner action required by this decision. RegisterPause and AlreadySatisfied never publish a
    // new application state on their own.
    PlayerCommandEffect effect{PlayerCommandEffect::Execute};
    // Success state policy. The owner reads the latest device, pause, and fault facts at commit
    // time instead of publishing `state` unconditionally.
    PlayerSuccessStatePolicy successPolicy{PlayerSuccessStatePolicy::Fixed};
};

[[nodiscard]] auto playerAppStateName(PlayerAppState state) noexcept -> std::string_view;
[[nodiscard]] auto playerCommandName(PlayerCommandKind kind) noexcept -> std::string_view;

// Pure admission and policy selection. Returns the state and policy the owner must establish, or
// a stable rejection:
//   player.command.unknown                    the command kind is not enumerated
//   player.command.state_inconsistent         Empty holds active content (A)
//   player.command.source_unavailable         Reload or initial Load has no configured source
//   player.command.transaction_in_progress    another transaction owns the application
//   player.command.load_initial_only          Load is only the initial installation
//   player.command.not_loaded                 the command needs committed content
//   player.command.not_playing                Pause needs an active transport
//   player.command.failed                     a Failed application rejects the command
//   player.command.duplicate                  the command is already satisfied and is rejected
//   player.command.seek_gameplay_unsupported  ordinary Seek is rejected in real-time gameplay
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
