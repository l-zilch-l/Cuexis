#pragma once

// TimingMap - deterministic bidirectional conversion between Beat and chartTimeMs.
// Tempo curves are integrated with a fixed quadrature rule and inversion uses a fixed
// iteration budget. Runtime queries allocate no memory.

#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/result.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace cuexis::chart {

struct TempoEvent final {
    RationalBeat startBeat;
    RationalBeat durationBeats;
    double startBpm{120.0};
    double endBpm{120.0};
    double startSlope{};
    double endSlope{};
};

struct TimingStop final {
    RationalBeat beat;
    double durationMs{};
};

struct BeatSample final {
    double beat{};
    bool inStop{};
    double stopProgress{};
    RationalBeat rationalBeat{RationalBeat::zero()};
};

class TimingMap final {
  public:
    // Legacy overload accepts any finite positive BPM for SDK 0.7.0 compatibility.
    [[nodiscard]] static auto create(double defaultBpm, double offsetMs) -> core::Result<TimingMap>;
    // Explicit tempo/stop overload enforces [1, 65536] BPM and fixed event/stop limits.
    [[nodiscard]] static auto create(double defaultBpm, double offsetMs,
                                     std::span<const TempoEvent> tempoEvents,
                                     std::span<const TimingStop> stops) -> core::Result<TimingMap>;

    [[nodiscard]] auto defaultBpm() const noexcept -> double;
    [[nodiscard]] auto offsetMs() const noexcept -> double;

    // chartTimeMs is relative to Beat 0. Offset only converts audio time to chart time.
    [[nodiscard]] auto beatToChartTimeMs(const RationalBeat& beat) const noexcept -> double;
    [[nodiscard]] auto beatToChartTimeMs(double beat) const -> core::Result<double>;
    [[nodiscard]] auto chartTimeMsToBeat(double chartTimeMs) const -> core::Result<double>;
    [[nodiscard]] auto sampleChartTimeMs(double chartTimeMs) const -> core::Result<BeatSample>;
    [[nodiscard]] auto audioTimeMsToChartTimeMs(double audioTimeMs) const -> core::Result<double>;
    [[nodiscard]] auto chartTimeMsToAudioTimeMs(double chartTimeMs) const -> core::Result<double>;

  private:
    static constexpr std::size_t maxIntegrationSegments = 16;

    struct CompiledTempoSegment final {
        double startNormalized{};
        double endNormalized{};
        double startTimeMs{};
        double endTimeMs{};
    };

    struct CompiledTempoEvent final {
        double startBeat{};
        double endBeat{};
        double startBpm{};
        double endBpm{};
        double startSlope{};
        double endSlope{};
        double startTimeMs{};
        double endTimeMs{};
        std::array<CompiledTempoSegment, maxIntegrationSegments> integrationSegments{};
        std::size_t integrationSegmentCount{};
    };

    struct CompiledStop final {
        double beat{};
        double startTimeMs{};
        double endTimeMs{};
        double cumulativeDurationMs{};
    };

    TimingMap(double defaultBpm, double offsetMs, std::vector<CompiledTempoEvent> tempoEvents,
              std::vector<CompiledStop> stops, double tempoTimeAtZero,
              double stopDurationBeforeZero) noexcept;

    [[nodiscard]] auto tempoTime(double beat) const noexcept -> double;
    [[nodiscard]] auto tempoBeat(double chartTimeMs) const noexcept -> double;
    [[nodiscard]] auto stopAdjustment(double beat) const noexcept -> double;

    double defaultBpm_{};
    double offsetMs_{};
    std::vector<CompiledTempoEvent> tempoEvents_;
    std::vector<CompiledStop> stops_;
    double tempoTimeAtZero_{};
    double stopDurationBeforeZero_{};
};

} // namespace cuexis::chart
