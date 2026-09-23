#pragma once

// Window facts and user input the control loop can consume without including SDL.

#include <cuexis/core/result.hpp>

#include <cstdint>
#include <vector>

namespace cuexis::player {

struct PlayerDrawableSize final {
    int width{0};
    int height{0};
};

// One user action a control surface can report. The action is independent of the windowing system
// and of the application state; PlayerController decides whether the resulting command is legal.
enum class PlayerInputAction : std::uint8_t {
    PlayPause = 0,
    Stop,
    SeekBackward,
    SeekForward,
    Reload,
    Rebuild,
    Quit,
};

// What the surface observed since the previous poll.
struct PlayerInput final {
    bool quitRequested{false};
    std::vector<PlayerInputAction> actions;
};

class PlayerSurface {
  public:
    virtual ~PlayerSurface() = default;

    PlayerSurface(const PlayerSurface&) = delete;
    auto operator=(const PlayerSurface&) -> PlayerSurface& = delete;
    PlayerSurface(PlayerSurface&&) = delete;
    auto operator=(PlayerSurface&&) -> PlayerSurface& = delete;

    // Drains the window event queue. Every action is reported exactly once per press.
    [[nodiscard]] virtual auto pollInput() -> core::Result<PlayerInput> = 0;
    [[nodiscard]] virtual auto drawableSize() -> core::Result<PlayerDrawableSize> = 0;

  protected:
    PlayerSurface() = default;
};

} // namespace cuexis::player
