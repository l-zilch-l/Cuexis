#include "player_control.hpp"

#include "frame_diagnostics.hpp"
#include "player_log.hpp"
#include "snapshot_scene.hpp"

#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/render/render_scene.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

namespace cuexis::player {
namespace {

constexpr std::uint32_t smokeTestFrameCount = 6;
constexpr std::uint32_t audioSmokeTestFrameCount = 90;
constexpr std::size_t chartInputMaxBytes = 16U * 1024U * 1024U;
constexpr std::array<double, smokeTestFrameCount> presentationSmokeTimes{0.0,    625.0,  1250.0,
                                                                         1750.0, 1750.0, 625.0};

class PlayerClock final {
  public:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] double nextChartTime(bool deterministic, std::uint32_t frameIndex) {
        if (deterministic) {
            return presentationSmokeTimes[std::min<std::size_t>(
                frameIndex, presentationSmokeTimes.size() - 1U)];
        }
        const auto now = Clock::now();
        return std::chrono::duration<double, std::milli>(now - started_).count();
    }

  private:
    const Clock::time_point started_{Clock::now()};
};

struct NullInputSource final {
    void poll() const noexcept {}
};

struct NullJudgeSystem final {
    void update(double) const noexcept {}
};

[[nodiscard]] auto clockModeFor(playback::PlaybackMode mode) -> player_support::PlaybackClockMode {
    switch (mode) {
    case playback::PlaybackMode::CuexisAudio:
        return player_support::PlaybackClockMode::CuexisAudio;
    case playback::PlaybackMode::HostClock:
        return player_support::PlaybackClockMode::HostClock;
    case playback::PlaybackMode::ChartClock:
        return player_support::PlaybackClockMode::ChartClock;
    }
    return player_support::PlaybackClockMode::ChartClock;
}

[[nodiscard]] auto freezeSessionConfig(playback::PlaybackMode mode,
                                       std::int64_t profileCorrectionUs)
    -> player_support::ResolvedSessionConfig {
    player_support::ResolvedSessionConfig config;
    config.clockMode = clockModeFor(mode);
    config.outputCorrectionUs =
        player_support::consumedOutputCorrectionUs(config.clockMode, profileCorrectionUs);
    return config;
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

[[nodiscard]] auto readBoundedFile(const std::filesystem::path& path,
                                   const std::filesystem::path& root, std::size_t maxBytes,
                                   std::string_view errorPrefix, std::string_view description)
    -> core::Result<std::string> {
    const auto prefix = std::string{errorPrefix};
    auto contents = filesystem::readBoundedTextFile(
        path, {.root = root,
               .maxBytes = maxBytes,
               .errors = {.rootUnavailable = prefix + ".open_failed",
                          .rootChanged = prefix + ".root_changed",
                          .openFailed = prefix + ".open_failed",
                          .outsideRoot = prefix + ".outside_root",
                          .notRegular = prefix + ".open_failed",
                          .tooLarge = prefix + ".file_too_large",
                          .readFailed = prefix + ".read_failed",
                          .changedDuringRead = prefix + ".changed_during_read"}});
    if (!contents) {
        return core::unexpected(
            std::move(contents.error()).withContext("description", std::string{description}));
    }
    return std::move(contents->text);
}

} // namespace

auto openConfiguredPlaybackSource(const PlayerOptions& options)
    -> core::Result<playback::PlaybackSource> {
    if (options.projectPath.has_value()) {
        auto source = playback::PlaybackSource::fromFilesystemProject(*options.projectPath);
        if (!source) {
            return core::unexpected(core::Error{"player.project.load_failed",
                                                "Filesystem Playback source loading failed"}
                                        .withCause(std::move(source.error())));
        }
        return source;
    }
    const auto chartPath = *options.chartPath;
    const auto chartRoot =
        chartPath.parent_path().empty() ? std::filesystem::path{"."} : chartPath.parent_path();
    auto chartText =
        readBoundedFile(chartPath, chartRoot, chartInputMaxBytes, "player.chart", "chart");
    if (!chartText) {
        return core::unexpected(std::move(chartText.error()));
    }
    return playback::PlaybackSource::fromChartText(std::move(*chartText));
}

auto preparePlayerContent(playback::PlaybackSession& session, const PlayerOptions& options,
                          std::int64_t profileCorrectionUs, PlayerLogger& logger)
    -> core::Result<PreparedPlayerContent> {
    auto mode = playback::PlaybackMode::ChartClock;
    auto sessionConfig = freezeSessionConfig(mode, profileCorrectionUs);
    auto sourceResult = openConfiguredPlaybackSource(options);
    if (!sourceResult) {
        return core::unexpected(std::move(sourceResult.error()));
    }
    auto preparedResult = session.prepareLoad(std::move(*sourceResult), mode);
    if (!preparedResult && preparedResult.error().code() == "playback.mode.content_mismatch") {
        mode = playback::PlaybackMode::CuexisAudio;
        sessionConfig = freezeSessionConfig(mode, profileCorrectionUs);
        sourceResult = openConfiguredPlaybackSource(options);
        if (!sourceResult) {
            return core::unexpected(std::move(sourceResult.error()));
        }
        preparedResult = session.prepareLoad(std::move(*sourceResult), mode);
    }
    auto sessionIdentity = player_support::resolvedSessionConfigIdentity(sessionConfig);
    logger.info("player.config", std::string{"Session identity "} + identityText(sessionIdentity));
    if (!preparedResult) {
        return core::unexpected(std::move(preparedResult.error()));
    }
    if (options.audioSmokeTest && mode != playback::PlaybackMode::CuexisAudio) {
        return core::unexpected(core::Error{"player.audio_smoke_test.audio_required",
                                            "Audio smoke test requires a chart with main music"});
    }
    auto prepared = std::move(*preparedResult);
    const auto* preparedInfo = prepared.contentInfo();
    if (preparedInfo == nullptr) {
        return core::unexpected(core::Error{"player.playback.prepared_invalid",
                                            "Prepared Playback content metadata is unavailable"});
    }
    const auto timingOffsetMs = preparedInfo->timingOffsetMs;
    auto timelineResult = playback::RuntimeTimeline::create(timingOffsetMs);
    if (!timelineResult) {
        return core::unexpected(std::move(timelineResult.error()));
    }
    return PreparedPlayerContent{.prepared = std::move(prepared),
                                 .timeline = std::move(*timelineResult),
                                 .chartClock = playback::ChartClock{timingOffsetMs},
                                 .mode = mode,
                                 .sessionConfig = sessionConfig,
                                 .timingOffsetMs = timingOffsetMs};
}

auto activatePreparedPlayback(playback::PlaybackSession& session,
                              playback::PreparedPlayback& prepared,
                              presentation_renderer::IPresentationRenderer& renderer,
                              PlayerLogger& logger) -> core::Result<void> {
    auto presentationCandidate = renderer.prepare(prepared, {.enableDebugPass = true});
    if (!presentationCandidate) {
        return core::unexpected(std::move(presentationCandidate.error())
                                    .withContext("operation", "prepare_presentation"));
    }
    if (auto committed = session.commit(std::move(prepared)); !committed) {
        renderer.discard(std::move(*presentationCandidate));
        return core::unexpected(std::move(committed.error()));
    }
    renderer.activate(std::move(*presentationCandidate));

    auto chartInfo = session.chartInfo();
    if (!chartInfo) {
        return core::unexpected(std::move(chartInfo.error()));
    }
    if (chartInfo->objectCount == 0) {
        return core::unexpected(
            core::Error{"player.chart.empty", "The committed chart contains no objects"});
    }
    logger.info("player.playback", std::string{"Prepared objects: "} +
                                       std::to_string(chartInfo->objectCount) +
                                       ", behaviors: " + std::to_string(chartInfo->behaviorCount) +
                                       ", resources: " + std::to_string(chartInfo->resourceCount));
    return {};
}

auto runPlayerFrameLoop(PlayerFrameLoop& input) -> core::Result<void> {
    PlayerClock clock;
    const NullInputSource inputSource;
    const NullJudgeSystem judgeSystem;
    const auto diagnosticsStarted = std::chrono::steady_clock::now();
    std::uint32_t renderedFrames = 0;
    bool quitRequested = false;
    playback::FrameSnapshot snapshot;
    render::RenderScene scene;
    while (!quitRequested) {
        quitRequested = input.surface.quitRequested();
        if (quitRequested) {
            break;
        }

        inputSource.poll();
        audio::AudioClockSnapshot audioClockSnapshot;
        core::Result<playback::RuntimeFrame> runtimeFrameResult = core::unexpected(
            core::Error{"player.timeline.unavailable", "No playback clock is available"});
        if (input.audio != nullptr) {
            if (input.hooks.beforeAudioService) {
                if (auto stepped = input.hooks.beforeAudioService(renderedFrames); !stepped) {
                    return core::unexpected(std::move(stepped.error()));
                }
            }
            if (auto serviced = input.audio->transport().service(); !serviced) {
                return core::unexpected(std::move(serviced.error()));
            }
            if (auto rebound = input.audio->recheckBoundDevice(); !rebound) {
                return core::unexpected(std::move(rebound.error()));
            }
            audioClockSnapshot = input.audio->transport().snapshot();
            auto consumedSource = audioClockSnapshot.source;
            if (input.sessionConfig.outputCorrectionUs != 0) {
                auto corrected = player_support::correctConsumedAudioPositionMs(
                    consumedSource.positionMs, input.sessionConfig.outputCorrectionUs);
                if (!corrected) {
                    return core::unexpected(std::move(corrected.error()));
                }
                consumedSource.positionMs = *corrected;
            }
            runtimeFrameResult = input.timeline.advance(consumedSource);
        } else {
            auto source = input.chartClock.sample(clock.nextChartTime(input.smokeTest, renderedFrames));
            if (!source) {
                return core::unexpected(std::move(source.error()));
            }
            runtimeFrameResult = input.timeline.advance(*source);
        }
        if (!runtimeFrameResult) {
            return core::unexpected(std::move(runtimeFrameResult.error()));
        }
        if (input.hooks.afterTimelineAdvance) {
            PlayerClockContext clockContext{.renderedFrames = renderedFrames,
                                            .session = input.session,
                                            .timeline = input.timeline,
                                            .chartClock = input.chartClock,
                                            .renderer = input.renderer,
                                            .audio = input.audio,
                                            .audioStore = input.audioStore,
                                            .activeAudioHandle = input.activeAudioHandle,
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
        const auto runtimeFrame = *runtimeFrameResult;
        judgeSystem.update(runtimeFrame.chartTimeMs);
        if (auto result = input.session.update(runtimeFrame); !result) {
            return core::unexpected(std::move(result.error()));
        }

        auto drawableSizeResult = input.surface.drawableSize();
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
        if (auto result = input.session.extractFrame({.width = width, .height = height}, snapshot);
            !result) {
            return core::unexpected(std::move(result.error()));
        }

        scene.clear();
        if (auto result = appendSnapshotAxes(snapshot, scene); !result) {
            return core::unexpected(std::move(result.error()));
        }
        if (input.diagnostics != nullptr) {
            input.diagnostics->captureFrame(renderedFrames, runtimeFrame, snapshot);
            if (input.audio != nullptr) {
                const double wallClockMs =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                              diagnosticsStarted)
                        .count();
                input.diagnostics->captureAudio(renderedFrames, wallClockMs, audioClockSnapshot,
                                                input.audio->transport().metrics());
            }
        }

        if (input.hooks.beforeSubmit) {
            if (auto stepped = input.hooks.beforeSubmit(snapshot, renderedFrames); !stepped) {
                return core::unexpected(std::move(stepped.error()));
            }
        }

        const auto renderStarted = std::chrono::steady_clock::now();
        auto submitted = input.renderer.submit(snapshot, &scene);
        if (!submitted) {
            if (submitted.error().code() == "presentation.renderer.surface.zero_size") {
                continue;
            }
            return core::unexpected(
                std::move(submitted.error()).withContext("frame", std::to_string(renderedFrames)));
        }
        if (auto presented = input.renderer.present(); !presented) {
            return core::unexpected(
                std::move(presented.error()).withContext("frame", std::to_string(renderedFrames)));
        }
        PlayerPresentedFrame presented{.renderedFrames = renderedFrames,
                                       .snapshot = snapshot,
                                       .summary = *submitted,
                                       .renderMicroseconds = 0.0,
                                       .renderer = input.renderer,
                                       .session = input.session};
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
