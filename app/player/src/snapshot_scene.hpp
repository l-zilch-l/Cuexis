#pragma once

// Player-owned adapter from SDK snapshots to backend-neutral debug geometry.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/render/render_scene.hpp>

namespace cuexis::player {

[[nodiscard]] auto appendSnapshotAxes(const playback::FrameSnapshot& snapshot,
                                      render::RenderScene& scene) -> core::Result<void>;

} // namespace cuexis::player
