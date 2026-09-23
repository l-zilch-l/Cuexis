#pragma once

// Smoke and audio-smoke scripts. Formal playback leaves these hooks empty.

#include "player_control.hpp"

#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/player_support/resolved_config.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>

#include <functional>
#include <optional>
#include <string_view>

namespace cuexis::player {

class PlayerSmokeBinding final {
  public:
    PlayerSmokeBinding(
        platform_sdl::SdlWindow& window, render_opengl::OpenGlBackend& backend,
        PlayerLogger& logger, bool smokeTest, bool audioSmokeTest,
        player_support::ResolvedSessionConfig sessionConfig, double timingOffsetMs,
        std::function<core::Result<playback::PlaybackSource>()> makeSource,
        std::function<core::Result<std::filesystem::path>(std::string_view)> projectDirectory,
        std::function<core::Result<audio::AudioClipHandle>(playback::PreparedPlayback&,
                                                           audio::AudioClipStore&)>
            prepareClip,
        PlayerAudioSeat* audio);

    [[nodiscard]] auto hooks() -> PlayerHooks;

  private:
    [[nodiscard]] auto beforeAudioService(std::uint32_t renderedFrames) -> core::Result<void>;
    [[nodiscard]] auto afterTimelineAdvance(PlayerClockContext& context) -> core::Result<void>;
    [[nodiscard]] auto beforeSubmit(const playback::FrameSnapshot& snapshot,
                                    std::uint32_t renderedFrames) -> core::Result<void>;
    [[nodiscard]] auto notePresented(const PlayerPresentedFrame& presented) -> core::Result<void>;
    [[nodiscard]] auto validatePresented(const PlayerPresentedFrame& presented)
        -> core::Result<void>;
    [[nodiscard]] auto waitForMinimized(bool wantMinimized) -> core::Result<void>;
    [[nodiscard]] auto verifyMinimizeRestore(const PlayerPresentedFrame& presented,
                                             std::uint64_t baselineDigest,
                                             const render_opengl::OpenGlPixelProbe& baselineProbe)
        -> core::Result<void>;
    [[nodiscard]] auto validateFrame(std::uint32_t frameIndex,
                                     const playback::FrameSnapshot& snapshot,
                                     const render_opengl::OpenGlDrawSummary& summary,
                                     const render_opengl::OpenGlPixelProbe& probe)
        -> core::Result<void>;

    platform_sdl::SdlWindow& window_;
    render_opengl::OpenGlBackend& backend_;
    PlayerLogger& logger_;
    bool smokeTest_{};
    bool audioSmokeTest_{};
    player_support::ResolvedSessionConfig sessionConfig_{};
    double timingOffsetMs_{};
    std::function<core::Result<playback::PlaybackSource>()> makeSource_;
    std::function<core::Result<std::filesystem::path>(std::string_view)> projectDirectory_;
    std::function<core::Result<audio::AudioClipHandle>(playback::PreparedPlayback&,
                                                       audio::AudioClipStore&)>
        prepareClip_;
    PlayerAudioSeat* audio_{};
    std::optional<render_opengl::OpenGlDrawSummary> omittedDebugSummary_;
    std::optional<render_opengl::OpenGlDrawSummary> emptyDebugSummary_;
};

} // namespace cuexis::player
