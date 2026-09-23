#include "player_app.hpp"

#include "frame_diagnostics.hpp"
#include "player_assembly.hpp"
#include "player_control.hpp"
#include "player_log.hpp"
#include "player_options.hpp"
#include "player_smoke.hpp"

#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/player_support/config_location.hpp>
#include <cuexis/version.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::player {
namespace {

constexpr std::string_view defaultProjectDirectory = "stage1d_project";
constexpr std::string_view smokeTestProjectDirectory = "stage3_project";

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
    if (!options.chartPath && !options.projectPath) {
        auto pathResult = playerProjectDirectory(options.smokeTest ? smokeTestProjectDirectory
                                                                   : defaultProjectDirectory);
        if (!pathResult) {
            return core::unexpected(std::move(pathResult.error()));
        }
        options.projectPath = std::move(*pathResult);
    }

    logger.info("player.startup",
                std::string{"Starting Cuexis Player "} + std::string{version::display});

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

    playback::PlaybackSession playbackSession;
    auto contentResult = preparePlayerContent(playbackSession, options,
                                              appConfig->profile.outputCorrectionUs, logger);
    if (!contentResult) {
        return core::unexpected(std::move(contentResult.error()));
    }
    auto content = std::move(*contentResult);

    audio::AudioClipStore audioStore;
    std::optional<audio::AudioClipHandle> activeAudioHandle;
    std::optional<audio_sdl::SdlAudioSubsystem> audioSubsystem;
    std::optional<audio_sdl::SdlAudioTransport> audioTransport;
    if (auto opened = openPlayerAudio(content.prepared, content.mode, appConfig->profile,
                                      appConfig->app.requested.gain, audioStore, activeAudioHandle,
                                      audioSubsystem, audioTransport, logger);
        !opened) {
        return core::unexpected(std::move(opened.error()));
    }
    std::unique_ptr<PlayerAudioSeat> audioSeat;
    if (audioTransport.has_value()) {
        audioSeat = makePlayerAudioSeat(*audioTransport);
    }

    auto runtimeResult = createPlayerRuntime(logger);
    if (!runtimeResult) {
        return core::unexpected(std::move(runtimeResult.error()));
    }
    auto sdlRuntime = std::move(*runtimeResult);
    auto windowResult = createPlayerWindow(sdlRuntime, appConfig->app.requested);
    if (!windowResult) {
        return core::unexpected(std::move(windowResult.error()));
    }
    auto window = std::move(*windowResult);
    auto surface = makePlayerSurface(window);
    auto backendResult = createPlayerBackend(sdlRuntime, window, appConfig->app.requested.vsync,
                                             options.shaderCacheDirectory, logger);
    if (!backendResult) {
        return core::unexpected(std::move(backendResult.error()));
    }
    auto backend = std::move(*backendResult);
    if (auto recorded = logEffectiveWindow(window, appConfig->app.requested,
                                           appConfig->app.requested.vsync, audioTransport.has_value(),
                                           appConfig->profile.id, logger);
        !recorded) {
        return core::unexpected(std::move(recorded.error()));
    }
    if (auto activated =
            activatePreparedPlayback(playbackSession, content.prepared, backend, logger);
        !activated) {
        return core::unexpected(std::move(activated.error()));
    }
    if (audioSeat) {
        if (auto played = audioSeat->transport().play(); !played) {
            return core::unexpected(std::move(played.error()));
        }
    }

    std::optional<PlayerSmokeBinding> smokeBinding;
    PlayerHooks hooks;
    if (options.smokeTest || options.audioSmokeTest) {
        smokeBinding.emplace(
            window, backend, logger, options.smokeTest, options.audioSmokeTest, content.sessionConfig,
            content.timingOffsetMs,
            [&options]() { return openConfiguredPlaybackSource(options); },
            [](std::string_view directory) { return playerProjectDirectory(directory); },
            [](playback::PreparedPlayback& prepared, audio::AudioClipStore& store) {
                return preparePlayerAudioClip(prepared, store);
            },
            audioSeat.get());
        hooks = smokeBinding->hooks();
    }
    std::optional<FrameDiagnostics> frameDiagnostics;
    if (options.frameStatsPrefix) {
        frameDiagnostics.emplace(*options.frameStatsPrefix);
    }

    PlayerFrameLoop loop{.session = playbackSession,
                         .timeline = content.timeline,
                         .chartClock = content.chartClock,
                         .renderer = backend,
                         .surface = *surface,
                         .audio = audioSeat.get(),
                         .audioStore = audioStore,
                         .activeAudioHandle = activeAudioHandle,
                         .sessionConfig = content.sessionConfig,
                         .smokeTest = options.smokeTest,
                         .audioSmokeTest = options.audioSmokeTest,
                         .diagnostics = frameDiagnostics ? &*frameDiagnostics : nullptr,
                         .hooks = std::move(hooks),
                         .logger = logger};
    if (auto ran = runPlayerFrameLoop(loop); !ran) {
        return core::unexpected(std::move(ran.error()));
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
        if (auto exported = frameDiagnostics->exportArtifacts(content.mode); !exported) {
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
    if (audioSeat) {
        if (auto result = audioSeat->unload(); !result) {
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
