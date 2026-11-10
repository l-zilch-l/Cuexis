#include "player_control_fixture.hpp"

// Seek, reload, device observation, input and shutdown cases.

TEST_CASE("PlayerController preserves the chart time across consecutive reloads",
          "[player][control][command][reload]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());
    fixture.controller->noteRuntimeFrame(
        cuexis::playback::RuntimeFrame{.chartTimeMs = 3000.0, .simulationDeltaTimeMs = 16.0});

    const auto before = snapshotBundle(fixture);
    for (int attempt = 0; attempt < 3; ++attempt) {
        fixture.sourceProject = (attempt % 2 == 0) ? std::string{otherChartClockProject}
                                                   : std::string{chartClockProject};
        const auto reloaded = fixture.reload();
        INFO("reload attempt " << attempt << " of " << fixture.sourceProject << " -> "
                               << (reloaded ? std::string{"ok"}
                                            : std::string{reloaded.error().code()}));
        REQUIRE(reloaded.has_value());
        CHECK(fixture.controller->state() == PlayerAppState::Playing);
        CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
        CHECK(fixture.controller->consumeBundleReplaced());
        CHECK_FALSE(fixture.controller->consumeBundleReplaced());
        // KeepChartTime resumes where the previous bundle stopped.
        auto target = fixture.controller->consumeChartSeekTarget();
        REQUIRE(target.has_value());
        CHECK(*target == Catch::Approx(before.chartTimeMs));
        fixture.controller->noteRuntimeFrame(
            cuexis::playback::RuntimeFrame{.chartTimeMs = 3000.0, .simulationDeltaTimeMs = 16.0});
    }

    // RestartAtZero is the explicit exception: it rewinds the chart clock instead.
    const auto restarted = fixture.reload(PlayerReloadPolicy::RestartAtZero);
    REQUIRE(restarted.has_value());
    const auto target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(0.0));
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
    CHECK(fixture.renderer.status().active);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);
}

TEST_CASE("PlayerController defines the ChartClock seek range as unbounded above",
          "[player][control][command][seek]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());

    // A ChartClock session has no decoded clip and therefore no known playable duration. The
    // documented semantic is that a seek validates finiteness and non-negativity and applies any
    // finite target, because the upper bound is only knowable where a duration exists.
    const auto far = fixture.seek(600000.0);
    REQUIRE(far.has_value());
    const auto applied = fixture.controller->consumeChartSeekTarget();
    REQUIRE(applied.has_value());
    CHECK(*applied == Catch::Approx(600000.0));
    // A seek is a discontinuity rather than an implicit continuity.
    const auto sampled = fixture.controller->chartClock().sample(0.0);
    REQUIRE(sampled.has_value());
    CHECK(sampled->discontinuityId > 0);
}

TEST_CASE("PlayerController still rejects a non-finite or negative ChartClock seek",
          "[player][control][command][seek]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());

    // The play transition stages its own target. Drain it so the refusals below are the only
    // thing that could stage another.
    REQUIRE(fixture.controller->consumeChartSeekTarget().has_value());

    // The lower bound is knowable without a duration, so it is enforced regardless of mode.
    const auto negative = fixture.seek(-1.0);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().code() == "player.command.seek_negative");
    CHECK(contextValue(negative.error(), "command") == "seek");

    const auto infinite = fixture.seek(std::numeric_limits<double>::infinity());
    REQUIRE_FALSE(infinite.has_value());
    CHECK(infinite.error().code() == "player.command.seek_not_finite");

    // Neither refusal reached the chart clock.
    CHECK_FALSE(fixture.controller->consumeChartSeekTarget().has_value());
}

TEST_CASE("PlayerController reload never switches the active PlaybackMode",
          "[player][control][command][reload]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    const auto before = snapshotBundle(fixture);

    // Content without an audio track cannot be reloaded into an audio-clock session.
    fixture.sourceProject = std::string{chartClockProject};
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.mode_change_requires_load");
    CHECK(contextValue(rejected.error(), "command") == "reload");
    checkBundleUnchanged(fixture, before);

    // Load is the explicit switch, and it closes the previous audio seat.
    REQUIRE(fixture.load(chartClockProject).has_value());
    CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK(fixture.store.metrics().registeredClips == 0);
}

