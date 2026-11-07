#include "player_control_fixture.hpp"

// Load, rejection, failure and ChartClock transport cases.

TEST_CASE("PlayerController loads ChartClock content and publishes one bundle",
          "[player][control][command]") {
    Fixture fixture;

    REQUIRE(fixture.load(chartClockProject).has_value());

    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
    CHECK(fixture.controller->sessionConfig().clockMode == PlaybackClockMode::ChartClock);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());
    CHECK_FALSE(fixture.controller->lastRuntimeFrame().chartTimeMs > 0.0);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.renderer.status().activeResourceCount > 0);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);
    CHECK(fixture.audio.openCalls == 0);

    // A transaction publishes the replaced bundle exactly once.
    CHECK(fixture.controller->consumeBundleReplaced());
    CHECK_FALSE(fixture.controller->consumeBundleReplaced());

    const auto bundle = snapshotBundle(fixture);
    CHECK(bundle.mode == PlaybackMode::ChartClock);
    CHECK_FALSE(bundle.mainMusicAssetId.has_value());
    CHECK(bundle.chartFormatVersion >= 1);
}

TEST_CASE("PlayerController rejects transport commands before content is loaded",
          "[player][control][command]") {
    Fixture fixture;

    CHECK(fixture.controller->state() == PlayerAppState::Empty);

    // The command table owns the decision, so a transport command without content is rejected
    // before any target validation runs.
    for (const auto kind :
         {PlayerCommandKind::Play, PlayerCommandKind::Pause, PlayerCommandKind::Stop,
          PlayerCommandKind::Seek, PlayerCommandKind::Reload}) {
        const auto rejected = fixture.apply(PlayerCommand{.kind = kind});
        REQUIRE_FALSE(rejected.has_value());
        INFO("command: " << cuexis::player_support::playerCommandName(kind));
        CHECK(rejected.error().code() == "player.command.not_loaded");
    }
    const auto negativeBeforeLoad = fixture.seek(-1.0);
    REQUIRE_FALSE(negativeBeforeLoad.has_value());
    CHECK(negativeBeforeLoad.error().code() == "player.command.not_loaded");

    // Rebuild without content and without a failure has nothing to recreate.
    const auto rebuild = fixture.rebuild();
    REQUIRE_FALSE(rebuild.has_value());
    CHECK(rebuild.error().code() == "player.command.not_loaded");
    CHECK(fixture.controller->state() == PlayerAppState::Empty);
    CHECK_FALSE(fixture.renderer.status().active);

    REQUIRE(fixture.load(chartClockProject).has_value());

    // Seek target validation is what rejects an unusable target once content is loaded.
    const auto negative = fixture.seek(-1.0);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().code() == "player.command.seek_negative");

    const auto notFinite = fixture.seek(std::nan(""));
    REQUIRE_FALSE(notFinite.has_value());
    CHECK(notFinite.error().code() == "player.command.seek_not_finite");

    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
}

TEST_CASE("PlayerController rejects a re-entrant command while a transaction owns the application",
          "[player][control][command]") {
    Fixture fixture;

    // The source port runs inside the load transaction, which is exactly the window in which a
    // second command must not be allowed to interleave.
    fixture.onMakeSource = [&fixture]() {
        if (fixture.reentrantCommand.has_value()) {
            const auto nested = fixture.controller->apply(std::move(*fixture.reentrantCommand));
            fixture.reentrantCommand.reset();
            if (!nested) {
                fixture.reentrantError = nested.error();
            }
        }
    };
    fixture.reentrantCommand.emplace(PlayerCommand{.kind = PlayerCommandKind::Load});

    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.reentrantError.has_value());
    CHECK(fixture.reentrantError->code() == "player.command.transaction_in_progress");
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.renderer.status().active);
}

