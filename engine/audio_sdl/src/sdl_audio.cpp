#include <cuexis/audio_sdl/sdl_audio.hpp>

#include <cuexis/core/error.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace cuexis::audio_sdl {

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "AudioSDL requires lock-free 64-bit callback counters");
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "AudioSDL requires lock-free 32-bit callback counters");

namespace {

[[nodiscard]] auto sdlError(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)}.withContext("sdl", SDL_GetError());
}

} // namespace

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

struct SdlAudioTransport::Impl final {
    struct CallbackCounters final {
        std::atomic<std::uint64_t> mixedDeviceFrames{};
        std::atomic<std::uint32_t> deviceSampleRate{};
        std::atomic<std::uint32_t> deviceChannels{};
    };

    struct PublishedClock final {
        std::atomic<std::uint64_t> sequence{};
        std::atomic<std::int64_t> presentedFrame{};
        std::atomic<std::int64_t> positionMicroseconds{};
        std::atomic<std::int64_t> latencyMicroseconds{};
        std::atomic<std::uint32_t> sampleRate{};
        std::atomic<std::uint64_t> discontinuityId{};
        std::atomic<int> state{static_cast<int>(audio::PlaybackState::Empty)};
    };

    Impl(std::shared_ptr<SdlAudioSubsystem::State> subsystemState, audio::AudioClipStore& clipStore,
         const audio::ValidatedAudioConfig& validatedConfig)
        : subsystem(std::move(subsystemState)), store(&clipStore),
          targetQueueMs(validatedConfig.targetQueueMs()),
          refillLowWaterMs(validatedConfig.refillLowWaterMs()), gain(validatedConfig.gain()),
          owner(std::this_thread::get_id()) {}

    ~Impl() {
        if (owner != std::this_thread::get_id()) {
            std::terminate();
        }
        closeStream();
    }

    [[nodiscard]] bool isOwner() const noexcept {
        return owner == std::this_thread::get_id();
    }

    [[nodiscard]] auto requireOwner(std::string_view operation) const -> core::Result<void> {
        if (!isOwner()) {
            return core::unexpected(core::Error{"audio.sdl.not_owner_thread",
                                                "SdlAudioTransport belongs to another thread"}
                                        .withContext("operation", std::string{operation}));
        }
        return {};
    }

    static void SDLCALL postmix(void* userdata, const SDL_AudioSpec* spec, float*, int byteLength) {
        auto* counters = static_cast<CallbackCounters*>(userdata);
        if (counters == nullptr || spec == nullptr || spec->channels <= 0 || spec->freq <= 0 ||
            byteLength <= 0) {
            return;
        }
        const auto frameBytes =
            static_cast<std::uint64_t>(sizeof(float)) * static_cast<std::uint64_t>(spec->channels);
        counters->mixedDeviceFrames.fetch_add(static_cast<std::uint64_t>(byteLength) / frameBytes,
                                              std::memory_order_relaxed);
        counters->deviceSampleRate.store(static_cast<std::uint32_t>(spec->freq),
                                         std::memory_order_relaxed);
        counters->deviceChannels.store(static_cast<std::uint32_t>(spec->channels),
                                       std::memory_order_relaxed);
    }

    void publish() noexcept {
        const auto sampleRate = clip ? clip->sampleRate() : 0U;
        const auto positionUs =
            sampleRate == 0 ? 0
                            : static_cast<std::int64_t>(
                                  std::llround(static_cast<double>(presentedFrame) * 1'000'000.0 /
                                               static_cast<double>(sampleRate)));
        const auto latencyUs =
            static_cast<std::int64_t>(std::llround(effective.estimatedOutputLatencyMs * 1000.0));
        published.sequence.fetch_add(1, std::memory_order_acq_rel);
        published.presentedFrame.store(presentedFrame, std::memory_order_relaxed);
        published.positionMicroseconds.store(positionUs, std::memory_order_relaxed);
        published.latencyMicroseconds.store(latencyUs, std::memory_order_relaxed);
        published.sampleRate.store(sampleRate, std::memory_order_relaxed);
        published.discontinuityId.store(discontinuityId, std::memory_order_relaxed);
        published.state.store(static_cast<int>(state), std::memory_order_relaxed);
        published.sequence.fetch_add(1, std::memory_order_release);
    }

    void advanceDiscontinuity() noexcept {
        ++discontinuityId;
        if (discontinuityId == 0) {
            ++discontinuityId;
        }
    }

