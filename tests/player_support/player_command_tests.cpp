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
using cuexis::player_support::PlayerCommandEffect;
using cuexis::player_support::PlayerCommandKind;
using cuexis::player_support::PlayerPauseAdmission;
using cuexis::player_support::PlayerSuccessStatePolicy;
using cuexis::player_support::PlayerTransportObservation;

constexpr std::array<PlayerAppState, 6> allStates{PlayerAppState::Empty,   PlayerAppState::Loaded,
                                                  PlayerAppState::Playing, PlayerAppState::Paused,
                                                  PlayerAppState::Stopped, PlayerAppState::Failed};

// Table order, not enum order: Load, Play, Pause, Stop, Seek, Retry, Reload, Rebuild.
constexpr std::array<PlayerCommandKind, 8> allCommands{
    PlayerCommandKind::Load,   PlayerCommandKind::Play,   PlayerCommandKind::Pause,
    PlayerCommandKind::Stop,   PlayerCommandKind::Seek,   PlayerCommandKind::Retry,
    PlayerCommandKind::Reload, PlayerCommandKind::Rebuild};

constexpr std::string_view kUnknown = "player.command.unknown";
constexpr std::string_view kStateInconsistent = "player.command.state_inconsistent";
constexpr std::string_view kSourceUnavailable = "player.command.source_unavailable";
constexpr std::string_view kTransactionInProgress = "player.command.transaction_in_progress";
constexpr std::string_view kLoadInitialOnly = "player.command.load_initial_only";
constexpr std::string_view kNotLoaded = "player.command.not_loaded";
constexpr std::string_view kNotPlaying = "player.command.not_playing";
constexpr std::string_view kFailed = "player.command.failed";
constexpr std::string_view kDuplicate = "player.command.duplicate";
constexpr std::string_view kSeekGameplayUnsupported = "player.command.seek_gameplay_unsupported";

struct Expectation final {
    bool allowed{};
    PlayerAppState state{PlayerAppState::Empty};
    std::string_view code{};
    bool duplicate{};
    PlayerCommandEffect effect{PlayerCommandEffect::Execute};
    PlayerSuccessStatePolicy policy{PlayerSuccessStatePolicy::Fixed};
};

[[nodiscard]] constexpr auto ok(PlayerAppState state,
                                PlayerSuccessStatePolicy policy = PlayerSuccessStatePolicy::Fixed,
                                PlayerCommandEffect effect = PlayerCommandEffect::Execute,
                                bool duplicate = false) -> Expectation {
    return Expectation{true, state, "", duplicate, effect, policy};
}

[[nodiscard]] constexpr auto no(std::string_view code, bool duplicate = false) -> Expectation {
    return Expectation{false, PlayerAppState::Empty, code, duplicate};
}

// One context per application state. The Paused row carries the already-held pause source so the
// base table exercises the idempotent Pause cell; the admission variants are covered separately.
constexpr std::array<PlayerCommandContext, 6> baseContexts{{
    {.state = PlayerAppState::Empty,
     .transactionInProgress = false,
     .hasActiveContent = false,
     .hasConfiguredSource = true,
     .initialLoad = true,
     .realtimeGameplay = false,
     .pauseAdmission = PlayerPauseAdmission::None},
    {.state = PlayerAppState::Loaded,
     .transactionInProgress = false,
     .hasActiveContent = true,
     .hasConfiguredSource = true,
     .initialLoad = false,
     .realtimeGameplay = false,
     .pauseAdmission = PlayerPauseAdmission::None},
    {.state = PlayerAppState::Playing,
     .transactionInProgress = false,
     .hasActiveContent = true,
     .hasConfiguredSource = true,
     .initialLoad = false,
     .realtimeGameplay = false,
     .pauseAdmission = PlayerPauseAdmission::None},
    {.state = PlayerAppState::Paused,
     .transactionInProgress = false,
     .hasActiveContent = true,
     .hasConfiguredSource = true,
     .initialLoad = false,
     .realtimeGameplay = false,
     .pauseAdmission = PlayerPauseAdmission::AlreadyHeld},
    {.state = PlayerAppState::Stopped,
     .transactionInProgress = false,
     .hasActiveContent = true,
     .hasConfiguredSource = true,
     .initialLoad = false,
     .realtimeGameplay = false,
     .pauseAdmission = PlayerPauseAdmission::None},
    {.state = PlayerAppState::Failed,
     .transactionInProgress = false,
     .hasActiveContent = true,
     .hasConfiguredSource = true,
     .initialLoad = false,
     .realtimeGameplay = false,
     .pauseAdmission = PlayerPauseAdmission::None},
}};

