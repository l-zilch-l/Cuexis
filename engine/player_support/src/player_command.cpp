#include <cuexis/player_support/player_command.hpp>

#include <cmath>
#include <string>

namespace cuexis::player_support {
namespace {

[[nodiscard]] auto reject(std::string_view code, std::string_view message, bool duplicate = false)
    -> core::Result<PlayerCommandDecision> {
    return core::unexpected(core::Error{std::string{code}, std::string{message}}.withContext(
        "duplicate", duplicate ? "true" : "false"));
}

[[nodiscard]] auto accepted(PlayerAppState state, PlayerCommandEffect effect,
                            PlayerSuccessStatePolicy policy, bool duplicate = false)
    -> core::Result<PlayerCommandDecision> {
    return PlayerCommandDecision{
        .state = state, .duplicate = duplicate, .effect = effect, .successPolicy = policy};
}

[[nodiscard]] auto executed(PlayerAppState state,
                            PlayerSuccessStatePolicy policy = PlayerSuccessStatePolicy::Fixed)
    -> core::Result<PlayerCommandDecision> {
    return accepted(state, PlayerCommandEffect::Execute, policy);
}

[[nodiscard]] auto rejectNotLoaded() -> core::Result<PlayerCommandDecision> {
    return reject("player.command.not_loaded", "No content is loaded");
}

[[nodiscard]] auto rejectFailed() -> core::Result<PlayerCommandDecision> {
    return reject("player.command.failed", "A failed application requires recovery first");
}

} // namespace

auto playerAppStateName(PlayerAppState state) noexcept -> std::string_view {
    switch (state) {
    case PlayerAppState::Empty:
        return "empty";
    case PlayerAppState::Loaded:
        return "loaded";
    case PlayerAppState::Playing:
        return "playing";
    case PlayerAppState::Paused:
        return "paused";
    case PlayerAppState::Stopped:
        return "stopped";
    case PlayerAppState::Failed:
        return "failed";
    }
    return "unknown";
}

auto playerCommandName(PlayerCommandKind kind) noexcept -> std::string_view {
    switch (kind) {
    case PlayerCommandKind::Load:
        return "load";
    case PlayerCommandKind::Play:
        return "play";
    case PlayerCommandKind::Pause:
        return "pause";
    case PlayerCommandKind::Stop:
        return "stop";
    case PlayerCommandKind::Seek:
        return "seek";
    case PlayerCommandKind::Reload:
        return "reload";
    case PlayerCommandKind::Rebuild:
        return "rebuild";
    case PlayerCommandKind::Retry:
        return "retry";
    }
    return "unknown";
}

auto evaluatePlayerCommand(const PlayerCommandContext& context, PlayerCommandKind kind)
    -> core::Result<PlayerCommandDecision> {
    // 1. An unknown kind is rejected before any state or source is inspected.
    switch (kind) {
    case PlayerCommandKind::Load:
    case PlayerCommandKind::Play:
    case PlayerCommandKind::Pause:
    case PlayerCommandKind::Stop:
    case PlayerCommandKind::Seek:
    case PlayerCommandKind::Retry:
    case PlayerCommandKind::Reload:
    case PlayerCommandKind::Rebuild:
        break;
    default:
        return reject("player.command.unknown", "Unknown application command");
    }

    const auto state = context.state;

    // 2. An Empty application that still holds prepared content is inconsistent; it cannot be
    //    repaired by any command in this table.
    if (state == PlayerAppState::Empty && context.hasActiveContent) {
        return reject("player.command.state_inconsistent",
                      "The empty application still holds prepared active content");
    }

    // 3. Reload needs a configured source before the transaction gate: without one it can never
    //    complete, so the missing source is the stable rejection even while a transaction runs.
    if (kind == PlayerCommandKind::Reload && !context.hasConfiguredSource) {
        return reject("player.command.source_unavailable",
                      "No configured source is available for Reload");
    }

    // 4. A transaction owns every overlapping command. A qualified independent Pause is the one
    //    exception: it registers or confirms a pause source and is not rejected by the mutex.
    if (context.transactionInProgress) {
        if (kind == PlayerCommandKind::Pause) {
            switch (context.pauseAdmission) {
            case PlayerPauseAdmission::CurrentRun:
                // The current run freezes as soon as the transaction's serial step completes.
                return accepted(PlayerAppState::Paused, PlayerCommandEffect::RegisterPause,
                                PlayerSuccessStatePolicy::Fixed);
            case PlayerPauseAdmission::PendingNewRun:
                // The source binds to the new run only when the transaction commits, so the
                // current application state is left untouched until that terminal is known.
                return accepted(state, PlayerCommandEffect::RegisterPause,
                                PlayerSuccessStatePolicy::Fixed);
            case PlayerPauseAdmission::AlreadyHeld:
                return accepted(PlayerAppState::Paused, PlayerCommandEffect::AlreadySatisfied,
                                PlayerSuccessStatePolicy::Fixed, true);
            case PlayerPauseAdmission::None:
                break;
            }
        }
        return reject("player.command.transaction_in_progress",
                      "A load, reload, rebuild, or retry transaction already owns the "
                      "application");
    }

    switch (kind) {
    case PlayerCommandKind::Load:
        // Only the internal first installation with a source may load. Content replacement is
        // Reload, and a failed first attempt also continues with Reload.
        if (state != PlayerAppState::Empty || !context.initialLoad) {
            return reject("player.command.load_initial_only",
                          "Load only admits the initial content installation");
        }
        if (!context.hasConfiguredSource) {
            return reject("player.command.source_unavailable",
                          "No configured source is available for the initial Load");
        }
        return executed(PlayerAppState::Loaded);
    case PlayerCommandKind::Play:
        if (state == PlayerAppState::Empty) {
            return rejectNotLoaded();
        }
        if (state == PlayerAppState::Failed) {
            return rejectFailed();
        }
        if (state == PlayerAppState::Playing) {
            return reject("player.command.duplicate", "The transport is already playing", true);
        }
        if (state == PlayerAppState::Paused) {
            // Resuming clears only the requesting reason; the owner keeps Paused while another
            // reason or a missing safety proof remains.
            return executed(PlayerAppState::Playing, PlayerSuccessStatePolicy::ResumeAfterReasons);
        }
        return executed(PlayerAppState::Playing);
    case PlayerCommandKind::Pause:
        if (state == PlayerAppState::Empty) {
            return rejectNotLoaded();
        }
        if (state == PlayerAppState::Failed) {
            return rejectFailed();
        }
        if (state == PlayerAppState::Playing) {
            return accepted(PlayerAppState::Paused, PlayerCommandEffect::RegisterPause,
                            PlayerSuccessStatePolicy::Fixed);
        }
        if (state == PlayerAppState::Paused) {
            // A confirmed source is idempotent and must not start a second freeze. An
            // unclassified request carries no source identity, so it keeps the stable duplicate
            // rejection instead of claiming the source is already held.
            if (context.pauseAdmission == PlayerPauseAdmission::AlreadyHeld) {
                return accepted(PlayerAppState::Paused, PlayerCommandEffect::AlreadySatisfied,
                                PlayerSuccessStatePolicy::Fixed, true);
            }
            if (context.pauseAdmission == PlayerPauseAdmission::None) {
                return reject("player.command.duplicate",
                              "The transport is already paused and the source is not classified",
                              true);
            }
            // A new independent source extends the existing pause.
            return accepted(PlayerAppState::Paused, PlayerCommandEffect::RegisterPause,
                            PlayerSuccessStatePolicy::Fixed);
        }
        return reject("player.command.not_playing", "The transport is not playing");
    case PlayerCommandKind::Stop:
        if (state == PlayerAppState::Empty) {
            return rejectNotLoaded();
        }
        if (state == PlayerAppState::Failed) {
            return rejectFailed();
        }
        if (state == PlayerAppState::Stopped) {
            return reject("player.command.duplicate", "The transport is already stopped", true);
        }
        return executed(PlayerAppState::Stopped);
    case PlayerCommandKind::Seek:
        if (state == PlayerAppState::Empty) {
            return rejectNotLoaded();
        }
        if (state == PlayerAppState::Failed) {
            return rejectFailed();
        }
        if (context.realtimeGameplay) {
            return reject("player.command.seek_gameplay_unsupported",
                          "Ordinary Seek is not available during real-time gameplay");
        }
        // Seeking never changes the transport state; the position is validated by the caller.
        return executed(state);
    case PlayerCommandKind::Retry:
        // Retry reuses prepared active content only; it never re-reads a configured source.
        if (!context.hasActiveContent) {
            return rejectNotLoaded();
        }
        // The owner starts at zero, restores necessary devices first, and applies a pending
        // independent Pause before any playback becomes visible.
        return executed(PlayerAppState::Playing, PlayerSuccessStatePolicy::RetryAfterDevices);
    case PlayerCommandKind::Reload:
        // The source was checked before the transaction gate.
        if (state == PlayerAppState::Empty) {
            return rejectNotLoaded();
        }
        // Reload always commits silently paused at zero on success, including recovery from
        // Failed, and binds any pending pause source to the new content.
        return executed(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused);
    case PlayerCommandKind::Rebuild:
        if (state == PlayerAppState::Empty) {
            return rejectNotLoaded();
        }
        if (state == PlayerAppState::Failed) {
            // Failed without A has nothing to rebuild; Failed with A may clear a presentation
            // cause, but a faulted run or another failure keeps the application Failed.
            if (!context.hasActiveContent) {
                return rejectNotLoaded();
            }
            return executed(PlayerAppState::Failed, PlayerSuccessStatePolicy::RebuildAfterFacts);
        }
        // Rebuilding outside Failed repairs presentation and device state in place.
        return executed(state);
    }
    return reject("player.command.unknown", "Unknown application command");
}

auto observeTransportState(PlayerAppState current, PlayerTransportObservation observed) noexcept
    -> PlayerAppState {
    switch (observed) {
    case PlayerTransportObservation::Error:
        return current == PlayerAppState::Empty ? current : PlayerAppState::Failed;
    case PlayerTransportObservation::Ended:
        // Only a playing application ends. A paused or stopped one keeps its position.
        return current == PlayerAppState::Playing ? PlayerAppState::Stopped : current;
    case PlayerTransportObservation::None:
    case PlayerTransportObservation::Playing:
    case PlayerTransportObservation::Paused:
    case PlayerTransportObservation::Stopped:
        return current;
    }
    return current;
}

auto validateSeekTargetMs(double targetMs, double playableDurationMs) -> core::Result<void> {
    if (!std::isfinite(targetMs)) {
        return core::unexpected(
            core::Error{"player.command.seek_not_finite", "The seek target must be finite"});
    }
    if (targetMs < 0.0) {
        return core::unexpected(
            core::Error{"player.command.seek_negative", "The seek target must not be negative"}
                .withContext("target_ms", std::to_string(targetMs)));
    }
    if (std::isfinite(playableDurationMs) && playableDurationMs >= 0.0 &&
        targetMs > playableDurationMs) {
        return core::unexpected(
            core::Error{"player.command.seek_outside",
                        "The seek target is beyond the playable range"}
                .withContext("target_ms", std::to_string(targetMs))
                .withContext("duration_ms", std::to_string(playableDurationMs)));
    }
    return {};
}

} // namespace cuexis::player_support
