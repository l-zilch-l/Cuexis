#include <cuexis/audio_sdl/sdl_audio.hpp>

#include <cuexis/core/error.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "sdl_audio_impl.hpp"

namespace cuexis::audio_sdl {

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "AudioSDL requires lock-free 64-bit callback counters");
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "AudioSDL requires lock-free 32-bit callback counters");
struct SdlAudioSubsystem::State final {
    State() : owner(std::this_thread::get_id()) {}

    ~State() {
        if (!initialized) {
            return;
        }
        if (owner != std::this_thread::get_id()) {
            std::terminate();
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        initialized = false;
        if (SDL_WasInit(0) == 0) {
            SDL_Quit();
        }
    }

    std::thread::id owner;
    bool initialized{};
};

auto SdlAudioSubsystem::create() -> core::Result<SdlAudioSubsystem> {
    auto state = std::make_shared<State>();
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return core::unexpected(
            sdlError("audio.sdl.init_failed", "SDL audio subsystem initialization failed"));
    }
    state->initialized = true;
    return SdlAudioSubsystem{std::move(state)};
}

auto SdlAudioSubsystem::enumeratePlaybackDevices() const
    -> core::Result<std::vector<PlaybackDeviceRecord>> {
    if (!state_ || !state_->initialized) {
        return core::unexpected(
            core::Error{"audio.sdl.subsystem_unavailable", "SDL audio subsystem is unavailable"});
    }
    if (state_->owner != std::this_thread::get_id()) {
        return core::unexpected(core::Error{"audio.sdl.not_owner_thread",
                                            "SDL audio subsystem belongs to another thread"});
    }
    return enumeratePlaybackDevicesUnlocked();
}

SdlAudioSubsystem::SdlAudioSubsystem(std::shared_ptr<State> state) noexcept
    : state_(std::move(state)) {}

SdlAudioSubsystem::~SdlAudioSubsystem() {
    if (state_ && state_->owner != std::this_thread::get_id()) {
        std::terminate();
    }
}

SdlAudioSubsystem::SdlAudioSubsystem(SdlAudioSubsystem&& other) noexcept
    : state_(std::move(other.state_)) {
    if (state_ && state_->owner != std::this_thread::get_id()) {
        std::terminate();
    }
}

auto SdlAudioSubsystem::operator=(SdlAudioSubsystem&& other) noexcept -> SdlAudioSubsystem& {
    if ((state_ && state_->owner != std::this_thread::get_id()) ||
        (other.state_ && other.state_->owner != std::this_thread::get_id())) {
        std::terminate();
    }
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

auto SdlAudioTransport::create(SdlAudioSubsystem& subsystem, audio::AudioClipStore& store,
                               const audio::ValidatedAudioConfig& config)
    -> core::Result<SdlAudioTransport> {
    if (!subsystem.state_ || !subsystem.state_->initialized) {
        return core::unexpected(
            core::Error{"audio.sdl.subsystem_unavailable", "SDL audio subsystem is unavailable"});
    }
    if (subsystem.state_->owner != std::this_thread::get_id()) {
        return core::unexpected(core::Error{"audio.sdl.not_owner_thread",
                                            "SDL audio subsystem belongs to another thread"});
    }
    return SdlAudioTransport{std::make_unique<Impl>(subsystem.state_, store, config)};
}

auto SdlAudioTransport::createForDevice(SdlAudioSubsystem& subsystem, audio::AudioClipStore& store,
                                        const audio::ValidatedAudioConfig& config,
                                        const PlaybackDeviceTarget& target)
    -> core::Result<SdlAudioTransport> {
    if (!subsystem.state_ || !subsystem.state_->initialized) {
        return core::unexpected(
            core::Error{"audio.sdl.subsystem_unavailable", "SDL audio subsystem is unavailable"});
    }
    if (subsystem.state_->owner != std::this_thread::get_id()) {
        return core::unexpected(core::Error{"audio.sdl.not_owner_thread",
                                            "SDL audio subsystem belongs to another thread"});
    }
    auto listed = subsystem.enumeratePlaybackDevices();
    if (!listed) {
        return core::unexpected(std::move(listed.error()));
    }
    auto matched = findUniqueDevice(*listed, target.driver, target.deviceName);
    if (!matched) {
        return core::unexpected(std::move(matched.error()));
    }
    if ((*matched)->instanceId != target.instanceId) {
        return core::unexpected(core::Error{
            "audio.sdl.device.stale", "The output device changed between enumeration and open"});
    }
    auto impl = std::make_unique<Impl>(subsystem.state_, store, config);
    impl->exactDevice = true;
    impl->boundInstanceId = target.instanceId;
    impl->boundDriver = target.driver;
    impl->boundDeviceName = target.deviceName;
    return SdlAudioTransport{std::move(impl)};
}

SdlAudioTransport::SdlAudioTransport(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

SdlAudioTransport::~SdlAudioTransport() = default;

SdlAudioTransport::SdlAudioTransport(SdlAudioTransport&& other) noexcept
    : impl_(std::move(other.impl_)) {
    if (impl_ && !impl_->isOwner()) {
        std::terminate();
    }
}

auto SdlAudioTransport::operator=(SdlAudioTransport&& other) noexcept -> SdlAudioTransport& {
    if ((impl_ && !impl_->isOwner()) || (other.impl_ && !other.impl_->isOwner())) {
        std::terminate();
    }
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}
} // namespace cuexis::audio_sdl
