#include "player_control.hpp"

#include "player_control_internal.hpp"
#include "player_log.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

namespace cuexis::player {
namespace {

using control_internal::TransactionGuard;

[[nodiscard]] auto reloadPolicyFor(player_support::PlayerReloadPolicy policy)
    -> playback::ReloadPolicy {
    return policy == player_support::PlayerReloadPolicy::RestartAtZero
               ? playback::ReloadPolicy::RestartAtZero
               : playback::ReloadPolicy::KeepChartTime;
}
[[nodiscard]] auto identityText(const std::array<std::uint8_t, 32>& identity) -> std::string {
    std::string text(identity.size() * 2, '0');
    for (std::size_t index = 0; index < identity.size(); ++index) {
        char pair[3] = {};
        std::snprintf(pair, sizeof(pair), "%02x", identity[index]);
        text[index * 2] = pair[0];
        text[index * 2 + 1] = pair[1];
    }
    return text;
}
} // namespace

auto PlayerController::apply(PlayerCommand command) -> core::Result<void> {
    auto decision = player_support::evaluatePlayerCommand(
        {.state = state_, .transactionInProgress = transactionInProgress_}, command.kind);
    if (!decision) {
        return core::unexpected(std::move(decision.error()));
    }
    core::Result<void> result = {};
    switch (command.kind) {
    case player_support::PlayerCommandKind::Load:
        result = runLoad(command);
        break;
    case player_support::PlayerCommandKind::Reload:
        result = runReload(command);
        break;
    case player_support::PlayerCommandKind::Rebuild:
        result = runRebuild(command);
        break;
    case player_support::PlayerCommandKind::Play:
        result = runPlay();
        break;
    case player_support::PlayerCommandKind::Pause:
        result = runPause();
        break;
    case player_support::PlayerCommandKind::Stop:
        result = runStop();
        break;
    case player_support::PlayerCommandKind::Seek:
        result = runSeek(command.seekTargetMs);
        break;
    }
    if (!result) {
        return core::unexpected(std::move(result.error()));
    }
    state_ = decision->state;
    return {};
}

auto PlayerController::sourceFor(PlayerCommand& command) -> core::Result<playback::PlaybackSource> {
    if (command.source.has_value()) {
        auto source = std::move(*command.source);
        command.source.reset();
        return source;
    }
    if (!ports_.makeSource) {
        return core::unexpected(core::Error{"player.command.source_unavailable",
                                            "No playback source factory is configured"});
    }
    return ports_.makeSource();
}

auto PlayerController::discardClip(const std::optional<audio::AudioClipHandle>& clip) -> void {
    if (!clip.has_value()) {
        return;
    }
    if (auto removed = audioStore_.remove(*clip); !removed) {
        logger_.warn("player.audio", "A discarded audio clip could not be removed");
    }
}

auto PlayerController::runLoad(PlayerCommand& command) -> core::Result<void> {
    TransactionGuard guard{transactionInProgress_};

    auto source = sourceFor(command);
    if (!source) {
        return core::unexpected(std::move(source.error()));
    }
    auto bundle = preparePlayerLoadBundle(std::move(*source),
                                          command.mode.value_or(playback::PlaybackMode::ChartClock),
                                          profileCorrectionUs_);
    if (!bundle && !command.mode.has_value() &&
        bundle.error().code() == "playback.mode.content_mismatch") {
        // The mode is never guessed from a failed load and the source is never prepared twice.
        // Content that needs another clock must name it on Load, which is the explicit switch
        // ADR 0042 requires. An explicit source cannot be re-read either, so both paths refuse.
        return core::unexpected(
            core::Error{"player.command.mode_required",
                        "Content whose clock differs from the default must be loaded with an "
                        "explicit mode"}
                .withCause(std::move(bundle.error())));
    }
    if (!bundle) {
        return core::unexpected(std::move(bundle.error()).withContext("command", "load"));
    }
    // The session object outlives this call because `commitBundle` takes ownership of it. Reading
    // the raw pointer first keeps the reference independent of argument evaluation order.
    auto* const loaded = bundle->session.get();
    return commitBundle(*loaded, std::move(bundle->session), std::move(bundle->prepared),
                        bundle->mode, bundle->config, true);
}

auto PlayerController::runReload(PlayerCommand& command) -> core::Result<void> {
    if (session_ == nullptr || !timeline_.has_value() || !chartClock_.has_value()) {
        return core::unexpected(core::Error{"player.command.not_loaded", "No content is loaded"});
    }
    TransactionGuard guard{transactionInProgress_};

    auto source = sourceFor(command);
    if (!source) {
        return core::unexpected(std::move(source.error()));
    }
    const auto policy = reloadPolicyFor(command.reloadPolicy);
    auto prepared = session_->prepareReload(std::move(*source), lastFrame_, policy);
    if (!prepared) {
        auto error = std::move(prepared.error());
        if (error.code() == "playback.mode.content_mismatch") {
            // Reload never changes PlaybackMode. Switching between content with and without an
            // audio track is an explicit Load.
            error = core::Error{"player.command.mode_change_requires_load",
                                "Reload keeps the active PlaybackMode; use Load to switch"}
                        .withCause(std::move(error));
        }
        return core::unexpected(std::move(error).withContext("command", "reload"));
    }
    // The restart policy also decides where the audio replacement and the chart clock resume.
    const bool restartTimeline = policy == playback::ReloadPolicy::RestartAtZero;
    return commitBundle(*session_, nullptr, std::move(*prepared), activeMode_, sessionConfig_,
                        restartTimeline);
}

auto PlayerController::runRebuild(PlayerCommand& command) -> core::Result<void> {
    if (session_ == nullptr) {
        if (state_ != player_support::PlayerAppState::Failed) {
            return core::unexpected(
                core::Error{"player.command.not_loaded", "No content is loaded"});
        }
        // The application failed before the first bundle existed, so there is nothing to rebuild.
        // Reopening the configured source is the recovery the command table promises for Failed.
        return runLoad(command);
    }
    TransactionGuard guard{transactionInProgress_};

    // The rebuild invalidates the active presentation and every renderer token, so the rest of
    // this transaction must succeed for the application to keep a renderable frame.
    if (auto rebuilt = renderer_.rebuild(); !rebuilt) {
        publishFailure("renderer_rebuild");
        return core::unexpected(std::move(rebuilt.error()).withContext("command", "rebuild"));
    }
    // A rebuild also recreates the device path. Closing the seat here is what makes Rebuild the
    // recovery for a device failure that left the transport in its error state: the transaction
    // then opens a fresh seat instead of reusing the failed one.
    if (audioSeat_ != nullptr) {
        if (auto unloaded = audioSeat_->unload(); !unloaded) {
            logger_.warn("player.audio", "The rebuilt audio seat could not be closed cleanly");
        }
        audioSeat_.reset();
    }
    auto source = sourceFor(command);
    if (!source) {
        publishFailure("renderer_rebuild_source");
        return core::unexpected(std::move(source.error()));
    }
    auto prepared = session_->prepareReload(std::move(*source), lastFrame_,
                                            playback::ReloadPolicy::KeepChartTime);
    if (!prepared) {
        publishFailure("renderer_rebuild_prepare");
        return core::unexpected(std::move(prepared.error()).withContext("command", "rebuild"));
    }
    auto result =
        commitBundle(*session_, nullptr, std::move(*prepared), activeMode_, sessionConfig_, false);
    if (!result) {
        publishFailure("renderer_rebuild_commit");
    }
    return result;
}

auto PlayerController::runPlay() -> core::Result<void> {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (gameplay_) {
        if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_) {
            auto audio = audioSeat_->transport().play();
            if (!audio)
                return core::unexpected(std::move(audio.error()));
        }
        auto control = session_->controlGameplay(playback::GameplayControl::Resume);
        if (!control) {
            publishFailure("gameplay_play_control");
            return core::unexpected(std::move(control.error()));
        }
        if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_)
            return {};
    }
