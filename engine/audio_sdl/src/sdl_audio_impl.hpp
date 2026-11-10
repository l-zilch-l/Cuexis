// Internal SDL audio state and transport implementation shared by the audio units.
#pragma once

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

namespace cuexis::audio_sdl {

[[nodiscard]] inline auto sdlError(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)}.withContext("sdl", SDL_GetError());
}

[[nodiscard]] inline auto emptyEffectiveSettings() noexcept -> audio::EffectiveAudioSettings {
    audio::EffectiveAudioSettings result{};
    // The default-route flag describes an opened SDL device, not an Empty transport.
    result.defaultRouteMayMigrate = false;
    return result;
}

[[nodiscard]] inline auto enumeratePlaybackDevicesUnlocked()
    -> core::Result<std::vector<PlaybackDeviceRecord>> {
    int count = 0;
    SDL_AudioDeviceID* identifiers = SDL_GetAudioPlaybackDevices(&count);
    if (identifiers == nullptr) {
        if (count > 0) {
            return core::unexpected(
                sdlError("audio.sdl.enumerate_failed", "SDL playback devices could not be listed"));
        }
        return std::vector<PlaybackDeviceRecord>{};
    }
    const char* driver = SDL_GetCurrentAudioDriver();
    const std::string driverName = driver == nullptr ? std::string{} : std::string{driver};
    std::vector<PlaybackDeviceRecord> devices;
    devices.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const char* name = SDL_GetAudioDeviceName(identifiers[index]);
        devices.push_back(PlaybackDeviceRecord{
            .instanceId = static_cast<std::uint32_t>(identifiers[index]),
            .driver = driverName,
            .deviceName = name == nullptr ? std::string{} : std::string{name},
        });
    }
    SDL_free(identifiers);
    return devices;
}

[[nodiscard]] inline auto findUniqueDevice(const std::vector<PlaybackDeviceRecord>& devices,
                                           std::string_view driver, std::string_view deviceName)
    -> core::Result<const PlaybackDeviceRecord*> {
    const PlaybackDeviceRecord* matched = nullptr;
    for (const auto& device : devices) {
        if (device.driver == driver && device.deviceName == deviceName) {
            if (matched != nullptr) {
                return core::unexpected(core::Error{"player.audio_profile.ambiguous",
                                                    "More than one output device matches the "
                                                    "requested name"});
            }
            matched = &device;
        }
    }
    if (matched == nullptr) {
        return core::unexpected(core::Error{"player.audio_profile.unmatched",
                                            "No output device matches the requested name"});
    }
    return matched;
}