    void resetCallbackCounters() noexcept {
        callback.mixedDeviceFrames.store(0, std::memory_order_relaxed);
        callback.deviceSampleRate.store(0, std::memory_order_relaxed);
        callback.deviceChannels.store(0, std::memory_order_relaxed);
        mixedDeviceFrameBaseline = 0;
    }

    void closeStream() noexcept {
        if (stream != nullptr) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
        device = 0;
        resetCallbackCounters();
        clip.reset();
    }

    [[nodiscard]] auto openLease(audio::AudioClipLease newClip, std::int64_t startFrame)
        -> core::Result<void> {
        const auto sampleRate = newClip->sampleRate();
        const auto channels = newClip->channels();
        SDL_AudioSpec sourceSpec{.format = SDL_AUDIO_F32,
                                 .channels = static_cast<int>(channels),
                                 .freq = static_cast<int>(sampleRate)};
        SDL_AudioStream* opened = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                                            &sourceSpec, nullptr, nullptr);
        if (opened == nullptr) {
            return core::unexpected(sdlError("audio.sdl.device_open_failed",
                                             "SDL playback device could not be opened"));
        }
        if (!SDL_SetAudioStreamGain(opened, gain)) {
            SDL_DestroyAudioStream(opened);
            return core::unexpected(
                sdlError("audio.sdl.gain_failed", "SDL audio stream gain could not be set"));
        }
        const SDL_AudioDeviceID openedDevice = SDL_GetAudioStreamDevice(opened);
        SDL_AudioSpec deviceSpec{};
        int deviceFrames = 0;
        if (openedDevice == 0 ||
            !SDL_GetAudioDeviceFormat(openedDevice, &deviceSpec, &deviceFrames) ||
            deviceSpec.freq <= 0 || deviceSpec.channels <= 0 || deviceFrames <= 0) {
            SDL_DestroyAudioStream(opened);
            return core::unexpected(sdlError("audio.sdl.device_format_failed",
                                             "SDL device format could not be queried"));
        }
        resetCallbackCounters();
        if (!SDL_SetAudioPostmixCallback(openedDevice, &Impl::postmix, &callback)) {
            SDL_DestroyAudioStream(opened);
            return core::unexpected(sdlError("audio.sdl.postmix_failed",
                                             "SDL postmix callback could not be installed"));
        }