#endif
    if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_ != nullptr) {
        if (auto played = audioSeat_->transport().play(); !played) {
            return core::unexpected(std::move(played.error()).withContext("command", "play"));
        }
        return {};
    }
    // A ChartClock session has no transport: the frame loop resumes from the frozen chart time.
    pendingChartSeekMs_ = chartResumeMs_;
    if (chartClock_.has_value()) {
        chartClock_->markDiscontinuity();
    }
    return {};
}

auto PlayerController::runPause() -> core::Result<void> {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (gameplay_) {
        if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_) {
            auto audio = audioSeat_->transport().pause();
            if (!audio)
                return core::unexpected(std::move(audio.error()));
        }
        auto control = session_->controlGameplay(playback::GameplayControl::Pause);
        if (!control) {
            publishFailure("gameplay_pause_control");
            return core::unexpected(std::move(control.error()));
        }
        if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_)
            return {};
    }
#endif
    if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_ != nullptr) {
        if (auto paused = audioSeat_->transport().pause(); !paused) {
            return core::unexpected(std::move(paused.error()).withContext("command", "pause"));
        }
        return {};
    }
    // The frame loop stops sampling the wall clock while the application is not playing, so the
    // frozen chart time is what a later Play resumes from.
    chartResumeMs_ = lastFrame_.chartTimeMs;
    return {};
}

