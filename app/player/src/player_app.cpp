//  Cuexis Player 实现 — 应用组合层
//  PlaybackSession（cuexis_playback SDK 门面）负责 Chart 加载/编译和会话生命周期
//  宿主窗口/渲染后端由 Player 自行创建管理（SDL + OpenGL）
//  Each frame consumes an owning FrameSnapshot through the OpenGL presentation adapter.
//  NullClock/NullInput/NullJudge 为阶段 1A/1B 占位

#include "player_app.hpp"
#include "frame_diagnostics.hpp"
#include "player_log.hpp"
#include "snapshot_scene.hpp"

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_config.hpp>
#include <cuexis/audio_sdl/sdl_audio.hpp>
#include <cuexis/audio_sdl/wav_decoder.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/playback/runtime_timeline.hpp>
#include <cuexis/render/render_scene.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>
#include <cuexis/version.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace cuexis::player {
namespace {

constexpr std::uint32_t smokeTestFrameCount = 6;
constexpr std::uint32_t audioSmokeTestFrameCount = 90;
constexpr auto audioSmokePauseDuration = std::chrono::seconds{2};
constexpr std::size_t chartInputMaxBytes = 16U * 1024U * 1024U;
constexpr std::string_view defaultProjectDirectory = "stage1d_project";
constexpr std::string_view smokeTestProjectDirectory = "stage3_project";
constexpr std::uint64_t presentationSmokeDigest = 18316288860163381829ULL;
constexpr std::array<double, smokeTestFrameCount> presentationSmokeTimes{0.0,    625.0,  1250.0,
                                                                         1750.0, 1750.0, 625.0};

[[nodiscard]] std::string_view audioStateName(audio::PlaybackState state) noexcept {
    switch (state) {
    case audio::PlaybackState::Empty:
        return "empty";
    case audio::PlaybackState::Stopped:
        return "stopped";
    case audio::PlaybackState::Playing:
        return "playing";
    case audio::PlaybackState::Paused:
        return "paused";
    case audio::PlaybackState::Ended:
        return "ended";
    case audio::PlaybackState::Error:
        return "error";
    }
    return "unknown";
}

struct PlayerOptions final {
    bool smokeTest{};
    bool audioSmokeTest{};
    std::optional<std::filesystem::path> chartPath;
    std::optional<std::filesystem::path> projectPath;
    std::optional<std::filesystem::path> frameStatsPrefix;
};

struct NullInputSource final {
    void poll() const noexcept {}
};

struct NullJudgeSystem final {
    void update(double) const noexcept {}
};

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

[[nodiscard]] auto parseOptions(int argumentCount, char** arguments)
    -> core::Result<PlayerOptions> {
    PlayerOptions options;
    for (int index = 1; index < argumentCount; ++index) {
        const std::string_view argument{arguments[index]};
        if (argument == "--smoke-test") {
            options.smokeTest = true;
            continue;
        }
        if (argument == "--audio-smoke-test") {
            options.audioSmokeTest = true;
            continue;
        }
        if (argument == "--chart") {
            if (options.chartPath.has_value()) {
                return core::unexpected(core::Error{"player.arguments.duplicate_chart",
                                                    "The chart option may only be provided once"});
            }
            if (++index >= argumentCount || std::string_view{arguments[index]}.empty() ||
                std::string_view{arguments[index]}.starts_with("--")) {
                return core::unexpected(core::Error{"player.arguments.chart_path_missing",
                                                    "The chart option requires a path"});
            }
            options.chartPath = std::filesystem::path{arguments[index]};
            continue;
        }
        if (argument == "--project") {
            if (options.projectPath.has_value()) {
                return core::unexpected(
                    core::Error{"player.arguments.duplicate_project",
                                "The project option may only be provided once"});
            }
            if (++index >= argumentCount || std::string_view{arguments[index]}.empty() ||
                std::string_view{arguments[index]}.starts_with("--")) {
                return core::unexpected(core::Error{
                    "player.arguments.project_path_missing",
                    "The project option requires a directory or cuexis.project.json path"});
            }
            options.projectPath = std::filesystem::path{arguments[index]};
            continue;
        }
        if (argument == "--frame-stats") {
            if (options.frameStatsPrefix.has_value()) {
                return core::unexpected(
                    core::Error{"player.arguments.duplicate_frame_stats",
                                "The frame stats option may only be provided once"});
            }
            if (++index >= argumentCount || std::string_view{arguments[index]}.empty() ||
                std::string_view{arguments[index]}.starts_with("--")) {
                return core::unexpected(
                    core::Error{"player.arguments.frame_stats_path_missing",
                                "The frame stats option requires an artifact path prefix"});
            }
            options.frameStatsPrefix = std::filesystem::path{arguments[index]};
            continue;
        }

        return core::unexpected(
            core::Error{"player.arguments.unknown", "Unknown command-line argument"}.withContext(
                "argument", std::string{argument}));
    }
    if (options.chartPath.has_value() && options.projectPath.has_value()) {
        return core::unexpected(
            core::Error{"player.arguments.project_chart_conflict",
                        "The project and chart options are mutually exclusive"});
    }
    if (options.smokeTest && options.audioSmokeTest) {
        return core::unexpected(core::Error{"player.arguments.smoke_test_conflict",
                                            "Smoke test modes are mutually exclusive"});
    }
    return options;
}

[[nodiscard]] auto defaultProjectPath(std::string_view directory)
    -> core::Result<std::filesystem::path> {
    auto basePath = platform_sdl::executableBasePath();
    if (!basePath) {
        return core::unexpected(std::move(basePath.error()));
    }
    return *basePath / "assets" / "projects" / directory;
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

[[nodiscard]] auto prepareAudioClip(playback::PreparedPlayback& prepared,
                                    audio::AudioClipStore& store)
    -> core::Result<audio::AudioClipHandle> {
    const auto source = prepared.mainMusicSource();
    if (!source) {
        return core::unexpected(core::Error{"player.audio.source_missing",
                                            "Prepared audio playback has no main music source"});
    }
    auto decoded = audio_sdl::WavDecoder::decode(source->bytes);
    if (!decoded) {
        return core::unexpected(
            std::move(decoded.error()).withContext("asset_id", std::string{source->assetId}));
    }
    auto handle = store.registerClip(std::move(*decoded));
    if (!handle) {
        return core::unexpected(
            std::move(handle.error()).withContext("asset_id", std::string{source->assetId}));
    }
    return *handle;
}

[[nodiscard]] auto validatePresentationSmokeFrame(std::uint32_t frameIndex,
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

} // namespace

auto run(int argumentCount, char** arguments, PlayerLogger& logger) -> core::Result<void> {
    auto optionsResult = parseOptions(argumentCount, arguments);
    if (!optionsResult) {
        return core::unexpected(std::move(optionsResult.error()));
    }
    auto options = std::move(optionsResult).value();

    if (!options.chartPath && !options.projectPath) {
        auto pathResult = defaultProjectPath(options.smokeTest ? smokeTestProjectDirectory
                                                               : defaultProjectDirectory);
        if (!pathResult) {
            return core::unexpected(std::move(pathResult.error()));
        }
        options.projectPath = std::move(pathResult).value();
    }

    logger.info("player.startup",
                std::string{"Starting Cuexis Player "} + std::string{version::display});

    auto makeSource = [&]() -> core::Result<playback::PlaybackSource> {
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
    };

    playback::PlaybackSession playbackSession;

    auto mode = playback::PlaybackMode::ChartClock;
    auto sourceResult = makeSource();
    if (!sourceResult) {
        return core::unexpected(std::move(sourceResult.error()));
    }
    auto preparedResult = playbackSession.prepareLoad(std::move(*sourceResult), mode);
    if (!preparedResult && preparedResult.error().code() == "playback.mode.content_mismatch") {
        mode = playback::PlaybackMode::CuexisAudio;
        sourceResult = makeSource();
        if (!sourceResult) {
            return core::unexpected(std::move(sourceResult.error()));
        }
        preparedResult = playbackSession.prepareLoad(std::move(*sourceResult), mode);
    }
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
    auto timelineResult = playback::RuntimeTimeline::create(preparedInfo->timingOffsetMs);
    if (!timelineResult) {
        return core::unexpected(std::move(timelineResult.error()));
    }
    auto timeline = std::move(*timelineResult);
    playback::ChartClock chartClock{preparedInfo->timingOffsetMs};

    audio::AudioClipStore audioStore;
    std::optional<audio::AudioClipHandle> activeAudioHandle;
    std::optional<audio_sdl::SdlAudioSubsystem> audioSubsystem;
    std::optional<audio_sdl::SdlAudioTransport> audioTransport;
    if (mode == playback::PlaybackMode::CuexisAudio) {
        auto handle = prepareAudioClip(prepared, audioStore);
        if (!handle) {
            return core::unexpected(std::move(handle.error()));
        }
        activeAudioHandle = *handle;

        auto config = audio::validateAudioConfig({});
        if (!config) {
            return core::unexpected(std::move(config.error()));
        }
        auto subsystem = audio_sdl::SdlAudioSubsystem::create();
        if (!subsystem) {
            return core::unexpected(std::move(subsystem.error()));
        }
        audioSubsystem.emplace(std::move(*subsystem));
        auto transport = audio_sdl::SdlAudioTransport::create(*audioSubsystem, audioStore, *config);
        if (!transport) {
            return core::unexpected(std::move(transport.error()));
        }
        audioTransport.emplace(std::move(*transport));
        if (auto loaded = audioTransport->load(*activeAudioHandle); !loaded) {
            return core::unexpected(std::move(loaded.error()));
        }
        const auto settings = audioTransport->effectiveSettings();
        logger.info("player.audio",
                    std::string{"Source: "} + std::to_string(settings.sourceSampleRate) + " Hz / " +
                        std::to_string(settings.sourceChannels) +
                        " ch, device: " + std::to_string(settings.deviceSampleRate) + " Hz / " +
                        std::to_string(settings.deviceChannels) +
                        " ch, buffer: " + std::to_string(settings.deviceBufferFrames) +
                        " frames, latency: " + std::to_string(settings.estimatedOutputLatencyMs) +
                        " ms");
    }

    auto runtimeResult = platform_sdl::SdlRuntime::create();
    if (!runtimeResult) {
        return core::unexpected(
            std::move(runtimeResult.error()).withContext("operation", "initialize_sdl"));
    }
    auto sdlRuntime = std::move(runtimeResult).value();

    const auto videoDriver = sdlRuntime.videoDriver();
    logger.info("player.sdl",
                std::string{"Video driver: "} +
                    (videoDriver.empty() ? std::string{"unknown"} : std::string{videoDriver}));

    auto openGlConfig = render_opengl::OpenGlConfig{};
    openGlConfig.logSink = logger.sink();
    auto configureResult = render_opengl::configureOpenGlContext(sdlRuntime, openGlConfig);
    if (!configureResult) {
        return core::unexpected(
            std::move(configureResult.error()).withContext("operation", "configure_opengl"));
    }

    platform_sdl::WindowConfig windowConfig{};
    windowConfig.title = std::string{"Cuexis Player "} + std::string{version::display};
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.resizable = true;
    windowConfig.highDpi = true;
    windowConfig.openGl = true;

    auto windowResult = platform_sdl::SdlWindow::create(sdlRuntime, windowConfig);
    if (!windowResult) {
        return core::unexpected(
            std::move(windowResult.error()).withContext("operation", "create_player_window"));
    }
    auto window = std::move(windowResult).value();

    auto backendResult =
        render_opengl::OpenGlBackend::create(window, std::move(configureResult).value());
    if (!backendResult) {
        return core::unexpected(
            std::move(backendResult.error()).withContext("operation", "create_opengl_backend"));
    }
    auto backend = std::move(backendResult).value();

    const auto& openGlInfo = backend.info();
    logger.info("player.opengl", std::string{"Version: "} + openGlInfo.version);
    logger.info("player.opengl", std::string{"Vendor: "} + openGlInfo.vendor);
    logger.info("player.opengl", std::string{"Renderer: "} + openGlInfo.renderer);

    auto presentationCandidate = backend.preparePresentation(prepared, {.enableDebugPass = true});
    if (!presentationCandidate) {
        return core::unexpected(std::move(presentationCandidate.error())
                                    .withContext("operation", "prepare_presentation"));
    }
    if (auto committed = playbackSession.commit(std::move(prepared)); !committed) {
        backend.discardPresentation(std::move(*presentationCandidate));
        return core::unexpected(std::move(committed.error()));
    }
    backend.activatePresentation(std::move(*presentationCandidate));

    auto chartInfo = playbackSession.chartInfo();
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

    if (audioTransport) {
        if (auto played = audioTransport->play(); !played) {
            return core::unexpected(std::move(played.error()));
        }
    }

    PlayerClock clock;
    const NullInputSource inputSource;
    const NullJudgeSystem judgeSystem;
    std::optional<FrameDiagnostics> frameDiagnostics;
    if (options.frameStatsPrefix) {
        frameDiagnostics.emplace(*options.frameStatsPrefix);
    }
    const auto diagnosticsStarted = std::chrono::steady_clock::now();
    std::uint32_t renderedFrames = 0;
    bool quitRequested = false;
    playback::FrameSnapshot snapshot;
    while (!quitRequested) {
        quitRequested = window.pollEvents().quitRequested;
        if (quitRequested) {
            break;
        }

        inputSource.poll();
        audio::AudioClockSnapshot audioClockSnapshot;
        core::Result<playback::RuntimeFrame> runtimeFrameResult = core::unexpected(
            core::Error{"player.timeline.unavailable", "No playback clock is available"});
        if (audioTransport) {
            if (options.audioSmokeTest) {
                if (renderedFrames == 15) {
                    if (auto paused = audioTransport->pause(); !paused) {
                        return core::unexpected(std::move(paused.error()));
                    }
                    const auto pausedClock = audioTransport->snapshot();
                    std::this_thread::sleep_for(audioSmokePauseDuration);
                    const auto heldClock = audioTransport->snapshot();
                    if (heldClock.source.state != audio::PlaybackState::Paused ||
                        heldClock.presentedFrame != pausedClock.presentedFrame ||
                        heldClock.source.positionMs != pausedClock.source.positionMs ||
                        heldClock.source.discontinuityId != pausedClock.source.discontinuityId) {
                        return core::unexpected(core::Error{
                            "player.audio_smoke_test.pause_clock_advanced",
                            "Audio clock changed during the required two-second pause"});
                    }
                    if (auto resumed = audioTransport->play(); !resumed) {
                        return core::unexpected(std::move(resumed.error()));
                    }
                    logger.info("player.audio_smoke_test",
                                "Two-second pause preserved the audio clock");
                } else if (renderedFrames == 30) {
                    if (auto sought = audioTransport->seekMs(500.0); !sought) {
                        return core::unexpected(std::move(sought.error()));
                    }
                } else if (renderedFrames == 45) {
                    if (auto stopped = audioTransport->stop(); !stopped) {
                        return core::unexpected(std::move(stopped.error()));
                    }
                } else if (renderedFrames == 46) {
                    if (auto restarted = audioTransport->play(); !restarted) {
                        return core::unexpected(std::move(restarted.error()));
                    }
                }
            }
            if (auto serviced = audioTransport->service(); !serviced) {
                return core::unexpected(std::move(serviced.error()));
            }
            audioClockSnapshot = audioTransport->snapshot();
            runtimeFrameResult = timeline.advance(audioClockSnapshot.source);

            if (options.audioSmokeTest && renderedFrames == 55) {
                if (!runtimeFrameResult) {
                    return core::unexpected(std::move(runtimeFrameResult.error()));
                }
                const auto contentBeforeFailure = playbackSession.contentInfo();
                if (!contentBeforeFailure) {
                    return core::unexpected(std::move(contentBeforeFailure.error()));
                }
                const auto clockBeforeFailure = audioTransport->snapshot();
                const auto rejected = playbackSession.prepareReload(
                    R"json({"format":"cuexis.chart","version":2})json", *runtimeFrameResult,
                    playback::ReloadPolicy::KeepChartTime);
                if (rejected) {
                    return core::unexpected(core::Error{
                        "player.audio_smoke_test.failed_reload_accepted",
                        "Invalid replacement chart unexpectedly prepared successfully"});
                }
                const auto contentAfterFailure = playbackSession.contentInfo();
                if (!contentAfterFailure) {
                    return core::unexpected(std::move(contentAfterFailure.error()));
                }
                const auto clockAfterFailure = audioTransport->snapshot();
                if (contentAfterFailure->chartId != contentBeforeFailure->chartId ||
                    contentAfterFailure->chartFormatVersion !=
                        contentBeforeFailure->chartFormatVersion ||
                    contentAfterFailure->timingOffsetMs != contentBeforeFailure->timingOffsetMs ||
                    contentAfterFailure->mode != contentBeforeFailure->mode ||
                    contentAfterFailure->mainMusicAssetId !=
                        contentBeforeFailure->mainMusicAssetId ||
                    clockAfterFailure.presentedFrame != clockBeforeFailure.presentedFrame ||
                    clockAfterFailure.source.state != clockBeforeFailure.source.state ||
                    clockAfterFailure.source.discontinuityId !=
                        clockBeforeFailure.source.discontinuityId) {
                    return core::unexpected(
                        core::Error{"player.audio_smoke_test.failed_reload_mutated_state",
                                    "Failed reload changed active playback or audio state"});
                }
                logger.info("player.audio_smoke_test",
                            "Failed reload preserved active playback and audio state");
            }

            if (options.audioSmokeTest && renderedFrames == 60) {
                if (!runtimeFrameResult) {
                    return core::unexpected(std::move(runtimeFrameResult.error()));
                }
                auto replacementSource = makeSource();
                if (!replacementSource) {
                    return core::unexpected(std::move(replacementSource.error()));
                }
                auto replacement = playbackSession.prepareReload(
                    std::move(*replacementSource), *runtimeFrameResult,
                    playback::ReloadPolicy::KeepChartTime);
                if (!replacement) {
                    return core::unexpected(std::move(replacement.error()));
                }
                auto replacementPresentation =
                    backend.preparePresentation(*replacement, {.enableDebugPass = true});
                if (!replacementPresentation) {
                    return core::unexpected(
                        std::move(replacementPresentation.error())
                            .withContext("operation", "prepare_reload_presentation"));
                }
                auto replacementHandle = prepareAudioClip(*replacement, audioStore);
                if (!replacementHandle) {
                    backend.discardPresentation(std::move(*replacementPresentation));
                    return core::unexpected(std::move(replacementHandle.error()));
                }
                if (auto replacementPrepared = audioTransport->prepareReplacement(
                        *replacementHandle, audioClockSnapshot.source.positionMs);
                    !replacementPrepared) {
                    const auto removed = audioStore.remove(*replacementHandle);
                    if (!removed) {
                        logger.warn("player.audio", "Replacement cleanup failed after prepare");
                    }
                    backend.discardPresentation(std::move(*replacementPresentation));
                    return core::unexpected(std::move(replacementPrepared.error()));
                }
                if (auto activated = audioTransport->activateReplacement(); !activated) {
                    const auto removed = audioStore.remove(*replacementHandle);
                    if (!removed) {
                        logger.warn("player.audio", "Replacement cleanup failed after activation");
                    }
                    backend.discardPresentation(std::move(*replacementPresentation));
                    return core::unexpected(std::move(activated.error()));
                }
                if (auto committed = playbackSession.commit(std::move(*replacement)); !committed) {
                    backend.discardPresentation(std::move(*replacementPresentation));
                    return core::unexpected(std::move(committed.error()));
                }
                backend.activatePresentation(std::move(*replacementPresentation));
                const auto contentInfo = playbackSession.contentInfo();
                if (!contentInfo) {
                    return core::unexpected(std::move(contentInfo.error()));
                }
                if (auto reset = timeline.reset(contentInfo->timingOffsetMs); !reset) {
                    return core::unexpected(std::move(reset.error()));
                }
                if (activeAudioHandle) {
                    if (auto removed = audioStore.remove(*activeAudioHandle); !removed) {
                        return core::unexpected(std::move(removed.error()));
                    }
                }
                activeAudioHandle = *replacementHandle;
                if (auto serviced = audioTransport->service(); !serviced) {
                    return core::unexpected(std::move(serviced.error()));
                }
                audioClockSnapshot = audioTransport->snapshot();
                runtimeFrameResult = timeline.advance(audioClockSnapshot.source);
                logger.info("player.audio_smoke_test", "Reload transaction completed");
            }
        } else {
            auto source = chartClock.sample(clock.nextChartTime(options.smokeTest, renderedFrames));
            if (!source) {
                return core::unexpected(std::move(source.error()));
            }
            runtimeFrameResult = timeline.advance(*source);
        }
        if (!runtimeFrameResult) {
            return core::unexpected(std::move(runtimeFrameResult.error()));
        }
        if (options.smokeTest && renderedFrames == 3) {
            const auto rejected = playbackSession.prepareReload(
                R"json({"format":"cuexis.chart","version":2})json", *runtimeFrameResult,
                playback::ReloadPolicy::KeepChartTime);
            if (rejected || !backend.hasActivePresentation()) {
                return core::unexpected(core::Error{
                    "player.smoke_test.failed_reload_mutated_state",
                    "Failed presentation reload did not preserve the active GPU cache"});
            }
            logger.info("player.smoke_test",
                        "Failed reload preserved the active OpenGL presentation cache");

            auto replacementSource = makeSource();
            if (!replacementSource) {
                return core::unexpected(std::move(replacementSource.error()));
            }
            auto rejectedPresentationCandidate =
                playbackSession.prepareReload(std::move(*replacementSource), *runtimeFrameResult,
                                              playback::ReloadPolicy::KeepChartTime);
            if (!rejectedPresentationCandidate) {
                return core::unexpected(std::move(rejectedPresentationCandidate.error()));
            }
            const auto unsupportedPresentation = backend.preparePresentation(
                *rejectedPresentationCandidate,
                {.version = 2, .portableProfileVersion = 1, .enableDebugPass = true});
            if (unsupportedPresentation || !backend.hasActivePresentation()) {
                return core::unexpected(core::Error{
                    "player.smoke_test.failed_adapter_prepare_mutated_state",
                    "Failed adapter preparation did not preserve the active GPU cache"});
            }
            logger.info(
                "player.smoke_test",
                "Failed adapter preparation preserved the active OpenGL presentation cache");
        }
        if (options.smokeTest && renderedFrames == 4) {
            auto replacementSource = makeSource();
            if (!replacementSource) {
                return core::unexpected(std::move(replacementSource.error()));
            }
            auto replacement =
                playbackSession.prepareReload(std::move(*replacementSource), *runtimeFrameResult,
                                              playback::ReloadPolicy::RestartAtZero);
            if (!replacement) {
                return core::unexpected(std::move(replacement.error()));
            }
            auto replacementPresentation =
                backend.preparePresentation(*replacement, {.enableDebugPass = true});
            if (!replacementPresentation) {
                return core::unexpected(std::move(replacementPresentation.error())
                                            .withContext("operation", "prepare_smoke_reload"));
            }
            if (auto committed = playbackSession.commit(std::move(*replacement)); !committed) {
                backend.discardPresentation(std::move(*replacementPresentation));
                return core::unexpected(std::move(committed.error()));
            }
            backend.activatePresentation(std::move(*replacementPresentation));
            const auto contentInfo = playbackSession.contentInfo();
            if (!contentInfo) {
                return core::unexpected(std::move(contentInfo.error()));
            }
            if (auto reset = timeline.reset(contentInfo->timingOffsetMs); !reset) {
                return core::unexpected(std::move(reset.error()));
            }
            auto source = chartClock.sample(0.0);
            if (!source) {
                return core::unexpected(std::move(source.error()));
            }
            runtimeFrameResult = timeline.advance(*source);
            if (!runtimeFrameResult) {
                return core::unexpected(std::move(runtimeFrameResult.error()));
            }
            logger.info("player.smoke_test",
                        "Successful reload activated a complete OpenGL presentation cache");
        }
        const auto runtimeFrame = *runtimeFrameResult;
        judgeSystem.update(runtimeFrame.chartTimeMs);
        if (auto result = playbackSession.update(runtimeFrame); !result) {
            return core::unexpected(std::move(result.error()));
        }

        auto drawableSizeResult = window.drawableSize();
        if (!drawableSizeResult) {
            return core::unexpected(std::move(drawableSizeResult.error())
                                        .withContext("operation", "query_drawable_size"));
        }
        const auto drawableSize = *drawableSizeResult;
        const auto width = static_cast<std::uint32_t>(std::max(drawableSize.width, 1));
        const auto height = static_cast<std::uint32_t>(std::max(drawableSize.height, 1));
        if (auto result =
                playbackSession.extractFrame({.width = width, .height = height}, snapshot);
            !result) {
            return core::unexpected(std::move(result.error()));
        }

        render::RenderScene scene;
        if (auto result = appendSnapshotAxes(snapshot, scene); !result) {
            return core::unexpected(std::move(result.error()));
        }
        if (frameDiagnostics) {
            frameDiagnostics->captureFrame(renderedFrames, runtimeFrame, snapshot);
            if (audioTransport) {
                const double wallClockMs =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                              diagnosticsStarted)
                        .count();
                frameDiagnostics->captureAudio(renderedFrames, wallClockMs, audioClockSnapshot,
                                               audioTransport->metrics());
            }
        }

        render_opengl::OpenGlDrawSummary drawSummary;
        render_opengl::OpenGlPixelProbe pixelProbe;
        const auto renderStarted = std::chrono::steady_clock::now();
        if (auto result = backend.renderPresentationFrame(
                snapshot, &scene, &drawSummary, options.smokeTest ? &pixelProbe : nullptr);
            !result) {
            return core::unexpected(
                std::move(result.error()).withContext("frame", std::to_string(renderedFrames)));
        }
        const double renderMicroseconds = std::chrono::duration<double, std::micro>(
                                              std::chrono::steady_clock::now() - renderStarted)
                                              .count();
        if (renderedFrames == 0) {
            logger.info(
                "player.snapshot",
                std::string{"Objects: "} + std::to_string(snapshot.objects.size()) +
                    ", opaque draws: " + std::to_string(drawSummary.opaque.size()) +
                    ", transparent draws: " + std::to_string(drawSummary.transparent.size()) +
                    ", debug commands: " + std::to_string(drawSummary.debugCommandCount));
        }
        if (options.smokeTest) {
            if (auto validated = validatePresentationSmokeFrame(renderedFrames, snapshot,
                                                                drawSummary, pixelProbe);
                !validated) {
                return core::unexpected(std::move(validated.error()));
            }
            logger.info("player.smoke_test",
                        std::string{"Frame "} + std::to_string(renderedFrames) +
                            " summary=" + std::to_string(drawSummary.digest) +
                            " pixel=" + std::to_string(pixelProbe.rgba[0]) + "," +
                            std::to_string(pixelProbe.rgba[1]) + "," +
                            std::to_string(pixelProbe.rgba[2]) + "," +
                            std::to_string(pixelProbe.rgba[3]) +
                            " render_us=" + std::to_string(renderMicroseconds));
        }

        ++renderedFrames;
        if (options.smokeTest && renderedFrames >= smokeTestFrameCount) {
            quitRequested = true;
        }
        if (options.audioSmokeTest && renderedFrames >= audioSmokeTestFrameCount) {
            quitRequested = true;
        }
    }

    if (options.smokeTest && renderedFrames != smokeTestFrameCount) {
        return core::unexpected(
            core::Error{"player.smoke_test.incomplete",
                        "Smoke test ended before rendering required frames"}
                .withContext("expected_frames", std::to_string(smokeTestFrameCount))
                .withContext("rendered_frames", std::to_string(renderedFrames)));
    }
    if (options.smokeTest) {
        logger.info("player.smoke_test",
                    std::string{"Completed frames: "} + std::to_string(renderedFrames));
    }
    if (options.audioSmokeTest && renderedFrames != audioSmokeTestFrameCount) {
        return core::unexpected(
            core::Error{"player.audio_smoke_test.incomplete",
                        "Audio smoke test ended before rendering required frames"}
                .withContext("expected_frames", std::to_string(audioSmokeTestFrameCount))
                .withContext("rendered_frames", std::to_string(renderedFrames)));
    }
    if (options.audioSmokeTest) {
        logger.info("player.audio_smoke_test",
                    std::string{"Completed frames: "} + std::to_string(renderedFrames));
    }

    if (audioTransport) {
        const auto finalClock = audioTransport->snapshot();
        const auto finalMetrics = audioTransport->metrics();
        logger.info(
            "player.audio",
            std::string{"Final state: "} + std::string{audioStateName(finalClock.source.state)} +
                ", queue: " + std::to_string(finalMetrics.queuedFrames) +
                " frames, discontinuity: " + std::to_string(finalClock.source.discontinuityId) +
                ", underruns: " + std::to_string(finalMetrics.underrunCount));
    }

    if (frameDiagnostics) {
        if (auto exported = frameDiagnostics->exportArtifacts(mode); !exported) {
            return core::unexpected(std::move(exported.error()));
        }
        logger.info("player.frame_stats",
                    std::string{"Exported frame rows: "} +
                        std::to_string(frameDiagnostics->capturedFrameRows()) + ", audio rows: " +
                        std::to_string(frameDiagnostics->capturedAudioRows()) + ", dropped: " +
                        std::to_string(frameDiagnostics->droppedFrameRows() +
                                       frameDiagnostics->droppedAudioRows()));
    }

    if (auto result = backend.close(); !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (audioTransport) {
        if (auto result = audioTransport->unload(); !result) {
            return core::unexpected(std::move(result.error()));
        }
    }
    if (activeAudioHandle) {
        if (auto result = audioStore.remove(*activeAudioHandle); !result) {
            return core::unexpected(std::move(result.error()));
        }
    }
    if (auto result = playbackSession.unload(); !result) {
        return core::unexpected(std::move(result.error()));
    }
    return {};
}

} // namespace cuexis::player