TEST_CASE("PlayerController reports a repeated command as a duplicate rejection",
          "[player][control][command]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());

    const auto again = fixture.play();
    REQUIRE_FALSE(again.has_value());
    CHECK(again.error().code() == "player.command.duplicate");
    CHECK(contextValue(again.error(), "duplicate") == "true");
    CHECK(fixture.controller->state() == PlayerAppState::Playing);

    REQUIRE(fixture.pause().has_value());
    const auto pausedAgain = fixture.pause();
    REQUIRE_FALSE(pausedAgain.has_value());
    CHECK(pausedAgain.error().code() == "player.command.duplicate");
    CHECK(contextValue(pausedAgain.error(), "duplicate") == "true");

    // A duplicate that is not a state repetition reports the real transition error instead.
    const auto notPlaying = fixture.seek(250.0);
    REQUIRE(notPlaying.has_value());
    const auto stopped = fixture.stop();
    REQUIRE(stopped.has_value());
    const auto stoppedAgain = fixture.stop();
    REQUIRE_FALSE(stoppedAgain.has_value());
    CHECK(stoppedAgain.error().code() == "player.command.duplicate");
    const auto pausedFromStopped = fixture.pause();
    REQUIRE_FALSE(pausedFromStopped.has_value());
    CHECK(pausedFromStopped.error().code() == "player.command.not_playing");
    CHECK(contextValue(pausedFromStopped.error(), "duplicate") == "false");
}

TEST_CASE("PlayerController keeps the active bundle when content preparation fails",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());
    fixture.controller->noteRuntimeFrame(
        cuexis::playback::RuntimeFrame{.chartTimeMs = 4200.0, .simulationDeltaTimeMs = 16.0});
    const auto before = snapshotBundle(fixture);

    auto source = PlaybackSource::fromChartText(std::string{missingResourceChart});
    REQUIRE(source.has_value());
    PlayerCommand command{.kind = PlayerCommandKind::Reload};
    command.source = std::move(*source);

    const auto rejected = fixture.apply(std::move(command));
    REQUIRE_FALSE(rejected.has_value());
    INFO("content preparation rejection: " << rejected.error().code());
    CHECK(rejected.error().code() != "player.command.not_loaded");
    CHECK_FALSE(rejected.error().code() == "player.command.transaction_in_progress");

    checkBundleUnchanged(fixture, before);
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
}

TEST_CASE("PlayerController keeps the active bundle when the renderer candidate fails",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());
    REQUIRE(fixture.play().has_value());
    fixture.controller->noteRuntimeFrame(
        cuexis::playback::RuntimeFrame{.chartTimeMs = 900.0, .simulationDeltaTimeMs = 16.0});
    const auto before = snapshotBundle(fixture);

    // A lost surface fails the renderer candidate before anything is activated.
    fixture.renderer.markDeviceLost();
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "presentation.renderer.surface.lost");
    CHECK(contextValue(rejected.error(), "operation") == "prepare_presentation");

    checkBundleUnchanged(fixture, before);
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);

    // Rebuild is the documented recovery for a lost device.
    REQUIRE(fixture.rebuild().has_value());
    CHECK_FALSE(fixture.renderer.status().deviceLost);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.renderer.status().activeResourceCount == before.activeResourceCount);
    CHECK(fixture.controller->state() == PlayerAppState::Playing);
}

