#pragma once

// SDL, OpenGL, and AudioSDL factories for the reference Player.

#include "player_audio_seat.hpp"
#include "player_log.hpp"
#include "player_surface.hpp"

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio_sdl/sdl_audio.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/player_support/resolved_config.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

namespace cuexis::player {

[[nodiscard]] auto playerExecutableBase() -> core::Result<std::filesystem::path>;

[[nodiscard]] auto playerProjectDirectory(std::string_view directory)
    -> core::Result<std::filesystem::path>;

[[nodiscard]] auto preparePlayerAudioClip(playback::PreparedPlayback& prepared,
                                          audio::AudioClipStore& store)
    -> core::Result<audio::AudioClipHandle>;

[[nodiscard]] auto openPlayerAudio(playback::PreparedPlayback& prepared,
                                   playback::PlaybackMode mode,
                                   const player_support::AudioDeviceProfile& profile, double gain,
                                   audio::AudioClipStore& store,
                                   std::optional<audio::AudioClipHandle>& activeHandle,
                                   std::optional<audio_sdl::SdlAudioSubsystem>& subsystem,
                                   std::optional<audio_sdl::SdlAudioTransport>& transport,
                                   PlayerLogger& logger) -> core::Result<void>;

[[nodiscard]] auto createPlayerRuntime(PlayerLogger& logger)
    -> core::Result<platform_sdl::SdlRuntime>;

[[nodiscard]] auto createPlayerWindow(platform_sdl::SdlRuntime& runtime,
                                      const player_support::UserPreferences& requested)
    -> core::Result<platform_sdl::SdlWindow>;

[[nodiscard]] auto createPlayerBackend(platform_sdl::SdlRuntime& runtime,
                                       platform_sdl::SdlWindow& window, bool vsync,
                                       const std::optional<std::filesystem::path>& shaderCache,
                                       PlayerLogger& logger)
    -> core::Result<render_opengl::OpenGlBackend>;

[[nodiscard]] auto logEffectiveWindow(platform_sdl::SdlWindow& window,
                                      const player_support::UserPreferences& requested, bool vsync,
                                      bool audioDeviceOpen, std::string_view profileId,
                                      PlayerLogger& logger) -> core::Result<void>;

[[nodiscard]] auto makePlayerSurface(platform_sdl::SdlWindow& window)
    -> std::unique_ptr<PlayerSurface>;

[[nodiscard]] auto makePlayerAudioSeat(audio_sdl::SdlAudioTransport& transport)
    -> std::unique_ptr<PlayerAudioSeat>;

} // namespace cuexis::player