        stream = opened;
        device = openedDevice;
        clip = std::move(newClip);
        segmentStartFrame = startFrame;
        submittedFrame = startFrame;
        presentedFrame = startFrame;
        queuedFrames.store(0, std::memory_order_relaxed);
        underrunEpisode = false;
        effective = audio::EffectiveAudioSettings{
            sampleRate,
            channels,
            static_cast<std::uint32_t>(deviceSpec.freq),
            static_cast<std::uint32_t>(deviceSpec.channels),
            static_cast<std::uint32_t>(deviceFrames),
            targetQueueMs,
            refillLowWaterMs,
            static_cast<double>(deviceFrames) * 1000.0 / static_cast<double>(deviceSpec.freq),
            deviceSpec.freq != static_cast<int>(sampleRate) ||
                deviceSpec.channels != static_cast<int>(channels),
            true};
        state = audio::PlaybackState::Stopped;
        advanceDiscontinuity();
        publish();
        return {};
    }

    void updatePresentedFrame() noexcept {
        if (!clip || state != audio::PlaybackState::Playing) {
            return;
        }
        const auto deviceRate = callback.deviceSampleRate.load(std::memory_order_relaxed);
        const auto mixed = callback.mixedDeviceFrames.load(std::memory_order_relaxed);
        const auto rate = deviceRate == 0 ? effective.deviceSampleRate : deviceRate;
        if (rate == 0) {
            return;
        }
        const auto mixedInSegment =
            mixed >= mixedDeviceFrameBaseline ? mixed - mixedDeviceFrameBaseline : 0;
        const auto latencyFrames = effective.deviceBufferFrames;
        const auto elapsedDeviceFrames =
            mixedInSegment > latencyFrames ? mixedInSegment - latencyFrames : 0;
        const long double converted = static_cast<long double>(elapsedDeviceFrames) *
                                      static_cast<long double>(clip->sampleRate()) /
                                      static_cast<long double>(rate);
        const auto advanced =
            converted > static_cast<long double>(std::numeric_limits<std::int64_t>::max())
                ? std::numeric_limits<std::int64_t>::max()
                : static_cast<std::int64_t>(converted);
        const auto candidate = std::clamp(segmentStartFrame + advanced, presentedFrame,
                                          std::min(submittedFrame, clip->frameCount()));
        presentedFrame = candidate;
        if (presentedFrame >= clip->frameCount()) {
            presentedFrame = clip->frameCount();
            state = audio::PlaybackState::Ended;
            static_cast<void>(SDL_PauseAudioStreamDevice(stream));
        }
    }

    [[nodiscard]] auto enterError(core::Error error) -> core::Result<void> {
        if (state != audio::PlaybackState::Error) {
            updatePresentedFrame();
            state = audio::PlaybackState::Error;
            advanceDiscontinuity();
            if (stream != nullptr) {
                static_cast<void>(SDL_PauseAudioStreamDevice(stream));
            }
            publish();
        }
        return core::unexpected(std::move(error));
    }

    std::shared_ptr<SdlAudioSubsystem::State> subsystem;
    audio::AudioClipStore* store{};
    std::uint32_t targetQueueMs{};
    std::uint32_t refillLowWaterMs{};
    float gain{};
    std::thread::id owner;

    SDL_AudioStream* stream{};
    SDL_AudioDeviceID device{};
    CallbackCounters callback;
    PublishedClock published;
    audio::AudioClipLease clip;
    std::optional<audio::AudioClipLease> replacementClip;
    std::int64_t replacementFrame{};
    audio::PlaybackState state{audio::PlaybackState::Empty};
    std::int64_t segmentStartFrame{};
    std::int64_t submittedFrame{};
    std::int64_t presentedFrame{};
    std::uint64_t mixedDeviceFrameBaseline{};
    std::uint64_t discontinuityId{};
    std::atomic<std::int64_t> queuedFrames{};
    std::atomic<std::uint64_t> underrunCount{};
    std::atomic<std::uint64_t> serviceCount{};
    bool underrunEpisode{};
    audio::EffectiveAudioSettings effective{};
};

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

auto SdlAudioTransport::load(audio::AudioClipHandle handle) -> core::Result<void> {
    if (auto owner = impl_->requireOwner("load"); !owner) {
        return owner;
    }
    if (impl_->state != audio::PlaybackState::Empty) {
        return core::unexpected(
            core::Error{"audio.transport.not_empty", "Transport must be Empty before load"});
    }
    auto lease = impl_->store->lease(handle);
    if (!lease) {
        return core::unexpected(std::move(lease.error()));
    }
    auto opened = impl_->openLease(std::move(*lease), 0);
    if (!opened) {
        return impl_->enterError(std::move(opened.error()));
    }
    return {};
}

auto SdlAudioTransport::play() -> core::Result<void> {
    if (auto owner = impl_->requireOwner("play"); !owner) {
        return owner;
    }
    if (impl_->state == audio::PlaybackState::Playing) {
        return {};
    }
    if (impl_->state != audio::PlaybackState::Stopped &&
        impl_->state != audio::PlaybackState::Paused) {
        return core::unexpected(
            core::Error{"audio.transport.play_invalid", "Transport cannot play from this state"});
    }
    impl_->state = audio::PlaybackState::Playing;
    if (auto serviced = service(); !serviced) {
        return serviced;
    }
    if (!SDL_ResumeAudioStreamDevice(impl_->stream)) {
        return impl_->enterError(
            sdlError("audio.sdl.resume_failed", "SDL audio stream could not resume"));
    }
    impl_->publish();
    return {};
}

auto SdlAudioTransport::pause() -> core::Result<void> {
    if (auto owner = impl_->requireOwner("pause"); !owner) {
        return owner;
    }
    if (impl_->state == audio::PlaybackState::Paused) {
        return {};
    }
    if (impl_->state != audio::PlaybackState::Playing) {
        return core::unexpected(
            core::Error{"audio.transport.pause_invalid", "Transport is not Playing"});
    }
    impl_->updatePresentedFrame();
    if (!SDL_PauseAudioStreamDevice(impl_->stream)) {
        return impl_->enterError(
            sdlError("audio.sdl.pause_failed", "SDL audio stream could not pause"));
    }
    impl_->state = audio::PlaybackState::Paused;
    impl_->publish();
    return {};
}

