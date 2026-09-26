#pragma once

// Smoke and audio-smoke scripts. Formal playback leaves these hooks empty.
// Every lifecycle step in these scripts goes through PlayerController; only the negative
// renderer probes prepare a candidate without committing it.

#include "player_control.hpp"

#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/player_support/resolved_config.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace cuexis::player {

class PlayerSmokeBinding final {
  public:
    PlayerSmokeBinding(
        platform_sdl::SdlWindow& window, render_opengl::OpenGlBackend& backend,
        PlayerLogger& logger, bool smokeTest, bool audioSmokeTest, PlayerController& controller,
        std::function<core::Result<playback::PlaybackSource>()> makeSource,
        std::function<core::Result<std::filesystem::path>(std::string_view)> projectDirectory);

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
    PlayerController& controller_;
    // Used only by the negative renderer probes at frames 3 and 4, which prepare a candidate that
    // must be rejected and never commit it.
    std::function<core::Result<playback::PlaybackSource>()> makeSource_;
    std::function<core::Result<std::filesystem::path>(std::string_view)> projectDirectory_;
    std::optional<render_opengl::OpenGlDrawSummary> omittedDebugSummary_;
    std::optional<render_opengl::OpenGlDrawSummary> emptyDebugSummary_;
};

} // namespace cuexis::player