TEST_CASE("PlayerController seeks an audio session in the source domain",
          "[player][control][command][audio]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    REQUIRE(fixture.audio.transport != nullptr);

    // The clip is one second long, so a one-second seek is inside the playable range.
    REQUIRE(fixture.seek(500.0).has_value());
    CHECK(fixture.audio.transport->snapshot().source.positionMs == Catch::Approx(500.0));

    const auto outside = fixture.seek(2500.0);
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().code() == "audio.transport.seek_range");
    CHECK(contextValue(outside.error(), "command") == "seek");
    // The rejected seek did not move the transport.
    CHECK(fixture.audio.transport->snapshot().source.positionMs == Catch::Approx(500.0));

    const auto negative = fixture.seek(-10.0);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().code() == "player.command.seek_negative");
}

TEST_CASE("PlayerController folds device observations into the application state",
          "[player][control][command][audio]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.play().has_value());

    // End of stream stops a playing application without losing its position.
    fixture.controller->observeTransport(cuexis::audio::SourceClockSample{
        .positionMs = 1000.0, .state = cuexis::audio::PlaybackState::Ended, .discontinuityId = 2});
    CHECK(fixture.controller->state() == PlayerAppState::Stopped);

    // A stopped application keeps its state while the device keeps reporting the end.
    fixture.controller->observeTransport(cuexis::audio::SourceClockSample{
        .positionMs = 1000.0, .state = cuexis::audio::PlaybackState::Ended, .discontinuityId = 2});
    CHECK(fixture.controller->state() == PlayerAppState::Stopped);

    // A device error is the observation that fails the application.
    fixture.controller->observeTransport(cuexis::audio::SourceClockSample{
        .positionMs = 1000.0, .state = cuexis::audio::PlaybackState::Error, .discontinuityId = 3});
    CHECK(fixture.controller->state() == PlayerAppState::Failed);

    const auto rejected = fixture.play();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.failed");
}