// rows: application state, columns: Load, Play, Pause, Stop, Seek, Retry, Reload, Rebuild.
// `state` is the owner's nominal success state; for the non-Fixed success policies the owner
// substitutes the facts it reads at commit time, so those cells are not device-success evidence.
constexpr auto matrix = std::array{
    // Empty: the initial installation is the only command that can start content.
    std::array{ok(PlayerAppState::Loaded), no(kNotLoaded), no(kNotLoaded), no(kNotLoaded),
               no(kNotLoaded), no(kNotLoaded), no(kNotLoaded), no(kNotLoaded)},
    // Loaded
    std::array{no(kLoadInitialOnly), ok(PlayerAppState::Playing), no(kNotPlaying),
               ok(PlayerAppState::Stopped), ok(PlayerAppState::Loaded),
               ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::RetryAfterDevices),
               ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused),
               ok(PlayerAppState::Loaded)},
    // Playing
    std::array{no(kLoadInitialOnly), no(kDuplicate, true),
               ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::Fixed,
                  PlayerCommandEffect::RegisterPause),
               ok(PlayerAppState::Stopped), ok(PlayerAppState::Playing),
               ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::RetryAfterDevices),
               ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused),
               ok(PlayerAppState::Playing)},
    // Paused: the same pause source is idempotent and Play only clears the requesting reason.
    std::array{no(kLoadInitialOnly),
               ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::ResumeAfterReasons),
               ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::Fixed,
                  PlayerCommandEffect::AlreadySatisfied, true),
               ok(PlayerAppState::Stopped), ok(PlayerAppState::Paused),
               ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::RetryAfterDevices),
               ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused),
               ok(PlayerAppState::Paused)},
    // Stopped
    std::array{no(kLoadInitialOnly), ok(PlayerAppState::Playing), no(kNotPlaying),
               no(kDuplicate, true), ok(PlayerAppState::Stopped),
               ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::RetryAfterDevices),
               ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused),
               ok(PlayerAppState::Stopped)},
    // Failed: active content A decides Retry and Rebuild; the configured source decides Reload.
    std::array{no(kLoadInitialOnly), no(kFailed), no(kFailed), no(kFailed), no(kFailed),
               ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::RetryAfterDevices),
               ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused),
               ok(PlayerAppState::Failed, PlayerSuccessStatePolicy::RebuildAfterFacts)},
};

[[nodiscard]] auto contextFor(PlayerAppState state) -> PlayerCommandContext {
    for (const auto& context : baseContexts) {
        if (context.state == state) {
            return context;
        }
    }
    return {};
}

void checkDecision(const PlayerCommandContext& context, PlayerCommandKind command,
                   const Expectation& expected) {
    const auto result = cuexis::player_support::evaluatePlayerCommand(context, command);
    INFO("state=" << cuexis::player_support::playerAppStateName(context.state)
                  << " command=" << cuexis::player_support::playerCommandName(command));
    REQUIRE(result.has_value() == expected.allowed);
    if (expected.allowed) {
        CHECK(result->state == expected.state);
        CHECK(result->duplicate == expected.duplicate);
        CHECK(result->effect == expected.effect);
        CHECK(result->successPolicy == expected.policy);
        return;
    }
    CHECK(result.error().code() == expected.code);
    bool foundDuplicate = false;
    for (const auto& entry : result.error().context()) {
        if (entry.key == "duplicate") {
            foundDuplicate = entry.value == (expected.duplicate ? "true" : "false");
        }
    }
    CHECK(foundDuplicate);
}

[[nodiscard]] auto evaluate(PlayerAppState state, PlayerCommandKind command)
    -> Result<PlayerCommandDecision> {
    return cuexis::player_support::evaluatePlayerCommand(contextFor(state), command);
}

} // namespace

