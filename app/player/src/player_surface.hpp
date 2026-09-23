#pragma once

// Window facts the control loop can consume without including SDL.

#include <cuexis/core/result.hpp>

namespace cuexis::player {

struct PlayerDrawableSize final {
    int width{0};
    int height{0};
};

class PlayerSurface {
  public:
    virtual ~PlayerSurface() = default;

    PlayerSurface(const PlayerSurface&) = delete;
    auto operator=(const PlayerSurface&) -> PlayerSurface& = delete;
    PlayerSurface(PlayerSurface&&) = delete;
    auto operator=(PlayerSurface&&) -> PlayerSurface& = delete;

    [[nodiscard]] virtual bool quitRequested() = 0;
    [[nodiscard]] virtual auto drawableSize() -> core::Result<PlayerDrawableSize> = 0;

  protected:
    PlayerSurface() = default;
};

} // namespace cuexis::player
