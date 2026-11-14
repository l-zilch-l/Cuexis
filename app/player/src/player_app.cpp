#include "player_app.hpp"

#include "frame_diagnostics.hpp"
#include "player_assembly.hpp"
#include "player_control.hpp"
#include "player_execution.hpp"
#include "player_log.hpp"
#include "player_options.hpp"
#include "player_smoke.hpp"
#include "player_state_name.hpp"
#include "scripted_playback_trace.hpp"

#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/player_support/config_location.hpp>
#include <cuexis/render/render_backend.hpp>
#include <cuexis/version.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::player {
namespace {

constexpr std::string_view defaultProjectDirectory = "stage1d_project";
constexpr std::string_view smokeTestProjectDirectory = "stage3_project";

// Startup is one long sequence that the user experiences as "nothing happens". The phases below
// are the units that already own the work, so a slow startup is attributable from the log alone:
// each mark reports the elapsed microseconds since the previous mark and the total since the
// first one. The clock starts when the Player has a logger, which is a few milliseconds after
// process start, so the totals are startup totals and not process-to-first-frame totals.
class StartupPhases final {
  public:
    explicit StartupPhases(PlayerLogger& logger) noexcept : logger_{logger}, last_{Clock::now()} {}

    void mark(std::string_view phase) {
        emit(phase, {});
    }

    // The first presented frame closes startup: it is the first moment content is on screen and the
    // first moment the frame loop can be compared with the frame budget. The draw summary is part
    // of the line because it names the size of the bundle that the timings above prepared.
    void markFirstFrame(const PlayerPresentedFrame& presented) {
        emit("first_frame_presented",
             " opaque " + std::to_string(presented.summary.opaque.size()) + " transparent " +
                 std::to_string(presented.summary.transparent.size()) + " render_us " +
                 std::to_string(static_cast<std::int64_t>(presented.renderMicroseconds)));
    }

  private:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] static auto microseconds(Clock::duration span) -> std::int64_t {
        return std::chrono::duration_cast<std::chrono::microseconds>(span).count();
    }

    void emit(std::string_view phase, const std::string& detail) {
        const auto now = Clock::now();
        logger_.info("player.startup_phase",
                     std::string{phase} + " elapsed_us " +
                         std::to_string(microseconds(now - last_)) + " total_us " +
                         std::to_string(microseconds(now - started_)) + detail);
        last_ = now;
    }

    PlayerLogger& logger_;
    Clock::time_point started_{Clock::now()};
    Clock::time_point last_;
};

// The still frame the window presents while the content prepares. It is deliberately dark: a
// bright flash would be more startling than the previous total absence of a window.
constexpr render::Color stillClearColor{0.06F, 0.06F, 0.08F, 1.0F};

// Opt-in start-up order: create the window before the content is prepared. Off by default, because
// a recorded startup measurement describes the historical order and the two orders are not
// interchangeable evidence.
[[nodiscard]] auto windowFirstStartupRequested() -> bool {
    static const bool requested = [] {
        char buffer[2]{};
        std::size_t length = 0;
        if (getenv_s(&length, buffer, sizeof buffer, "CUEXIS_PLAYER_WINDOW_FIRST") != 0 ||
            length != 2) {
            return false;
        }
        return buffer[0] == '1';
    }();
    return requested;
}

} // namespace