TEST_CASE("PlayerController loads content with no audio track and no object",
          "[player][control][command]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    CHECK(fixture.store.metrics().registeredClips == 1);

    // Switching to content without an audio track is an explicit Load, and it drops the device.
    fixture.sourceProject.clear();
    fixture.explicitChart = std::string{objectlessChart};
    const auto loaded = fixture.apply(PlayerCommand{.kind = PlayerCommandKind::Load});
    INFO("objectless load: " << (loaded ? std::string{"ok"} : std::string{loaded.error().code()}));
    REQUIRE(loaded.has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK(fixture.audio.unloadCalls == 1);
    CHECK(fixture.store.metrics().registeredClips == 0);
    CHECK(fixture.renderer.status().active);

    const auto info = fixture.controller->session().chartInfo();
    REQUIRE(info.has_value());
    CHECK(info->objectCount == 0);

    // A frame over content with no object is still a valid frame.
    fixture.controller->noteRuntimeFrame(cuexis::playback::RuntimeFrame{.chartTimeMs = 0.0});
    REQUIRE(fixture.controller->session().update({.chartTimeMs = 0.0}).has_value());
    const auto snapshot = fixture.controller->session().extractFrame({.width = 64, .height = 64});
    REQUIRE(snapshot.has_value());
    CHECK(snapshot->objects.empty());
}

TEST_CASE("PlayerController translates user input into the shared command layer",
          "[player][control][command][input]") {
    // PlayPause is the only state-dependent action: it pauses what plays and plays everything else.
    CHECK(playerCommandForInput(PlayerInputAction::PlayPause, PlayerAppState::Playing, 0.0).kind ==
          PlayerCommandKind::Pause);
    for (const auto state : {PlayerAppState::Empty, PlayerAppState::Loaded, PlayerAppState::Paused,
                             PlayerAppState::Stopped, PlayerAppState::Failed}) {
        CHECK(playerCommandForInput(PlayerInputAction::PlayPause, state, 0.0).kind ==
              PlayerCommandKind::Play);
    }

    // Relative seek clamps at zero, because a negative chart time is never a legal target, and
    // never clamps above, because the transport owns the upper bound.
    const auto backward =
        playerCommandForInput(PlayerInputAction::SeekBackward, PlayerAppState::Playing, 3000.0);
    CHECK(backward.kind == PlayerCommandKind::Seek);
    CHECK(backward.seekTargetMs == Catch::Approx(0.0));
    const auto backwardInside =
        playerCommandForInput(PlayerInputAction::SeekBackward, PlayerAppState::Playing, 12000.0);
    CHECK(backwardInside.seekTargetMs == Catch::Approx(12000.0 - playerSeekStepMs));
    const auto forward =
        playerCommandForInput(PlayerInputAction::SeekForward, PlayerAppState::Playing, 1000.0);
    CHECK(forward.seekTargetMs == Catch::Approx(1000.0 + playerSeekStepMs));

    CHECK(playerCommandForInput(PlayerInputAction::Stop, PlayerAppState::Playing, 0.0).kind ==
          PlayerCommandKind::Stop);
    CHECK(playerCommandForInput(PlayerInputAction::Reload, PlayerAppState::Playing, 0.0)
              .reloadPolicy == PlayerReloadPolicy::RestartAtZero);
    CHECK(playerCommandForInput(PlayerInputAction::Rebuild, PlayerAppState::Playing, 0.0).kind ==
          PlayerCommandKind::Rebuild);

    // The produced commands are the same values a smoke script or the startup path hands over, so a
    // user action runs through the transaction, not around it.
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(
        fixture
            .apply(playerCommandForInput(PlayerInputAction::PlayPause, fixture.controller->state(),
                                         fixture.controller->lastRuntimeFrame().chartTimeMs))
            .has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
    REQUIRE(
        fixture
            .apply(playerCommandForInput(PlayerInputAction::PlayPause, fixture.controller->state(),
                                         fixture.controller->lastRuntimeFrame().chartTimeMs))
            .has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Paused);

    // A rejected action reports the same stable code the command table would report directly.
    Fixture empty;
    const auto rejected = empty.apply(
        playerCommandForInput(PlayerInputAction::Reload, empty.controller->state(), 0.0));
    CHECK_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.not_loaded");

    // Every action has a stable trace name.
    CHECK(playerInputActionName(PlayerInputAction::PlayPause) == "play_pause");
    CHECK(playerInputActionName(PlayerInputAction::SeekBackward) == "seek_backward");
    CHECK(playerInputActionName(PlayerInputAction::Quit) == "quit");
}

TEST_CASE("PlayerController keeps the discontinuity contract when input replaces the bundle",
          "[player][control][command][input][discontinuity]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());

    // Frame 2 replaces the bundle from the input path, which runs before that frame samples its
    // clock. The frame the session receives is therefore the re-advanced one, and it still has to
    // obey the rule that the first frame of a new discontinuity reports a zero delta.
    std::vector<std::vector<PlayerInputAction>> script(5);
    script[2] = {PlayerInputAction::Reload};
    ScriptedSurface surface{std::move(script)};

    cuexis::player::PlayerFrameLoop loop{.controller = *fixture.controller,
                                         .renderer = fixture.renderer,
                                         .surface = surface,
                                         .smokeTest = false,
                                         .audioSmokeTest = false,
                                         .diagnostics = nullptr,
                                         .hooks = {},
                                         .logger = *fixture.logger};
    const auto ran = runPlayerFrameLoop(loop);
    INFO("frame loop: " << (ran ? std::string{"ok"} : std::string{ran.error().code()}));
    REQUIRE(ran.has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
}

TEST_CASE("PlayerController shuts audio, the clip, and the session down in order",
          "[player][control][command][shutdown]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    CHECK(fixture.store.metrics().registeredClips == 1);

    REQUIRE(fixture.controller->shutdown().has_value());

    CHECK(fixture.audio.unloadCalls == 1);
    CHECK(fixture.store.metrics().registeredClips == 0);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());
    // The runtime session is released and the application owns nothing, so no later command can
    // report content it no longer has.
    CHECK(fixture.controller->state() == PlayerAppState::Empty);
    CHECK_FALSE(fixture.controller->session().chartInfo().has_value());
    const auto rejected = fixture.play();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.not_loaded");
}
