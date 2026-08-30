#pragma once

// Backend-neutral source clock, transport state, diagnostics, and control interfaces.

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_export.hpp>
#include <cuexis/core/result.hpp>

#include <atomic>
#include <cstdint>

namespace cuexis::audio {

enum class PlaybackState {
    Empty,
    Stopped,
    Playing,
    Paused,
    Ended,
    Error,
};

struct SourceClockSample final {
    // Abstract source-domain time. Finite negative values represent pre-roll; physical audio
    // transports validate their seek and frame positions separately.
    // positionMs must not decrease within one discontinuity segment, regardless of state. A host
    // that publishes Ended and then resets to Stopped at zero must increment discontinuityId first.
    double positionMs{};
    PlaybackState state{PlaybackState::Stopped};
    std::uint64_t discontinuityId{};
};

struct AudioClockSnapshot final {
    SourceClockSample source{};
    std::int64_t presentedFrame{};
    std::uint32_t sampleRate{};
    double estimatedOutputLatencyMs{};
};

struct AudioMetricsSnapshot final {
    std::int64_t queuedFrames{};
    std::uint64_t underrunCount{};
    std::uint64_t serviceCount{};
};

struct EffectiveAudioSettings final {
    std::uint32_t sourceSampleRate{};
    std::uint32_t sourceChannels{};
    std::uint32_t deviceSampleRate{};
    std::uint32_t deviceChannels{};
    std::uint32_t deviceBufferFrames{};
    std::uint32_t targetQueueMs{};
    std::uint32_t refillLowWaterMs{};
    double estimatedOutputLatencyMs{};
    bool formatConverted{};
    bool defaultRouteMayMigrate{true};
};

[[nodiscard]] CUEXIS_AUDIO_API auto validateSourceClockSample(const SourceClockSample& sample)
    -> core::Result<void>;

// HostClock is a single-owner source clock with seqlock publication.
// The owner thread must call submit(). snapshot() may run concurrently and returns a complete
// sample without allocating or throwing. Concurrent submit() calls remain unsupported.
class HostClock final {
  public:
    HostClock() = default;

    // Submit a sample from the HostClock owner thread only.
    [[nodiscard]] CUEXIS_AUDIO_API auto submit(const SourceClockSample& sample)
        -> core::Result<void>;

    // Read a complete published sample, even while submit() is running.
    [[nodiscard]] CUEXIS_AUDIO_API SourceClockSample snapshot() const noexcept;

  private:
    std::atomic<std::uint64_t> sequence_{};
    std::atomic<double> positionMs_{};
    std::atomic<int> state_{static_cast<int>(PlaybackState::Stopped)};
    std::atomic<std::uint64_t> discontinuityId_{};
    bool initialized_{};
};

class CUEXIS_AUDIO_API IAudioClock {
  public:
    virtual ~IAudioClock();
    [[nodiscard]] virtual AudioClockSnapshot snapshot() const noexcept = 0;
};

class CUEXIS_AUDIO_API IAudioTransport : public IAudioClock {
  public:
    ~IAudioTransport() override;

    [[nodiscard]] virtual auto load(AudioClipHandle handle) -> core::Result<void> = 0;
    [[nodiscard]] virtual auto play() -> core::Result<void> = 0;
    [[nodiscard]] virtual auto pause() -> core::Result<void> = 0;
    [[nodiscard]] virtual auto stop() -> core::Result<void> = 0;
    [[nodiscard]] virtual auto seekMs(double positionMs) -> core::Result<void> = 0;
    [[nodiscard]] virtual auto unload() -> core::Result<void> = 0;
    [[nodiscard]] virtual auto service() -> core::Result<void> = 0;
    [[nodiscard]] virtual AudioMetricsSnapshot metrics() const noexcept = 0;
    [[nodiscard]] virtual EffectiveAudioSettings effectiveSettings() const noexcept = 0;
};

} // namespace cuexis::audio
