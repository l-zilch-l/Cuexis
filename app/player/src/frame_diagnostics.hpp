#pragma once

#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_session.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace cuexis::player {

struct TraceLimits final {
    std::size_t maxRows{65536};
    std::size_t maxBytes{16U * 1024U * 1024U};
};

class FrameDiagnostics final {
  public:
    explicit FrameDiagnostics(std::filesystem::path prefix, TraceLimits limits = {});

    void captureFrame(std::uint64_t frameIndex, const playback::RuntimeFrame& frame,
                      const playback::FrameSnapshot& snapshot) noexcept;
    void captureAudio(std::uint64_t frameIndex, double wallClockMs,
                      const audio::AudioClockSnapshot& clock,
                      const audio::AudioMetricsSnapshot& metrics) noexcept;

    [[nodiscard]] auto exportArtifacts(playback::PlaybackMode mode) const -> core::Result<void>;

    [[nodiscard]] std::size_t capturedFrameRows() const noexcept;
    [[nodiscard]] std::size_t capturedAudioRows() const noexcept;
    [[nodiscard]] std::size_t droppedFrameRows() const noexcept;
    [[nodiscard]] std::size_t droppedAudioRows() const noexcept;

    [[nodiscard]] static std::uint64_t frameHash(const playback::RuntimeFrame& frame,
                                                 const playback::FrameSnapshot& snapshot) noexcept;

  private:
    struct FrameRow final {
        std::uint64_t frameIndex{};
        double chartTimeMs{};
        double simulationDeltaTimeMs{};
        std::uint64_t discontinuityId{};
        std::uint64_t hash{};
    };

    struct AudioRow final {
        std::uint64_t frameIndex{};
        double wallClockMs{};
        double sourcePositionMs{};
        double estimatedOutputLatencyMs{};
        std::int64_t queuedFrames{};
        std::uint64_t underrunCount{};
        audio::PlaybackState state{audio::PlaybackState::Empty};
    };

    std::filesystem::path prefix_;
    TraceLimits limits_;
    std::vector<FrameRow> frames_;
    std::vector<AudioRow> audio_;
    std::size_t droppedFrames_{};
    std::size_t droppedAudio_{};
};

} // namespace cuexis::player
