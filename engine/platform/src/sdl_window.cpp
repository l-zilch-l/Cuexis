//  SdlWindow 实现 — SDL3 窗口创建与事件轮询
//  create(): 创建 SDL_Window 并可选设置 OpenGL 属性
//  SdlWindowLease: shared_ptr 共享窗口状态，确保后端使用时窗口保持存活
//  所有操作必须在关联的 SdlRuntime 所有者线程（SDL 主线程）执行

#include <cuexis/platform_sdl/sdl_window.hpp>

#include <memory>
#include <string>
#include <utility>

#include <SDL3/SDL.h>

#include <cuexis/core/error.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>

#include "sdl_state.hpp"

namespace cuexis::platform_sdl {
namespace {

struct WindowDeleter final {
    void operator()(SDL_Window* window) const noexcept {
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
    }
};

void assertOwner(const std::shared_ptr<detail::WindowState>& state) noexcept {
    if (state) {
        state->runtime->threadChecker.assertCurrent();
    }
}

} // namespace

core::Result<SdlWindow> SdlWindow::create(SdlRuntime& runtime, const WindowConfig& config) {
    auto runtimeState = runtime.state_;
    if (!runtimeState) {
        return core::unexpected(core::Error{"platform.sdl.runtime_unavailable",
                                            "Cannot create a window without an SDL runtime"});
    }
    if (!SDL_IsMainThread() || !runtimeState->threadChecker.isCurrent()) {
        return core::unexpected(core::Error{"platform.sdl.not_main_thread",
                                            "SDL windows must be created on the runtime thread"});
    }
    runtimeState->threadChecker.assertCurrent();

    if (config.title.empty()) {
        return core::unexpected(
            core::Error{"platform.sdl.invalid_config", "Window title must not be empty"}
                .withContext("field", "title"));
    }
    if (config.width <= 0 || config.height <= 0) {
        return core::unexpected(core::Error{"platform.sdl.invalid_config",
                                            "Window dimensions must be greater than zero"}
                                    .withContext("width", std::to_string(config.width))
                                    .withContext("height", std::to_string(config.height)));
    }
    if (!runtimeState->activeWindow.expired()) {
        return core::unexpected(core::Error{"platform.sdl.window_already_active",
                                            "Stage 0 supports one active SDL window"});
    }

    SDL_WindowFlags flags = 0;
    if (config.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (config.highDpi) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    if (config.openGl) {
        flags |= SDL_WINDOW_OPENGL;
    }

    std::unique_ptr<SDL_Window, WindowDeleter> window{
        SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags)};
    if (!window) {
        return core::unexpected(core::Error{"platform.sdl.window_create_failed", SDL_GetError()});
    }

    const SDL_WindowID windowId = SDL_GetWindowID(window.get());
    if (windowId == 0) {
        return core::unexpected(core::Error{"platform.sdl.window_id_failed", SDL_GetError()});
    }

    auto state = std::make_shared<detail::WindowState>(runtimeState, window.get(), windowId);
    window.release();
    runtimeState->activeWindow = state;
    return SdlWindow{std::move(state)};
}

SdlWindow::SdlWindow(std::shared_ptr<detail::WindowState> state) noexcept
    : state_(std::move(state)) {}

SdlWindow::~SdlWindow() {
    assertOwner(state_);
}

SdlWindow::SdlWindow(SdlWindow&& other) noexcept : state_(std::move(other.state_)) {
    assertOwner(state_);
}

SdlWindow& SdlWindow::operator=(SdlWindow&& other) noexcept {
    assertOwner(state_);
    assertOwner(other.state_);
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

WindowEvents SdlWindow::pollEvents() {
    assertOwner(state_);

    WindowEvents result{};
    if (!state_) {
        return result;
    }

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                             event.window.windowID == state_->windowId)) {
            result.quitRequested = true;
        }
    }
    return result;
}

core::Result<DrawableSize> SdlWindow::drawableSize() const {
    assertOwner(state_);

    if (!state_) {
        return core::unexpected(core::Error{"platform.sdl.window_size_failed",
                                            "Cannot query the size of an empty SDL window"});
    }

    DrawableSize result{};
    if (!SDL_GetWindowSizeInPixels(state_->window, &result.width, &result.height)) {
        return core::unexpected(core::Error{"platform.sdl.window_size_failed", SDL_GetError()});
    }
    return result;
}

SdlWindowLease SdlWindow::lease() const {
    assertOwner(state_);
    return SdlWindowLease{state_};
}

SdlWindowLease::SdlWindowLease(std::shared_ptr<detail::WindowState> state) noexcept
    : state_(std::move(state)) {}

SdlWindowLease::~SdlWindowLease() {
    assertOwner(state_);
}

SdlWindowLease::SdlWindowLease(const SdlWindowLease& other) : state_(other.state_) {
    assertOwner(state_);
}

SdlWindowLease& SdlWindowLease::operator=(const SdlWindowLease& other) {
    assertOwner(state_);
    assertOwner(other.state_);
    if (this != &other) {
        state_ = other.state_;
    }
    return *this;
}

SdlWindowLease::SdlWindowLease(SdlWindowLease&& other) noexcept : state_(std::move(other.state_)) {
    assertOwner(state_);
}

SdlWindowLease& SdlWindowLease::operator=(SdlWindowLease&& other) noexcept {
    assertOwner(state_);
    assertOwner(other.state_);
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

bool SdlWindowLease::valid() const noexcept {
    assertOwner(state_);
    return state_ && state_->window != nullptr;
}

void* SdlWindowLease::nativeHandle() const noexcept {
    assertOwner(state_);
    return state_ ? state_->window : nullptr;
}

namespace detail {

WindowState::WindowState(std::shared_ptr<RuntimeState> runtimeState, SDL_Window* nativeWindow,
                         const SDL_WindowID nativeWindowId) noexcept
    : runtime(std::move(runtimeState)), window(nativeWindow), windowId(nativeWindowId) {}

WindowState::~WindowState() {
    runtime->threadChecker.assertCurrent();
    SDL_DestroyWindow(window);
    window = nullptr;
}

} // namespace detail
} // namespace cuexis::platform_sdl
