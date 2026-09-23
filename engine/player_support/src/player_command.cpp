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

[[nodiscard]] auto accepted(PlayerAppState state, bool duplicate = false)
    -> core::Result<PlayerCommandDecision> {
    return PlayerCommandDecision{.state = state, .duplicate = duplicate};
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
    }
    return "unknown";
}

auto evaluatePlayerCommand(const PlayerCommandContext& context, PlayerCommandKind kind)
    -> core::Result<PlayerCommandDecision> {
    if (context.transactionInProgress) {
        return reject("player.command.transaction_in_progress",
                      "A load, reload, or rebuild transaction already owns the application");
    }
    const auto state = context.state;
    switch (kind) {
    case PlayerCommandKind::Load:
        // Explicit replacement. The mode is re-resolved from the configured source, so this is
        // also the entry that switches between content with and without an audio track.
        return accepted(PlayerAppState::Loaded);
    case PlayerCommandKind::Rebuild:
        // The only recovery from Failed; other states keep their position.
        return accepted(state == PlayerAppState::Failed ? PlayerAppState::Loaded : state);
    case PlayerCommandKind::Play:
        if (state == PlayerAppState::Empty) {
            return reject("player.command.not_loaded", "No content is loaded");
        }
        if (state == PlayerAppState::Failed) {
            return reject("player.command.failed", "A failed application requires Rebuild or Load");
        }
        if (state == PlayerAppState::Playing) {
            return reject("player.command.duplicate", "The transport is already playing", true);
        }
        return accepted(PlayerAppState::Playing);
    case PlayerCommandKind::Pause:
        if (state == PlayerAppState::Empty) {
            return reject("player.command.not_loaded", "No content is loaded");
        }
        if (state == PlayerAppState::Failed) {
            return reject("player.command.failed", "A failed application requires Rebuild or Load");
        }
        if (state == PlayerAppState::Paused) {
            return reject("player.command.duplicate", "The transport is already paused", true);
        }
        if (state != PlayerAppState::Playing) {
            return reject("player.command.not_playing", "The transport is not playing");
        }
        return accepted(PlayerAppState::Paused);
    case PlayerCommandKind::Stop:
        if (state == PlayerAppState::Empty) {
            return reject("player.command.not_loaded", "No content is loaded");
        }
        if (state == PlayerAppState::Failed) {
            return reject("player.command.failed", "A failed application requires Rebuild or Load");
        }
        if (state == PlayerAppState::Stopped) {
            return reject("player.command.duplicate", "The transport is already stopped", true);
        }
        return accepted(PlayerAppState::Stopped);
    case PlayerCommandKind::Seek:
        // Seeking while paused is explicitly allowed and keeps the paused state.
        if (state == PlayerAppState::Empty) {
            return reject("player.command.not_loaded", "No content is loaded");
        }
        if (state == PlayerAppState::Failed) {
            return reject("player.command.failed", "A failed application requires Rebuild or Load");
        }
        return accepted(state);
    case PlayerCommandKind::Reload:
        // Reload never changes PlaybackMode and never changes the transport position.
        if (state == PlayerAppState::Empty) {
            return reject("player.command.not_loaded", "No content is loaded");
        }
        if (state == PlayerAppState::Failed) {
            return reject("player.command.failed", "A failed application requires Rebuild or Load");
        }
        return accepted(state);
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
