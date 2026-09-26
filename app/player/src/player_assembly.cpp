#include "player_assembly.hpp"

#include <cuexis/audio/audio_config.hpp>
#include <cuexis/audio_sdl/wav_decoder.hpp>
#include <cuexis/player_support/audio_device_profile.hpp>
#include <cuexis/version.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::player {
namespace {

// Maps a named window key to one application action. Only key presses act; releases are ignored,
// so holding a key does not repeat the action.
[[nodiscard]] auto inputActionFor(platform_sdl::WindowKey key) -> std::optional<PlayerInputAction> {
    switch (key) {
    case platform_sdl::WindowKey::Space:
        return PlayerInputAction::PlayPause;
    case platform_sdl::WindowKey::Left:
        return PlayerInputAction::SeekBackward;
    case platform_sdl::WindowKey::Right:
        return PlayerInputAction::SeekForward;
    case platform_sdl::WindowKey::R:
        return PlayerInputAction::Reload;
    case platform_sdl::WindowKey::S:
        return PlayerInputAction::Stop;
    case platform_sdl::WindowKey::B:
        return PlayerInputAction::Rebuild;
    case platform_sdl::WindowKey::Escape:
        return PlayerInputAction::Quit;
    case platform_sdl::WindowKey::Unknown:
        return std::nullopt;
    }
    return std::nullopt;
}

class SdlPlayerSurface final : public PlayerSurface {
  public:
    explicit SdlPlayerSurface(platform_sdl::SdlWindow& window) : window_(window) {}

    [[nodiscard]] auto pollInput() -> core::Result<PlayerInput> override {
        const auto events = window_.pollEvents();
        PlayerInput input;
        input.quitRequested = events.quitRequested;
        input.actions.reserve(events.keys.size());
        for (const auto& key : events.keys) {
            if (!key.pressed) {
                continue;
            }
            if (auto action = inputActionFor(key.key); action.has_value()) {
                input.actions.push_back(*action);
            }
        }
        return input;
    }

    [[nodiscard]] auto drawableSize() -> core::Result<PlayerDrawableSize> override {
        auto size = window_.drawableSize();
        if (!size) {
            return core::unexpected(std::move(size.error()));
        }
        return PlayerDrawableSize{.width = size->width, .height = size->height};
    }

  private:
    platform_sdl::SdlWindow& window_;
};

class SdlPlayerAudioSeat final : public PlayerAudioSeat {
  public:
    SdlPlayerAudioSeat(audio_sdl::SdlAudioSubsystem subsystem,
                       audio_sdl::SdlAudioTransport transport)
        : subsystem_(std::move(subsystem)), transport_(std::move(transport)) {}

    [[nodiscard]] auto transport() -> audio::IAudioTransport& override {
        return transport_;
    }

    [[nodiscard]] auto recheckBoundDevice() -> core::Result<void> override {
        return transport_.recheckBoundDevice();
    }

    [[nodiscard]] auto prepareReplacement(audio::AudioClipHandle handle, double positionMs)
        -> core::Result<void> override {
        return transport_.prepareReplacement(handle, positionMs);
    }

    [[nodiscard]] auto activateReplacement() -> core::Result<void> override {
        return transport_.activateReplacement();
    }

    [[nodiscard]] auto applyGain(float gain) -> core::Result<void> override {
        return transport_.applyGain(gain);
    }

    [[nodiscard]] auto unload() -> core::Result<void> override {
        return transport_.unload();
    }