auto run(int argumentCount, char** arguments, PlayerLogger& logger) -> core::Result<void> {
    auto optionsResult = parsePlayerOptions(argumentCount, arguments);
    if (!optionsResult) {
        return core::unexpected(std::move(optionsResult.error()));
    }
    auto options = std::move(*optionsResult);

    auto executableBase = playerExecutableBase();
    if (!executableBase) {
        return core::unexpected(std::move(executableBase.error()));
    }
    if (!options.chartPath && !options.projectPath && !options.cxcPath) {
        auto pathResult = playerProjectDirectory(options.smokeTest ? smokeTestProjectDirectory
                                                                   : defaultProjectDirectory);
        if (!pathResult) {
            return core::unexpected(std::move(pathResult.error()));
        }
        options.projectPath = std::move(*pathResult);
    }

    logger.info("player.startup",
                std::string{"Starting Cuexis Player "} + std::string{version::display});
#ifdef CUEXIS_EXPERIMENTAL_BUILD
    logger.info("player.build_flavor", "experimental");
#else
    logger.info("player.build_flavor", "production");
#endif

    const auto preferencesSchema =
        *executableBase / "assets" / "schemas" / "cuexis.player-preferences.v1.schema.json";
    const auto profileSchema =
        *executableBase / "assets" / "schemas" / "cuexis.audio-device-profile.v1.schema.json";
    std::filesystem::path configDirectory;
    if (options.smokeTest || options.audioSmokeTest) {
        configDirectory = std::filesystem::temp_directory_path() / "cuexis-player-smoke-config";
        std::error_code createError;
        std::filesystem::create_directories(configDirectory, createError);
        if (createError) {
            return core::unexpected(core::Error{"player.preferences.directory_unavailable",
                                                "The smoke config directory could not be created"});
        }
        std::filesystem::remove(player_support::preferencesFilePath(configDirectory), createError);
        auto profilePath = player_support::audioProfileFilePath(configDirectory, "system-default");
        if (profilePath) {
            std::filesystem::remove(*profilePath, createError);
        }
    } else {
        auto userDirectory = player_support::userConfigDirectory(
            player_support::captureConfigDirectoryEnvironment());
        if (!userDirectory) {
            return core::unexpected(std::move(userDirectory.error()));
        }
        configDirectory = std::move(*userDirectory);
    }
    auto appConfig =
        player_support::loadAppConfig(configDirectory, preferencesSchema, profileSchema);
    if (!appConfig) {
        return core::unexpected(std::move(appConfig.error()));
    }
    logger.info("player.config", std::string{"Profile "} + appConfig->profile.id + ", gain " +
                                     std::to_string(appConfig->app.requested.gain));

    // Content is resolved before any device exists. A content error is deterministic and must not
    // hide behind an environment error: on a machine that cannot create an OpenGL context, a
    // missing chart still has to be reported as a missing chart. The resolved source is kept and
    // handed to the startup prepare below, so one startup reads, parses, and validates the
    // configured content exactly once instead of resolving it a second time inside the Load
    // transaction.
    StartupPhases startup{logger};
    auto content = openConfiguredPlaybackSource(options);
    if (!content) {
        return core::unexpected(std::move(content.error()));
    }
    startup.mark("content_resolved");

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    if (options.executionTestOnly)
        return runExecutionPlayer(options, *appConfig, logger);
#endif

    // The startup Load names its clock. The audio smoke test is defined against the audio clock, so
    // it selects CuexisAudio; otherwise an explicit --mode wins, and the default is the ChartClock
    // the controller would use anyway. Nothing is probed from a failed load.
    auto startupMode = playback::PlaybackMode::ChartClock;
    if (options.clock.has_value()) {
        switch (*options.clock) {
        case PlayerClockOption::Host:
            startupMode = playback::PlaybackMode::HostClock;
            break;
        case PlayerClockOption::Audio:
            startupMode = playback::PlaybackMode::CuexisAudio;
            break;
        case PlayerClockOption::Chart:
            startupMode = playback::PlaybackMode::ChartClock;
            break;
        }
    } else if (options.audioSmokeTest) {
        startupMode = playback::PlaybackMode::CuexisAudio;
    }

    // The CPU half of the startup Load normally runs before any device exists: no window, no GL
    // context, no audio device. Preparing there means a large bundle never holds a message-free
    // window on the desktop, and content that cannot be prepared fails before the first device is
    // created. The controller adopts the finished bundle once the presentation backend can accept
    // it.
    //
    // That order hides the whole cost inside one silent phase. Preparing the 40 000 entity chart
    // spends about two seconds of CPU before the process owns a window, so there is nothing on the
    // desktop to look at and nothing to close, and a user who started the wrong file can only kill
    // the process. The opt-in order below reverses the two steps: the runtime, the window, the
    // surface and the GL backend exist first and one still frame is presented, so the application
    // is visible from about 0.4 s. Prepare itself stays on this thread: a PlaybackSession owns the
    // thread that constructed it and terminates if a prepared bundle is destroyed anywhere else,
    // so the window cannot repaint while prepare runs. Reversing the order is still worth it, since
    // a visible loading window that honours a close request beats a process that owns no window at
    // all. The default order is unchanged, because every recorded startup number describes it.
    const bool deviceFirst = windowFirstStartupRequested();
    std::optional<platform_sdl::SdlRuntime> runtimeHolder;
    std::optional<platform_sdl::SdlWindow> windowHolder;
    std::optional<render_opengl::OpenGlBackend> backendHolder;
    std::unique_ptr<PlayerSurface> surfaceHolder;
    std::vector<std::uint32_t> gameplayKeys;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    std::optional<PlayerGameplayProfile> gameplayProfile;
#endif
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
    ScriptedPlaybackTrace measurementTrace;
#endif

    auto openDevices = [&]() -> core::Result<void> {
        auto runtimeResult = createPlayerRuntime(logger);
        if (!runtimeResult) {
            return core::unexpected(std::move(runtimeResult.error()));
        }
        runtimeHolder.emplace(std::move(*runtimeResult));
        auto windowResult = createPlayerWindow(*runtimeHolder, appConfig->app.requested);
        if (!windowResult) {
            return core::unexpected(std::move(windowResult.error()));
        }
        windowHolder.emplace(std::move(*windowResult));
        startup.mark("window_created");
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (options.gameplayConfiguration) {
            auto profile = readPlayerGameplay(options);
            if (!profile) {
                return core::unexpected(std::move(profile.error()));
            }
            for (const auto& key : profile->keys) {
                gameplayKeys.push_back(key.scanCode);
            }
            gameplayProfile = std::move(*profile);
        }
#endif
        surfaceHolder = makePlayerSurface(*windowHolder, std::move(gameplayKeys));
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
        if (measurementTrace.active()) {
            surfaceHolder = std::make_unique<ScriptedPlaybackSurface>(
                std::move(surfaceHolder),
                static_cast<SDL_Window*>(windowHolder->lease().nativeHandle()), measurementTrace);
        }
#endif
        auto backendResult =
            createPlayerBackend(*runtimeHolder, *windowHolder, appConfig->app.requested.vsync,
                                options.shaderCacheDirectory, logger);
        if (!backendResult) {
            return core::unexpected(std::move(backendResult.error()));
        }
        backendHolder.emplace(std::move(*backendResult));
        startup.mark("backend_created");
        return {};
    };

    if (deviceFirst) {
        if (auto opened = openDevices(); !opened) {
            return core::unexpected(std::move(opened.error()));
        }
        render::RenderFrame still{};
        still.clearColor = stillClearColor;
        if (auto size = surfaceHolder->drawableSize(); size) {
            still.extent.width = static_cast<std::uint32_t>(std::max(size->width, 0));
            still.extent.height = static_cast<std::uint32_t>(std::max(size->height, 0));
        }
        if (auto presented = backendHolder->renderFrame(still); !presented) {
            logger.warn("player.startup_still_frame",
                        std::string{"The startup still frame failed: "} +
                            std::string{presented.error().message()});
        }
        if (auto input = surfaceHolder->pollInput(); input && input->quitRequested) {
            logger.info("player.startup_close_requested",
                        "The window closed before the content began to prepare");
            return {};
        }
    }

    auto startupLoad = preparePlayerLoadBundle(std::move(*content), startupMode,
                                               appConfig->profile.outputCorrectionUs);
    if (!startupLoad) {
        return core::unexpected(std::move(startupLoad.error()).withContext("command", "load"));
    }
    startup.mark("content_prepared");

    if (!deviceFirst) {
        if (auto opened = openDevices(); !opened) {
            return core::unexpected(std::move(opened.error()));
        }
    } else if (auto input = surfaceHolder->pollInput(); input && input->quitRequested) {
        logger.info("player.startup_close_requested",
                    "The window closed while the content was preparing");
        return {};
    }

    audio::AudioClipStore audioStore;
    auto& window = *windowHolder;
    auto& backend = *backendHolder;
    auto surface = std::move(surfaceHolder);

    // The control layer owns the active bundle and drives every transaction. The assembly layer
    // only supplies the source, clip decoding, and the audio device opener.
    PlayerControlPorts ports;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    ports.gameplay = std::move(gameplayProfile);
#endif
    ports.makeSource = [&options]() { return openConfiguredPlaybackSource(options); };
    ports.prepareClip = [](playback::PreparedPlayback& prepared, audio::AudioClipStore& store) {
        return preparePlayerAudioClip(prepared, store);
    };
    ports.openAudio = makePlayerAudioOpener(appConfig->profile, audioStore, logger);
    PlayerController controller{backend,
                                audioStore,
                                appConfig->app.requested.gain,
                                appConfig->profile.outputCorrectionUs,
                                std::move(ports),
                                logger};

    // The bundle prepared before the device existed becomes the active bundle. Only the part of the
    // Load transaction that needs a renderer, an audio device, and a published state runs here.
    if (auto adopted = controller.adoptStartupLoad(std::move(*startupLoad)); !adopted) {
        return core::unexpected(std::move(adopted.error()));
    }
    startup.mark("content_committed");
    if (auto recorded = logEffectiveWindow(
            window, appConfig->app.requested, appConfig->app.requested.vsync, controller.gain(),
            controller.audio() != nullptr, appConfig->profile.id, logger);
        !recorded) {
        return core::unexpected(std::move(recorded.error()));
    }
    if (auto played =
            controller.apply(PlayerCommand{.kind = player_support::PlayerCommandKind::Play});
        !played) {
        return core::unexpected(std::move(played.error()));
    }
    startup.mark("transport_playing");

    std::optional<PlayerSmokeBinding> smokeBinding;
    PlayerHooks hooks;
    if (options.smokeTest || options.audioSmokeTest) {
        smokeBinding.emplace(
            window, backend, logger, options.smokeTest, options.audioSmokeTest, controller,
            [&options]() { return openConfiguredPlaybackSource(options); },
            [](std::string_view directory) { return playerProjectDirectory(directory); });
        hooks = smokeBinding->hooks();
    }
    std::optional<FrameDiagnostics> frameDiagnostics;
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
    std::int64_t measuredRequest{}, measuredReady{};
    if (measurementTrace.active()) {
        const auto stamp = [] {
            return std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
        };
        if (options.smokeTest || options.audioSmokeTest)
            return core::unexpected(
                core::Error{"player.measurement.mode", "Script and smoke cannot share a run"});
        hooks.beforeAudioService = [&, stamp](std::uint32_t) -> core::Result<void> {
            measuredRequest = stamp();
            return {};
        };
        hooks.beforeSubmit = [&, stamp](const playback::FrameSnapshot&,
                                        std::uint32_t) -> core::Result<void> {
            measuredReady = stamp();
            return {};
        };
        hooks.notePresented = [&](const PlayerPresentedFrame&) -> core::Result<void> {
            measurementTrace.firstPresent(controller.audio() != nullptr);
            measurementTrace.present(measuredRequest, measuredReady);
            if (auto* audio = controller.audio()) {
                const auto clock = audio->transport().snapshot();
                if (clock.sampleRate)
                    measurementTrace.audio(0, audio->transport().metrics().underrunCount);
                measurementTrace.transport(clock.source.positionMs,
                                           clock.source.state == audio::PlaybackState::Ended);
            }
            return {};
        };
    }
    const bool measuredHasAudio = controller.audio() != nullptr;
#endif
    // The first presented frame ends startup, and both the smoke binding and the measurement trace
    // already own notePresented, so the probe chains instead of replacing them.
    {
        std::function<core::Result<void>(const PlayerPresentedFrame&)> innerPresented;
        if (hooks.notePresented) {
            innerPresented = std::move(hooks.notePresented);
        }
        hooks.notePresented =
            [&startup, firstFrameSeen = false, innerPresented = std::move(innerPresented)](
                const PlayerPresentedFrame& presented) mutable -> core::Result<void> {
            if (!firstFrameSeen) {
                firstFrameSeen = true;
                startup.markFirstFrame(presented);
            }
            if (innerPresented) {
                return innerPresented(presented);
            }
            return {};
        };
    }
    if (options.frameStatsPrefix) {
        frameDiagnostics.emplace(*options.frameStatsPrefix);
    }

    PlayerFrameLoop loop{.controller = controller,
                         .renderer = backend,
                         .surface = *surface,
                         .smokeTest = options.smokeTest,
                         .audioSmokeTest = options.audioSmokeTest,
                         .debugAxes = options.debugAxes,
                         .showFps = options.showFps,
                         .diagnostics = frameDiagnostics ? &*frameDiagnostics : nullptr,
                         .hooks = std::move(hooks),
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
                         .measurementTrace = &measurementTrace,
#endif
                         .logger = logger};
    if (auto ran = runPlayerFrameLoop(loop); !ran) {
        return core::unexpected(std::move(ran.error()));
    }

    if (auto* audio = controller.audio(); audio != nullptr) {
        const auto finalClock = audio->transport().snapshot();
        const auto finalMetrics = audio->transport().metrics();
        logger.info(
            "player.audio",
            std::string{"Final state: "} + std::string{playbackStateName(finalClock.source.state)} +
                ", queue: " + std::to_string(finalMetrics.queuedFrames) +
                " frames, discontinuity: " + std::to_string(finalClock.source.discontinuityId) +
                ", underruns: " + std::to_string(finalMetrics.underrunCount));
    }
    if (frameDiagnostics) {
        if (auto exported = frameDiagnostics->exportArtifacts(controller.mode()); !exported) {
            return core::unexpected(std::move(exported.error()));
        }
        logger.info("player.frame_stats",
                    std::string{"Exported frame rows: "} +
                        std::to_string(frameDiagnostics->capturedFrameRows()) + ", audio rows: " +
                        std::to_string(frameDiagnostics->capturedAudioRows()) + ", dropped: " +
                        std::to_string(frameDiagnostics->droppedFrameRows() +
                                       frameDiagnostics->droppedAudioRows()));
    }

    if (auto closed = backend.close(); !closed) {
        return core::unexpected(std::move(closed.error()));
    }
    if (auto shut = controller.shutdown(); !shut) {
        return core::unexpected(std::move(shut.error()));
    }
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
    measurementTrace.finish(measuredHasAudio, true, false);
#endif
    return {};
}

} // namespace cuexis::player
