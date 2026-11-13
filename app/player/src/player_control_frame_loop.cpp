#include "player_control.hpp"

#include "frame_diagnostics.hpp"
#include "player_log.hpp"
#include "snapshot_scene.hpp"
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
#include "scripted_playback_trace.hpp"
#endif

#include <cuexis/render/render_scene.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace cuexis::player {

constexpr std::uint32_t smokeTestFrameCount = 6;
constexpr std::uint32_t audioSmokeTestFrameCount = 90;
constexpr std::size_t chartInputMaxBytes = 16U * 1024U * 1024U;
constexpr std::array<double, smokeTestFrameCount> presentationSmokeTimes{0.0,    625.0,  1250.0,
                                                                         1750.0, 1750.0, 625.0};

// Wall clock for a ChartClock session. A seek or a resume rebases the origin so the sampled chart
// time continues from the requested target.
class PlayerClock final {
  public:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] double nextChartTime(bool deterministic, std::uint32_t frameIndex) {
        if (deterministic) {
            last_ = presentationSmokeTimes[std::min<std::size_t>(
                frameIndex, presentationSmokeTimes.size() - 1U)];
            return last_;
        }
        last_ = base_ + elapsedMs();
        return last_;
    }

    void rebase(double targetMs) noexcept {
        base_ = targetMs - elapsedMs();
        last_ = targetMs;
    }

    [[nodiscard]] double lastChartTime() const noexcept {
        return last_;
    }

  private:
    [[nodiscard]] double elapsedMs() const noexcept {
        return std::chrono::duration<double, std::milli>(Clock::now() - started_).count();
    }

    const Clock::time_point started_{Clock::now()};
    double base_{};
    double last_{};
};

struct NullJudgeSystem final {
    void update(double) const noexcept {}
};