auto PlayerController::runStop() -> core::Result<void> {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (gameplay_) {
        if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_) {
            auto audio = audioSeat_->transport().stop();
            if (!audio)
                return core::unexpected(std::move(audio.error()));
        }
        auto control = session_->controlGameplay(playback::GameplayControl::Stop);
        if (!control) {
            publishFailure("gameplay_stop_control");
            return core::unexpected(std::move(control.error()));
        }
        gameplay_->reset(true);
        if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_)
            return {};
    }
#endif
    if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_ != nullptr) {
        if (auto stopped = audioSeat_->transport().stop(); !stopped) {
            return core::unexpected(std::move(stopped.error()).withContext("command", "stop"));
        }
        return {};
    }
    // A ChartClock session rewinds to chart time zero. This is a transport rewind, not the
    // RestartAtZero reload policy, and it publishes one discontinuity.
    return seekChartClock(0.0);
}

auto PlayerController::runSeek(double targetMs) -> core::Result<void> {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (gameplay_)
        return core::unexpected(core::Error{"player.arguments.unknown",
                                            "Gameplay seek requires an explicit typed Tick"});
#endif
    // The application layer does not know the playable duration; the audio transport enforces the
    // upper bound against the decoded clip, and a ChartClock session has no duration to check.
    if (auto valid = player_support::validateSeekTargetMs(targetMs, -1.0); !valid) {
        return core::unexpected(std::move(valid.error()).withContext("command", "seek"));
    }
    if (activeMode_ == playback::PlaybackMode::CuexisAudio && audioSeat_ != nullptr) {
        return seekAudio(targetMs);
    }
    return seekChartClock(targetMs);
}

auto PlayerController::seekChartClock(double targetMs) -> core::Result<void> {
    pendingChartSeekMs_ = targetMs;
    chartResumeMs_ = targetMs;
    if (chartClock_.has_value()) {
        chartClock_->markDiscontinuity();
    }
    return {};
}

auto PlayerController::seekAudio(double targetMs) -> core::Result<void> {
    const auto targetUs = static_cast<std::int64_t>(std::llround(targetMs * 1000.0));
    const auto offsetUs = static_cast<std::int64_t>(std::llround(timingOffsetMs_ * 1000.0));
    auto sourceUs = player_support::reverseSeekSourcePositionUs(targetUs, offsetUs,
                                                                sessionConfig_.outputCorrectionUs);
    if (!sourceUs) {
        return core::unexpected(std::move(sourceUs.error()).withContext("command", "seek"));
    }
    if (*sourceUs < 0) {
        return core::unexpected(
            core::Error{"player.command.seek_outside",
                        "The corrected seek source is outside the playable range"}
                .withContext("target_ms", std::to_string(targetMs)));
    }
    auto& transport = audioSeat_->transport();
    if (auto sought = transport.seekMs(static_cast<double>(*sourceUs) / 1000.0); !sought) {
        return core::unexpected(std::move(sought.error()).withContext("command", "seek"));
    }
    return {};
}

