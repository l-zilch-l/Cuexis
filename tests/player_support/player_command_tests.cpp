// Command table and application state transitions. No GPU, window, or audio device is required.

#include <cuexis/player_support/player_command.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <string_view>

namespace {

using cuexis::core::Result;
using cuexis::player_support::PlayerAppState;
using cuexis::player_support::PlayerCommandContext;
using cuexis::player_support::PlayerCommandDecision;
using cuexis::player_support::PlayerCommandKind;
using cuexis::player_support::PlayerTransportObservation;

constexpr std::array<PlayerAppState, 6> allStates{PlayerAppState::Empty,   PlayerAppState::Loaded,
                                                  PlayerAppState::Playing, PlayerAppState::Paused,
                                                  PlayerAppState::Stopped, PlayerAppState::Failed};

constexpr std::array<PlayerCommandKind, 7> allCommands{
    PlayerCommandKind::Load,   PlayerCommandKind::Play, PlayerCommandKind::Pause,
    PlayerCommandKind::Stop,   PlayerCommandKind::Seek, PlayerCommandKind::Reload,
    PlayerCommandKind::Rebuild};

struct Expectation final {
    bool allowed{};
    PlayerAppState state{PlayerAppState::Empty};
    std::string_view code{};
    bool duplicate{};
};

// rows: application state, columns: Load, Play, Pause, Stop, Seek, Reload, Rebuild
constexpr std::array<std::array<Expectation, allCommands.size()>, allStates.size()> matrix{{
    // Empty
    {{{true, PlayerAppState::Loaded, "", false},
      {false, PlayerAppState::Empty, "player.command.not_loaded", false},
      {false, PlayerAppState::Empty, "player.command.not_loaded", false},
      {false, PlayerAppState::Empty, "player.command.not_loaded", false},
      {false, PlayerAppState::Empty, "player.command.not_loaded", false},
      {false, PlayerAppState::Empty, "player.command.not_loaded", false},
      {true, PlayerAppState::Empty, "", false}}},
    // Loaded
    {{{true, PlayerAppState::Loaded, "", false},
      {true, PlayerAppState::Playing, "", false},
      {false, PlayerAppState::Loaded, "player.command.not_playing", false},
      {true, PlayerAppState::Stopped, "", false},
      {true, PlayerAppState::Loaded, "", false},
      {true, PlayerAppState::Loaded, "", false},
      {true, PlayerAppState::Loaded, "", false}}},
    // Playing
    {{{true, PlayerAppState::Loaded, "", false},
      {false, PlayerAppState::Playing, "player.command.duplicate", true},
      {true, PlayerAppState::Paused, "", false},
      {true, PlayerAppState::Stopped, "", false},
      {true, PlayerAppState::Playing, "", false},
      {true, PlayerAppState::Playing, "", false},
      {true, PlayerAppState::Playing, "", false}}},
    // Paused
    {{{true, PlayerAppState::Loaded, "", false},
      {true, PlayerAppState::Playing, "", false},
      {false, PlayerAppState::Paused, "player.command.duplicate", true},
      {true, PlayerAppState::Stopped, "", false},
      {true, PlayerAppState::Paused, "", false},
      {true, PlayerAppState::Paused, "", false},
      {true, PlayerAppState::Paused, "", false}}},
    // Stopped
    {{{true, PlayerAppState::Loaded, "", false},
      {true, PlayerAppState::Playing, "", false},
      {false, PlayerAppState::Stopped, "player.command.not_playing", false},
      {false, PlayerAppState::Stopped, "player.command.duplicate", true},
      {true, PlayerAppState::Stopped, "", false},
      {true, PlayerAppState::Stopped, "", false},
      {true, PlayerAppState::Stopped, "", false}}},
    // Failed
    {{{true, PlayerAppState::Loaded, "", false},
      {false, PlayerAppState::Failed, "player.command.failed", false},
      {false, PlayerAppState::Failed, "player.command.failed", false},
      {false, PlayerAppState::Failed, "player.command.failed", false},
      {false, PlayerAppState::Failed, "player.command.failed", false},
      {false, PlayerAppState::Failed, "player.command.failed", false},
      {true, PlayerAppState::Loaded, "", false}}},
}};

[[nodiscard]] auto evaluate(PlayerAppState state, PlayerCommandKind command, bool busy = false)
    -> Result<PlayerCommandDecision> {
    return cuexis::player_support::evaluatePlayerCommand(
        PlayerCommandContext{.state = state, .transactionInProgress = busy}, command);
}

} // namespace

TEST_CASE("Every application state and command pair follows the published table",
          "[player][command]") {
    for (std::size_t stateIndex = 0; stateIndex < allStates.size(); ++stateIndex) {
        for (std::size_t commandIndex = 0; commandIndex < allCommands.size(); ++commandIndex) {
            const auto& expected = matrix[stateIndex][commandIndex];
            const auto state = allStates[stateIndex];
            const auto command = allCommands[commandIndex];
            const auto result = evaluate(state, command);
            INFO("state=" << cuexis::player_support::playerAppStateName(state)
                          << " command=" << cuexis::player_support::playerCommandName(command));
            REQUIRE(result.has_value() == expected.allowed);
            if (expected.allowed) {
                CHECK(result->state == expected.state);
                CHECK_FALSE(result->duplicate);
            } else {
                CHECK(result.error().code() == expected.code);
                bool foundDuplicate = false;
                for (const auto& entry : result.error().context()) {
                    if (entry.key == "duplicate") {
                        foundDuplicate = entry.value == (expected.duplicate ? "true" : "false");
                    }
                }
                CHECK(foundDuplicate);
            }
        }
    }
}

