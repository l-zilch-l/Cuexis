//  Judgement typed kernel - S7A-2 timebase implementation.
//
//  This file is the exact-rational half of S7A-2: the four Tick domains, the checked Tick
//  arithmetic, the RationalBeat -> judgementTick mapping, the canonical same-tick ordering and the
//  commit window. Input normalization is the other half of the batch and is not implemented here.
//
//  Three rules shape every function below.
//
//    1. Nothing approximates. A beat, a tempo and a stop duration are exact rationals, and the
//       mapping is the exact accumulated tick time from the origin beat to the mapped beat: the
//       integral of the tempo rate outside the stop intervals, plus each stop's declared duration
//       added once at that stop's end beat. No float, no double and no host math library take part.
//
//    2. Everything that does not fit is a stable rejection. Exact rationals are carried as a
//       reduced pair of signed 64-bit integers, and every add, subtract, multiply, divide and
//       compare is checked. A value that would leave that representation is reported as a
//       budget_exceeded failure instead of being saturated, truncated or allowed to reach undefined
//       behaviour. There is deliberately no wider-integer shim: a portable 128-bit helper would add
//       a temporary integer typedef to a batch whose frozen contract is "signed 64 bit, reject on
//       overflow".
//
//    3. No exception crosses this boundary. Every failure path returns core::Result with an owning
//       diagnostic, and no function that builds one is noexcept.

#include <cuexis/judgement/timebase.hpp>

#include "source_codes.hpp"