  private:
    // Declared first so the transport is destroyed before the subsystem it was created from.
    audio_sdl::SdlAudioSubsystem subsystem_;
    audio_sdl::SdlAudioTransport transport_;
};

[[nodiscard]] auto openSeatForProfile(const player_support::AudioDeviceProfile& profile,
                                      audio::AudioClipStore& store, audio::AudioClipHandle handle,
                                      double gain, double startPositionMs, PlayerLogger& logger)
    -> core::Result<std::unique_ptr<PlayerAudioSeat>> {
    auto config = audio::validateAudioConfig({});
    if (!config) {
        return core::unexpected(std::move(config.error()));
    }
    auto createdSubsystem = audio_sdl::SdlAudioSubsystem::create();
    if (!createdSubsystem) {
        return core::unexpected(std::move(createdSubsystem.error()));
    }
    auto subsystem = std::move(*createdSubsystem);
    core::Result<audio_sdl::SdlAudioTransport> createdTransport =
        core::unexpected(core::Error{"player.audio.unopened", "Audio transport was not created"});
    if (profile.selector == player_support::AudioSelectorKind::SystemDefault) {
        createdTransport = audio_sdl::SdlAudioTransport::create(subsystem, store, *config);
    } else {
        auto devices = subsystem.enumeratePlaybackDevices();
        if (!devices) {
            return core::unexpected(std::move(devices.error()));
        }
        std::vector<player_support::AudioOutputDevice> listed;
        listed.reserve(devices->size());
        for (const auto& device : *devices) {
            listed.push_back(player_support::AudioOutputDevice{.driver = device.driver,
                                                               .deviceName = device.deviceName});
        }
        auto matched = player_support::matchAudioDevice(profile, listed);
        if (!matched) {
            return core::unexpected(std::move(matched.error()));
        }
        const audio_sdl::PlaybackDeviceRecord* selected = nullptr;
        for (const auto& device : *devices) {
            if (device.driver == matched->driver && device.deviceName == matched->deviceName) {
                if (selected != nullptr) {
                    return core::unexpected(
                        core::Error{"player.audio_profile.ambiguous",
                                    "More than one output device matches the profile"});
                }
                selected = &device;
            }
        }
        if (selected == nullptr) {
            return core::unexpected(core::Error{"player.audio_profile.unmatched",
                                                "No output device matches the explicit profile"});
        }
        createdTransport = audio_sdl::SdlAudioTransport::createForDevice(
            subsystem, store, *config,
            audio_sdl::PlaybackDeviceTarget{.instanceId = selected->instanceId,
                                            .driver = selected->driver,
                                            .deviceName = selected->deviceName});
    }
    if (!createdTransport) {
        return core::unexpected(std::move(createdTransport.error()));
    }
    auto transport = std::move(*createdTransport);
    if (auto gained = transport.applyGain(static_cast<float>(gain)); !gained) {
        return core::unexpected(std::move(gained.error()));
    }
    if (auto loaded = transport.load(handle); !loaded) {
        return core::unexpected(std::move(loaded.error()));
    }
    // A newly opened device starts at the source origin, so a transaction that preserved its
    // position positions the fresh stream before it is published. The seat is not playing yet, and
    // this still happens inside the step that is allowed to fail on a physical device.
    if (startPositionMs > 0.0) {
        if (auto sought = transport.seekMs(startPositionMs); !sought) {
            return core::unexpected(std::move(sought.error()));
        }
    }
    const auto settings = transport.effectiveSettings();
    logger.info("player.audio",
                std::string{"Source: "} + std::to_string(settings.sourceSampleRate) + " Hz / " +
                    std::to_string(settings.sourceChannels) +
                    " ch, device: " + std::to_string(settings.deviceSampleRate) + " Hz / " +
                    std::to_string(settings.deviceChannels) +
                    " ch, buffer: " + std::to_string(settings.deviceBufferFrames) +
                    " frames, latency: " + std::to_string(settings.estimatedOutputLatencyMs) +
                    " ms");
    return std::make_unique<SdlPlayerAudioSeat>(std::move(subsystem), std::move(transport));
}

} // namespace

auto playerExecutableBase() -> core::Result<std::filesystem::path> {
    return platform_sdl::executableBasePath();
}

auto playerProjectDirectory(std::string_view directory) -> core::Result<std::filesystem::path> {
    auto basePath = platform_sdl::executableBasePath();
    if (!basePath) {
        return core::unexpected(std::move(basePath.error()));
    }
    return *basePath / "assets" / "projects" / directory;
}

auto preparePlayerAudioClip(playback::PreparedPlayback& prepared, audio::AudioClipStore& store)
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

auto makePlayerAudioOpener(const player_support::AudioDeviceProfile& profile,
                           audio::AudioClipStore& store, PlayerLogger& logger)
    -> PlayerAudioOpener {
    return [&profile, &store,
            &logger](const std::optional<audio::AudioClipHandle>& clip, double gain,
                     double startPositionMs) -> core::Result<std::unique_ptr<PlayerAudioSeat>> {
        if (!clip.has_value()) {
            // The target has no audio track, so the control layer drops any previous device.
            return std::unique_ptr<PlayerAudioSeat>{};
        }
        return openSeatForProfile(profile, store, *clip, gain, startPositionMs, logger);
    };
}