TEST_CASE("A transaction in progress rejects every command", "[player][command]") {
    for (const auto state : allStates) {
        for (const auto command : allCommands) {
            const auto result = evaluate(state, command, true);
            INFO("state=" << cuexis::player_support::playerAppStateName(state)
                          << " command=" << cuexis::player_support::playerCommandName(command));
            REQUIRE_FALSE(result.has_value());
            CHECK(result.error().code() == "player.command.transaction_in_progress");
        }
    }
}

TEST_CASE("Duplicate commands are reported as duplicates, not as invalid transitions",
          "[player][command]") {
    const auto playing = evaluate(PlayerAppState::Playing, PlayerCommandKind::Play);
    REQUIRE_FALSE(playing.has_value());
    CHECK(playing.error().code() == "player.command.duplicate");

    const auto paused = evaluate(PlayerAppState::Paused, PlayerCommandKind::Pause);
    REQUIRE_FALSE(paused.has_value());
    CHECK(paused.error().code() == "player.command.duplicate");

    const auto stopped = evaluate(PlayerAppState::Stopped, PlayerCommandKind::Stop);
    REQUIRE_FALSE(stopped.has_value());
    CHECK(stopped.error().code() == "player.command.duplicate");

    const auto pausedPlay = evaluate(PlayerAppState::Paused, PlayerCommandKind::Play);
    REQUIRE(pausedPlay.has_value());
    CHECK(pausedPlay->state == PlayerAppState::Playing);
}

TEST_CASE("Reload and Seek keep the application state and never change the mode",
          "[player][command]") {
    for (const auto state : {PlayerAppState::Loaded, PlayerAppState::Playing,
                             PlayerAppState::Paused, PlayerAppState::Stopped}) {
        const auto reload = evaluate(state, PlayerCommandKind::Reload);
        REQUIRE(reload.has_value());
        CHECK(reload->state == state);

        const auto seek = evaluate(state, PlayerCommandKind::Seek);
        REQUIRE(seek.has_value());
        CHECK(seek->state == state);
    }
}

TEST_CASE("Only Rebuild and Load leave the Failed state", "[player][command]") {
    const auto rebuild = evaluate(PlayerAppState::Failed, PlayerCommandKind::Rebuild);
    REQUIRE(rebuild.has_value());
    CHECK(rebuild->state == PlayerAppState::Loaded);

    const auto load = evaluate(PlayerAppState::Failed, PlayerCommandKind::Load);
    REQUIRE(load.has_value());
    CHECK(load->state == PlayerAppState::Loaded);

    for (const auto command :
         {PlayerCommandKind::Play, PlayerCommandKind::Pause, PlayerCommandKind::Stop,
          PlayerCommandKind::Seek, PlayerCommandKind::Reload}) {
        const auto rejected = evaluate(PlayerAppState::Failed, command);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "player.command.failed");
    }
}

TEST_CASE("Transport observations only end a playing application", "[player][command]") {
    using cuexis::player_support::observeTransportState;

    for (const auto state : allStates) {
        for (const auto observed :
             {PlayerTransportObservation::None, PlayerTransportObservation::Playing,
              PlayerTransportObservation::Paused, PlayerTransportObservation::Stopped}) {
            CHECK(observeTransportState(state, observed) == state);
        }
    }

    // An audio track that reaches EOF stops a playing application and nothing else.
    CHECK(observeTransportState(PlayerAppState::Playing, PlayerTransportObservation::Ended) ==
          PlayerAppState::Stopped);
    for (const auto state :
         {PlayerAppState::Loaded, PlayerAppState::Paused, PlayerAppState::Stopped}) {
        CHECK(observeTransportState(state, PlayerTransportObservation::Ended) == state);
    }

    // A transport error fails every non-empty state, including an already failed one.
    for (const auto state :
         {PlayerAppState::Loaded, PlayerAppState::Playing, PlayerAppState::Paused,
          PlayerAppState::Stopped, PlayerAppState::Failed}) {
        CHECK(observeTransportState(state, PlayerTransportObservation::Error) ==
              PlayerAppState::Failed);
    }
    CHECK(observeTransportState(PlayerAppState::Empty, PlayerTransportObservation::Error) ==
          PlayerAppState::Empty);
}

TEST_CASE("Seek targets are finite, non-negative, and never clamped", "[player][command]") {
    using cuexis::player_support::validateSeekTargetMs;

    CHECK(validateSeekTargetMs(0.0, 1000.0).has_value());
    CHECK(validateSeekTargetMs(1000.0, 1000.0).has_value());
    // An unknown duration only bounds the target below.
    CHECK(validateSeekTargetMs(5000.0, -1.0).has_value());

    const auto negative = validateSeekTargetMs(-0.001, 1000.0);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().code() == "player.command.seek_negative");

    const auto outside = validateSeekTargetMs(1000.001, 1000.0);
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().code() == "player.command.seek_outside");

    const auto notFinite = validateSeekTargetMs(std::numeric_limits<double>::infinity(), 1000.0);
    REQUIRE_FALSE(notFinite.has_value());
    CHECK(notFinite.error().code() == "player.command.seek_not_finite");
}
