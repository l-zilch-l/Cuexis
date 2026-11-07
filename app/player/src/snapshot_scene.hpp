#pragma once

// Player-owned adapter from SDK snapshots to backend-neutral debug geometry.

#include <chrono>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/render/render_scene.hpp>
#include <optional>

namespace cuexis::player {

class PlayerFpsMeter final {
  public:
    void notePresented() noexcept;
    [[nodiscard]] auto reading() const noexcept -> std::optional<unsigned> {
        return reading_;
    }

  private:
    std::chrono::steady_clock::time_point started_{std::chrono::steady_clock::now()};
    unsigned frames_{};
    std::optional<unsigned> reading_;
};

[[nodiscard]] auto appendSnapshotAxes(const playback::FrameSnapshot& snapshot,
                                      render::RenderScene& scene) -> core::Result<void>;

[[nodiscard]] auto appendFpsOverlay(const playback::FrameSnapshot& snapshot,
                                    std::optional<unsigned> fps, render::RenderScene& scene)
    -> core::Result<void>;

} // namespace cuexis::player
