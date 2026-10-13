#pragma once

#include <cstdint>
#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <memory>
#include <optional>
#include <string>

namespace cuexis::playback {
CUEXIS_ABI_WARNING_PUSH
struct RealtimeRational final {
    std::int64_t numerator, denominator;
    friend auto operator==(RealtimeRational, RealtimeRational) -> bool = default;
};
struct ClockFramePosition final {
    std::int64_t frame, rate;
    friend auto operator==(ClockFramePosition, ClockFramePosition) -> bool = default;
};
enum class ClockPlaybackState : std::uint8_t { Playing, Paused, EndOfStream, Failed };
enum class ClockQuality : std::uint8_t { Exact, Estimated, Unknown };
struct ClockObservation final {
    std::string monotonicDomain;
    std::uint64_t segment, publicationSequence;
    std::int64_t sampleMonotonicUs, validUntilMonotonicUs, errorBoundUs;
    std::optional<ClockFramePosition> source, presented, deviceMixed;
    ClockPlaybackState playback;
    ClockQuality quality;
    bool underrun, deviceChanged, rateChanged;
    // Optional finite source endpoint. Extrapolation stops here until explicit EOF/tail.
    std::optional<ClockFramePosition> maximumPresented;
    friend auto operator==(const ClockObservation&, const ClockObservation&) -> bool = default;
};
struct SchedulingProfile final {
    std::optional<std::int64_t> observationValidityUs, maximumErrorUs;
    std::optional<std::uint64_t> retainedMessageCapacity;
    std::string measurementIdentity;
    bool testOnly;
};
struct ClockMappingConfiguration final {
    RealtimeRational timingOffsetUs, inputCompensationUs, displayLeadUs, fixedRate;
};
struct ClockTicks final {
    std::int64_t observationTick, horizon, presentationTick;
    std::uint64_t segment, publicationSequence;
    std::int64_t errorBoundUs;
    friend auto operator==(ClockTicks, ClockTicks) -> bool = default;
};
struct SeekClockPosition final {
    RealtimeRational sourceUs;
    std::int64_t observationTick, horizon, presentationTick;
};
struct TailProfile final {
    RealtimeRational fixedRate;
    std::int64_t inputDeadline, maximumAnchorErrorUs;
    std::string measurementIdentity;
    bool testOnly;
};
struct TailClockTransition final {
    ClockObservation eof;
    std::uint64_t tailSegment;
    std::int64_t transitionMonotonicUs, terminalHorizon, inputDeadline;
    std::int64_t lastTimer{}, closeThreshold{};
};
// The adapter supplies presented position once, including its latency estimate.
// This bridge never subtracts output latency a second time or restamps a cached sample.
class CUEXIS_PLAYBACK_API RealtimeClock final {
  public:
    // Bind judgement-affecting mapping parameters; display lead is intentionally excluded.
    [[nodiscard]] static auto calibrationIdentity(ClockMappingConfiguration)
        -> core::Result<std::string>;
    // Pure inverse mapping of a logical seek horizon. This is not a device observation
    // and grants no permission to advance. Fixed rate applies only to elapsed extrapolation.
    [[nodiscard]] static auto seekPosition(ClockMappingConfiguration, std::int64_t horizon)
        -> core::Result<SeekClockPosition>;
    [[nodiscard]] static auto create(SchedulingProfile, ClockMappingConfiguration,
                                     std::string monotonicDomain, std::uint64_t segment)
        -> core::Result<RealtimeClock>;
    ~RealtimeClock() noexcept;
    RealtimeClock(RealtimeClock&&) noexcept;
    auto operator=(RealtimeClock&&) noexcept -> RealtimeClock&;
    RealtimeClock(const RealtimeClock&) = delete;
    auto operator=(const RealtimeClock&) -> RealtimeClock& = delete;
    [[nodiscard]] auto sample(const ClockObservation&, std::int64_t nowMonotonicUs)
        -> core::Result<ClockTicks>;
    // Capture-only frozen Paused observation; never grants permission to advance.
    [[nodiscard]] auto observePaused(const ClockObservation&, std::int64_t nowMonotonicUs)
        -> core::Result<ClockTicks>;
    [[nodiscard]] auto startSegment(std::uint64_t, const ClockObservation&,
                                    std::int64_t nowMonotonicUs) -> core::Result<ClockTicks>;
    [[nodiscard]] auto startTail(const ClockObservation&, std::int64_t nowMonotonicUs,
                                 std::uint64_t tailSegment, TailProfile, std::int64_t lastTimer,
                                 std::int64_t closeThreshold) -> core::Result<TailClockTransition>;
    [[nodiscard]] auto sampleTail(std::int64_t nowMonotonicUs) -> core::Result<ClockTicks>;
    [[nodiscard]] auto pausedTailTicks() const -> core::Result<ClockTicks>;
    [[nodiscard]] auto observeEof(const ClockObservation&, std::int64_t nowMonotonicUs)
        -> core::Result<ClockTicks>;
    [[nodiscard]] auto pauseTail(std::int64_t nowMonotonicUs) -> core::Result<void>;
    // Apply the authoritative synthetic source pause action received by a replica owner.
    [[nodiscard]] auto adoptTailPause(std::int64_t sourcePauseMonotonicUs) -> core::Result<void>;
    [[nodiscard]] auto resumeTail(std::int64_t nowMonotonicUs, std::uint64_t newSegment,
                                  std::optional<std::int64_t> minimumTerminal = {})
        -> core::Result<void>;
    [[nodiscard]] auto permitsAdvance() const noexcept -> bool;

  private:
    struct Impl;
    explicit RealtimeClock(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl_;
};
CUEXIS_ABI_WARNING_POP
} // namespace cuexis::playback