struct SdlAudioTransport::Impl final {
    struct CallbackCounters final {
        std::atomic<std::uint64_t> sequence{};
        std::atomic<std::int64_t> sampleMonotonicUs{};
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
        std::atomic<std::uint32_t> effectiveSourceSampleRate{};
        std::atomic<std::uint32_t> effectiveSourceChannels{};
        std::atomic<std::uint32_t> effectiveDeviceSampleRate{};
        std::atomic<std::uint32_t> effectiveDeviceChannels{};
        std::atomic<std::uint32_t> effectiveDeviceBufferFrames{};
        std::atomic<std::uint32_t> effectiveTargetQueueMs{};
        std::atomic<std::uint32_t> effectiveRefillLowWaterMs{};
        std::atomic<std::uint64_t> effectiveEstimatedOutputLatencyBits{};
        std::atomic<std::uint32_t> effectiveFormatConverted{};
        std::atomic<std::uint32_t> effectiveDefaultRouteMayMigrate{};
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
        counters->sequence.fetch_add(1, std::memory_order_acq_rel);
        counters->mixedDeviceFrames.fetch_add(static_cast<std::uint64_t>(byteLength) / frameBytes,
                                              std::memory_order_relaxed);
        counters->deviceSampleRate.store(static_cast<std::uint32_t>(spec->freq),
                                         std::memory_order_relaxed);
        counters->deviceChannels.store(static_cast<std::uint32_t>(spec->channels),
                                       std::memory_order_relaxed);
        counters->sampleMonotonicUs.store(std::chrono::duration_cast<std::chrono::microseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count(),
                                          std::memory_order_relaxed);
        counters->sequence.fetch_add(1, std::memory_order_release);
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
        published.effectiveSourceSampleRate.store(effective.sourceSampleRate,
                                                  std::memory_order_relaxed);
        published.effectiveSourceChannels.store(effective.sourceChannels,
                                                std::memory_order_relaxed);
        published.effectiveDeviceSampleRate.store(effective.deviceSampleRate,
                                                  std::memory_order_relaxed);
        published.effectiveDeviceChannels.store(effective.deviceChannels,
                                                std::memory_order_relaxed);
        published.effectiveDeviceBufferFrames.store(effective.deviceBufferFrames,
                                                    std::memory_order_relaxed);
        published.effectiveTargetQueueMs.store(effective.targetQueueMs, std::memory_order_relaxed);
        published.effectiveRefillLowWaterMs.store(effective.refillLowWaterMs,
                                                  std::memory_order_relaxed);
        published.effectiveEstimatedOutputLatencyBits.store(
            std::bit_cast<std::uint64_t>(effective.estimatedOutputLatencyMs),
            std::memory_order_relaxed);
        published.effectiveFormatConverted.store(effective.formatConverted ? 1U : 0U,
                                                 std::memory_order_relaxed);
        published.effectiveDefaultRouteMayMigrate.store(effective.defaultRouteMayMigrate ? 1U : 0U,
                                                        std::memory_order_relaxed);
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
        callbackAttached = false;
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
        SDL_AudioDeviceID playbackDevice = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
        if (exactDevice) {
            auto listed = enumeratePlaybackDevicesUnlocked();
            if (!listed) {
                return core::unexpected(std::move(listed.error()));
            }
            auto matched = findUniqueDevice(*listed, boundDriver, boundDeviceName);
            if (!matched) {
                return core::unexpected(std::move(matched.error()));
            }
            if ((*matched)->instanceId != boundInstanceId) {
                return core::unexpected(
                    core::Error{"audio.sdl.device.stale",
                                "The output device changed between enumeration and open"});
            }
            playbackDevice = static_cast<SDL_AudioDeviceID>((*matched)->instanceId);
        }
        SDL_AudioStream* opened =
            SDL_OpenAudioDeviceStream(playbackDevice, &sourceSpec, nullptr, nullptr);
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
        callbackAttached = true;
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
            !exactDevice};
        state = audio::PlaybackState::Stopped;
        advanceDiscontinuity();
        publish();
        return {};
    }

    void updatePresentedFrame() noexcept {
        if (!clip || state != audio::PlaybackState::Playing) {
            return;
        }
        std::uint32_t deviceRate{};
        std::uint64_t mixed{}, sequence{};
        std::int64_t sampleUs{};
        bool coherent = false;
        for (unsigned attempt = 0; attempt != 8; ++attempt) {
            sequence = callback.sequence.load(std::memory_order_acquire);
            if (sequence & 1U)
                continue;
            deviceRate = callback.deviceSampleRate.load(std::memory_order_relaxed);
            mixed = callback.mixedDeviceFrames.load(std::memory_order_relaxed);
            sampleUs = callback.sampleMonotonicUs.load(std::memory_order_relaxed);
            if (sequence == callback.sequence.load(std::memory_order_acquire)) {
                coherent = true;
                break;
            }
        }
        if (!coherent)
            return;
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
        const auto upperBound = std::max<std::int64_t>(
            0, std::min(submittedFrame, static_cast<std::int64_t>(clip->frameCount())));
        const auto monotonicFloor = std::min(std::max<std::int64_t>(presentedFrame, 0), upperBound);
        const auto rawCandidate =
            advanced > 0 && segmentStartFrame > std::numeric_limits<std::int64_t>::max() - advanced
                ? std::numeric_limits<std::int64_t>::max()
                : segmentStartFrame + advanced;
        const auto candidate = std::min(std::max(rawCandidate, monotonicFloor), upperBound);
        presentedFrame = candidate;
        if (sequence > clockAfterSequence && sampleUs > 0) {
            // AudioClip's byte bound keeps this product representable. Match the clock bridge's
            // nearest-even conversion, including odd seek frames at half-microsecond boundaries.
            const auto scaled = presentedFrame * 1000000;
            auto positionUs = scaled / clip->sampleRate();
            const auto remainder = scaled % clip->sampleRate();
            const auto complement = clip->sampleRate() - remainder;
            if (remainder > complement || (remainder == complement && (positionUs & 1)))
                ++positionUs;
            const auto observedAnchor = sampleUs - positionUs;
            clockAnchorUs =
                clockAnchorUs ? std::min(*clockAnchorUs, observedAnchor) : observedAnchor;
            presentedSampleUs = *clockAnchorUs + positionUs;
            // Refine toward an earlier observed anchor only: samples cannot be future-dated and
            // a delayed callback cannot move the derived clock backward. Report the discrepancy;
            // the host's explicit error budget still applies.
            clockErrorUs = std::max(clockErrorUs, std::abs(sampleUs - *presentedSampleUs));
        }
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
    bool callbackAttached{};
    CallbackCounters callback;
    PublishedClock published;
    audio::AudioClipLease clip;
    std::optional<audio::AudioClipLease> replacementClip;
    std::int64_t replacementFrame{};
    audio::PlaybackState state{audio::PlaybackState::Empty};
    std::int64_t segmentStartFrame{};
    std::int64_t submittedFrame{};
    std::int64_t presentedFrame{};
    std::optional<std::int64_t> clockAnchorUs, presentedSampleUs;
    std::uint64_t clockAfterSequence{};
    std::int64_t clockErrorUs{};
    std::uint64_t mixedDeviceFrameBaseline{};
    std::uint64_t discontinuityId{};
    std::atomic<std::int64_t> queuedFrames{};
    std::atomic<std::uint64_t> underrunCount{};
    std::atomic<std::uint64_t> serviceCount{};
    bool underrunEpisode{};
    bool exactDevice{};
    std::uint32_t boundInstanceId{};
    std::string boundDriver{};
    std::string boundDeviceName{};
    audio::EffectiveAudioSettings effective = emptyEffectiveSettings();
};

} // namespace cuexis::audio_sdl
