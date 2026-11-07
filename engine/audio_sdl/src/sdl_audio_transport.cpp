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

auto SdlAudioTransport::applyGain(float gain) -> core::Result<void> {
    if (auto owner = impl_->requireOwner("apply_gain"); !owner) {
        return owner;
    }
    if (!std::isfinite(gain) || gain < 0.0F || gain > 1.0F) {
        return core::unexpected(
            core::Error{"audio.sdl.gain_invalid", "gain must be finite and in [0, 1]"});
    }
    impl_->gain = gain;
    if (impl_->stream != nullptr && !SDL_SetAudioStreamGain(impl_->stream, gain)) {
        return core::unexpected(
            sdlError("audio.sdl.gain_failed", "SDL audio stream gain could not be set"));
    }
    return {};
}

auto SdlAudioTransport::recheckBoundDevice() -> core::Result<void> {
    if (auto owner = impl_->requireOwner("recheck_device"); !owner) {
        return owner;
    }
    if (impl_->exactDevice) {
        auto listed = enumeratePlaybackDevicesUnlocked();
        if (!listed) {
            return impl_->enterError(std::move(listed.error()));
        }
        auto matched = findUniqueDevice(*listed, impl_->boundDriver, impl_->boundDeviceName);
        if (!matched) {
            return impl_->enterError(
                std::move(matched.error()).withContext("profile_device", impl_->boundDeviceName));
        }
        impl_->boundInstanceId = (*matched)->instanceId;
    }
    if (impl_->stream == nullptr || impl_->device == 0 || impl_->effective.deviceSampleRate == 0 ||
        impl_->effective.deviceChannels == 0) {
        return {};
    }
    SDL_AudioSpec spec{};
    int frames = 0;
    if (!SDL_GetAudioDeviceFormat(impl_->device, &spec, &frames) || spec.freq <= 0 ||
        spec.channels <= 0) {
        return impl_->enterError(
            core::Error{"audio.sdl.device.lost", "The opened audio device is no longer available"});
    }
    if (static_cast<std::uint32_t>(spec.freq) != impl_->effective.deviceSampleRate ||
        static_cast<std::uint32_t>(spec.channels) != impl_->effective.deviceChannels) {
        return impl_->enterError(core::Error{"player.audio_profile.format_changed",
                                             "The opened audio device format changed"});
    }
    return {};
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

auto SdlAudioTransport::isolateCallbacks() -> core::Result<bool> try {
    if (auto owner = impl_->requireOwner("isolate_callbacks"); !owner)
        return core::unexpected(owner.error());
    if (impl_->state == audio::PlaybackState::Playing)
        return core::unexpected(
            core::Error{"audio.sdl.isolation_invalid", "Pause or stop before callback isolation"});
    if (impl_->device && impl_->callbackAttached) {
        // SDL synchronizes this setter against the device mixing iteration.
        if (!SDL_SetAudioPostmixCallback(impl_->device, nullptr, nullptr))
            return core::unexpected(
                sdlError("audio.sdl.isolation_failed", "SDL callback could not be detached"));
        impl_->callbackAttached = false;
    }
    return true;
} catch (...) {
    return core::unexpected(
        core::Error{"audio.sdl.isolation_failed", "SDL callback isolation could not be confirmed"});
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
    if (!impl_->callbackAttached) {
        if (!SDL_SetAudioPostmixCallback(impl_->device, &Impl::postmix, &impl_->callback))
            return impl_->enterError(
                sdlError("audio.sdl.postmix_failed", "SDL callback could not be reattached"));
        impl_->callbackAttached = true;
    }
    impl_->clockAnchorUs.reset();
    impl_->presentedSampleUs.reset();
    impl_->clockErrorUs = 0;
    impl_->clockAfterSequence = impl_->callback.sequence.load(std::memory_order_acquire);
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

auto SdlAudioTransport::presentedClockObservation() const noexcept -> PresentedClockObservation {
    PresentedClockObservation result;
    if (!impl_ || !impl_->isOwner())
        return result;
    result.clock = snapshot();
    if (impl_->clip)
        result.maximumPresentedFrame = static_cast<std::int64_t>(impl_->clip->frameCount());
    if (impl_->state == audio::PlaybackState::Playing ||
        impl_->state == audio::PlaybackState::Ended) {
        result.sampleMonotonicUs = impl_->presentedSampleUs;
        result.errorBoundUs = impl_->clockErrorUs;
    } else {
        result.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count();
    }
    return result;
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
    // A postmix callback already in flight may race this relaxed reset. That can only reduce the
    // precision of the presented-position estimate; it is not cross-thread synchronization.
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
    // A postmix callback already in flight may race this relaxed reset. That can only reduce the
    // precision of the presented-position estimate; it is not cross-thread synchronization.
    impl_->callback.mixedDeviceFrames.store(0, std::memory_order_relaxed);
    impl_->mixedDeviceFrameBaseline = 0;
    impl_->queuedFrames.store(0, std::memory_order_relaxed);
    impl_->state = wasPlaying ? audio::PlaybackState::Playing : audio::PlaybackState::Paused;
    impl_->clockAnchorUs.reset();
    impl_->presentedSampleUs.reset();
    impl_->clockErrorUs = 0;
    impl_->clockAfterSequence = impl_->callback.sequence.load(std::memory_order_acquire);
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
    impl_->effective = emptyEffectiveSettings();
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
    if (!impl_) {
        return emptyEffectiveSettings();
    }

    struct SnapshotCache final {
        const Impl* owner{};
        audio::EffectiveAudioSettings settings = emptyEffectiveSettings();
    };
    thread_local SnapshotCache cache;
    if (cache.owner != impl_.get()) {
        cache.owner = impl_.get();
        cache.settings = emptyEffectiveSettings();
    }

    constexpr std::uint32_t maxAttempts = 8;
    for (std::uint32_t attempt = 0; attempt < maxAttempts; ++attempt) {
        const auto before = impl_->published.sequence.load(std::memory_order_acquire);
        if ((before & 1U) != 0) {
            continue;
        }

        audio::EffectiveAudioSettings result;
        result.sourceSampleRate =
            impl_->published.effectiveSourceSampleRate.load(std::memory_order_relaxed);
        result.sourceChannels =
            impl_->published.effectiveSourceChannels.load(std::memory_order_relaxed);
        result.deviceSampleRate =
            impl_->published.effectiveDeviceSampleRate.load(std::memory_order_relaxed);
        result.deviceChannels =
            impl_->published.effectiveDeviceChannels.load(std::memory_order_relaxed);
        result.deviceBufferFrames =
            impl_->published.effectiveDeviceBufferFrames.load(std::memory_order_relaxed);
        result.targetQueueMs =
            impl_->published.effectiveTargetQueueMs.load(std::memory_order_relaxed);
        result.refillLowWaterMs =
            impl_->published.effectiveRefillLowWaterMs.load(std::memory_order_relaxed);
        result.estimatedOutputLatencyMs = std::bit_cast<double>(
            impl_->published.effectiveEstimatedOutputLatencyBits.load(std::memory_order_relaxed));
        result.formatConverted =
            impl_->published.effectiveFormatConverted.load(std::memory_order_relaxed) != 0;
        result.defaultRouteMayMigrate =
            impl_->published.effectiveDefaultRouteMayMigrate.load(std::memory_order_relaxed) != 0;

        const auto after = impl_->published.sequence.load(std::memory_order_acquire);
        if (before == after && (after & 1U) == 0) {
            cache.settings = result;
            return result;
        }
    }
    return cache.settings;
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
    auto replacement = std::move(*impl_->replacementClip);
    impl_->replacementClip.reset();
    impl_->closeStream();
    auto opened = impl_->openLease(std::move(replacement), impl_->replacementFrame);
    if (!opened) {
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
