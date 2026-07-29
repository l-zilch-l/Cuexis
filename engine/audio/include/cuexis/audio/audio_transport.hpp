#pragma once

// Backend-neutral source clock, transport state, diagnostics, and control interfaces.

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_export.hpp>
#include <cuexis/core/result.hpp>

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

class CUEXIS_AUDIO_API HostClock final {
  public:
    HostClock() = default;

    [[nodiscard]] auto submit(const SourceClockSample& sample) -> core::Result<void>;
    [[nodiscard]] SourceClockSample snapshot() const noexcept;

  private:
    SourceClockSample sample_{};
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