auto SdlAudioTransport::stop() -> core::Result<void> {
    if (auto owner = impl_->requireOwner("stop"); !owner) {
        return owner;
    }
    if (impl_->state == audio::PlaybackState::Empty ||
        impl_->state == audio::PlaybackState::Error) {
        return core::unexpected(
            core::Error{"audio.transport.stop_invalid", "Transport has no stoppable clip"});
    }
    if (impl_->state == audio::PlaybackState::Stopped && impl_->presentedFrame == 0) {
        return {};
    }
    if (!SDL_PauseAudioStreamDevice(impl_->stream)) {
        return impl_->enterError(
            sdlError("audio.sdl.pause_failed", "SDL audio stream could not pause"));
    }
    if (!SDL_ClearAudioStream(impl_->stream)) {
        return impl_->enterError(
            sdlError("audio.sdl.clear_failed", "SDL audio stream could not be cleared"));
    }
    const bool changed = impl_->presentedFrame != 0 || impl_->submittedFrame != 0;
    impl_->segmentStartFrame = 0;
    impl_->submittedFrame = 0;
    impl_->presentedFrame = 0;
    impl_->callback.mixedDeviceFrames.store(0, std::memory_order_relaxed);
    impl_->mixedDeviceFrameBaseline = 0;
    impl_->queuedFrames.store(0, std::memory_order_relaxed);
    impl_->state = audio::PlaybackState::Stopped;
    if (changed) {
        impl_->advanceDiscontinuity();
    }
    impl_->publish();
    return {};
}

auto SdlAudioTransport::seekMs(double positionMs) -> core::Result<void> {
    if (auto owner = impl_->requireOwner("seek"); !owner) {
        return owner;
    }
    if (!impl_->clip || impl_->state == audio::PlaybackState::Empty ||
        impl_->state == audio::PlaybackState::Error) {
        return core::unexpected(
            core::Error{"audio.transport.seek_invalid", "Transport has no seekable clip"});
    }
    if (!std::isfinite(positionMs) || positionMs < 0.0 || positionMs > impl_->clip->durationMs()) {
        return core::unexpected(
            core::Error{"audio.transport.seek_range", "Seek position must be in [0, durationMs]"});
    }
    const auto requested = static_cast<std::int64_t>(
        std::llround(positionMs * static_cast<double>(impl_->clip->sampleRate()) / 1000.0));
    const auto frame = std::clamp<std::int64_t>(requested, 0, impl_->clip->frameCount());
    const bool wasPlaying = impl_->state == audio::PlaybackState::Playing;
    if (!SDL_PauseAudioStreamDevice(impl_->stream)) {
        return impl_->enterError(
            sdlError("audio.sdl.pause_failed", "SDL audio stream could not pause"));
    }
    if (!SDL_ClearAudioStream(impl_->stream)) {
        return impl_->enterError(
            sdlError("audio.sdl.clear_failed", "SDL audio stream could not be cleared"));
    }
    const bool changed = frame != impl_->presentedFrame;
    impl_->segmentStartFrame = frame;
    impl_->submittedFrame = frame;
    impl_->presentedFrame = frame;
    impl_->callback.mixedDeviceFrames.store(0, std::memory_order_relaxed);
    impl_->mixedDeviceFrameBaseline = 0;
    impl_->queuedFrames.store(0, std::memory_order_relaxed);
    impl_->state = wasPlaying ? audio::PlaybackState::Playing : audio::PlaybackState::Paused;
    if (changed) {
        impl_->advanceDiscontinuity();
    }
    if (wasPlaying) {
        if (auto serviced = service(); !serviced) {
            return serviced;
        }
        if (!SDL_ResumeAudioStreamDevice(impl_->stream)) {
            return impl_->enterError(
                sdlError("audio.sdl.resume_failed", "SDL audio stream could not resume"));
        }
    }
    impl_->publish();
    return {};
}

auto SdlAudioTransport::unload() -> core::Result<void> {
    if (auto owner = impl_->requireOwner("unload"); !owner) {
        return owner;
    }
    if (impl_->state == audio::PlaybackState::Empty) {
        return {};
    }
    impl_->closeStream();
    impl_->replacementClip.reset();
    impl_->state = audio::PlaybackState::Empty;
    impl_->segmentStartFrame = 0;
    impl_->submittedFrame = 0;
    impl_->presentedFrame = 0;
    impl_->effective = {};
    impl_->advanceDiscontinuity();
    impl_->publish();
    return {};
}