TEST_CASE("Every application state and command pair follows the published table",
          "[player][command]") {
    SECTION("the six states by eight commands matrix") {
        for (std::size_t stateIndex = 0; stateIndex < baseContexts.size(); ++stateIndex) {
            for (std::size_t commandIndex = 0; commandIndex < allCommands.size(); ++commandIndex) {
                checkDecision(baseContexts[stateIndex], allCommands[commandIndex],
                              matrix[stateIndex][commandIndex]);
            }
        }
    }

    SECTION("Failed depends on whether prepared active content A exists") {
        auto withoutContent = contextFor(PlayerAppState::Failed);
        withoutContent.hasActiveContent = false;

        // Retry reuses A and Rebuild repairs A; neither exists without it. Reload still rebuilds
        // from the configured source, starting paused at zero.
        checkDecision(withoutContent, PlayerCommandKind::Retry, no(kNotLoaded));
        checkDecision(withoutContent, PlayerCommandKind::Rebuild, no(kNotLoaded));
        checkDecision(withoutContent, PlayerCommandKind::Reload,
                      ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused));
        checkDecision(withoutContent, PlayerCommandKind::Load, no(kLoadInitialOnly));
        checkDecision(withoutContent, PlayerCommandKind::Play, no(kFailed));
        checkDecision(withoutContent, PlayerCommandKind::Pause, no(kFailed));
        checkDecision(withoutContent, PlayerCommandKind::Stop, no(kFailed));
        checkDecision(withoutContent, PlayerCommandKind::Seek, no(kFailed));

        // A failed first installation has no content but may still have a source.
        auto withoutContentNoSource = withoutContent;
        withoutContentNoSource.hasConfiguredSource = false;
        checkDecision(withoutContentNoSource, PlayerCommandKind::Reload, no(kSourceUnavailable));
        checkDecision(withoutContentNoSource, PlayerCommandKind::Rebuild, no(kNotLoaded));
    }

    SECTION("a configured source gates Reload in every state and the initial Load") {
        for (const auto state : allStates) {
            auto context = contextFor(state);
            context.hasConfiguredSource = false;
            checkDecision(context, PlayerCommandKind::Reload, no(kSourceUnavailable));
        }
        // The missing source is reported before the transaction mutex.
        auto busy = contextFor(PlayerAppState::Playing);
        busy.hasConfiguredSource = false;
        busy.transactionInProgress = true;
        checkDecision(busy, PlayerCommandKind::Reload, no(kSourceUnavailable));

        auto empty = contextFor(PlayerAppState::Empty);
        empty.hasConfiguredSource = false;
        checkDecision(empty, PlayerCommandKind::Load, no(kSourceUnavailable));
    }

    SECTION("the initial Load attempt is consumed on acceptance") {
        auto initial = contextFor(PlayerAppState::Empty);
        checkDecision(initial, PlayerCommandKind::Load, ok(PlayerAppState::Loaded));

        auto exhausted = initial;
        exhausted.initialLoad = false;
        checkDecision(exhausted, PlayerCommandKind::Load, no(kLoadInitialOnly));

        // A configured source does not reopen Load after the first installation.
        auto loaded = contextFor(PlayerAppState::Loaded);
        loaded.initialLoad = true;
        checkDecision(loaded, PlayerCommandKind::Load, no(kLoadInitialOnly));
    }

    SECTION("real-time gameplay rejects the ordinary Seek domain") {
        for (const auto state : {PlayerAppState::Loaded, PlayerAppState::Playing,
                                 PlayerAppState::Paused, PlayerAppState::Stopped}) {
            auto context = contextFor(state);
            context.realtimeGameplay = true;
            checkDecision(context, PlayerCommandKind::Seek, no(kSeekGameplayUnsupported));
            context.realtimeGameplay = false;
            checkDecision(context, PlayerCommandKind::Seek, ok(state));
        }
        // The state gate still precedes the gameplay-purpose gate.
        auto empty = contextFor(PlayerAppState::Empty);
        empty.realtimeGameplay = true;
        checkDecision(empty, PlayerCommandKind::Seek, no(kNotLoaded));
        auto failed = contextFor(PlayerAppState::Failed);
        failed.realtimeGameplay = true;
        checkDecision(failed, PlayerCommandKind::Seek, no(kFailed));
    }

    SECTION("pause admission distinguishes current, pending, and held sources") {
        // CurrentRun: a new independent source freezes the active run once.
        auto playingCurrent = contextFor(PlayerAppState::Playing);
        playingCurrent.pauseAdmission = PlayerPauseAdmission::CurrentRun;
        checkDecision(playingCurrent, PlayerCommandKind::Pause,
                      ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::Fixed,
                         PlayerCommandEffect::RegisterPause));
        auto pausedCurrent = contextFor(PlayerAppState::Paused);
        pausedCurrent.pauseAdmission = PlayerPauseAdmission::CurrentRun;
        checkDecision(pausedCurrent, PlayerCommandKind::Pause,
                      ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::Fixed,
                         PlayerCommandEffect::RegisterPause));

        // PendingNewRun: accepted while a Retry/Reload transaction is pending; the current
        // application state is unchanged until the transaction terminal is known.
        auto playingPending = contextFor(PlayerAppState::Playing);
        playingPending.transactionInProgress = true;
        playingPending.pauseAdmission = PlayerPauseAdmission::PendingNewRun;
        checkDecision(playingPending, PlayerCommandKind::Pause,
                      ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::Fixed,
                         PlayerCommandEffect::RegisterPause));
        auto pausedPending = contextFor(PlayerAppState::Paused);
        pausedPending.transactionInProgress = true;
        pausedPending.pauseAdmission = PlayerPauseAdmission::PendingNewRun;
        checkDecision(pausedPending, PlayerCommandKind::Pause,
                      ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::Fixed,
                         PlayerCommandEffect::RegisterPause));

        // AlreadyHeld: a confirmed source is idempotent and starts no second freeze.
        checkDecision(contextFor(PlayerAppState::Paused), PlayerCommandKind::Pause,
                      ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::Fixed,
                         PlayerCommandEffect::AlreadySatisfied, true));
        auto heldBusy = contextFor(PlayerAppState::Paused);
        heldBusy.transactionInProgress = true;
        checkDecision(heldBusy, PlayerCommandKind::Pause,
                      ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::Fixed,
                         PlayerCommandEffect::AlreadySatisfied, true));

        // An unclassified request carries no source identity; the old stable duplicate
        // rejection remains until the owner supplies a pause admission.
        auto pausedUnclassified = contextFor(PlayerAppState::Paused);
        pausedUnclassified.pauseAdmission = PlayerPauseAdmission::None;
        checkDecision(pausedUnclassified, PlayerCommandKind::Pause, no(kDuplicate, true));
    }

    SECTION("a transaction admits only a qualified Pause") {
        for (const auto& base : baseContexts) {
            for (const auto command : allCommands) {
                auto busy = base;
                busy.transactionInProgress = true;
                busy.pauseAdmission = PlayerPauseAdmission::None;
                checkDecision(busy, command, no(kTransactionInProgress));
            }
        }
        // A qualified Pause is exempt from the mutex; see the admission section for the three
        // variants. An unclassified Pause remains an overlapping command.
        auto currentRun = contextFor(PlayerAppState::Playing);
        currentRun.transactionInProgress = true;
        currentRun.pauseAdmission = PlayerPauseAdmission::CurrentRun;
        const auto admitted =
            cuexis::player_support::evaluatePlayerCommand(currentRun, PlayerCommandKind::Pause);
        REQUIRE(admitted.has_value());
        CHECK(admitted->effect == PlayerCommandEffect::RegisterPause);
        CHECK_FALSE(admitted->duplicate);
    }

    SECTION("Empty with active content is inconsistent before any state table lookup") {
        auto inconsistent = contextFor(PlayerAppState::Empty);
        inconsistent.hasActiveContent = true;
        for (const auto command : allCommands) {
            checkDecision(inconsistent, command, no(kStateInconsistent));
        }
        // The inconsistency also wins over the transaction mutex, but not over an unknown kind.
        inconsistent.transactionInProgress = true;
        checkDecision(inconsistent, PlayerCommandKind::Play, no(kStateInconsistent));
        const auto unknown = cuexis::player_support::evaluatePlayerCommand(
            inconsistent, static_cast<PlayerCommandKind>(0));
        REQUIRE_FALSE(unknown.has_value());
        CHECK(unknown.error().code() == kUnknown);
    }

    SECTION("unknown command kinds are rejected") {
        for (const auto kind :
             {static_cast<PlayerCommandKind>(0), static_cast<PlayerCommandKind>(9),
              static_cast<PlayerCommandKind>(255)}) {
            const auto result = cuexis::player_support::evaluatePlayerCommand(
                contextFor(PlayerAppState::Playing), kind);
            REQUIRE_FALSE(result.has_value());
            CHECK(result.error().code() == kUnknown);
        }
    }

    SECTION("accepted cells publish their effect and success policy") {
        checkDecision(contextFor(PlayerAppState::Failed), PlayerCommandKind::Retry,
                      ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::RetryAfterDevices));
        checkDecision(contextFor(PlayerAppState::Playing), PlayerCommandKind::Reload,
                      ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused));
        checkDecision(contextFor(PlayerAppState::Paused), PlayerCommandKind::Play,
                      ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::ResumeAfterReasons));
        checkDecision(contextFor(PlayerAppState::Failed), PlayerCommandKind::Rebuild,
                      ok(PlayerAppState::Failed, PlayerSuccessStatePolicy::RebuildAfterFacts));
        for (const auto state : {PlayerAppState::Loaded, PlayerAppState::Stopped}) {
            checkDecision(contextFor(state), PlayerCommandKind::Load, no(kLoadInitialOnly));
        }
        // Load, Stop, ordinary Seek, and non-Failed Rebuild stay on the Fixed policy.
        checkDecision(contextFor(PlayerAppState::Empty), PlayerCommandKind::Load,
                      ok(PlayerAppState::Loaded, PlayerSuccessStatePolicy::Fixed));
        checkDecision(contextFor(PlayerAppState::Loaded), PlayerCommandKind::Stop,
                      ok(PlayerAppState::Stopped, PlayerSuccessStatePolicy::Fixed));
        checkDecision(contextFor(PlayerAppState::Playing), PlayerCommandKind::Seek,
                      ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::Fixed));
        checkDecision(contextFor(PlayerAppState::Stopped), PlayerCommandKind::Rebuild,
                      ok(PlayerAppState::Stopped, PlayerSuccessStatePolicy::Fixed));
    }

    SECTION("Retry appends to the enum and has a stable name") {
        CHECK(static_cast<int>(PlayerCommandKind::Load) == 1);
        CHECK(static_cast<int>(PlayerCommandKind::Reload) == 6);
        CHECK(static_cast<int>(PlayerCommandKind::Rebuild) == 7);
        CHECK(static_cast<int>(PlayerCommandKind::Retry) == 8);
        CHECK(cuexis::player_support::playerCommandName(PlayerCommandKind::Retry) == "retry");
    }
}

