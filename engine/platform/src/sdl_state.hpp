#pragma once

#include <memory>

#include <SDL3/SDL.h>

#include <cuexis/core/thread_checker.hpp>

namespace cuexis::platform_sdl::detail {

struct WindowState;

struct RuntimeState final {
    RuntimeState() = default;
    ~RuntimeState();

    RuntimeState(const RuntimeState&) = delete;
    RuntimeState& operator=(const RuntimeState&) = delete;

    core::ThreadChecker threadChecker{};
    std::weak_ptr<WindowState> activeWindow{};
    bool videoInitialized{false};
};

struct WindowState final {
    WindowState(std::shared_ptr<RuntimeState> runtimeState, SDL_Window* nativeWindow,
                SDL_WindowID nativeWindowId) noexcept;
    ~WindowState();

    WindowState(const WindowState&) = delete;
    WindowState& operator=(const WindowState&) = delete;

    std::shared_ptr<RuntimeState> runtime;
    SDL_Window* window{nullptr};
    SDL_WindowID windowId{0};
};

} // namespace cuexis::platform_sdl::detail
