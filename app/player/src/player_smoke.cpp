#include "player_smoke.hpp"

#include "player_log.hpp"
#include "snapshot_scene.hpp"

#include <cuexis/player_support/audio_device_profile.hpp>
#include <cuexis/render/render_scene.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>

namespace cuexis::player {
namespace {

constexpr std::uint64_t presentationSmokeDigest = 18316288860163381829ULL;
constexpr std::string_view legacySmokeTestProjectDirectory = "stage1b_project";
constexpr auto audioSmokePauseDuration = std::chrono::seconds{2};

[[nodiscard]] auto copyNeutralSummary(const presentation_renderer::DrawSummary& summary)
    -> render_opengl::OpenGlDrawSummary {
    render_opengl::OpenGlDrawSummary copied;
    copied.debugPassEnabled = summary.debugPassEnabled;
    copied.debugCommandCount = summary.debugCommandCount;
    copied.digest = summary.digest;
    copied.opaque.resize(summary.opaque.size());
    copied.transparent.resize(summary.transparent.size());
    return copied;
}

} // namespace

PlayerSmokeBinding::PlayerSmokeBinding(
    platform_sdl::SdlWindow& window, render_opengl::OpenGlBackend& backend, PlayerLogger& logger,
    bool smokeTest, bool audioSmokeTest, player_support::ResolvedSessionConfig sessionConfig,
    double timingOffsetMs, std::function<core::Result<playback::PlaybackSource>()> makeSource,
    std::function<core::Result<std::filesystem::path>(std::string_view)> projectDirectory,
    std::function<core::Result<audio::AudioClipHandle>(playback::PreparedPlayback&,
                                                       audio::AudioClipStore&)>
        prepareClip,
    PlayerAudioSeat* audio)
    : window_(window), backend_(backend), logger_(logger), smokeTest_(smokeTest),
      audioSmokeTest_(audioSmokeTest), sessionConfig_(sessionConfig),
      timingOffsetMs_(timingOffsetMs), makeSource_(std::move(makeSource)),
      projectDirectory_(std::move(projectDirectory)), prepareClip_(std::move(prepareClip)),
      audio_(audio) {}

auto PlayerSmokeBinding::hooks() -> PlayerHooks {
    PlayerHooks bound;
    if (audioSmokeTest_) {
        bound.beforeAudioService = [this](std::uint32_t renderedFrames) {
            return beforeAudioService(renderedFrames);
        };
    }
    if (smokeTest_ || audioSmokeTest_) {
        bound.afterTimelineAdvance = [this](PlayerClockContext& context) {
            return afterTimelineAdvance(context);
        };
    }
    if (smokeTest_) {
        bound.beforeSubmit = [this](const playback::FrameSnapshot& snapshot,
                                    std::uint32_t renderedFrames) {
            return beforeSubmit(snapshot, renderedFrames);
        };
        bound.notePresented = [this](const PlayerPresentedFrame& presented) {
            return notePresented(presented);
        };
        bound.validatePresented = [this](const PlayerPresentedFrame& presented) {
            return validatePresented(presented);
        };
    }
    return bound;
}

auto PlayerSmokeBinding::beforeAudioService(std::uint32_t renderedFrames) -> core::Result<void> {
    if (audio_ == nullptr) {
        return core::unexpected(
            core::Error{"player.audio.unopened", "Audio transport was not created"});
    }
    auto& transport = audio_->transport();
    if (renderedFrames == 15) {
        if (auto paused = transport.pause(); !paused) {
            return core::unexpected(std::move(paused.error()));
        }
        const auto pausedClock = transport.snapshot();
        std::this_thread::sleep_for(audioSmokePauseDuration);
        const auto heldClock = transport.snapshot();
        if (heldClock.source.state != audio::PlaybackState::Paused ||
            heldClock.presentedFrame != pausedClock.presentedFrame ||
            heldClock.source.positionMs != pausedClock.source.positionMs ||
            heldClock.source.discontinuityId != pausedClock.source.discontinuityId) {
            return core::unexpected(
                core::Error{"player.audio_smoke_test.pause_clock_advanced",
                            "Audio clock changed during the required two-second pause"});
        }
        if (auto resumed = transport.play(); !resumed) {
            return core::unexpected(std::move(resumed.error()));
        }
        logger_.info("player.audio_smoke_test", "Two-second pause preserved the audio clock");
    } else if (renderedFrames == 30) {
        const auto targetChartUs = std::int64_t{500000};
        const auto offsetUs = std::llround(timingOffsetMs_ * 1000.0);
        auto sourceUs = player_support::reverseSeekSourcePositionUs(
            targetChartUs, offsetUs, sessionConfig_.outputCorrectionUs);
        if (!sourceUs) {
            return core::unexpected(std::move(sourceUs.error()));
        }
        if (*sourceUs < 0) {
            return core::unexpected(
                core::Error{"player.audio_profile.seek_outside",
                            "The corrected seek source is outside the playable range"});
        }
        if (auto sought = transport.seekMs(static_cast<double>(*sourceUs) / 1000.0); !sought) {
            return core::unexpected(std::move(sought.error()));
        }
    } else if (renderedFrames == 45) {
        if (auto stopped = transport.stop(); !stopped) {
            return core::unexpected(std::move(stopped.error()));
        }
    } else if (renderedFrames == 46) {
        if (auto restarted = transport.play(); !restarted) {
            return core::unexpected(std::move(restarted.error()));
        }
    }
    return {};
}

auto PlayerSmokeBinding::afterTimelineAdvance(PlayerClockContext& context) -> core::Result<void> {
    if (audioSmokeTest_ && context.renderedFrames == 55) {
        if (!context.runtimeFrame) {
            return core::unexpected(std::move(context.runtimeFrame.error()));
        }
        if (context.audio == nullptr) {
            return core::unexpected(
                core::Error{"player.audio.unopened", "Audio transport was not created"});
        }
        const auto contentBeforeFailure = context.session.contentInfo();
        if (!contentBeforeFailure) {
            return core::unexpected(std::move(contentBeforeFailure.error()));
        }
        const auto clockBeforeFailure = context.audio->transport().snapshot();
        const auto rejected = context.session.prepareReload(
            R"json({"format":"cuexis.chart","version":2})json", *context.runtimeFrame,
            playback::ReloadPolicy::KeepChartTime);
        if (rejected) {
            return core::unexpected(
                core::Error{"player.audio_smoke_test.failed_reload_accepted",
                            "Invalid replacement chart unexpectedly prepared successfully"});
        }
        const auto contentAfterFailure = context.session.contentInfo();
        if (!contentAfterFailure) {
            return core::unexpected(std::move(contentAfterFailure.error()));
        }
        const auto clockAfterFailure = context.audio->transport().snapshot();
        if (contentAfterFailure->chartId != contentBeforeFailure->chartId ||
            contentAfterFailure->chartFormatVersion != contentBeforeFailure->chartFormatVersion ||
            contentAfterFailure->timingOffsetMs != contentBeforeFailure->timingOffsetMs ||
            contentAfterFailure->mode != contentBeforeFailure->mode ||
            contentAfterFailure->mainMusicAssetId != contentBeforeFailure->mainMusicAssetId ||
            clockAfterFailure.presentedFrame != clockBeforeFailure.presentedFrame ||
            clockAfterFailure.source.state != clockBeforeFailure.source.state ||
            clockAfterFailure.source.discontinuityId != clockBeforeFailure.source.discontinuityId) {
            return core::unexpected(
                core::Error{"player.audio_smoke_test.failed_reload_mutated_state",
                            "Failed reload changed active playback or audio state"});
        }
        context.logger.info("player.audio_smoke_test",
                            "Failed reload preserved active playback and audio state");
    }

    if (audioSmokeTest_ && context.renderedFrames == 60) {
        if (!context.runtimeFrame) {
            return core::unexpected(std::move(context.runtimeFrame.error()));
        }
        if (context.audio == nullptr) {
            return core::unexpected(
                core::Error{"player.audio.unopened", "Audio transport was not created"});
        }
        auto replacementSource = makeSource_();
        if (!replacementSource) {
            return core::unexpected(std::move(replacementSource.error()));
        }
        auto replacement =
            context.session.prepareReload(std::move(*replacementSource), *context.runtimeFrame,
                                          playback::ReloadPolicy::KeepChartTime);
        if (!replacement) {
            return core::unexpected(std::move(replacement.error()));
        }
        auto replacementPresentation =
            context.renderer.prepare(*replacement, {.enableDebugPass = true});
        if (!replacementPresentation) {
            return core::unexpected(std::move(replacementPresentation.error())
                                        .withContext("operation", "prepare_reload_presentation"));
        }
        auto replacementHandle = prepareClip_(*replacement, context.audioStore);
        if (!replacementHandle) {
            context.renderer.discard(std::move(*replacementPresentation));
            return core::unexpected(std::move(replacementHandle.error()));
        }
        if (auto replacementPrepared = context.audio->prepareReplacement(
                *replacementHandle, context.audioClock.source.positionMs);
            !replacementPrepared) {
            const auto removed = context.audioStore.remove(*replacementHandle);
            if (!removed) {
                context.logger.warn("player.audio", "Replacement cleanup failed after prepare");
            }
            context.renderer.discard(std::move(*replacementPresentation));
            return core::unexpected(std::move(replacementPrepared.error()));
        }
        if (auto activated = context.audio->activateReplacement(); !activated) {
            const auto removed = context.audioStore.remove(*replacementHandle);
            if (!removed) {
                context.logger.warn("player.audio", "Replacement cleanup failed after activation");
            }
            context.renderer.discard(std::move(*replacementPresentation));
            return core::unexpected(std::move(activated.error()));
        }
        if (auto committed = context.session.commit(std::move(*replacement)); !committed) {
            context.renderer.discard(std::move(*replacementPresentation));
            return core::unexpected(std::move(committed.error()));
        }
        context.renderer.activate(std::move(*replacementPresentation));
        const auto contentInfo = context.session.contentInfo();
        if (!contentInfo) {
            return core::unexpected(std::move(contentInfo.error()));
        }
        if (auto reset = context.timeline.reset(contentInfo->timingOffsetMs); !reset) {
            return core::unexpected(std::move(reset.error()));
        }
        if (context.activeAudioHandle) {
            if (auto removed = context.audioStore.remove(*context.activeAudioHandle); !removed) {
                return core::unexpected(std::move(removed.error()));
            }
        }
        context.activeAudioHandle = *replacementHandle;
        if (auto serviced = context.audio->transport().service(); !serviced) {
            return core::unexpected(std::move(serviced.error()));
        }
        context.audioClock = context.audio->transport().snapshot();
        context.runtimeFrame = context.timeline.advance(context.audioClock.source);
        context.logger.info("player.audio_smoke_test", "Reload transaction completed");
    }

    if (smokeTest_ && context.renderedFrames == 3) {
        const auto rejected = context.session.prepareReload(
            R"json({"format":"cuexis.chart","version":2})json", *context.runtimeFrame,
            playback::ReloadPolicy::KeepChartTime);
        if (rejected || !backend_.hasActivePresentation()) {
            return core::unexpected(
                core::Error{"player.smoke_test.failed_reload_mutated_state",
                            "Failed presentation reload did not preserve the active GPU cache"});
        }
        context.logger.info("player.smoke_test",
                            "Failed reload preserved the active OpenGL presentation cache");

        auto replacementSource = makeSource_();
        if (!replacementSource) {
            return core::unexpected(std::move(replacementSource.error()));
        }
        auto rejectedPresentationCandidate =
            context.session.prepareReload(std::move(*replacementSource), *context.runtimeFrame,
                                          playback::ReloadPolicy::KeepChartTime);
        if (!rejectedPresentationCandidate) {
            return core::unexpected(std::move(rejectedPresentationCandidate.error()));
        }
        const auto unsupportedPresentation = context.renderer.prepare(
            *rejectedPresentationCandidate,
            {.version = 2, .portableProfileVersion = 2, .enableDebugPass = true});
        if (unsupportedPresentation || !backend_.hasActivePresentation()) {
            return core::unexpected(
                core::Error{"player.smoke_test.failed_adapter_prepare_mutated_state",
                            "Failed adapter preparation did not preserve the active GPU cache"});
        }
        context.logger.info(
            "player.smoke_test",
            "Failed adapter preparation preserved the active OpenGL presentation cache");

        auto legacyProjectPath = projectDirectory_(legacySmokeTestProjectDirectory);
        if (!legacyProjectPath) {
            return core::unexpected(std::move(legacyProjectPath.error()));
        }
        auto legacySource = playback::PlaybackSource::fromFilesystemProject(*legacyProjectPath);
        if (!legacySource) {
            return core::unexpected(std::move(legacySource.error()));
        }
        auto legacyReplacement = context.session.prepareReload(
            std::move(*legacySource), *context.runtimeFrame, playback::ReloadPolicy::KeepChartTime);
        if (!legacyReplacement) {
            return core::unexpected(std::move(legacyReplacement.error()));
        }
        const auto legacyPresentation =
            context.renderer.prepare(*legacyReplacement, {.enableDebugPass = true});
        if (legacyPresentation ||
            legacyPresentation.error().code() !=
                "render.opengl.presentation.portable_candidate_required" ||
            !backend_.hasActivePresentation()) {
            return core::unexpected(core::Error{
                "player.smoke_test.legacy_adapter_prepare_accepted",
                "OpenGL accepted a legacy candidate or changed the active presentation"});
        }
        context.logger.info("player.smoke_test",
                            "Legacy presentation candidate was rejected before Playback commit");
    }

    if (smokeTest_ && context.renderedFrames == 4) {
        auto replacementSource = makeSource_();
        if (!replacementSource) {
            return core::unexpected(std::move(replacementSource.error()));
        }
        auto replacement =
            context.session.prepareReload(std::move(*replacementSource), *context.runtimeFrame,
                                          playback::ReloadPolicy::RestartAtZero);
        if (!replacement) {
            return core::unexpected(std::move(replacement.error()));
        }
        auto replacementPresentation =
            context.renderer.prepare(*replacement, {.enableDebugPass = true});
        if (!replacementPresentation) {
            return core::unexpected(std::move(replacementPresentation.error())
                                        .withContext("operation", "prepare_smoke_reload"));
        }

        auto competingSource = makeSource_();
        if (!competingSource) {
            context.renderer.discard(std::move(*replacementPresentation));
            return core::unexpected(std::move(competingSource.error()));
        }
        auto competingReplacement =
            context.session.prepareReload(std::move(*competingSource), *context.runtimeFrame,
                                          playback::ReloadPolicy::RestartAtZero);
        if (!competingReplacement) {
            context.renderer.discard(std::move(*replacementPresentation));
            return core::unexpected(std::move(competingReplacement.error()));
        }
        const auto competingPresentation =
            context.renderer.prepare(*competingReplacement, {.enableDebugPass = true});
        if (competingPresentation ||
            competingPresentation.error().code() !=
                "render.opengl.presentation.candidate_outstanding" ||
            !replacementPresentation->valid() || !backend_.hasActivePresentation()) {
            context.renderer.discard(std::move(*replacementPresentation));
            return core::unexpected(core::Error{
                "player.smoke_test.outstanding_candidate_overwritten",
                "A second OpenGL candidate was accepted or invalidated the first candidate"});
        }
        if (auto committed = context.session.commit(std::move(*replacement)); !committed) {
            context.renderer.discard(std::move(*replacementPresentation));
            return core::unexpected(std::move(committed.error()));
        }
        context.renderer.activate(std::move(*replacementPresentation));
        context.logger.info("player.smoke_test",
                            "Outstanding candidate rejection preserved the first candidate");
        const auto contentInfo = context.session.contentInfo();
        if (!contentInfo) {
            return core::unexpected(std::move(contentInfo.error()));
        }
        if (auto reset = context.timeline.reset(contentInfo->timingOffsetMs); !reset) {
            return core::unexpected(std::move(reset.error()));
        }
        auto source = context.chartClock.sample(0.0);
        if (!source) {
            return core::unexpected(std::move(source.error()));
        }
        context.runtimeFrame = context.timeline.advance(*source);
        if (!context.runtimeFrame) {
            return core::unexpected(std::move(context.runtimeFrame.error()));
        }
        context.logger.info("player.smoke_test",
                            "Successful reload activated a complete OpenGL presentation cache");
    }
    return {};
}

auto PlayerSmokeBinding::beforeSubmit(const playback::FrameSnapshot& snapshot,
                                      std::uint32_t renderedFrames) -> core::Result<void> {
    if (renderedFrames != 1) {
        return {};
    }
    omittedDebugSummary_.emplace();
    if (auto result = backend_.renderPresentationFrame(snapshot, nullptr, &*omittedDebugSummary_);
        !result) {
        return core::unexpected(
            std::move(result.error()).withContext("operation", "debug_summary_omitted"));
    }
    render::RenderScene emptyScene;
    emptyDebugSummary_.emplace();
    if (auto result = backend_.renderPresentationFrame(snapshot, &emptyScene, &*emptyDebugSummary_);
        !result) {
        return core::unexpected(
            std::move(result.error()).withContext("operation", "debug_summary_empty"));
    }
    return {};
}

auto PlayerSmokeBinding::notePresented(const PlayerPresentedFrame& presented)
    -> core::Result<void> {
    if (!omittedDebugSummary_ || !emptyDebugSummary_) {
        return {};
    }
    const auto drawSummary = copyNeutralSummary(presented.summary);
    if (omittedDebugSummary_->debugPassEnabled != emptyDebugSummary_->debugPassEnabled ||
        omittedDebugSummary_->debugPassEnabled != drawSummary.debugPassEnabled ||
        omittedDebugSummary_->digest != emptyDebugSummary_->digest ||
        omittedDebugSummary_->digest != drawSummary.digest) {
        return core::unexpected(
            core::Error{"player.smoke_test.debug_summary_mismatch",
                        "OpenGL Debug summary differs across scene argument forms"}
                .withContext("omitted_digest", std::to_string(omittedDebugSummary_->digest))
                .withContext("empty_digest", std::to_string(emptyDebugSummary_->digest))
                .withContext("populated_digest", std::to_string(drawSummary.digest)));
    }
    logger_.info("player.smoke_test",
                 "Debug summary parity held for omitted, empty, and populated scenes");
    omittedDebugSummary_.reset();
    emptyDebugSummary_.reset();
    return {};
}

auto PlayerSmokeBinding::validatePresented(const PlayerPresentedFrame& presented)
    -> core::Result<void> {
    const auto drawSummary = copyNeutralSummary(presented.summary);
    const auto pixelProbe = backend_.lastPixelProbe();
    if (auto validated =
            validateFrame(presented.renderedFrames, presented.snapshot, drawSummary, pixelProbe);
        !validated) {
        return core::unexpected(std::move(validated.error()));
    }
    logger_.info("player.smoke_test",
                 std::string{"Frame "} + std::to_string(presented.renderedFrames) +
                     " summary=" + std::to_string(drawSummary.digest) +
                     " pixel=" + std::to_string(pixelProbe.rgba[0]) + "," +
                     std::to_string(pixelProbe.rgba[1]) + "," + std::to_string(pixelProbe.rgba[2]) +
                     "," + std::to_string(pixelProbe.rgba[3]) +
                     " render_us=" + std::to_string(presented.renderMicroseconds));
    if (presented.renderedFrames == 0) {
        if (auto minimized = verifyMinimizeRestore(presented, drawSummary.digest, pixelProbe);
            !minimized) {
            return core::unexpected(std::move(minimized.error()));
        }
    }
    return {};
}

auto PlayerSmokeBinding::waitForMinimized(bool wantMinimized) -> core::Result<void> {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (std::chrono::steady_clock::now() < deadline) {
        if (window_.pollEvents().quitRequested) {
            return core::unexpected(
                core::Error{"player.smoke_test.minimize_quit",
                            "The smoke window closed during minimize or restore"});
        }
        auto state = window_.minimized();
        if (!state) {
            return core::unexpected(std::move(state.error()));
        }
        if (*state == wantMinimized) {
            return {};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{16});
    }
    return core::unexpected(core::Error{"player.smoke_test.minimize_timeout",
                                        wantMinimized ? "The smoke window did not minimize"
                                                      : "The smoke window did not restore"});
}

auto PlayerSmokeBinding::verifyMinimizeRestore(const PlayerPresentedFrame& presented,
                                               std::uint64_t baselineDigest,
                                               const render_opengl::OpenGlPixelProbe& baselineProbe)
    -> core::Result<void> {
    if (!presented.renderer.hasActivePresentation()) {
        return core::unexpected(core::Error{"player.smoke_test.minimize_lost_cache",
                                            "Minimize started without an active presentation"});
    }
    if (auto minimized = window_.setMinimized(true); !minimized) {
        return core::unexpected(std::move(minimized.error()));
    }
    if (auto waited = waitForMinimized(true); !waited) {
        return core::unexpected(std::move(waited.error()));
    }
    auto minimizedSize = window_.drawableSize();
    if (!minimizedSize) {
        return core::unexpected(std::move(minimizedSize.error()));
    }
    const auto suspendedWidth = static_cast<std::uint32_t>(std::max(minimizedSize->width, 0));
    const auto suspendedHeight = static_cast<std::uint32_t>(std::max(minimizedSize->height, 0));
    if (auto resized = presented.renderer.resize(suspendedWidth, suspendedHeight); !resized) {
        return core::unexpected(std::move(resized.error()));
    }
    if (suspendedWidth == 0 || suspendedHeight == 0) {
        playback::FrameSnapshot suspendedSnapshot;
        if (auto extracted = presented.session.extractFrame(
                {.width = suspendedWidth, .height = suspendedHeight}, suspendedSnapshot);
            !extracted) {
            return core::unexpected(std::move(extracted.error()));
        }
        auto suspended = presented.renderer.submit(suspendedSnapshot, nullptr);
        if (suspended) {
            return core::unexpected(core::Error{"player.smoke_test.minimize_still_submitted",
                                                "A zero-size minimized surface still submitted"});
        }
        if (suspended.error().code() != "presentation.renderer.surface.zero_size") {
            return core::unexpected(std::move(suspended.error()));
        }
    }
    if (!presented.renderer.hasActivePresentation()) {
        return core::unexpected(core::Error{"player.smoke_test.minimize_lost_cache",
                                            "Minimize dropped the active presentation"});
    }
    logger_.info("player.smoke_test", std::string{"Minimized drawable "} +
                                          std::to_string(minimizedSize->width) + "x" +
                                          std::to_string(minimizedSize->height));

    if (auto restored = window_.setMinimized(false); !restored) {
        return core::unexpected(std::move(restored.error()));
    }
    if (auto waited = waitForMinimized(false); !waited) {
        return core::unexpected(std::move(waited.error()));
    }
    auto restoredSize = window_.drawableSize();
    if (!restoredSize || restoredSize->width <= 0 || restoredSize->height <= 0) {
        return core::unexpected(core::Error{"player.smoke_test.restore_size_invalid",
                                            "The restored smoke window has no drawable size"});
    }
    const auto width = static_cast<std::uint32_t>(restoredSize->width);
    const auto height = static_cast<std::uint32_t>(restoredSize->height);
    if (auto resized = presented.renderer.resize(width, height); !resized) {
        return core::unexpected(std::move(resized.error()));
    }
    playback::FrameSnapshot restoredSnapshot;
    if (auto extracted =
            presented.session.extractFrame({.width = width, .height = height}, restoredSnapshot);
        !extracted) {
        return core::unexpected(std::move(extracted.error()));
    }
    render::RenderScene restoredScene;
    if (auto axes = appendSnapshotAxes(restoredSnapshot, restoredScene); !axes) {
        return core::unexpected(std::move(axes.error()));
    }
    auto submitted = presented.renderer.submit(restoredSnapshot, &restoredScene);
    if (!submitted) {
        return core::unexpected(std::move(submitted.error()));
    }
    if (auto presentedFrame = presented.renderer.present(); !presentedFrame) {
        return core::unexpected(std::move(presentedFrame.error()));
    }
    if (submitted->digest != baselineDigest) {
        return core::unexpected(core::Error{"player.smoke_test.restore_digest_mismatch",
                                            "Restoring the window changed the presentation digest"}
                                    .withContext("baseline", std::to_string(baselineDigest))
                                    .withContext("restored", std::to_string(submitted->digest)));
    }
    const auto probe = backend_.lastPixelProbe();
    for (std::size_t component = 0; component < 4; ++component) {
        if (probe.rgba[component] != baselineProbe.rgba[component]) {
            return core::unexpected(core::Error{"player.smoke_test.restore_pixel_mismatch",
                                                "Restoring the window changed the center pixel"});
        }
    }
    if (!presented.renderer.hasActivePresentation()) {
        return core::unexpected(core::Error{"player.smoke_test.restore_lost_cache",
                                            "Restore dropped the active presentation"});
    }
    logger_.info("player.smoke_test", std::string{"Restore kept digest "} +
                                          std::to_string(baselineDigest) + " at drawable " +
                                          std::to_string(restoredSize->width) + "x" +
                                          std::to_string(restoredSize->height));
    return {};
}

auto PlayerSmokeBinding::validateFrame(std::uint32_t frameIndex,
                                       const playback::FrameSnapshot& snapshot,
                                       const render_opengl::OpenGlDrawSummary& summary,
                                       const render_opengl::OpenGlPixelProbe& probe)
    -> core::Result<void> {
    const bool expectedEmpty = frameIndex == 2;
    const bool expectedOpaque = frameIndex == 0 || frameIndex == 4;
    if (expectedEmpty) {
        if (!summary.opaque.empty() || !summary.transparent.empty() || probe.presentationDrawn) {
            return core::unexpected(
                core::Error{"player.smoke_test.invisible_frame_drew",
                            "Invisible smoke frame produced presentation draws"});
        }
    } else if (expectedOpaque) {
        if (summary.opaque.size() != 1 || !summary.transparent.empty() ||
            !probe.presentationDrawn) {
            return core::unexpected(core::Error{"player.smoke_test.opaque_frame_invalid",
                                                "Opaque smoke frame did not draw one Mesh"});
        }
    } else if (!summary.opaque.empty() || summary.transparent.size() != 1 ||
               !probe.presentationDrawn) {
        return core::unexpected(core::Error{"player.smoke_test.transparent_frame_invalid",
                                            "Transparent smoke frame did not draw one Mesh"});
    }

    const auto toByte = [](float value) {
        return static_cast<int>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    };
    const std::array clearBytes{toByte(snapshot.clearRed), toByte(snapshot.clearGreen),
                                toByte(snapshot.clearBlue), toByte(snapshot.clearAlpha)};
    int colorDifference = 0;
    for (std::size_t component = 0; component < 3; ++component) {
        colorDifference +=
            std::abs(static_cast<int>(probe.rgba[component]) - clearBytes[component]);
    }
    if (expectedEmpty && colorDifference > 9) {
        return core::unexpected(
            core::Error{"player.smoke_test.clear_pixel_invalid",
                        "Invisible smoke frame did not preserve the clear color"});
    }
    if (!expectedEmpty && colorDifference < 12) {
        return core::unexpected(core::Error{"player.smoke_test.presentation_pixel_missing",
                                            "Visible smoke frame did not change the center pixel"});
    }
    const auto pixelNear = [&probe](const std::array<int, 4>& expected, int tolerance) {
        for (std::size_t component = 0; component < expected.size(); ++component) {
            if (std::abs(static_cast<int>(probe.rgba[component]) - expected[component]) >
                tolerance) {
                return false;
            }
        }
        return true;
    };
    if (expectedOpaque && !pixelNear({255, 204, 51, 255}, 8)) {
        return core::unexpected(
            core::Error{"player.smoke_test.unlit_material_pixel_invalid",
                        "Opaque smoke frame did not produce the expected Unlit Material color"});
    }
    if ((frameIndex == 1 || frameIndex == 5) && !pixelNear({51, 32, 71, 192}, 16)) {
        return core::unexpected(
            core::Error{"player.smoke_test.textured_blend_pixel_invalid",
                        "Transparent smoke frame did not produce the expected textured blend"});
    }
    if ((frameIndex == 1 || frameIndex == 5) && summary.digest != presentationSmokeDigest) {
        return core::unexpected(
            core::Error{"player.smoke_test.presentation_digest_mismatch",
                        "OpenGL draw summary differs from the Validation Sink golden"}
                .withContext("expected", std::to_string(presentationSmokeDigest))
                .withContext("actual", std::to_string(summary.digest)));
    }
    return {};
}

} // namespace cuexis::player