TEST_CASE("PlayerController rejects a candidate that went stale before the precheck",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    const auto before = snapshotBundle(fixture);

    // A renderer generation change between candidate preparation and the precheck makes the
    // candidate stale. The precheck must reject it instead of activating a token the renderer no
    // longer knows. The clip port is the only application port that runs inside that window.
    bool rebuiltOnce = false;
    fixture.onPrepareClip = [&fixture, &rebuiltOnce]() {
        if (!rebuiltOnce) {
            rebuiltOnce = true;
            REQUIRE(fixture.renderer.rebuild().has_value());
        }
    };

    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rebuiltOnce);
    CHECK(rejected.error().code() == "presentation.renderer.token.stale");
    CHECK(contextValue(rejected.error(), "stage") == "precheck");

    // The bundle, its identity, and its audio handle are untouched, nothing reached the device,
    // and no candidate leaks.
    const auto after = snapshotBundle(fixture);
    CHECK(after.chartId == before.chartId);
    CHECK(after.configIdentity == before.configIdentity);
    CHECK(after.audioHandle == before.audioHandle);
    CHECK(after.state == before.state);
    CHECK(fixture.audio.activateReplacementCalls == 0);
    CHECK(fixture.controller->audio() != nullptr);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);

    // Rebuild republishes a complete bundle after the injected generation change.
    fixture.onPrepareClip = {};
    REQUIRE(fixture.rebuild().has_value());
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.renderer.status().activeResourceCount == before.activeResourceCount);
    CHECK(fixture.audio.openCalls == 2);
}

TEST_CASE("PlayerController Load refuses audio content under the default clock without a mode",
          "[player][control][command][mode]") {
    Fixture fixture;

    // The configured source carries a main music track, so the default ChartClock does not match.
    // The controller must refuse instead of re-reading the source and preparing CuexisAudio: an
    // automatic retry after a mode failure is rejected by ADR 0042, and the mode is never guessed
    // from a failed load.
    const auto rejected = fixture.load(audioProject);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.mode_required");
    // The cause keeps the engine's own diagnosis of why the default clock did not fit.
    REQUIRE(rejected.error().cause() != nullptr);
    CHECK(rejected.error().cause()->code() == "playback.mode.content_mismatch");

    // The refused load prepared the source once and never decoded a clip or opened a device.
    CHECK(fixture.clipPreparationCalls == 0);
    CHECK(fixture.audio.openCalls == 0);
    CHECK(fixture.controller->state() == PlayerAppState::Empty);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);

    // Naming the clock on Load is the explicit switch the ADR requires, and it succeeds.
    const auto accepted = fixture.loadAudio(audioProject);
    REQUIRE(accepted.has_value());
    CHECK(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
}

TEST_CASE("PlayerController Load refuses an explicit audio source that names no mode",
          "[player][control][command][mode]") {
    Fixture fixture;

    // An explicit source cannot be re-read at all, so a named mode is the only way through.
    auto explicitSource = projectSource(audioProject);
    REQUIRE(explicitSource.has_value());
    auto command = PlayerCommand{.kind = PlayerCommandKind::Load};
    command.source = std::move(*explicitSource);

    const auto rejected = fixture.apply(std::move(command));
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "player.command.mode_required");
    CHECK(fixture.clipPreparationCalls == 0);
    CHECK(fixture.controller->state() == PlayerAppState::Empty);
}

TEST_CASE("PlayerController reports a decoder-stage clip failure without publishing a bundle",
          "[player][control][command][failure][decoder]") {
    Fixture fixture;

    // The clip port decodes the chart's main music. Failing it exercises the decoder stage the
    // staged-failure acceptance list names, which no earlier case reached.
    fixture.failClipPreparation = true;
    const auto rejected = fixture.loadAudio(audioProject);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "test.clip.decode_failed");
    // The decoder runs before any device is opened, so the failure is a prepare failure.
    CHECK(fixture.clipPreparationCalls == 1);
    CHECK(fixture.audio.openCalls == 0);

    // A prepare-stage failure publishes nothing and leaves the controller empty.
    CHECK(fixture.controller->state() == PlayerAppState::Empty);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());
    CHECK(fixture.store.metrics().registeredClips == 0);
    CHECK_FALSE(fixture.renderer.status().active);
    CHECK_FALSE(fixture.renderer.status().candidateOutstanding);

    // Retrying with the port healthy loads normally, so the failure left no residue.
    fixture.failClipPreparation = false;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.store.metrics().registeredClips == 1);
}

