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

    // Content is resolved before any device exists. A content error is deterministic and must not
    // hide behind an environment error: on a machine that cannot create an OpenGL context, a
    // missing chart still has to be reported as a missing chart. The transaction resolves the
    // configured source again, so the audio mode probe can still re-read it.
    if (auto content = openConfiguredPlaybackSource(options); !content) {
        return core::unexpected(std::move(content.error()));
    }

    audio::AudioClipStore audioStore;

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

    // The control layer owns the active bundle and drives every transaction. The assembly layer
    // only supplies the source, clip decoding, and the audio device opener.
    PlayerControlPorts ports;
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

    if (auto loaded =
            controller.apply(PlayerCommand{.kind = player_support::PlayerCommandKind::Load});
        !loaded) {
        return core::unexpected(std::move(loaded.error()));
    }
    if (auto recorded =
            logEffectiveWindow(window, appConfig->app.requested, appConfig->app.requested.vsync,
                               controller.audio() != nullptr, appConfig->profile.id, logger);
        !recorded) {
        return core::unexpected(std::move(recorded.error()));
    }
    if (auto played =
            controller.apply(PlayerCommand{.kind = player_support::PlayerCommandKind::Play});
        !played) {
        return core::unexpected(std::move(played.error()));
    }

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
    if (options.frameStatsPrefix) {
        frameDiagnostics.emplace(*options.frameStatsPrefix);
    }

    PlayerFrameLoop loop{.controller = controller,
                         .renderer = backend,
                         .surface = *surface,
                         .smokeTest = options.smokeTest,
                         .audioSmokeTest = options.audioSmokeTest,
                         .diagnostics = frameDiagnostics ? &*frameDiagnostics : nullptr,
                         .hooks = std::move(hooks),
                         .logger = logger};
    if (auto ran = runPlayerFrameLoop(loop); !ran) {
        return core::unexpected(std::move(ran.error()));
    }

    if (auto* audio = controller.audio(); audio != nullptr) {
        const auto finalClock = audio->transport().snapshot();
        const auto finalMetrics = audio->transport().metrics();
        logger.info(
            "player.audio",
            std::string{"Final state: "} + std::string{audioStateName(finalClock.source.state)} +
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
    return {};
}

} // namespace cuexis::player