auto SdlAudioTransport::service() -> core::Result<void> {
    if (auto owner = impl_->requireOwner("service"); !owner) {
        return owner;
    }
    impl_->serviceCount.fetch_add(1, std::memory_order_relaxed);
    if (impl_->state == audio::PlaybackState::Error) {
        return core::unexpected(
            core::Error{"audio.transport.error", "Transport is in Error state"});
    }
    if (impl_->state != audio::PlaybackState::Playing) {
        impl_->publish();
        return {};
    }

    SDL_AudioSpec deviceSpec{};
    int deviceFrames = 0;
    if (!SDL_GetAudioDeviceFormat(impl_->device, &deviceSpec, &deviceFrames)) {
        return impl_->enterError(
            sdlError("audio.sdl.device_lost", "SDL audio device format is unavailable"));
    }
    if (deviceSpec.freq != static_cast<int>(impl_->effective.deviceSampleRate) ||
        deviceSpec.channels != static_cast<int>(impl_->effective.deviceChannels) ||
        deviceFrames != static_cast<int>(impl_->effective.deviceBufferFrames)) {
        return impl_->enterError(core::Error{"audio.sdl.device_format_changed",
                                             "SDL audio device format changed during playback"});
    }

    const int queuedBytes = SDL_GetAudioStreamQueued(impl_->stream);
    if (queuedBytes < 0) {
        return impl_->enterError(
            sdlError("audio.sdl.queue_query_failed", "SDL audio queue could not be queried"));
    }
    const auto frameBytes = static_cast<std::int64_t>(impl_->clip->channels() * sizeof(float));
    auto queued = static_cast<std::int64_t>(queuedBytes) / frameBytes;
    impl_->queuedFrames.store(queued, std::memory_order_relaxed);
    const auto targetFrames = std::max<std::int64_t>(
        1, static_cast<std::int64_t>(impl_->clip->sampleRate()) * impl_->targetQueueMs / 1000);
    const auto lowWaterFrames = std::max<std::int64_t>(
        1, static_cast<std::int64_t>(impl_->clip->sampleRate()) * impl_->refillLowWaterMs / 1000);
    if (queued == 0 && impl_->submittedFrame < impl_->clip->frameCount() &&
        !impl_->underrunEpisode && impl_->submittedFrame > impl_->presentedFrame) {
        impl_->updatePresentedFrame();
        impl_->segmentStartFrame = impl_->presentedFrame;
        impl_->mixedDeviceFrameBaseline =
            impl_->callback.mixedDeviceFrames.load(std::memory_order_relaxed);
        impl_->underrunCount.fetch_add(1, std::memory_order_relaxed);
        impl_->underrunEpisode = true;
    }
    if (queued < lowWaterFrames && impl_->submittedFrame < impl_->clip->frameCount()) {
        const auto remaining = impl_->clip->frameCount() - impl_->submittedFrame;
        const auto frames = std::min(remaining, targetFrames - queued);
        const auto sampleOffset =
            static_cast<std::size_t>(impl_->submittedFrame) * impl_->clip->channels();
        const auto bytes = frames * frameBytes;
        if (bytes > std::numeric_limits<int>::max() ||
            !SDL_PutAudioStreamData(impl_->stream, impl_->clip->samples().data() + sampleOffset,
                                    static_cast<int>(bytes))) {
            return impl_->enterError(
                sdlError("audio.sdl.queue_write_failed", "PCM data could not be queued"));
        }
        impl_->submittedFrame += frames;
        queued += frames;
        impl_->queuedFrames.store(queued, std::memory_order_relaxed);
        impl_->underrunEpisode = false;
    }
    impl_->updatePresentedFrame();
    impl_->publish();
    return {};
}

