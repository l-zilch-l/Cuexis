#pragma once

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/result.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace cuexis::test_support {

class FakeAudioTransport final : public audio::IAudioTransport {
  public:
    explicit FakeAudioTransport(audio::AudioClipStore& store) noexcept : store_(&store) {}

    [[nodiscard]] auto load(audio::AudioClipHandle handle) -> core::Result<void> override {
        if (state_ != audio::PlaybackState::Empty) {
            return failure("audio.transport.not_empty", "Transport must be Empty before load");
        }
        auto lease = store_->lease(handle);
        if (!lease) {
            return core::unexpected(std::move(lease.error()));
        }
        clip_ = std::move(*lease);
        frame_ = 0;
        state_ = audio::PlaybackState::Stopped;
        advanceDiscontinuity();
        return {};
    }

    [[nodiscard]] auto play() -> core::Result<void> override {
        if (state_ == audio::PlaybackState::Playing) {
            return {};
        }
        if (state_ != audio::PlaybackState::Stopped && state_ != audio::PlaybackState::Paused) {
            return failure("audio.transport.play_invalid", "Transport cannot play from this state");
        }
        state_ = audio::PlaybackState::Playing;
        return service();
    }

    [[nodiscard]] auto pause() -> core::Result<void> override {
        if (state_ == audio::PlaybackState::Paused) {
            return {};
        }
        if (state_ != audio::PlaybackState::Playing) {
            return failure("audio.transport.pause_invalid", "Transport is not Playing");
        }
        state_ = audio::PlaybackState::Paused;
        return {};
    }

    [[nodiscard]] auto stop() -> core::Result<void> override {
        if (!clip_ || state_ == audio::PlaybackState::Error) {
            return failure("audio.transport.stop_invalid", "Transport has no stoppable clip");
        }
        if (state_ == audio::PlaybackState::Stopped && frame_ == 0) {
            return {};
        }
        const bool changed = frame_ != 0;
        frame_ = 0;
        state_ = audio::PlaybackState::Stopped;
        underrun_ = false;
        if (changed) {
            advanceDiscontinuity();
        }
        return {};
    }

    [[nodiscard]] auto seekMs(double positionMs) -> core::Result<void> override {
        if (!clip_ || state_ == audio::PlaybackState::Error) {
            return failure("audio.transport.seek_invalid", "Transport has no seekable clip");
        }
        if (!std::isfinite(positionMs) || positionMs < 0.0 || positionMs > clip_->durationMs()) {
            return failure("audio.transport.seek_range", "Seek position is outside the clip");
        }
        const bool wasPlaying = state_ == audio::PlaybackState::Playing;
        const auto nextFrame = static_cast<std::int64_t>(
            std::llround(positionMs * static_cast<double>(clip_->sampleRate()) / 1000.0));
        const bool changed = nextFrame != frame_;
        frame_ = std::clamp<std::int64_t>(nextFrame, 0, clip_->frameCount());
        state_ = wasPlaying ? audio::PlaybackState::Playing : audio::PlaybackState::Paused;
        underrun_ = false;
        if (changed) {
            advanceDiscontinuity();
        }
        return {};
    }

    [[nodiscard]] auto unload() -> core::Result<void> override {
        if (state_ == audio::PlaybackState::Empty) {
            return {};
        }
        clip_.reset();
        replacement_.reset();
        frame_ = 0;
        state_ = audio::PlaybackState::Empty;
        underrun_ = false;
        advanceDiscontinuity();
        return {};
    }

    [[nodiscard]] auto service() -> core::Result<void> override {
        ++serviceCount_;
        if (state_ == audio::PlaybackState::Error) {
            return failure("audio.transport.error", "Transport is in Error state");
        }
        if (failNextService_) {
            failNextService_ = false;
            enterError();
            return failure("audio.fake.service_failed", "Injected fake device failure");
        }
        return {};
    }

    [[nodiscard]] audio::AudioClockSnapshot snapshot() const noexcept override {
        const auto sampleRate = clip_ ? clip_->sampleRate() : 0U;
        const auto position = sampleRate == 0 ? 0.0
                                              : static_cast<double>(frame_) * 1000.0 /
                                                    static_cast<double>(sampleRate);
        return {{position, state_, discontinuity_}, frame_, sampleRate, 0.0};
    }

    [[nodiscard]] audio::AudioMetricsSnapshot metrics() const noexcept override {
        return {.queuedFrames = 0, .underrunCount = underrunCount_, .serviceCount = serviceCount_};
    }

    [[nodiscard]] audio::EffectiveAudioSettings effectiveSettings() const noexcept override {
        return {.sourceSampleRate = clip_ ? clip_->sampleRate() : 0U,
                .sourceChannels = clip_ ? clip_->channels() : 0U,
                .deviceSampleRate = clip_ ? clip_->sampleRate() : 0U,
                .deviceChannels = clip_ ? clip_->channels() : 0U,
                .defaultRouteMayMigrate = false};
    }

    [[nodiscard]] auto submit(const audio::SourceClockSample& sample) -> core::Result<void> {
        if (!clip_) {
            return failure("audio.fake.empty", "Fake transport has no loaded clip");
        }
        if (auto valid = audio::validateSourceClockSample(sample); !valid) {
            return valid;
        }
        if (sample.positionMs > clip_->durationMs()) {
            return failure("audio.fake.position_range", "Fake clock sample exceeds clip duration");
        }
        frame_ = static_cast<std::int64_t>(
            std::llround(sample.positionMs * static_cast<double>(clip_->sampleRate()) / 1000.0));
        state_ = sample.state;
        discontinuity_ = sample.discontinuityId;
        underrun_ = false;
        return {};
    }

    [[nodiscard]] auto advance(double deltaMs) -> core::Result<void> {
        if (!std::isfinite(deltaMs) || deltaMs < 0.0) {
            return failure("audio.fake.delta_invalid",
                           "Fake transport delta must be finite and non-negative");
        }
        if (!clip_ || state_ != audio::PlaybackState::Playing || underrun_) {
            return {};
        }
        const auto deltaFrames = static_cast<std::int64_t>(
            std::llround(deltaMs * static_cast<double>(clip_->sampleRate()) / 1000.0));
        frame_ = std::min(frame_ + deltaFrames, clip_->frameCount());
        if (frame_ == clip_->frameCount()) {
            state_ = audio::PlaybackState::Ended;
        }
        return {};
    }

    void beginUnderrun() noexcept {
        if (state_ == audio::PlaybackState::Playing && !underrun_) {
            underrun_ = true;
            ++underrunCount_;
        }
    }

    void recoverUnderrun() noexcept {
        underrun_ = false;
    }

    void failNextService() noexcept {
        failNextService_ = true;
    }

    [[nodiscard]] auto prepareReplacement(audio::AudioClipHandle handle, double positionMs)
        -> core::Result<void> {
        if (!clip_ || state_ == audio::PlaybackState::Error) {
            return failure("audio.transport.replacement_invalid", "Transport has no active clip");
        }
        auto lease = store_->lease(handle);
        if (!lease) {
            return core::unexpected(std::move(lease.error()));
        }
        const auto* replacementClip = lease->get();
        if (!std::isfinite(positionMs) || positionMs < 0.0 ||
            positionMs > replacementClip->durationMs()) {
            return failure("audio.transport.replacement_position_invalid",
                           "Replacement position is outside the clip");
        }
        replacementFrame_ = static_cast<std::int64_t>(
            std::llround(positionMs * static_cast<double>(replacementClip->sampleRate()) / 1000.0));
        replacement_.emplace(std::move(*lease));
        return {};
    }

    [[nodiscard]] auto activateReplacement() -> core::Result<void> {
        if (!replacement_) {
            return failure("audio.transport.replacement_missing", "No replacement is prepared");
        }
        if (failNextReplacement_) {
            failNextReplacement_ = false;
            replacement_.reset();
            enterError();
            return failure("audio.fake.replacement_failed", "Injected replacement failure");
        }
        const bool wasPlaying = state_ == audio::PlaybackState::Playing;
        clip_ = std::move(*replacement_);
        replacement_.reset();
        frame_ = replacementFrame_;
        state_ = wasPlaying ? audio::PlaybackState::Playing : audio::PlaybackState::Paused;
        underrun_ = false;
        advanceDiscontinuity();
        return {};
    }

    void failNextReplacementActivation() noexcept {
        failNextReplacement_ = true;
    }

  private:
    [[nodiscard]] static auto failure(std::string code, std::string message) -> core::Result<void> {
        return core::unexpected(core::Error{std::move(code), std::move(message)});
    }

    void advanceDiscontinuity() noexcept {
        ++discontinuity_;
        if (discontinuity_ == 0) {
            ++discontinuity_;
        }
    }

    void enterError() noexcept {
        if (state_ != audio::PlaybackState::Error) {
            state_ = audio::PlaybackState::Error;
            underrun_ = false;
            advanceDiscontinuity();
        }
    }

    audio::AudioClipStore* store_{};
    audio::AudioClipLease clip_;
    std::optional<audio::AudioClipLease> replacement_;
    std::int64_t frame_{};
    std::int64_t replacementFrame_{};
    audio::PlaybackState state_{audio::PlaybackState::Empty};
    std::uint64_t discontinuity_{};
    std::uint64_t underrunCount_{};
    std::uint64_t serviceCount_{};
    bool underrun_{};
    bool failNextService_{};
    bool failNextReplacement_{};
};

} // namespace cuexis::test_support