auto runPlayerFrameLoop(PlayerFrameLoop& input) -> core::Result<void> {
    PlayerClock clock;
    const NullJudgeSystem judgeSystem;
    const auto diagnosticsStarted = std::chrono::steady_clock::now();
    std::uint32_t renderedFrames = 0;
    PlayerFpsMeter fpsMeter;
    bool quitRequested = false;
    bool traceReloadFrame = false;
    playback::FrameSnapshot snapshot;
    render::RenderScene scene;
    auto& controller = input.controller;

    // Samples the active clock and advances the timeline. Called once per frame, and again when a
    // command inside a hook replaced the active bundle.
    const auto advanceFrame = [&](playback::RuntimeFrame& frame,
                                  audio::AudioClockSnapshot& audioClock) -> core::Result<void> {
        if (auto* audio = controller.audio(); audio != nullptr) {
            if (auto serviced = audio->transport().service(); !serviced) {
                return core::unexpected(std::move(serviced.error()));
            }
            if (auto rebound = audio->recheckBoundDevice(); !rebound) {
                return core::unexpected(std::move(rebound.error()));
            }
            audioClock = audio->transport().snapshot();
            controller.observeTransport(audioClock.source);
            auto consumedSource = audioClock.source;
            if (controller.sessionConfig().outputCorrectionUs != 0) {
                auto corrected = player_support::correctConsumedAudioPositionMs(
                    consumedSource.positionMs, controller.sessionConfig().outputCorrectionUs);
                if (!corrected) {
                    return core::unexpected(std::move(corrected.error()));
                }
                consumedSource.positionMs = *corrected;
            }
            auto advanced = controller.timeline().advance(consumedSource);
            if (!advanced) {
                return core::unexpected(std::move(advanced.error()));
            }
            frame = *advanced;
            return {};
        }
        double chartTimeMs = 0.0;
        if (auto target = controller.consumeChartSeekTarget(); target.has_value()) {
            clock.rebase(*target);
            chartTimeMs = *target;
        } else if (controller.state() == player_support::PlayerAppState::Playing) {
            chartTimeMs = clock.nextChartTime(input.smokeTest, renderedFrames);
        } else {
            chartTimeMs = clock.lastChartTime();
        }
        auto source = controller.chartClock().sample(chartTimeMs);
        if (!source) {
            return core::unexpected(std::move(source.error()));
        }
        auto advanced = controller.timeline().advance(*source);
        if (!advanced) {
            return core::unexpected(std::move(advanced.error()));
        }
        frame = *advanced;
        return {};
    };

    while (!quitRequested) {
        // The discontinuity the session last received. A command below may replace the bundle, and
        // the frame this iteration delivers is then the first frame of a new discontinuity.
        const auto deliveredDiscontinuityId = controller.lastRuntimeFrame().timeDiscontinuityId;
        auto observedInput = input.surface.pollInput();
        if (!observedInput) {
            return core::unexpected(std::move(observedInput.error()));
        }
        quitRequested = observedInput->quitRequested;
        if (quitRequested) {
            break;
        }
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        auto* gameplay = controller.gameplay();
        bool admitGameplayInput = controller.state() == player_support::PlayerAppState::Playing &&
                                  !observedInput->focusLost;
        if (gameplay && observedInput->focusLost &&
            controller.state() == player_support::PlayerAppState::Playing) {
            auto paused =
                controller.apply(PlayerCommand{.kind = player_support::PlayerCommandKind::Pause});
            if (!paused)
                return core::unexpected(std::move(paused.error()));
        }
#endif
        // Every user action enters through the same command entry as the smoke scripts. A rejected
        // action is a warning, not a failure: pressing Pause before content exists must not stop
        // the player.
        for (const auto action : observedInput->actions) {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
            if (gameplay && observedInput->focusLost && action != PlayerInputAction::Quit)
                continue;
            if (gameplay)
                admitGameplayInput = false;
            if (gameplay && (action == PlayerInputAction::SeekBackward ||
                             action == PlayerInputAction::SeekForward)) {
                const auto h = gameplay->horizon().value, distance = gameplay->profile().hStep;
                if (action == PlayerInputAction::SeekForward &&
                    h > std::numeric_limits<std::int64_t>::max() - distance)
                    return core::unexpected(
                        core::Error{"player.arguments.unknown", "Gameplay seek Tick overflow"});
                const auto target = action == PlayerInputAction::SeekBackward
                                        ? std::max<std::int64_t>(0, h - distance)
                                        : h + distance;
                auto sought =
                    gameplay->seek(controller.session(), {target}, controller.lastRuntimeFrame());
                if (sought && controller.state() != player_support::PlayerAppState::Playing)
                    sought = controller.session().controlGameplay(playback::GameplayControl::Pause);
                if (!sought)
                    input.logger.warn("player.command", std::string{sought.error().code()});
                continue;
            }
#endif
            if (action == PlayerInputAction::Quit) {
                quitRequested = true;
                break;
            }
            auto command = playerCommandForInput(action, controller.state(),
                                                 controller.lastRuntimeFrame().chartTimeMs);
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
            const bool measuredPlay = command.kind == player_support::PlayerCommandKind::Play;
            if (input.measurementTrace &&
                (action == PlayerInputAction::PlayPause || action == PlayerInputAction::Reload)) {
                if (measuredPlay && controller.state() != player_support::PlayerAppState::Paused)
                    input.measurementTrace->begin("play");
                else
                    input.measurementTrace->action(action == PlayerInputAction::Reload,
                                                   controller.state() ==
                                                       player_support::PlayerAppState::Paused);
            }
#endif
            if (auto applied = controller.apply(std::move(command)); !applied) {
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
                if (input.measurementTrace && input.measurementTrace->active())
                    return core::unexpected(std::move(applied.error()));
#endif
                input.logger.warn("player.command", std::string{"Input action rejected: "} +
                                                        std::string{applied.error().code()});
            } else {
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
                if (input.measurementTrace)
                    input.measurementTrace->confirmed();
                if (input.measurementTrace && (measuredPlay || action == PlayerInputAction::Reload))
                    input.measurementTrace->resumed();
#endif
                if (action == PlayerInputAction::Reload)
                    traceReloadFrame = true;
                input.logger.info("player.input", std::string{"Applied user action: "} +
                                                      std::string{playerInputActionName(action)});
            }
        }
        if (quitRequested) {
            break;
        }

        if (input.hooks.beforeAudioService) {
            if (auto stepped = input.hooks.beforeAudioService(renderedFrames); !stepped) {
                return core::unexpected(std::move(stepped.error()));
            }
        }

        audio::AudioClockSnapshot audioClockSnapshot;
        playback::RuntimeFrame advancedFrame{};
        if (traceReloadFrame)
            input.logger.info("player.reload.frame", "Begin audio service and timeline advance");
        auto advanceResult = advanceFrame(advancedFrame, audioClockSnapshot);
        if (traceReloadFrame)
            input.logger.info("player.reload.frame", "Audio service and timeline advance returned");
        core::Result<playback::RuntimeFrame> runtimeFrameResult =
            advanceResult ? core::Result<playback::RuntimeFrame>{advancedFrame}
                          : core::unexpected(std::move(advanceResult.error()));
        if (runtimeFrameResult) {
            controller.noteRuntimeFrame(*runtimeFrameResult);
        }
        if (input.hooks.afterTimelineAdvance) {
            PlayerClockContext clockContext{.renderedFrames = renderedFrames,
                                            .controller = controller,
                                            .renderer = input.renderer,
                                            .runtimeFrame = runtimeFrameResult,
                                            .audioClock = audioClockSnapshot,
                                            .logger = input.logger};
            if (auto stepped = input.hooks.afterTimelineAdvance(clockContext); !stepped) {
                return core::unexpected(std::move(stepped.error()));
            }
        }
        if (!runtimeFrameResult) {
            return core::unexpected(std::move(runtimeFrameResult.error()));
        }
        // A transaction inside the hook replaced the active bundle, so this frame must belong to
        // the new content rather than to the bundle that was active when the frame started.
        if (controller.consumeBundleReplaced()) {
            auto refreshed = advanceFrame(advancedFrame, audioClockSnapshot);
            if (!refreshed) {
                return core::unexpected(std::move(refreshed.error()));
            }
            // The re-advanced frame is the one the session receives, so it carries the first frame
            // of the new bundle. When it also carries a new discontinuity it must report a zero
            // delta, exactly like any other first frame after a discontinuity: the session compares
            // it against the frame the previous iteration delivered, not against the superseded
            // sample this iteration took first.
            if (advancedFrame.timeDiscontinuityId != deliveredDiscontinuityId) {
                advancedFrame.simulationDeltaTimeMs = 0.0;
            }
            runtimeFrameResult = advancedFrame;
        }
        const auto runtimeFrame = *runtimeFrameResult;
        controller.noteRuntimeFrame(runtimeFrame);
        judgeSystem.update(runtimeFrame.chartTimeMs);
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (auto* adapter = controller.gameplay(); adapter) {
            if (controller.state() == player_support::PlayerAppState::Playing) {
                auto score = adapter->step(controller.session(),
                                           admitGameplayInput ? *observedInput : PlayerInput{},
                                           runtimeFrame);
                if (!score)
                    return core::unexpected(std::move(score.error()));
                input.logger.info("player.gameplay",
                                  "H=" + std::to_string(adapter->horizon().value) +
                                      " score=" + std::to_string(score->score) +
                                      " combo=" + std::to_string(score->combo) +
                                      " hits=" + std::to_string(score->hits) + " misses=" +
                                      std::to_string(score->misses) + " completeReplay=same");
            }
        } else
#endif
            if (auto result = controller.session().update(runtimeFrame); !result)
            return core::unexpected(std::move(result.error()));

        auto drawableSizeResult = input.surface.drawableSize();
        if (traceReloadFrame)
            input.logger.info("player.reload.frame", "Runtime update returned");
        if (!drawableSizeResult) {
            return core::unexpected(std::move(drawableSizeResult.error())
                                        .withContext("operation", "query_drawable_size"));
        }
        const auto width = static_cast<std::uint32_t>(std::max(drawableSizeResult->width, 0));
        const auto height = static_cast<std::uint32_t>(std::max(drawableSizeResult->height, 0));
        if (auto resized = input.renderer.resize(width, height); !resized) {
            return core::unexpected(
                std::move(resized.error()).withContext("operation", "resize_presentation"));
        }
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
        if (input.measurementTrace)
            input.measurementTrace->confirmed("resize");
#endif
        if (auto result =
                controller.session().extractFrame({.width = width, .height = height}, snapshot);
            !result) {
            return core::unexpected(std::move(result.error()));
        }

        scene.clear();
        if (traceReloadFrame)
            input.logger.info("player.reload.frame", "Frame extraction returned");
        if (input.debugAxes)
            if (auto result = appendSnapshotAxes(snapshot, scene); !result) {
                return core::unexpected(std::move(result.error()));
            }
        if (input.diagnostics != nullptr) {
            input.diagnostics->captureFrame(renderedFrames, runtimeFrame, snapshot);
            if (auto* audio = controller.audio(); audio != nullptr) {
                const double wallClockMs =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                              diagnosticsStarted)
                        .count();
                input.diagnostics->captureAudio(renderedFrames, wallClockMs, audioClockSnapshot,
                                                audio->transport().metrics());
            }
        }

        if (input.hooks.beforeSubmit) {
            if (auto stepped = input.hooks.beforeSubmit(snapshot, renderedFrames); !stepped) {
                return core::unexpected(std::move(stepped.error()));
            }
        }

        const auto renderStarted = std::chrono::steady_clock::now();
        if (input.showFps)
            if (auto overlay = appendFpsOverlay(snapshot, fpsMeter.reading(), scene); !overlay)
                return overlay;
        auto submitted = input.renderer.submit(snapshot, &scene);
        if (traceReloadFrame)
            input.logger.info("player.reload.frame", "Renderer submit returned");
        if (!submitted) {
            if (submitted.error().code() == "presentation.renderer.surface.zero_size") {
                continue;
            }
            controller.markFailed("present_submit");
            return core::unexpected(
                std::move(submitted.error()).withContext("frame", std::to_string(renderedFrames)));
        }
        if (auto presented = input.renderer.present(); !presented) {
            controller.markFailed("present");
            return core::unexpected(
                std::move(presented.error()).withContext("frame", std::to_string(renderedFrames)));
        }
        if (traceReloadFrame) {
            input.logger.info("player.reload.frame",
                              "Renderer present returned; first frame complete");
            traceReloadFrame = false;
        }
        PlayerPresentedFrame presented{.renderedFrames = renderedFrames,
                                       .snapshot = snapshot,
                                       .summary = *submitted,
                                       .renderMicroseconds = 0.0,
                                       .renderer = input.renderer,
                                       .controller = controller};
        if (input.showFps)
            fpsMeter.notePresented();
        if (input.hooks.notePresented) {
            if (auto noted = input.hooks.notePresented(presented); !noted) {
                return core::unexpected(std::move(noted.error()));
            }
        }
        presented.renderMicroseconds = std::chrono::duration<double, std::micro>(
                                           std::chrono::steady_clock::now() - renderStarted)
                                           .count();
        if (renderedFrames == 0) {
            input.logger.info(
                "player.snapshot",
                std::string{"Objects: "} + std::to_string(snapshot.objects.size()) +
                    ", opaque draws: " + std::to_string(submitted->opaque.size()) +
                    ", transparent draws: " + std::to_string(submitted->transparent.size()) +
                    ", debug commands: " + std::to_string(submitted->debugCommandCount));
        }
        if (input.hooks.validatePresented) {
            if (auto validated = input.hooks.validatePresented(presented); !validated) {
                return core::unexpected(std::move(validated.error()));
            }
        }

        ++renderedFrames;
        if (input.smokeTest && renderedFrames >= smokeTestFrameCount) {
            quitRequested = true;
        }
        if (input.audioSmokeTest && renderedFrames >= audioSmokeTestFrameCount) {
            quitRequested = true;
        }
    }

    if (input.smokeTest && renderedFrames != smokeTestFrameCount) {
        return core::unexpected(
            core::Error{"player.smoke_test.incomplete",
                        "Smoke test ended before rendering required frames"}
                .withContext("expected_frames", std::to_string(smokeTestFrameCount))
                .withContext("rendered_frames", std::to_string(renderedFrames)));
    }
    if (input.smokeTest) {
        input.logger.info("player.smoke_test",
                          std::string{"Completed frames: "} + std::to_string(renderedFrames));
    }
    if (input.audioSmokeTest && renderedFrames != audioSmokeTestFrameCount) {
        return core::unexpected(
            core::Error{"player.audio_smoke_test.incomplete",
                        "Audio smoke test ended before rendering required frames"}
                .withContext("expected_frames", std::to_string(audioSmokeTestFrameCount))
                .withContext("rendered_frames", std::to_string(renderedFrames)));
    }
    if (input.audioSmokeTest) {
        input.logger.info("player.audio_smoke_test",
                          std::string{"Completed frames: "} + std::to_string(renderedFrames));
    }
    return {};
}

} // namespace cuexis::player
