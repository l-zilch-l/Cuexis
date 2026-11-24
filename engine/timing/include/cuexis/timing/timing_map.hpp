#pragma once

//  TimingMap - the single Beat <-> microsecond mapping shared by the author compiler and the
//  compiled-artifact Reader.
//
//  One implementation owns both directions and both quantisation boundaries. Beat positions are
//  exact rationals; chart time is a signed 64-bit microsecond tick. Discrete anchor ticks are the
//  authority for every integer comparison: a beat that was saved as an anchor returns exactly its
//  saved tick, and no query recomputes or replaces a saved anchor. Continuous interpolation and the
//  inverse direction are non-binding values for sampling only.
//
//  Tempo curves use the same bounded continuous integration as the old Chart TimingMap (fixed
//  Hermite curve and fixed quadrature per segment, extrapolation outside the declared events).
//  Stop duration is added once at the stop beat: the stop occupies `[S, E)` in the reverse
//  direction, and beat 0 is fixed to tick 0.

#include <cuexis/core/result.hpp>
#include <cuexis/timing/rational_beat.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace cuexis::timing {

namespace detail {
struct TimingMapStorage;
}

//  One complete tick value on the microsecond scale.
struct TimeUs final {
    std::int64_t value{};

    friend constexpr auto operator==(TimeUs, TimeUs) noexcept -> bool = default;
    friend constexpr auto operator<=>(TimeUs, TimeUs) noexcept = default;
};

//  One tempo curve section. `startBeat` is the exact beat at which the section becomes effective;
//  the section lasts `durationBeats`. The BPM curve is a Hermite curve between `startBpm` and
//  `endBpm` with the declared end slopes.
struct TempoEvent final {
    RationalBeat startBeat{RationalBeat::zero()};
    RationalBeat durationBeats{RationalBeat::zero()};
    double startBpm{120.0};
    double endBpm{120.0};
    double startSlope{};
    double endSlope{};
};

//  One Stop declaration: chart time freezes for `durationMs` at `beat`.
struct Stop final {
    RationalBeat beat{RationalBeat::zero()};
    double durationMs{};
};

//  One saved discrete tempo boundary. A boundary beat carries exactly the saved tick.
struct TempoBoundary final {
    RationalBeat beat{RationalBeat::zero()};
    TimeUs at{};
};

//  One saved Stop extent: `[start, end)` in the reverse direction.
struct StopBoundary final {
    RationalBeat beat{RationalBeat::zero()};
    TimeUs start{};
    TimeUs end{};
};

//  One saved exact beat anchor.
struct AnchoredBeat final {
    RationalBeat beat{RationalBeat::zero()};
    TimeUs at{};
};

//  The canonical source view. The map copies and normalizes it on construction; the view itself is
//  only borrowed for the call.
struct TimingSourceView final {
    double defaultBpm{120.0};
    std::span<const TempoEvent> tempos;
    std::span<const Stop> stops;
};

//  The saved discrete boundary tables.
struct TimingBoundariesView final {
    std::span<const TempoBoundary> tempos;
    std::span<const StopBoundary> stops;
    std::span<const AnchoredBeat> anchors;
};

//  One reverse sample. `beat` is continuous interpolation only; `stopBeat` is the exact Stop beat
//  while a tick is inside a Stop. `stopProgress` is zero outside `[S, E)`.
struct BeatSample final {
    double beat{};
    std::optional<RationalBeat> stopBeat;
    bool inStop{};
    double stopProgress{};
};

class TimingMap final {
  public:
    TimingMap(const TimingMap&) noexcept = default;
    TimingMap(TimingMap&&) noexcept = default;
    auto operator=(const TimingMap&) noexcept -> TimingMap& = default;
    auto operator=(TimingMap&&) noexcept -> TimingMap& = default;
    ~TimingMap() noexcept = default;

    //  Compiles one source snapshot. Discrete boundaries are the union of every tempo start/end
    //  beat, every Stop beat, Beat 0 and `requiredAnchors`, each exactly deduplicated. A beat that
    //  collides with a Stop beat takes the Stop start tick.
    [[nodiscard]] static auto compile(TimingSourceView source,
                                      std::span<const RationalBeat> requiredAnchors)
        -> core::Result<TimingMap>;

    //  Rebuilds a map from saved discrete boundaries. The source snapshot must describe exactly the
    //  key sets the boundaries carry; the saved ticks are validated for key coverage, monotonicity
    //  and Stop extents and are never recomputed or replaced.
    [[nodiscard]] static auto fromCompiled(TimingSourceView source, TimingBoundariesView boundaries,
                                           std::span<const RationalBeat> requiredAnchors)
        -> core::Result<TimingMap>;

    [[nodiscard]] auto boundaries() const noexcept -> TimingBoundariesView;
    [[nodiscard]] auto matchesSource(TimingSourceView source) const noexcept -> bool;

    //  The discrete forward conversion. A saved anchor returns its saved tick exactly; every other
    //  beat integrates continuously and rounds half to even. The result never replaces a saved
    //  anchor.
    [[nodiscard]] auto beatToTime(RationalBeat beat) const -> core::Result<TimeUs>;

    //  The reverse conversion. Ticks inside a Stop return the exact Stop beat; a tick equal to a
    //  Stop end returns the same beat outside the Stop. Every other tick is a continuous sample for
    //  interpolation only.
    [[nodiscard]] auto timeToBeat(TimeUs time) const -> core::Result<BeatSample>;

  private:
    explicit TimingMap(std::shared_ptr<const detail::TimingMapStorage> storage) noexcept;

    std::shared_ptr<const detail::TimingMapStorage> storage_;
};

//  Nearest integer, exactly half rounded to even, independent of the current floating-point
//  rounding mode. Rejects a non-finite value and a result outside the signed 64-bit range; the cast
//  is never taken before the range check.
[[nodiscard]] auto roundMicroseconds(double microseconds) -> core::Result<TimeUs>;

} // namespace cuexis::timing
