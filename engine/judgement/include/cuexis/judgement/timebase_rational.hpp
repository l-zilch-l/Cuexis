#pragma once

//  Judgement typed kernel - exact rational beat and duration values.

#include <cuexis/core/result.hpp>

#include <compare>
#include <cstdint>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Exact rational values (Spec 3.7.2 item 1)
//  ---------------------------------------------------------------------------------------------
//
//  An author beat and a tempo are exact rational values. These classes store one signed rational in
//  lowest terms and perform only integer arithmetic: no float, no double and no host math library
//  take part, which is exactly what Spec 3.7.2 item 1 requires of the mapping and what the S7A-2
//  forbidden-consumption list repeats ("floating point or a host math library taking part in Tick
//  decisions" is forbidden).
//
//  The denominator is always strictly positive and the pair is always coprime, so the sign lives
//  entirely in the numerator and comparison, rounding and negation are symmetric by construction.
//  Coprimality also makes value equality a memberwise comparison instead of a cross product that
//  could itself overflow.
//
//  Construction is checked, so the two-argument constructor is private and the only way to build a
//  rational is the named factory. There is deliberately no default constructor: a default-
//  constructed rational would be the value 0/1, and the S7A-2 ruling forbids 0/1 from becoming an
//  implicit declaration ("no implicit zero value and no default unit"). A caller that really wants
//  the zero beat has to spell `RationalBeat::create(0, 1)` and handle the Result, so the zero is a
//  declaration at the call site instead of a value the type hands out on its own.
//
//  A zero denominator, an INT64_MIN denominator and an INT64_MIN numerator are all rejected: the
//  last one because |INT64_MIN| is not representable in the numerator type.

class RationalBeat final {
  public:
    //  The only way to build a checked rational beat. Rejects a zero denominator, an INT64_MIN
    //  operand and a numerator whose magnitude cannot be reduced into the signed 64-bit range, with
    //  the budget_exceeded category instead of producing an unrepresentable value.
    [[nodiscard]] static auto create(std::int64_t numerator, std::int64_t denominator)
        -> core::Result<RationalBeat>;

    [[nodiscard]] constexpr auto numerator() const noexcept -> std::int64_t {
        return numerator_;
    }
    [[nodiscard]] constexpr auto denominator() const noexcept -> std::int64_t {
        return denominator_;
    }

    [[nodiscard]] constexpr auto isZero() const noexcept -> bool {
        return numerator_ == 0;
    }

    friend constexpr auto operator==(RationalBeat, RationalBeat) noexcept -> bool = default;

  private:
    constexpr RationalBeat(std::int64_t numerator, std::int64_t denominator) noexcept
        : numerator_(numerator), denominator_(denominator) {}

    std::int64_t numerator_;
    std::int64_t denominator_;
};

//  A tempo is an exact rational duration per beat, expressed in the tick-scale numerator unit of
//  the profile that consumes it. It is a rational, not a fixed-point value, because the mapping has
//  to stay exact until the final rounding step.
//
//  Like RationalBeat it has no default constructor: a zero duration is never a usable tempo, and
//  the ruling keeps "this profile never declared a tempo" in the declared state of the profile
//  instead of in the value of the duration. `RationalDuration::create(0, 1)` is available where a
//  zero really is being declared, and validatePrepare rejects it as a non-positive tempo.
class RationalDuration final {
  public:
    //  Same checked contract as RationalBeat::create.
    [[nodiscard]] static auto create(std::int64_t numerator, std::int64_t denominator)
        -> core::Result<RationalDuration>;

    [[nodiscard]] constexpr auto numerator() const noexcept -> std::int64_t {
        return numerator_;
    }
    [[nodiscard]] constexpr auto denominator() const noexcept -> std::int64_t {
        return denominator_;
    }

    friend constexpr auto operator==(RationalDuration, RationalDuration) noexcept -> bool = default;

  private:
    constexpr RationalDuration(std::int64_t numerator, std::int64_t denominator) noexcept
        : numerator_(numerator), denominator_(denominator) {}

    std::int64_t numerator_;
    std::int64_t denominator_;
};

} // namespace cuexis::judgement