auto PlayerController::commitBundle(playback::PlaybackSession& session,
                                    std::unique_ptr<playback::PlaybackSession> adopt,
                                    playback::PreparedPlayback&& prepared,
                                    playback::PlaybackMode mode,
                                    player_support::ResolvedSessionConfig config,
                                    bool restartTimeline) -> core::Result<void> {
    // Step 1: validate the content metadata and freeze this operation's time and configuration.
    const auto* info = prepared.contentInfo();
    if (info == nullptr) {
        return core::unexpected(core::Error{"player.playback.prepared_invalid",
                                            "Prepared Playback content metadata is unavailable"});
    }
    const auto timingOffsetMs = info->timingOffsetMs;
    if (!std::isfinite(timingOffsetMs)) {
        return core::unexpected(core::Error{"player.playback.timing_offset_invalid",
                                            "The prepared timing offset must be finite"});
    }
    // Captured before the frame record is reset, because a keeping transaction resumes here.
    const double preservedChartTimeMs = lastFrame_.chartTimeMs;
    std::optional<playback::RuntimeTimeline> nextTimeline;
    if (timeline_.has_value()) {
        nextTimeline = *timeline_;
        if (auto reset = nextTimeline->reset(timingOffsetMs); !reset) {
            return core::unexpected(std::move(reset.error()));
        }
    } else {
        auto created = playback::RuntimeTimeline::create(timingOffsetMs);
        if (!created) {
            return core::unexpected(std::move(created.error()));
        }
        nextTimeline = std::move(*created);
    }

    // Step 2: prepare the renderer candidate and the audio replacement. Nothing is activated yet.
    auto candidate = renderer_.prepare(prepared, {.enableDebugPass = true});
    if (!candidate) {
        return core::unexpected(
            std::move(candidate.error()).withContext("operation", "prepare_presentation"));
    }

    const bool needsAudio = mode == playback::PlaybackMode::CuexisAudio;
    const bool deviceChanges =
        needsAudio ? (audioSeat_ == nullptr || activeMode_ != mode) : (audioSeat_ != nullptr);
    // The source position the new stream must start at, whether it replaces the stream on the
    // current device or is the first stream of a freshly opened one.
    const double startPositionMs = restartTimeline ? 0.0 : lastSourcePositionMs_;
    std::optional<audio::AudioClipHandle> replacementClip;
    if (needsAudio) {
        auto clip = ports_.prepareClip(prepared, audioStore_);
        if (!clip) {
            renderer_.discard(std::move(*candidate));
            return core::unexpected(std::move(clip.error()).withContext("stage", "audio_prepare"));
        }
        replacementClip = *clip;
        // A same-device replacement completes its stream preparation here, so no allocation is
        // deferred until after the previous stream is closed.
        if (!deviceChanges && audioSeat_ != nullptr) {
            if (auto staged = audioSeat_->prepareReplacement(*replacementClip, startPositionMs);
                !staged) {
                discardClip(replacementClip);
                renderer_.discard(std::move(*candidate));
                return core::unexpected(
                    std::move(staged.error()).withContext("stage", "audio_prepare"));
            }
        }
    }

    // Step 3: precheck every owner, generation, and token. Nothing after this point may fail
    // because a candidate went stale.
    if (auto accepted = renderer_.accepts(*candidate); !accepted) {
        discardClip(replacementClip);
        renderer_.discard(std::move(*candidate));
        return core::unexpected(std::move(accepted.error()).withContext("stage", "precheck"));
    }

    // Step 4: activate audio. This is the last step allowed to fail on a physical device.
    std::unique_ptr<PlayerAudioSeat> stagedSeat;
    if (needsAudio && deviceChanges) {
        auto opened = ports_.openAudio(replacementClip, gain_, startPositionMs);
        if (!opened) {
            discardClip(replacementClip);
            renderer_.discard(std::move(*candidate));
            publishFailure("audio_activation");
            return core::unexpected(
                std::move(opened.error()).withContext("stage", "audio_activate"));
        }
        stagedSeat = std::move(*opened);
    } else if (needsAudio && audioSeat_ != nullptr) {
        if (auto activated = audioSeat_->activateReplacement(); !activated) {
            discardClip(replacementClip);
            renderer_.discard(std::move(*candidate));
            publishFailure("audio_activation");
            return core::unexpected(
                std::move(activated.error()).withContext("stage", "audio_activate"));
        }
    }

    // Step 5: the no-failure content exchange.
    if (auto committed = session.commit(std::move(prepared)); !committed) {
        discardClip(replacementClip);
        renderer_.discard(std::move(*candidate));
        publishFailure("commit");
        return core::unexpected(std::move(committed.error()).withContext("stage", "commit"));
    }
    renderer_.activate(std::move(*candidate));

    // Step 6: publish the active bundle, the effective settings, and one discontinuity.
    if (adopt != nullptr) {
        session_ = std::move(adopt);
    }
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (ports_.gameplay) {
        if (!gameplay_)
            gameplay_.emplace(*ports_.gameplay);
        gameplay_->reset(restartTimeline);
        const auto control = state_ == player_support::PlayerAppState::Playing
                                 ? playback::GameplayControl::Resume
                                 : playback::GameplayControl::Pause;
        auto controlled = session_->controlGameplay(control);
        if (!controlled)
            return core::unexpected(std::move(controlled.error()));
    }
#endif
    timeline_ = std::move(*nextTimeline);
    chartClock_.emplace(timingOffsetMs);
    activeMode_ = mode;
    sessionConfig_ = config;
    timingOffsetMs_ = timingOffsetMs;
    // A ChartClock bundle has no transport to reposition, so the frame loop rebases the chart
    // clock on its next sample: to zero for a restart, or to the pre-transaction time for a keep.
    if (mode == playback::PlaybackMode::ChartClock) {
        const double targetMs = restartTimeline ? 0.0 : preservedChartTimeMs;
        pendingChartSeekMs_ = targetMs;
        chartResumeMs_ = targetMs;
    } else {
        pendingChartSeekMs_.reset();
        chartResumeMs_ = 0.0;
    }
    // The frame record stays as it was: it is the last frame the session actually received, and it
    // is both the reload target and the discontinuity the next frame must be measured against.
    // Clearing it here would claim a frame the session never saw.
    if (restartTimeline) {
        lastSourcePositionMs_ = 0.0;
    }
    if (deviceChanges || (needsAudio && audioSeat_ == nullptr)) {
        // The previous device is released only after the new stream is active. A release failure
        // cannot roll the committed bundle back, so it is reported and the transaction stands.
        if (audioSeat_ != nullptr && audioSeat_ != stagedSeat) {
            if (auto unloaded = audioSeat_->unload(); !unloaded) {
                logger_.warn("player.audio", "The previous audio device could not be released");
            }
        }
        audioSeat_ = std::move(stagedSeat);
    }

    const auto previousClip = activeClip_;
    activeClip_ = replacementClip;
    if (previousClip.has_value() && (!activeClip_.has_value() || *previousClip != *activeClip_)) {
        if (auto removed = audioStore_.remove(*previousClip); !removed) {
            logger_.warn("player.audio", "The previous audio clip could not be removed");
        }
    }

    bundleReplaced_ = true;
    const auto identity = player_support::resolvedSessionConfigIdentity(sessionConfig_);
    logger_.info("player.config", std::string{"Session identity "} + identityText(identity));
    if (auto chartInfo = session_->chartInfo(); chartInfo) {
        if (chartInfo->objectCount == 0) {
            logger_.warn("player.chart", "The committed chart contains no objects");
        }
        logger_.info("player.playback",
                     std::string{"Prepared objects: "} + std::to_string(chartInfo->objectCount) +
                         ", behaviors: " + std::to_string(chartInfo->behaviorCount) +
                         ", resources: " + std::to_string(chartInfo->resourceCount));
    } else {
        logger_.warn("player.playback", "Committed chart metadata is unavailable");
    }
    return {};
}

auto PlayerController::shutdown() -> core::Result<void> {
    core::Result<void> first = {};
    if (audioSeat_ != nullptr) {
        if (auto unloaded = audioSeat_->unload(); !unloaded) {
            first = core::unexpected(std::move(unloaded.error()));
        }
        audioSeat_.reset();
    }
    if (activeClip_.has_value()) {
        if (auto removed = audioStore_.remove(*activeClip_); !removed) {
            if (first.has_value()) {
                first = core::unexpected(std::move(removed.error()));
            }
        }
        activeClip_.reset();
    }
    if (session_ != nullptr) {
        if (auto unloaded = session_->unload(); !unloaded) {
            if (first.has_value()) {
                first = core::unexpected(std::move(unloaded.error()));
            }
        }
    }
    // The application owns no bundle after this point, so no later command may report content.
    state_ = player_support::PlayerAppState::Empty;
    return first;
}

} // namespace cuexis::player