#include <cuexis/judgement/diagnostic.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace cuexis::judgement {
namespace {

constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();

//  |value| as an unsigned magnitude. Safe for INT64_MIN, whose magnitude needs the unsigned range.
[[nodiscard]] constexpr auto magnitude(std::int64_t value) noexcept -> std::uint64_t {
    return value < 0 ? (~static_cast<std::uint64_t>(value) + 1U)
                     : static_cast<std::uint64_t>(value);
}

[[nodiscard]] constexpr auto gcd(std::uint64_t left, std::uint64_t right) noexcept
    -> std::uint64_t {
    while (right != 0U) {
        const std::uint64_t remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

//  ---------------------------------------------------------------------------------------------
//  Checked signed 64-bit integer arithmetic
//  ---------------------------------------------------------------------------------------------

//  left * right, or nullopt when the exact product is not representable.
[[nodiscard]] constexpr auto checkedMultiply(std::int64_t left, std::int64_t right) noexcept
    -> std::optional<std::int64_t> {
    if (left == 0 || right == 0) {
        return std::int64_t{0};
    }
    const bool negative = (left < 0) != (right < 0);
    const std::uint64_t limit = negative ? static_cast<std::uint64_t>(kInt64Max) + 1U
                                         : static_cast<std::uint64_t>(kInt64Max);
    const std::uint64_t leftMagnitude = magnitude(left);
    const std::uint64_t rightMagnitude = magnitude(right);
    if (leftMagnitude > limit / rightMagnitude) {
        return std::nullopt;
    }
    const std::uint64_t product = leftMagnitude * rightMagnitude;
    if (!negative) {
        return static_cast<std::int64_t>(product);
    }
    if (product == static_cast<std::uint64_t>(kInt64Max) + 1U) {
        return kInt64Min;
    }
    return -static_cast<std::int64_t>(product);
}

//  left + right, or nullopt when the exact sum is not representable.
[[nodiscard]] constexpr auto checkedAdd(std::int64_t left, std::int64_t right) noexcept
    -> std::optional<std::int64_t> {
    if (right > 0 && left > kInt64Max - right) {
        return std::nullopt;
    }
    if (right < 0 && left < kInt64Min - right) {
        return std::nullopt;
    }
    return left + right;
}

//  left - right, or nullopt when the exact difference is not representable. INT64_MIN - INT64_MIN
//  is 0 and INT64_MIN - 0 is INT64_MIN: both are representable and neither is rejected. Rejecting
//  them would report a range failure for a value that is inside the frozen range.
[[nodiscard]] constexpr auto checkedSubtract(std::int64_t left, std::int64_t right) noexcept
    -> std::optional<std::int64_t> {
    if (right == kInt64Min) {
        if (left >= 0) {
            return std::nullopt;
        }
        //  left + 2^63, representable because left <= -1.
        return static_cast<std::int64_t>(left + kInt64Max) + 1;
    }
    return checkedAdd(left, -right);
}

//  ---------------------------------------------------------------------------------------------
//  Diagnostics
//  ---------------------------------------------------------------------------------------------

[[nodiscard]] auto rejection(std::string_view code, std::string_view category,
                             std::string_view summary, std::string_view path) -> core::Error {
    const Diagnostic diagnostic{
        .code = code,
        .category = category,
        .severity = codes::kErrorSeverity,
        //  None of these rejections mutates session state, so none of them faults the session.
        .faulted = false,
        .summary = summary,
        .context =
            DiagnosticContext{
                .fieldPath = DiagnosticFieldPath{.section = codes::kTimebaseSection, .path = path},
                .requirement =
                    DiagnosticRequirementRef{.kind = codes::kAbsent, .identity = codes::kAbsent},
                .identity = DiagnosticIdentityComponent{.component = codes::kAbsent,
                                                        .token = codes::kAbsent},
                .budget = nullptr,
                .capabilityId = codes::kAbsent,
                .remediation = codes::kAbsent,
                .rawTime = codes::kAbsent,
            },
    };
    return toError(diagnostic);
}

//  A value left the signed 64-bit range of the frozen representation of Spec 3.7.1 item 4. Maps to
//  the Spec 9.2 budget_exceeded category and the Spec 9.3 "out of range is atomic failure" path.
[[nodiscard]] auto tickOverflowError(std::string_view path) -> core::Error {
    return rejection(codes::kTickOverflowCode, codes::kBudgetExceededCategory,
                     "a timebase value left the signed 64-bit range of the frozen representation",
                     path);
}

//  A declared profile value left the domain its own declaration requires (Spec 3.7.8: a declared
//  value that cannot be satisfied is a budget_exceeded failure, not a capability rejection).
[[nodiscard]] auto profileValueError(std::string_view path) -> core::Error {
    return rejection(codes::kProfileValueOutOfRangeCode, codes::kBudgetExceededCategory,
                     "a declared timebase value is outside the domain its declaration requires",
                     path);
}

//  A rational pair that is not a representable exact rational. The path names the rational itself
//  rather than a beat, because the same token is returned by the beat factory and by the duration
//  factory.
[[nodiscard]] auto rationalInvalidError() -> core::Error {
    return rejection(codes::kRationalInvalidCode, codes::kBudgetExceededCategory,
                     "the value is not a representable exact rational", codes::kRationalPath);
}

//  The declared profile structure is not a valid canonical declaration.
[[nodiscard]] auto profileInvalidError(std::string_view path) -> core::Error {
    return rejection(codes::kProfileInvalidCode, codes::kInvalidRelationCategory,
                     "the declared timebase profile is not a valid declaration", path);
}

[[nodiscard]] auto lateParameterPendingError(std::string_view path) -> core::Error {
    return rejection(codes::kLatePolicyPendingCode, codes::kLatePolicyIncompleteCategory,
                     "a late-policy parameter is still pending measurement", path);
}

[[nodiscard]] auto latePolicyUndeclaredError() -> core::Error {
    return rejection(codes::kLatePolicyUndeclaredCode, codes::kLatePolicyIncompleteCategory,
                     "the session declared no late event policy", codes::kLatePolicyPath);
}

[[nodiscard]] auto intervalReversedError() -> core::Error {
    return rejection(codes::kIntervalReversedCode, codes::kInvalidRelationCategory,
                     "the interval end precedes its start", codes::kIntervalPath);
}

[[nodiscard]] auto commitWindowInvalidError() -> core::Error {
    return rejection(codes::kCommitWindowInvalidCode, codes::kInvalidRelationCategory,
                     "the commit window boundaries contradict its commit tick",
                     codes::kCommitWindowPath);
}

[[nodiscard]] auto commitNotAtCommitTickError() -> core::Error {
    return rejection(codes::kCommitNotAtCommitTickCode, codes::kInvalidRelationCategory,
                     "a fact commit was attempted at a tick that is not the window's commit tick",
                     codes::kCommitWindowPath);
}

//  ---------------------------------------------------------------------------------------------
//  Internal exact rational
//  ---------------------------------------------------------------------------------------------
//
//  Always reduced (gcd == 1) with a strictly positive denominator. Reduction is what keeps the
//  intermediate operands small and what makes value equality a memberwise comparison, so the
//  mapper never depends on a cross product that could itself overflow.

struct Rational final {
    std::int64_t numerator{0};
    std::int64_t denominator{1};
};

//  Reduces and canonicalises. nullopt when the pair is not a representable exact rational: a zero
//  or INT64_MIN denominator, or an INT64_MIN numerator whose magnitude has no signed 64-bit form.
[[nodiscard]] auto normalize(std::int64_t numerator, std::int64_t denominator)
    -> std::optional<Rational> {
    if (denominator == 0 || denominator == kInt64Min || numerator == kInt64Min) {
        return std::nullopt;
    }
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    if (numerator == 0) {
        return Rational{0, 1};
    }
    const auto divisor = static_cast<std::int64_t>(
        gcd(magnitude(numerator), static_cast<std::uint64_t>(denominator)));
    return Rational{numerator / divisor, denominator / divisor};
}

[[nodiscard]] auto add(Rational left, Rational right) -> std::optional<Rational> {
    const auto common =
        static_cast<std::int64_t>(gcd(static_cast<std::uint64_t>(left.denominator),
                                      static_cast<std::uint64_t>(right.denominator)));
    const std::int64_t leftScale = right.denominator / common;
    const std::int64_t rightScale = left.denominator / common;
    const auto leftTerm = checkedMultiply(left.numerator, leftScale);
    const auto rightTerm = checkedMultiply(right.numerator, rightScale);
    if (!leftTerm.has_value() || !rightTerm.has_value()) {
        return std::nullopt;
    }
    const auto sum = checkedAdd(*leftTerm, *rightTerm);
    const auto denominator = checkedMultiply(left.denominator, leftScale);
    if (!sum.has_value() || !denominator.has_value()) {
        return std::nullopt;
    }
    return normalize(*sum, *denominator);
}

[[nodiscard]] auto subtract(Rational left, Rational right) -> std::optional<Rational> {
    if (right.numerator == kInt64Min) {
        return std::nullopt;
    }
    right.numerator = -right.numerator;
    return add(left, right);
}

//  Cross-cancels before multiplying so that a product of two reduced rationals stays as small as
//  the exact result allows.
[[nodiscard]] auto multiply(Rational left, Rational right) -> std::optional<Rational> {
    if (left.numerator == 0 || right.numerator == 0) {
        return Rational{0, 1};
    }
    const auto leftCancel = static_cast<std::int64_t>(
        gcd(magnitude(left.numerator), static_cast<std::uint64_t>(right.denominator)));
    left.numerator /= leftCancel;
    right.denominator /= leftCancel;
    const auto rightCancel = static_cast<std::int64_t>(
        gcd(magnitude(right.numerator), static_cast<std::uint64_t>(left.denominator)));
    right.numerator /= rightCancel;
    left.denominator /= rightCancel;
    const auto numerator = checkedMultiply(left.numerator, right.numerator);
    const auto denominator = checkedMultiply(left.denominator, right.denominator);
    if (!numerator.has_value() || !denominator.has_value()) {
        return std::nullopt;
    }
    return normalize(*numerator, *denominator);
}

//  Three-way comparison without a cross product that could overflow: both sides are scaled to the
//  least common denominator first. nullopt when even that product is not representable, which is
//  the same stable rejection as any other range failure.
[[nodiscard]] auto compare(Rational left, Rational right) -> std::optional<int> {
    const auto common =
        static_cast<std::int64_t>(gcd(static_cast<std::uint64_t>(left.denominator),
                                      static_cast<std::uint64_t>(right.denominator)));
    const std::int64_t leftScale = right.denominator / common;
    const std::int64_t rightScale = left.denominator / common;
    const auto leftTerm = checkedMultiply(left.numerator, leftScale);
    const auto rightTerm = checkedMultiply(right.numerator, rightScale);
    if (!leftTerm.has_value() || !rightTerm.has_value()) {
        return std::nullopt;
    }
    if (*leftTerm < *rightTerm) {
        return -1;
    }
    if (*leftTerm > *rightTerm) {
        return 1;
    }
    return 0;
}

//  Rounds an exact rational to the nearest integer, ties to the even side, symmetrically for
//  negative values (Spec 3.7.2 item 2). nullopt when the rounded value is not representable.
//
//  No intermediate can overflow: the remainder is in [0, denominator), the denominator is at most
//  INT64_MAX, and the tie test is therefore done in the unsigned 64-bit range where 2 * remainder
//  always fits.
//
//  The two range guards are defence rather than reachable code. A reduced rational never carries an
//  INT64_MIN numerator and always has a positive denominator, so |value| <= INT64_MAX, and rounding
//  a value in [-INT64_MAX, INT64_MAX] to the nearest integer stays inside that range. They are kept
//  because a future change to the representation must not silently become undefined behaviour here.
[[nodiscard]] auto roundHalfToEven(Rational value) -> std::optional<std::int64_t> {
    std::int64_t quotient = value.numerator / value.denominator;
    std::int64_t remainder = value.numerator % value.denominator;
    if (remainder < 0) {
        if (quotient == kInt64Min) {
            return std::nullopt;
        }
        --quotient;
        remainder += value.denominator;
    }
    const std::uint64_t twice = static_cast<std::uint64_t>(remainder) * 2U;
    const auto denominator = static_cast<std::uint64_t>(value.denominator);
    if (twice < denominator) {
        return quotient;
    }
    if (twice > denominator) {
        if (quotient == kInt64Max) {
            return std::nullopt;
        }
        return quotient + 1;
    }
    if (quotient % 2 == 0) {
        return quotient;
    }
    if (quotient == kInt64Max) {
        return std::nullopt;
    }
    return quotient + 1;
}

[[nodiscard]] constexpr auto rationalOf(RationalBeat value) noexcept -> Rational {
    return Rational{value.numerator(), value.denominator()};
}

[[nodiscard]] constexpr auto rationalOf(RationalDuration value) noexcept -> Rational {
    return Rational{value.numerator(), value.denominator()};
}

//  ---------------------------------------------------------------------------------------------
//  Profile validation
//  ---------------------------------------------------------------------------------------------
//
//  The mapper refuses to run on a profile it cannot honour, so this is the shared precondition of
//  validatePrepare and mapBeatToTick. Rejection rule used throughout the batch:
//
//    * a numeric value outside the domain its own declaration requires  -> budget_exceeded
//      (Spec 3.7.6 item 3 / Spec 3.7.8 "a declared value that cannot be satisfied");
//    * a declaration whose canonical structure or ordering is not valid -> invalid_relation
//      (none of the nine Spec 9.2 categories is a "malformed declaration" category, and inventing
//      a tenth would preempt CM-D04);
//    * a value that does not fit the signed 64-bit representation      -> budget_exceeded
//      (Spec 3.7.1 item 4).

[[nodiscard]] auto validateProfile(const TimebaseProfile& profile) -> core::Result<void> {
    if (profile.profileId.empty()) {
        return core::unexpected(profileInvalidError(codes::kProfileIdPath));
    }
    if (profile.unitToken.empty()) {
        return core::unexpected(profileInvalidError(codes::kUnitPath));
    }
    if (profile.tickScale.numerator() <= 0) {
        return core::unexpected(profileValueError(codes::kTickScalePath));
    }
    //  A profile that never declared a tempo is structurally incomplete, not out of range: there is
    //  no value to range-check. The declaration-side category is invalid_relation, the same one a
    //  missing profile identity or unit token gets, and it is reported on the initialTempo path so
    //  the two spellings stay distinguishable.
    if (!profile.initialTempo.has_value()) {
        return core::unexpected(profileInvalidError(codes::kInitialTempoPath));
    }
    if (profile.initialTempo->numerator() <= 0) {
        return core::unexpected(profileValueError(codes::kInitialTempoPath));
    }

    for (std::size_t index = 0; index < profile.tempoSections.size(); ++index) {
        const TempoSection& section = profile.tempoSections[index];
        if (section.durationPerBeat.numerator() <= 0) {
            return core::unexpected(profileValueError(codes::kTempoSectionsPath));
        }
        if (index == 0) {
            continue;
        }
        const auto order = compare(rationalOf(section.startBeat),
                                   rationalOf(profile.tempoSections[index - 1].startBeat));
        if (!order.has_value()) {
            return core::unexpected(tickOverflowError(codes::kTempoSectionsPath));
        }
        //  Strictly increasing: an unordered or duplicated section is rejected rather than
        //  resolved by container order.
        if (*order <= 0) {
            return core::unexpected(profileInvalidError(codes::kTempoSectionsPath));
        }
    }

    for (std::size_t index = 0; index < profile.stopSections.size(); ++index) {
        const StopSection& section = profile.stopSections[index];
        if (section.duration.numerator() <= 0) {
            return core::unexpected(profileValueError(codes::kStopSectionsPath));
        }
        const auto span = compare(rationalOf(section.startBeat), rationalOf(section.endBeat));
        if (!span.has_value()) {
            return core::unexpected(tickOverflowError(codes::kStopSectionsPath));
        }
        //  A degenerate stop interval is rejected instead of being given an invented
        //  instantaneous meaning.
        if (*span >= 0) {
            return core::unexpected(profileInvalidError(codes::kStopSectionsPath));
        }
        if (index == 0) {
            continue;
        }
        const auto gap = compare(rationalOf(profile.stopSections[index - 1].endBeat),
                                 rationalOf(section.startBeat));
        if (!gap.has_value()) {
            return core::unexpected(tickOverflowError(codes::kStopSectionsPath));
        }
        //  Non-overlapping: overlapping stops would need a resolution rule this batch does not own.
        if (*gap > 0) {
            return core::unexpected(profileInvalidError(codes::kStopSectionsPath));
        }
    }
    return {};
}

//  ---------------------------------------------------------------------------------------------
//  The exact mapping
//  ---------------------------------------------------------------------------------------------

//  The tick rate in effect at `beat` outside every stop interval: initialTempo before the first
//  declared section, then the last section whose startBeat is not after `beat`.
[[nodiscard]] auto tempoRateAt(const TimebaseProfile& profile, Rational beat)
    -> std::optional<Rational> {
    Rational rate = rationalOf(*profile.initialTempo);
    for (const TempoSection& section : profile.tempoSections) {
        const auto effective = compare(rationalOf(section.startBeat), beat);
        if (!effective.has_value()) {
            return std::nullopt;
        }
        if (*effective <= 0) {
            rate = rationalOf(section.durationPerBeat);
        }
    }
    return rate;
}

//  The stop jump that is completed strictly after `low` and at or before `high`: the sum of the
//  declared durations of every stop whose endBeat lies in (low, high]. This is the "add the
//  declared duration once, at endBeat" half of the model, expressed in the direction the caller
//  integrates: for a beat after the origin the interval is (origin, beat], and for a beat before
//  the origin it is (origin, beat] mirrored, that is (beat, origin]. Both spellings share the same
//  half-open convention, so an interval that merely starts inside a stop does not add anything yet
//  and an interval that reaches the stop's endBeat does.
//
//  A zero total means "no stop ends inside this interval", which is the common case.
[[nodiscard]] auto stopJumps(const TimebaseProfile& profile, Rational low, Rational high)
    -> std::optional<Rational> {
    Rational total{0, 1};
    for (const StopSection& section : profile.stopSections) {
        const Rational endBeat = rationalOf(section.endBeat);
        const auto afterLow = compare(endBeat, low);
        const auto atOrBeforeHigh = compare(endBeat, high);
        if (!afterLow.has_value() || !atOrBeforeHigh.has_value()) {
            return std::nullopt;
        }
        if (*afterLow > 0 && *atOrBeforeHigh <= 0) {
            const auto sum = add(total, rationalOf(section.duration));
            if (!sum.has_value()) {
                return std::nullopt;
            }
            total = *sum;
        }
    }
    return total;
}

//  The exact integral of the profile's tick rate from `low` to `high`, where low < high. Zero on an
//  empty interval. The rate function is piecewise constant with breakpoints at the tempo section
//  starts and the stop boundaries, and it is identically zero inside a stop interval: a stop
//  freezes the mapping instead of replacing the tempo with a slower internal rate. Its declared
//  duration is not part of this integral and is added by stopJumps at the stop's endBeat.
[[nodiscard]] auto integrateRate(const TimebaseProfile& profile, Rational low, Rational high)
    -> std::optional<Rational> {
    std::vector<Rational> cuts;
    cuts.push_back(low);
    cuts.push_back(high);
    for (const TempoSection& section : profile.tempoSections) {
        const Rational start = rationalOf(section.startBeat);
        const auto afterLow = compare(start, low);
        const auto beforeHigh = compare(start, high);
        if (!afterLow.has_value() || !beforeHigh.has_value()) {
            return std::nullopt;
        }
        if (*afterLow > 0 && *beforeHigh < 0) {
            cuts.push_back(start);
        }
    }
    for (const StopSection& section : profile.stopSections) {
        for (const Rational& boundary :
             {rationalOf(section.startBeat), rationalOf(section.endBeat)}) {
            const auto afterLow = compare(boundary, low);
            const auto beforeHigh = compare(boundary, high);
            if (!afterLow.has_value() || !beforeHigh.has_value()) {
                return std::nullopt;
            }
            if (*afterLow > 0 && *beforeHigh < 0) {
                cuts.push_back(boundary);
            }
        }
    }

    //  Insertion sort with explicit failure propagation: the cut count is chart-scale, and a
    //  comparator that cannot report a range failure would hide one.
    for (std::size_t index = 1; index < cuts.size(); ++index) {
        const Rational value = cuts[index];
        std::size_t position = index;
        while (position > 0) {
            const auto previous = compare(cuts[position - 1], value);
            if (!previous.has_value()) {
                return std::nullopt;
            }
            if (*previous <= 0) {
                break;
            }
            cuts[position] = cuts[position - 1];
            --position;
        }
        cuts[position] = value;
    }

    Rational total{0, 1};
    for (std::size_t index = 0; index + 1 < cuts.size(); ++index) {
        const Rational& from = cuts[index];
        const Rational& to = cuts[index + 1];
        const auto same = compare(from, to);
        if (!same.has_value()) {
            return std::nullopt;
        }
        if (*same == 0) {
            continue;
        }

        //  The cut set contains every stop boundary, so a segment is either wholly inside one stop
        //  or wholly outside every stop. Inside a stop the rate is exactly zero: the interval
        //  freezes the mapping, and its declared duration is the jump that stopJumps adds at the
        //  stop's endBeat rather than an internal rate.
        std::optional<Rational> rate;
        for (const StopSection& section : profile.stopSections) {
            const auto afterStart = compare(from, rationalOf(section.startBeat));
            const auto beforeEnd = compare(from, rationalOf(section.endBeat));
            if (!afterStart.has_value() || !beforeEnd.has_value()) {
                return std::nullopt;
            }
            if (*afterStart >= 0 && *beforeEnd < 0) {
                rate = Rational{0, 1};
                break;
            }
        }
        if (!rate.has_value()) {
            rate = tempoRateAt(profile, from);
        }
        if (!rate.has_value()) {
            return std::nullopt;
        }

        const auto length = subtract(to, from);
        if (!length.has_value()) {
            return std::nullopt;
        }
        const auto contribution = multiply(*length, *rate);
        if (!contribution.has_value()) {
            return std::nullopt;
        }
        const auto sum = add(total, *contribution);
        if (!sum.has_value()) {
            return std::nullopt;
        }
        total = *sum;
    }
    return total;
}

} // namespace

//  ---------------------------------------------------------------------------------------------
//  Exact rational values
//  ---------------------------------------------------------------------------------------------

auto RationalBeat::create(std::int64_t numerator, std::int64_t denominator)
    -> core::Result<RationalBeat> {
    const auto value = normalize(numerator, denominator);
    if (!value.has_value()) {
        return core::unexpected(rationalInvalidError());
    }
    return RationalBeat{value->numerator, value->denominator};
}

auto RationalDuration::create(std::int64_t numerator, std::int64_t denominator)
    -> core::Result<RationalDuration> {
    const auto value = normalize(numerator, denominator);
    if (!value.has_value()) {
        return core::unexpected(rationalInvalidError());
    }
    return RationalDuration{value->numerator, value->denominator};
}

//  ---------------------------------------------------------------------------------------------
//  Tick arithmetic
//  ---------------------------------------------------------------------------------------------

auto offsetTicks(Tick origin, TickSpan span) -> core::Result<Tick> {
    const auto sum = checkedAdd(origin.value(), span.value());
    if (!sum.has_value()) {
        return core::unexpected(tickOverflowError(codes::kTickPath));
    }
    return Tick{*sum};
}

auto differenceTicks(Tick left, Tick right) -> core::Result<TickDelta> {
    const auto difference = checkedSubtract(left.value(), right.value());
    if (!difference.has_value()) {
        return core::unexpected(tickOverflowError(codes::kTickPath));
    }
    return TickDelta{*difference};
}

auto makeInterval(Tick start, Tick end) -> core::Result<TimeInterval> {
    if (end < start) {
        return core::unexpected(intervalReversedError());
    }
    return TimeInterval{.start = start, .end = end};
}

//  ---------------------------------------------------------------------------------------------
//  TimebaseProfile
//  ---------------------------------------------------------------------------------------------

auto makeMicrosecondTimebase() -> TimebaseProfile {
    //  tickScale = 1/1: one tick is exactly one microsecond. A unit declaration, not a limit.
    const auto tickScale = RationalDuration::create(1, 1);
    assert(tickScale.has_value() && "1/1 is always a representable exact rational");
    const auto originBeat = RationalBeat::create(0, 1);
    assert(originBeat.has_value() && "0/1 is always a representable exact rational");
    //  Every declared field is named here, including the origin beat, so the declaration does not
    //  depend on a member default. A rational has no default constructor, which is what makes this
    //  spelling the only one that compiles.
    //
    //  initialTempo is explicitly set to the absent state: a tempo is chart content expressed in
    //  microseconds per beat, so baking one in here would be an invented default and
    //  validatePrepare rejects the profile until the chart declares one.
    return TimebaseProfile{
        .profileId = "engine.tick.us.v1",
        .unitToken = "us",
        .tickScale = *tickScale,
        .originBeat = *originBeat,
        .initialTempo = std::nullopt,
        .tempoSections = {},
        .stopSections = {},
    };
}

auto validatePrepare(const TimebaseProfile& profile, const LatePolicyParameters& latePolicy)
    -> core::Result<void> {
    const auto profileStatus = validateProfile(profile);
    if (!profileStatus.has_value()) {
        return core::unexpected(profileStatus.error());
    }

    if (!latePolicy.policy.has_value()) {
        return core::unexpected(latePolicyUndeclaredError());
    }
    if (latePolicy.finalizationWatermark.isPendingMeasurement()) {
        return core::unexpected(lateParameterPendingError(codes::kFinalizationWatermarkPath));
    }
    if (latePolicy.maxQueueHop.isPendingMeasurement()) {
        return core::unexpected(lateParameterPendingError(codes::kMaxQueueHopPath));
    }
    if (latePolicy.windowCloseThreshold.isPendingMeasurement()) {
        return core::unexpected(lateParameterPendingError(codes::kWindowClosePath));
    }
    if (latePolicy.windowOpenThreshold.isPendingMeasurement()) {
        return core::unexpected(lateParameterPendingError(codes::kWindowOpenPath));
    }
    return {};
}

//  ---------------------------------------------------------------------------------------------
//  RationalBeat -> judgementTick
//  ---------------------------------------------------------------------------------------------

auto mapBeatToTick(const TimebaseProfile& profile, RationalBeat beat)
    -> core::Result<JudgementTick> {
    //  The public entry point re-validates: a caller may reach it without validatePrepare, and a
    //  division by a zero scale or a zero tempo must be a stable rejection, not undefined
    //  behaviour.
    const auto profileStatus = validateProfile(profile);
    if (!profileStatus.has_value()) {
        return core::unexpected(profileStatus.error());
    }

    const Rational origin = rationalOf(profile.originBeat);
    const Rational target = rationalOf(beat);

    const auto order = compare(origin, target);
    if (!order.has_value()) {
        return core::unexpected(tickOverflowError(codes::kBeatPath));
    }
    if (*order == 0) {
        return JudgementTick{Tick{0}};
    }

    //  Beats before the origin accumulate in the opposite direction, so the whole accumulated value
    //  is negated once instead of taking a separate rounding path. The stop jumps use the same
    //  half-open direction: (origin, beat] forwards, (beat, origin] backwards. That direction
    //  cannot be mirrored, so a mirrored stop configuration is not necessarily odd-symmetric.
    const bool negated = *order > 0;
    const Rational low = negated ? target : origin;
    const Rational high = negated ? origin : target;

    const auto integral = integrateRate(profile, low, high);
    if (!integral.has_value()) {
        return core::unexpected(tickOverflowError(codes::kBeatPath));
    }
    //  The stop durations that end inside the interval are added once, in the exact domain, before
    //  the single rounding step below. Adding them after rounding would be a second rounding path
    //  and would not satisfy tick(endBeat) = tick(startBeat) + duration exactly.
    const auto jumps = stopJumps(profile, low, high);
    if (!jumps.has_value()) {
        return core::unexpected(tickOverflowError(codes::kBeatPath));
    }
    const auto accumulated = add(*integral, *jumps);
    if (!accumulated.has_value()) {
        return core::unexpected(tickOverflowError(codes::kBeatPath));
    }

    //  The accumulated time is non-negative, so negating its numerator cannot overflow, and the
    //  rounded value is negated in the exact domain before the range check: -2^63 is representable
    //  even though +2^63 is not, and rounding first would lose that.
    Rational signedValue = *accumulated;
    if (negated) {
        signedValue.numerator = -signedValue.numerator;
    }
    const auto rounded = roundHalfToEven(signedValue);
    if (!rounded.has_value()) {
        return core::unexpected(tickOverflowError(codes::kBeatPath));
    }
    return JudgementTick{Tick{*rounded}};
}

//  ---------------------------------------------------------------------------------------------
//  Same-tick tie rule
//  ---------------------------------------------------------------------------------------------

auto canonicalOrder(const TickCollisionKey& left, const TickCollisionKey& right) noexcept -> bool {
    if (left.tick != right.tick) {
        return left.tick < right.tick;
    }
    const auto leftKind = static_cast<std::uint8_t>(left.originKind);
    const auto rightKind = static_cast<std::uint8_t>(right.originKind);
    if (leftKind != rightKind) {
        //  The comparison is on the declared enumerator order of OriginKind, never on a module
        //  registration order.
        return leftKind < rightKind;
    }
    return left.canonicalOrdinal < right.canonicalOrdinal;
}

//  ---------------------------------------------------------------------------------------------
//  commitTick and the commit window
//  ---------------------------------------------------------------------------------------------

auto commitAt(Tick tick, const CommitWindow& window) -> core::Result<void> {
    const Tick commitTick = window.commitTick.tick();
    if (commitTick < window.openTick || window.closeTick < commitTick) {
        return core::unexpected(commitWindowInvalidError());
    }
    if (tick != commitTick) {
        return core::unexpected(commitNotAtCommitTickError());
    }
    return {};
}

} // namespace cuexis::judgement