TEST_CASE("A transaction in progress admits only a qualified Pause", "[player][command]") {
    for (const auto& base : baseContexts) {
        for (const auto command : allCommands) {
            auto busy = base;
            busy.transactionInProgress = true;
            busy.pauseAdmission = PlayerPauseAdmission::None;
            const auto result = cuexis::player_support::evaluatePlayerCommand(busy, command);
            INFO("state=" << cuexis::player_support::playerAppStateName(base.state)
                          << " command=" << cuexis::player_support::playerCommandName(command));
            REQUIRE_FALSE(result.has_value());
            CHECK(result.error().code() == kTransactionInProgress);
        }
    }

    auto currentRun = contextFor(PlayerAppState::Playing);
    currentRun.transactionInProgress = true;
    currentRun.pauseAdmission = PlayerPauseAdmission::CurrentRun;
    const auto admitted =
        cuexis::player_support::evaluatePlayerCommand(currentRun, PlayerCommandKind::Pause);
    REQUIRE(admitted.has_value());
    CHECK(admitted->state == PlayerAppState::Paused);
    CHECK(admitted->effect == PlayerCommandEffect::RegisterPause);
    CHECK_FALSE(admitted->duplicate);
}

TEST_CASE("Duplicate commands are reported as duplicates, not as invalid transitions",
          "[player][command]") {
    const auto playing = evaluate(PlayerAppState::Playing, PlayerCommandKind::Play);
    REQUIRE_FALSE(playing.has_value());
    CHECK(playing.error().code() == kDuplicate);

    const auto stopped = evaluate(PlayerAppState::Stopped, PlayerCommandKind::Stop);
    REQUIRE_FALSE(stopped.has_value());
    CHECK(stopped.error().code() == kDuplicate);

    // Pausing an already paused application with a held source succeeds idempotently instead of
    // reporting the old duplicate rejection.
    const auto paused = evaluate(PlayerAppState::Paused, PlayerCommandKind::Pause);
    REQUIRE(paused.has_value());
    CHECK(paused->state == PlayerAppState::Paused);
    CHECK(paused->duplicate);
    CHECK(paused->effect == PlayerCommandEffect::AlreadySatisfied);

    // Without a classified source the stable duplicate rejection is preserved.
    auto unclassified = contextFor(PlayerAppState::Paused);
    unclassified.pauseAdmission = PlayerPauseAdmission::None;
    const auto unclassifiedPause =
        cuexis::player_support::evaluatePlayerCommand(unclassified, PlayerCommandKind::Pause);
    REQUIRE_FALSE(unclassifiedPause.has_value());
    CHECK(unclassifiedPause.error().code() == kDuplicate);

    const auto pausedPlay = evaluate(PlayerAppState::Paused, PlayerCommandKind::Play);
    REQUIRE(pausedPlay.has_value());
    CHECK(pausedPlay->state == PlayerAppState::Playing);
}