TEST_CASE("PlayerController keeps the published bundle when a decoder-stage reload fails",
          "[player][control][command][failure][decoder]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    const auto before = snapshotBundle(fixture);
    REQUIRE(before.audioHandle.has_value());

    // Reloading with a failing decoder must not disturb the active bundle, its identity, or its
    // audio handle. Only the audio activation step may publish a physical failure.
    fixture.failClipPreparation = true;
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "test.clip.decode_failed");

    checkBundleUnchanged(fixture, before, PlayerAppState::Playing);
    CHECK(fixture.store.metrics().registeredClips == 1);
    CHECK(fixture.audio.unloadCalls == 0);
}

TEST_CASE("PlayerController enters Failed when audio activation fails and keeps the old bundle",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.controller->mode() == PlaybackMode::CuexisAudio);
    REQUIRE(fixture.play().has_value());
    REQUIRE(fixture.audio.openCalls == 1);
    CHECK(fixture.audio.lastGain == Catch::Approx(fixture.gain));
    CHECK(fixture.audio.lastStartPositionMs == Catch::Approx(0.0));

    const auto before = snapshotBundle(fixture);
    REQUIRE(before.audioHandle.has_value());

    // The device is already open, so a reload stages a replacement stream on it.
    fixture.audio.failActivateReplacement = true;
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "test.audio.replacement_activate_failed");
    CHECK(contextValue(rejected.error(), "stage") == "audio_activate");
    CHECK(fixture.audio.prepareReplacementCalls == 1);
    CHECK(fixture.audio.activateReplacementCalls == 1);

    // A physical device failure is the one step the transaction cannot roll back: the application
    // reports Failed and keeps the published bundle, identity, and handles.
    checkBundleUnchanged(fixture, before, PlayerAppState::Failed);
    CHECK(fixture.controller->audio() != nullptr);
    CHECK(fixture.audio.unloadCalls == 0);
    CHECK(fixture.renderer.status().active);
}

TEST_CASE("PlayerController enters Failed when the commit fails and keeps the published handles",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    const auto before = snapshotBundle(fixture);
    REQUIRE(before.audioHandle.has_value());

    // The clip port consumes the candidate between preparation and the commit, which is the only
    // application port that runs inside that window.
    fixture.stealPreparedCandidate = true;
    const auto rejected = fixture.reload();
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "playback.prepared.invalid");
    CHECK(contextValue(rejected.error(), "stage") == "commit");

    checkBundleUnchanged(fixture, before, PlayerAppState::Failed);
    CHECK(fixture.controller->audio() != nullptr);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.clipPreparationCalls == 2);
    // The rejected clip was discarded; the active clip and the seat survive.
    CHECK(fixture.store.metrics().registeredClips == 1);
}

TEST_CASE("PlayerController enters Failed when the audio device cannot be opened",
          "[player][control][command][failure]") {
    Fixture fixture;
    fixture.audio.failOpen = true;

    const auto rejected = fixture.loadAudio(audioProject);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "test.audio.device_open_failed");
    CHECK(contextValue(rejected.error(), "stage") == "audio_activate");
    CHECK(fixture.controller->state() == PlayerAppState::Failed);
    CHECK(fixture.audio.openCalls == 1);
    // Nothing was published: no presentation, no clip, and no device.
    CHECK_FALSE(fixture.renderer.status().active);
    CHECK(fixture.store.metrics().registeredClips == 0);
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());

    // Rebuild is still the recovery from Failed, even when the failure happened before the first
    // bundle existed. With no bundle to rebuild, Rebuild falls back to a fresh Load, which must
    // name the clock because the controller never guesses a mode from a failed load.
    fixture.audio.failOpen = false;
    REQUIRE(fixture
                .apply(PlayerCommand{.kind = PlayerCommandKind::Rebuild,
                                     .mode = PlaybackMode::CuexisAudio})
                .has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.controller->audio() != nullptr);
    CHECK(fixture.store.metrics().registeredClips == 1);
}