auto createPlayerRuntime(PlayerLogger& logger) -> core::Result<platform_sdl::SdlRuntime> {
    auto runtimeResult = platform_sdl::SdlRuntime::create();
    if (!runtimeResult) {
        return core::unexpected(
            std::move(runtimeResult.error()).withContext("operation", "initialize_sdl"));
    }
    auto runtime = std::move(*runtimeResult);
    const auto videoDriver = runtime.videoDriver();
    logger.info("player.sdl",
                std::string{"Video driver: "} +
                    (videoDriver.empty() ? std::string{"unknown"} : std::string{videoDriver}));
    return runtime;
}

auto createPlayerWindow(platform_sdl::SdlRuntime& runtime,
                        const player_support::UserPreferences& requested)
    -> core::Result<platform_sdl::SdlWindow> {
    platform_sdl::WindowConfig windowConfig{};
    windowConfig.title = std::string{"Cuexis Player "} + std::string{version::display};
    windowConfig.width = requested.windowWidth;
    windowConfig.height = requested.windowHeight;
    windowConfig.fullscreen = requested.fullscreen;
    windowConfig.resizable = true;
    windowConfig.highDpi = true;
    windowConfig.openGl = true;
    auto windowResult = platform_sdl::SdlWindow::create(runtime, windowConfig);
    if (!windowResult) {
        return core::unexpected(
            std::move(windowResult.error()).withContext("operation", "create_player_window"));
    }
    return std::move(*windowResult);
}

auto createPlayerBackend(platform_sdl::SdlRuntime& runtime, platform_sdl::SdlWindow& window,
                         bool vsync, const std::optional<std::filesystem::path>& shaderCache,
                         PlayerLogger& logger) -> core::Result<render_opengl::OpenGlBackend> {
    auto openGlConfig = render_opengl::OpenGlConfig{};
    openGlConfig.logSink = logger.sink();
    openGlConfig.vsync = vsync;
    auto configureResult = render_opengl::configureOpenGlContext(runtime, openGlConfig);
    if (!configureResult) {
        return core::unexpected(
            std::move(configureResult.error()).withContext("operation", "configure_opengl"));
    }
    auto backendResult = render_opengl::OpenGlBackend::create(window, std::move(*configureResult));
    if (!backendResult) {
        return core::unexpected(
            std::move(backendResult.error()).withContext("operation", "create_opengl_backend"));
    }
    auto backend = std::move(*backendResult);
    if (shaderCache.has_value()) {
        backend.setShaderCacheDirectory(*shaderCache);
    }
    const auto& openGlInfo = backend.info();
    logger.info("player.opengl", std::string{"Version: "} + openGlInfo.version);
    logger.info("player.opengl", std::string{"Vendor: "} + openGlInfo.vendor);
    logger.info("player.opengl", std::string{"Renderer: "} + openGlInfo.renderer);
    return backend;
}

auto logEffectiveWindow(platform_sdl::SdlWindow& window,
                        const player_support::UserPreferences& requested, bool vsync,
                        bool audioDeviceOpen, std::string_view profileId, PlayerLogger& logger)
    -> core::Result<void> {
    const auto drawable = window.drawableSize();
    if (!drawable) {
        return core::unexpected(std::move(drawable.error()));
    }
    player_support::EffectiveSettings effective;
    effective.requested = requested;
    effective.appliedWindowWidth = drawable->width;
    effective.appliedWindowHeight = drawable->height;
    effective.appliedFullscreen = requested.fullscreen;
    effective.appliedVsync = vsync;
    effective.appliedGain = requested.gain;
    effective.appliedProfileId = std::string{profileId};
    effective.audioDeviceOpen = audioDeviceOpen;
    logger.info("player.config", std::string{"Effective window "} +
                                     std::to_string(effective.appliedWindowWidth) + "x" +
                                     std::to_string(effective.appliedWindowHeight) + ", audio " +
                                     (effective.audioDeviceOpen ? "open" : "closed"));
    return {};
}

auto makePlayerSurface(platform_sdl::SdlWindow& window) -> std::unique_ptr<PlayerSurface> {
    return std::make_unique<SdlPlayerSurface>(window);
}

} // namespace cuexis::player