TEST_CASE("Reload commits paused at zero and Seek keeps the application state",
          "[player][command]") {
    for (const auto state : {PlayerAppState::Loaded, PlayerAppState::Playing,
                             PlayerAppState::Paused, PlayerAppState::Stopped}) {
        const auto reload = evaluate(state, PlayerCommandKind::Reload);
        REQUIRE(reload.has_value());
        CHECK(reload->state == PlayerAppState::Paused);
        CHECK(reload->successPolicy == PlayerSuccessStatePolicy::ReloadPaused);

        const auto seek = evaluate(state, PlayerCommandKind::Seek);
        REQUIRE(seek.has_value());
        CHECK(seek->state == state);
    }
    // Failed with a configured source recovers to the same silent paused-at-zero success.
    const auto reload = evaluate(PlayerAppState::Failed, PlayerCommandKind::Reload);
    REQUIRE(reload.has_value());
    CHECK(reload->state == PlayerAppState::Paused);
    CHECK(reload->successPolicy == PlayerSuccessStatePolicy::ReloadPaused);
}

TEST_CASE("Failed recovery depends on active content and the configured source",
          "[player][command]") {
    const auto withContent = contextFor(PlayerAppState::Failed);
    checkDecision(withContent, PlayerCommandKind::Retry,
                  ok(PlayerAppState::Playing, PlayerSuccessStatePolicy::RetryAfterDevices));
    checkDecision(withContent, PlayerCommandKind::Rebuild,
                  ok(PlayerAppState::Failed, PlayerSuccessStatePolicy::RebuildAfterFacts));
    checkDecision(withContent, PlayerCommandKind::Reload,
                  ok(PlayerAppState::Paused, PlayerSuccessStatePolicy::ReloadPaused));

    auto withoutContent = withContent;
    withoutContent.hasActiveContent = false;
    for (const auto command : {PlayerCommandKind::Play, PlayerCommandKind::Pause,
                               PlayerCommandKind::Stop, PlayerCommandKind::Seek}) {
        checkDecision(withoutContent, command, no(kFailed));
    }
    checkDecision(withoutContent, PlayerCommandKind::Retry, no(kNotLoaded));
    checkDecision(withoutContent, PlayerCommandKind::Rebuild, no(kNotLoaded));

    withoutContent.hasConfiguredSource = false;
    checkDecision(withoutContent, PlayerCommandKind::Reload, no(kSourceUnavailable));
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