TEST_CASE("PlayerController leaves Failed only through Load or Rebuild",
          "[player][control][command][failure]") {
    Fixture fixture;
    REQUIRE(fixture.loadAudio(audioProject).has_value());
    REQUIRE(fixture.play().has_value());
    fixture.audio.failActivateReplacement = true;
    REQUIRE_FALSE(fixture.reload().has_value());
    REQUIRE(fixture.controller->state() == PlayerAppState::Failed);

    for (const auto kind :
         {PlayerCommandKind::Play, PlayerCommandKind::Pause, PlayerCommandKind::Stop,
          PlayerCommandKind::Seek, PlayerCommandKind::Reload}) {
        const auto rejected = fixture.apply(PlayerCommand{.kind = kind});
        REQUIRE_FALSE(rejected.has_value());
        INFO("command: " << cuexis::player_support::playerCommandName(kind));
        CHECK(rejected.error().code() == "player.command.failed");
    }
    CHECK(fixture.controller->state() == PlayerAppState::Failed);

    // Rebuild repairs the renderer and the device path, and publishes the state the table gives
    // the recovery: Loaded, so the caller resumes with an explicit Play.
    fixture.audio.failActivateReplacement = false;
    REQUIRE(fixture.rebuild().has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.renderer.status().active);
    CHECK(fixture.audio.unloadCalls == 1);
    CHECK(fixture.audio.openCalls == 2);
    CHECK(fixture.audio.prepareReplacementCalls == 1);
    CHECK(fixture.audio.activateReplacementCalls == 1);
    CHECK(fixture.store.metrics().registeredClips == 1);
    REQUIRE(fixture.play().has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Playing);

    // Load is the other accepted exit.
    fixture.audio.failActivateReplacement = true;
    REQUIRE_FALSE(fixture.reload().has_value());
    REQUIRE(fixture.controller->state() == PlayerAppState::Failed);
    REQUIRE(fixture.load(chartClockProject).has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Loaded);
    CHECK(fixture.controller->mode() == PlaybackMode::ChartClock);
    // Loading content without an audio track closes the previous seat through the bundle swap.
    CHECK(fixture.controller->audio() == nullptr);
    CHECK_FALSE(fixture.controller->activeAudioHandle().has_value());
    CHECK(fixture.store.metrics().registeredClips == 0);
}

TEST_CASE("PlayerController ChartClock transport commands publish one discontinuity each",
          "[player][control][command][clock]") {
    Fixture fixture;
    REQUIRE(fixture.load(chartClockProject).has_value());

    const auto start = fixture.controller->chartClock().sample(0.0);
    REQUIRE(start.has_value());
    const auto startDiscontinuity = start->discontinuityId;

    REQUIRE(fixture.play().has_value());
    auto target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(0.0));
    CHECK_FALSE(fixture.controller->consumeChartSeekTarget().has_value());

    // Pause freezes the last presented chart time; Play resumes from it.
    fixture.controller->noteRuntimeFrame(
        cuexis::playback::RuntimeFrame{.chartTimeMs = 1750.0, .simulationDeltaTimeMs = 16.0});
    REQUIRE(fixture.pause().has_value());
    CHECK_FALSE(fixture.controller->consumeChartSeekTarget().has_value());
    REQUIRE(fixture.play().has_value());
    target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(1750.0));

    // Stop rewinds a ChartClock session to chart time zero.
    REQUIRE(fixture.stop().has_value());
    target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(0.0));

    // Seeking while stopped is accepted and keeps the stopped state.
    REQUIRE(fixture.seek(2500.0).has_value());
    CHECK(fixture.controller->state() == PlayerAppState::Stopped);
    target = fixture.controller->consumeChartSeekTarget();
    REQUIRE(target.has_value());
    CHECK(*target == Catch::Approx(2500.0));

    const auto end = fixture.controller->chartClock().sample(0.0);
    REQUIRE(end.has_value());
    CHECK(end->discontinuityId > startDiscontinuity);
}