audio::AudioClockSnapshot SdlAudioTransport::snapshot() const noexcept {
    if (!impl_) {
        return {};
    }
    audio::AudioClockSnapshot result;
    for (;;) {
        const auto before = impl_->published.sequence.load(std::memory_order_acquire);
        if ((before & 1U) != 0) {
            continue;
        }
        result.presentedFrame = impl_->published.presentedFrame.load(std::memory_order_relaxed);
        result.sampleRate = impl_->published.sampleRate.load(std::memory_order_relaxed);
        result.source.positionMs = static_cast<double>(impl_->published.positionMicroseconds.load(
                                       std::memory_order_relaxed)) /
                                   1000.0;
        result.estimatedOutputLatencyMs =
            static_cast<double>(
                impl_->published.latencyMicroseconds.load(std::memory_order_relaxed)) /
            1000.0;
        result.source.discontinuityId =
            impl_->published.discontinuityId.load(std::memory_order_relaxed);
        result.source.state = static_cast<audio::PlaybackState>(
            impl_->published.state.load(std::memory_order_relaxed));
        const auto after = impl_->published.sequence.load(std::memory_order_acquire);
        if (before == after) {
            return result;
        }
    }
}

audio::AudioMetricsSnapshot SdlAudioTransport::metrics() const noexcept {
    if (!impl_) {
        return {};
    }
    return {impl_->queuedFrames.load(std::memory_order_relaxed),
            impl_->underrunCount.load(std::memory_order_relaxed),
            impl_->serviceCount.load(std::memory_order_relaxed)};
}

audio::EffectiveAudioSettings SdlAudioTransport::effectiveSettings() const noexcept {
    return impl_ ? impl_->effective : audio::EffectiveAudioSettings{};
}

auto SdlAudioTransport::prepareReplacement(audio::AudioClipHandle handle, double positionMs)
    -> core::Result<void> {
    if (auto owner = impl_->requireOwner("prepare_replacement"); !owner) {
        return owner;
    }
    if (!impl_->clip || impl_->state == audio::PlaybackState::Empty ||
        impl_->state == audio::PlaybackState::Error) {
        return core::unexpected(core::Error{"audio.transport.replacement_invalid",
                                            "Transport has no replaceable active clip"});
    }
    auto lease = impl_->store->lease(handle);
    if (!lease) {
        return core::unexpected(std::move(lease.error()));
    }
    const auto* replacement = lease->get();
    if (!std::isfinite(positionMs) || positionMs < 0.0 || positionMs > replacement->durationMs()) {
        return core::unexpected(core::Error{"audio.transport.replacement_position_invalid",
                                            "Replacement position is outside the clip"});
    }
    impl_->replacementFrame = static_cast<std::int64_t>(
        std::llround(positionMs * static_cast<double>(replacement->sampleRate()) / 1000.0));
    impl_->replacementFrame =
        std::clamp<std::int64_t>(impl_->replacementFrame, 0, replacement->frameCount());
    impl_->replacementClip.emplace(std::move(*lease));
    return {};
}

auto SdlAudioTransport::activateReplacement() -> core::Result<void> {
    if (auto owner = impl_->requireOwner("activate_replacement"); !owner) {
        return owner;
    }
    if (!impl_->replacementClip) {
        return core::unexpected(core::Error{"audio.transport.replacement_missing",
                                            "No replacement clip has been prepared"});
    }
    const bool wasPlaying = impl_->state == audio::PlaybackState::Playing;
    const auto previousState = impl_->state;
    const auto previousFrame = impl_->presentedFrame;
    const auto previousSegmentStart = impl_->segmentStartFrame;
    const auto previousSubmittedFrame = impl_->submittedFrame;
    auto previousClip = std::move(impl_->clip);
    auto replacement = std::move(*impl_->replacementClip);
    impl_->replacementClip.reset();
    impl_->closeStream();
    auto opened = impl_->openLease(std::move(replacement), impl_->replacementFrame);
    if (!opened) {
        impl_->clip = std::move(previousClip);
        impl_->state = previousState;
        impl_->presentedFrame = previousFrame;
        impl_->segmentStartFrame = previousSegmentStart;
        impl_->submittedFrame = previousSubmittedFrame;
        return impl_->enterError(std::move(opened.error()));
    }
    impl_->state = wasPlaying ? audio::PlaybackState::Playing : audio::PlaybackState::Paused;
    if (wasPlaying) {
        if (auto serviced = service(); !serviced) {
            return serviced;
        }
        if (!SDL_ResumeAudioStreamDevice(impl_->stream)) {
            return impl_->enterError(
                sdlError("audio.sdl.resume_failed", "SDL replacement stream could not resume"));
        }
    }
    impl_->publish();
    return {};
}

void SdlAudioTransport::cancelReplacement() noexcept {
    if (impl_ && impl_->isOwner()) {
        impl_->replacementClip.reset();
    }
}

} // namespace cuexis::audio_sdl
