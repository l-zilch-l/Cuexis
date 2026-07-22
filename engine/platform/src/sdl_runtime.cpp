//  SdlRuntime 实现 — SDL3 视频运行时 RAII 管理
//  create(): 初始化 SDL 视频子系统并探测驱动；destroy: 在主线程清理
//  executableBasePath(): Windows 通过 GetModuleFileNameW 解析，POSIX 通过 /proc/self/exe
//  SDL3 依赖仅在 platform_sdl 模块内持有，不泄漏到业务层

#include <cuexis/platform_sdl/sdl_runtime.hpp>

#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3/SDL.h>

#include <cuexis/core/error.hpp>

#include "sdl_state.hpp"

namespace cuexis::platform_sdl {
namespace {

std::weak_ptr<detail::RuntimeState> sharedRuntimeState;

void assertOwner(const std::shared_ptr<detail::RuntimeState>& state) noexcept {
    if (state) {
        state->threadChecker.assertCurrent();
    }
}

} // namespace

auto executableBasePath() -> core::Result<std::filesystem::path> {
    const char* const path = SDL_GetBasePath();
    if (path == nullptr || *path == '\0') {
        return core::unexpected(core::Error{"platform.sdl.base_path_unavailable",
                                            "Could not resolve the executable base path"});
    }
    const std::string_view utf8Path{path};
    return std::filesystem::path{std::u8string{utf8Path.begin(), utf8Path.end()}};
}

core::Result<SdlRuntime> SdlRuntime::create() {
    if (!SDL_IsMainThread()) {
        return core::unexpected(core::Error{"platform.sdl.not_main_thread",
                                            "SDL video must be initialized on the main thread"});
    }

    if (auto state = sharedRuntimeState.lock()) {
        if (!state->threadChecker.isCurrent()) {
            return core::unexpected(core::Error{"platform.sdl.not_main_thread",
                                                "The SDL runtime belongs to another thread"});
        }
        state->threadChecker.assertCurrent();
        return SdlRuntime{std::move(state)};
    }

    auto state = std::make_shared<detail::RuntimeState>();
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return core::unexpected(core::Error{"platform.sdl.init_failed", SDL_GetError()});
    }

    state->videoInitialized = true;
    sharedRuntimeState = state;
    return SdlRuntime{std::move(state)};
}

SdlRuntime::SdlRuntime(std::shared_ptr<detail::RuntimeState> state) noexcept
    : state_(std::move(state)) {}

SdlRuntime::~SdlRuntime() {
    assertOwner(state_);
}

SdlRuntime::SdlRuntime(SdlRuntime&& other) noexcept : state_(std::move(other.state_)) {
    assertOwner(state_);
}

SdlRuntime& SdlRuntime::operator=(SdlRuntime&& other) noexcept {
    assertOwner(state_);
    assertOwner(other.state_);
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

std::string_view SdlRuntime::videoDriver() const noexcept {
    if (!state_) {
        return {};
    }

    state_->threadChecker.assertCurrent();
    const char* const driver = SDL_GetCurrentVideoDriver();
    return driver == nullptr ? std::string_view{} : std::string_view{driver};
}

namespace detail {

RuntimeState::~RuntimeState() {
    if (!videoInitialized) {
        return;
    }

    if (!threadChecker.isCurrent()) {
        std::terminate();
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    videoInitialized = false;

    if (SDL_WasInit(0) == 0) {
        SDL_Quit();
    }
}

} // namespace detail
} // namespace cuexis::platform_sdl
