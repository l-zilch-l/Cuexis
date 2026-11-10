#pragma once

//  Judgement typed kernel - S7A-2 tick domains and tick arithmetic.
//
//  Four distinct signed 64-bit integer domains (ChartTick, judgementTick, observationTick and
//  commitTick) plus the generic Tick carrier, so mixing two domains is unrepresentable rather
//  than merely discouraged. Every operation that can leave the representable range reports a
//  stable core::Result rejection instead of saturating or wrapping.

#include <cuexis/core/result.hpp>

#include <compare>
#include <cstdint>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Tick domains (Spec 0.1, Spec 3.7.1, ABI domain 1)
//  ---------------------------------------------------------------------------------------------
//
//  One distinct type per time domain. Each carries exactly one signed 64-bit integer, which is the
//  width the round 2 ruling froze; the unit is deliberately not part of the representation and can
//  only be read from a TimebaseProfile. The wrapper makes "which domain is this" part of the type
//  instead of part of a comment, so a ChartTick cannot silently be used where a judgementTick is
//  required.
//
//  A default-constructed wrapper is the declared zero value of its own domain (tick 0), not an
//  "unspecified" placeholder and not a unit default: there is no conversion from it to any other
//  domain and no arithmetic on it that a TimebaseProfile did not authorise.

//  Signed 64-bit logical time value. Generic carrier for arithmetic; not a domain of its own.
class Tick final {
  public:
    constexpr Tick() noexcept = default;
    explicit constexpr Tick(std::int64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::int64_t {
        return value_;
    }

    friend constexpr auto operator==(Tick, Tick) noexcept -> bool = default;
    friend constexpr auto operator<=>(Tick, Tick) noexcept = default;

  private:
    std::int64_t value_{0};
};

//  Chart anchors, requirement windows and coordination windows (Spec 0.1).
class ChartTick final {
  public:
    constexpr ChartTick() noexcept = default;
    explicit constexpr ChartTick(Tick value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto tick() const noexcept -> Tick {
        return value_;
    }

    friend constexpr auto operator==(ChartTick, ChartTick) noexcept -> bool = default;
    friend constexpr auto operator<=>(ChartTick, ChartTick) noexcept = default;

  private:
    Tick value_{};
};

//  Prepare-time mapped, run-time read-only judgement time domain (Spec 0.1).
class JudgementTick final {
  public:
    constexpr JudgementTick() noexcept = default;
    explicit constexpr JudgementTick(Tick value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto tick() const noexcept -> Tick {
        return value_;
    }

    friend constexpr auto operator==(JudgementTick, JudgementTick) noexcept -> bool = default;
    friend constexpr auto operator<=>(JudgementTick, JudgementTick) noexcept = default;

  private:
    Tick value_{};
};

//  The canonical judgement time of one observation. Its only canonical source is the calibrated
//  session clock captured when input enters the judgement pipeline; device time, host arrival time,
//  audio time and render frame time stay diagnostic context (Spec 3.7.3).
class ObservationTick final {
  public:
    constexpr ObservationTick() noexcept = default;
    explicit constexpr ObservationTick(Tick value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto tick() const noexcept -> Tick {
        return value_;
    }

    friend constexpr auto operator==(ObservationTick, ObservationTick) noexcept -> bool = default;
    friend constexpr auto operator<=>(ObservationTick, ObservationTick) noexcept = default;

  private:
    Tick value_{};
};

//  The tick at which a transaction commits. Facts commit here and nowhere else (Spec 3.7.5).
class CommitTick final {
  public:
    constexpr CommitTick() noexcept = default;
    explicit constexpr CommitTick(Tick value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto tick() const noexcept -> Tick {
        return value_;
    }

    friend constexpr auto operator==(CommitTick, CommitTick) noexcept -> bool = default;
    friend constexpr auto operator<=>(CommitTick, CommitTick) noexcept = default;

  private:
    Tick value_{};
};

//  Half-open [start, end) extent over the time domain.
struct TimeInterval final {
    Tick start{};
    Tick end{};

    friend constexpr auto operator==(TimeInterval, TimeInterval) noexcept -> bool = default;
};

//  Tick displacement and signed tick difference. Both are signed 64-bit, and both the displacement
//  and the difference reject on overflow instead of saturating or truncating (Spec 3.7.1 item 4).
class TickSpan final {
  public:
    constexpr TickSpan() noexcept = default;
    explicit constexpr TickSpan(std::int64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::int64_t {
        return value_;
    }

    friend constexpr auto operator==(TickSpan, TickSpan) noexcept -> bool = default;

  private:
    std::int64_t value_{0};
};

class TickDelta final {
  public:
    constexpr TickDelta() noexcept = default;
    explicit constexpr TickDelta(std::int64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr auto value() const noexcept -> std::int64_t {
        return value_;
    }

    friend constexpr auto operator==(TickDelta, TickDelta) noexcept -> bool = default;

  private:
    std::int64_t value_{0};
};

//  Tick arithmetic entry points. Every one of them returns Result because the width the round 2
//  ruling froze is a bounded width: an operation that leaves the representable range is a stable
//  rejection, not a wrapped or clamped value.
//
//  None of them is noexcept. A rejection carries an owning diagnostic (a code, a category, a
//  summary and context tokens), and building that text allocates; a noexcept signature would turn
//  an allocation failure into std::terminate at the module boundary.

//  origin + span, rejecting when the sum leaves the signed 64-bit range.
[[nodiscard]] auto offsetTicks(Tick origin, TickSpan span) -> core::Result<Tick>;
//  left - right, rejecting when the difference leaves the signed 64-bit range. Both operands may
//  hold any signed 64-bit value: the subtraction is checked, so INT64_MIN - 0 is the representable
//  result INT64_MIN and only a true out-of-range difference is rejected. The magnitude of a
//  difference has no representation of its own, so a caller that needs |delta| has to narrow it
//  itself and handle the INT64_MIN case; no helper here hides that.
[[nodiscard]] auto differenceTicks(Tick left, Tick right) -> core::Result<TickDelta>;
//  The half-open interval between two ticks, rejecting when end precedes start.
[[nodiscard]] auto makeInterval(Tick start, Tick end) -> core::Result<TimeInterval>;

} // namespace cuexis::judgement
