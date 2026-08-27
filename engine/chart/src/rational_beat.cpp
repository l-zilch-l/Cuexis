#include <cuexis/chart/rational_beat.hpp>

#include <cuexis/core/error.hpp>

#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <utility>

namespace cuexis::chart {
namespace {

[[nodiscard]] constexpr auto magnitude(std::int64_t value) noexcept -> std::uint64_t {
    if (value >= 0) {
        return static_cast<std::uint64_t>(value);
    }
    return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

[[nodiscard]] auto signedValue(std::uint64_t value, bool negative) -> core::Result<std::int64_t> {
    constexpr auto positiveLimit =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    constexpr auto negativeLimit = positiveLimit + 1U;
    if ((!negative && value > positiveLimit) || (negative && value > negativeLimit)) {
        return core::unexpected(core::Error{"chart.beat.out_of_range",
                                            "Beat numerator is outside signed 64-bit range"});
    }
    if (negative && value == negativeLimit) {
        return std::numeric_limits<std::int64_t>::min();
    }
    const auto result = static_cast<std::int64_t>(value);
    return negative ? -result : result;
}

[[nodiscard]] auto comparePositive(std::uint64_t leftNumerator, std::uint64_t leftDenominator,
                                   std::uint64_t rightNumerator,
                                   std::uint64_t rightDenominator) noexcept
    -> std::strong_ordering {
    bool reversed = false;
    while (true) {
        const auto leftQuotient = leftNumerator / leftDenominator;
        const auto rightQuotient = rightNumerator / rightDenominator;
        if (leftQuotient != rightQuotient) {
            const bool less = leftQuotient < rightQuotient;
            return (less != reversed) ? std::strong_ordering::less : std::strong_ordering::greater;
        }

        const auto leftRemainder = leftNumerator % leftDenominator;
        const auto rightRemainder = rightNumerator % rightDenominator;
        if (leftRemainder == 0 || rightRemainder == 0) {
            if (leftRemainder == rightRemainder) {
                return std::strong_ordering::equal;
            }
            const bool less = leftRemainder == 0;
            return (less != reversed) ? std::strong_ordering::less : std::strong_ordering::greater;
        }

        leftNumerator = leftDenominator;
        leftDenominator = leftRemainder;
        rightNumerator = rightDenominator;
        rightDenominator = rightRemainder;
        reversed = !reversed;
    }
}

[[nodiscard]] auto checkedMultiply(std::int64_t left, std::int64_t right)
    -> core::Result<std::int64_t> {
    if (left == 0 || right == 0) {
        return 0;
    }
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((left == -1 && right == minimum) || (right == -1 && left == minimum)) {
        return core::unexpected(
            core::Error{"chart.beat.out_of_range", "Beat arithmetic overflowed"});
    }
    if ((left > 0 && right > 0 && left > maximum / right) ||
        (left > 0 && right < 0 && right < minimum / left) ||
        (left < 0 && right > 0 && left < minimum / right) ||
        (left < 0 && right < 0 && left < maximum / right)) {
        return core::unexpected(
            core::Error{"chart.beat.out_of_range", "Beat arithmetic overflowed"});
    }
    return left * right;
}

[[nodiscard]] auto checkedAdd(std::int64_t left, std::int64_t right) -> core::Result<std::int64_t> {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right)) {
        return core::unexpected(
            core::Error{"chart.beat.out_of_range", "Beat arithmetic overflowed"});
    }
    return left + right;
}

} // namespace

auto RationalBeat::create(std::int64_t numerator, std::int64_t denominator)
    -> core::Result<RationalBeat> {
    if (denominator <= 0) {
        return core::unexpected(
            core::Error{"chart.beat.invalid_denominator", "Beat denominator must be positive"}
                .withContext("denominator", std::to_string(denominator)));
    }
    if (numerator == 0) {
        return RationalBeat{0, 1};
    }

    const auto divisor = std::gcd(magnitude(numerator), static_cast<std::uint64_t>(denominator));
    const auto reducedMagnitude = magnitude(numerator) / divisor;
    const auto reducedDenominator = static_cast<std::uint64_t>(denominator) / divisor;
    auto reducedNumerator = signedValue(reducedMagnitude, numerator < 0);
    if (!reducedNumerator) {
        return core::unexpected(std::move(reducedNumerator.error()));
    }
    return RationalBeat{*reducedNumerator, static_cast<std::int64_t>(reducedDenominator)};
}

auto RationalBeat::toDouble() const noexcept -> double {
    return static_cast<double>(numerator_) / static_cast<double>(denominator_);
}

auto RationalBeat::toString() const -> std::string {
    return std::to_string(numerator_) + "/" + std::to_string(denominator_);
}

auto operator<=>(const RationalBeat& left, const RationalBeat& right) noexcept
    -> std::strong_ordering {
    const bool leftNegative = left.numerator_ < 0;
    const bool rightNegative = right.numerator_ < 0;
    if (leftNegative != rightNegative) {
        return leftNegative ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    const auto order = comparePositive(
        magnitude(left.numerator_), static_cast<std::uint64_t>(left.denominator_),
        magnitude(right.numerator_), static_cast<std::uint64_t>(right.denominator_));
    if (!leftNegative) {
        return order;
    }
    if (order == std::strong_ordering::less) {
        return std::strong_ordering::greater;
    }
    if (order == std::strong_ordering::greater) {
        return std::strong_ordering::less;
    }
    return std::strong_ordering::equal;
}

auto subtractRationalBeats(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat> {
    auto negatedNumerator = checkedMultiply(right.numerator(), -1);
    if (!negatedNumerator) {
        return core::unexpected(std::move(negatedNumerator.error()));
    }
    auto negated = RationalBeat::create(*negatedNumerator, right.denominator());
    if (!negated) {
        return core::unexpected(std::move(negated.error()));
    }
    return addRationalBeats(left, *negated);
}

auto multiplyRationalBeats(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat> {
    const auto gcdLeftNumRightDen =
        std::gcd(magnitude(left.numerator()), static_cast<std::uint64_t>(right.denominator()));
    const auto gcdRightNumLeftDen =
        std::gcd(magnitude(right.numerator()), static_cast<std::uint64_t>(left.denominator()));
    auto leftNumerator =
        signedValue(magnitude(left.numerator()) / gcdLeftNumRightDen, left.numerator() < 0);
    auto rightNumerator =
        signedValue(magnitude(right.numerator()) / gcdRightNumLeftDen, right.numerator() < 0);
    if (!leftNumerator || !rightNumerator) {
        return core::unexpected(
            core::Error{"chart.beat.out_of_range", "Beat multiplication overflowed"});
    }
    auto numerator = checkedMultiply(*leftNumerator, *rightNumerator);
    auto denominator =
        checkedMultiply(static_cast<std::int64_t>(static_cast<std::uint64_t>(left.denominator()) /
                                                  gcdRightNumLeftDen),
                        static_cast<std::int64_t>(static_cast<std::uint64_t>(right.denominator()) /
                                                  gcdLeftNumRightDen));
    if (!numerator || !denominator) {
        return core::unexpected(
            core::Error{"chart.beat.out_of_range", "Beat multiplication overflowed"});
    }
    return RationalBeat::create(*numerator, *denominator);
}

auto divideRationalBeats(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat> {
    if (right.numerator() == 0) {
        return core::unexpected(
            core::Error{"chart.beat.invalid_denominator", "Beat division by zero"});
    }
    const auto reciprocalNumerator =
        right.numerator() < 0 ? -right.denominator() : right.denominator();
    auto reciprocalDenominator = signedValue(magnitude(right.numerator()), false);
    if (!reciprocalDenominator) {
        return core::unexpected(core::Error{"chart.beat.out_of_range", "Beat division overflowed"});
    }
    auto reciprocal = RationalBeat::create(reciprocalNumerator, *reciprocalDenominator);
    if (!reciprocal) {
        return core::unexpected(std::move(reciprocal.error()));
    }
    return multiplyRationalBeats(left, *reciprocal);
}

auto floorRationalBeats(const RationalBeat& value) -> core::Result<std::int64_t> {
    const auto quotient = value.numerator() / value.denominator();
    if (value.numerator() >= 0 || value.numerator() % value.denominator() == 0) {
        return quotient;
    }
    auto floored = checkedAdd(quotient, -1);
    if (!floored) {
        return core::unexpected(core::Error{"chart.beat.out_of_range", "Beat floor overflowed"});
    }
    return *floored;
}

auto addRationalBeats(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat> {
    const auto divisor = std::gcd(left.denominator(), right.denominator());
    const auto leftFactor = right.denominator() / divisor;
    const auto rightFactor = left.denominator() / divisor;
    auto leftNumerator = checkedMultiply(left.numerator(), leftFactor);
    auto rightNumerator = checkedMultiply(right.numerator(), rightFactor);
    auto denominator = checkedMultiply(left.denominator(), leftFactor);
    if (!leftNumerator || !rightNumerator || !denominator) {
        return core::unexpected(core::Error{"chart.beat.out_of_range", "Beat addition overflowed"});
    }
    auto numerator = checkedAdd(*leftNumerator, *rightNumerator);
    if (!numerator) {
        return core::unexpected(core::Error{"chart.beat.out_of_range", "Beat addition overflowed"});
    }
    return RationalBeat::create(*numerator, *denominator);
}

auto rationalBeatMidpoint(const RationalBeat& left, const RationalBeat& right)
    -> core::Result<RationalBeat> {
    auto sum = addRationalBeats(left, right);
    if (!sum) {
        return core::unexpected(std::move(sum.error()));
    }
    if ((sum->numerator() % 2) == 0) {
        return RationalBeat::create(sum->numerator() / 2, sum->denominator());
    }
    auto denominator = checkedMultiply(sum->denominator(), 2);
    if (!denominator) {
        return core::unexpected(
            core::Error{"chart.beat.midpoint_out_of_range", "Beat midpoint overflowed"});
    }
    return RationalBeat::create(sum->numerator(), *denominator);
}

auto approximateRationalBeat(double beat, std::int64_t maxDenominator)
    -> core::Result<RationalBeat> {
    if (!std::isfinite(beat)) {
        return core::unexpected(
            core::Error{"chart.beat.non_finite", "Beat approximation requires a finite value"});
    }
    if (maxDenominator <= 0) {
        return core::unexpected(
            core::Error{"chart.beat.invalid_denominator", "Beat denominator must be positive"}
                .withContext("denominator", std::to_string(maxDenominator)));
    }

    const bool negative = std::signbit(beat);
    const double magnitude = std::abs(beat);
    constexpr auto intMax = static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (magnitude >= intMax) {
        return core::unexpected(
            core::Error{"chart.beat.out_of_range", "Beat approximation overflowed"});
    }

    std::int64_t previousNumerator = 1;
    std::int64_t previousDenominator = 0;
    std::int64_t numerator = static_cast<std::int64_t>(std::floor(magnitude));
    std::int64_t denominator = 1;
    double remaining = magnitude - static_cast<double>(numerator);
    constexpr double exactTolerance = 1.0e-12;

    const auto finish = [&](std::int64_t valueNumerator,
                            std::int64_t valueDenominator) -> core::Result<RationalBeat> {
        auto signedNumerator = negative ? checkedMultiply(valueNumerator, -1)
                                        : core::Result<std::int64_t>{valueNumerator};
        if (!signedNumerator) {
            return core::unexpected(
                core::Error{"chart.beat.out_of_range", "Beat approximation overflowed"});
        }
        return RationalBeat::create(*signedNumerator, valueDenominator);
    };

    if (remaining <= exactTolerance || maxDenominator == 1) {
        return finish(numerator, denominator);
    }

    while (true) {
        remaining = 1.0 / remaining;
        if (!std::isfinite(remaining) || remaining >= intMax) {
            return finish(numerator, denominator);
        }
        const auto term = static_cast<std::int64_t>(std::floor(remaining));
        auto nextNumerator = checkedMultiply(term, numerator);
        auto nextDenominator = checkedMultiply(term, denominator);
        if (!nextNumerator || !nextDenominator) {
            return finish(numerator, denominator);
        }
        nextNumerator = checkedAdd(*nextNumerator, previousNumerator);
        nextDenominator = checkedAdd(*nextDenominator, previousDenominator);
        if (!nextNumerator || !nextDenominator) {
            return finish(numerator, denominator);
        }
        if (*nextDenominator > maxDenominator) {
            const auto room = maxDenominator - previousDenominator;
            if (room <= 0 || denominator <= 0) {
                return finish(numerator, denominator);
            }
            const auto scaled = room / denominator;
            if (scaled <= 0) {
                return finish(numerator, denominator);
            }
            auto scaledNumerator = checkedMultiply(scaled, numerator);
            auto scaledDenominator = checkedMultiply(scaled, denominator);
            if (!scaledNumerator || !scaledDenominator) {
                return finish(numerator, denominator);
            }
            scaledNumerator = checkedAdd(*scaledNumerator, previousNumerator);
            scaledDenominator = checkedAdd(*scaledDenominator, previousDenominator);
            if (!scaledNumerator || !scaledDenominator || *scaledDenominator > maxDenominator) {
                return finish(numerator, denominator);
            }
            return finish(*scaledNumerator, *scaledDenominator);
        }

        previousNumerator = numerator;
        previousDenominator = denominator;
        numerator = *nextNumerator;
        denominator = *nextDenominator;
        remaining -= static_cast<double>(term);
        if (remaining <= exactTolerance) {
            return finish(numerator, denominator);
        }
        const auto reconstructed =
            static_cast<double>(numerator) / static_cast<double>(denominator);
        if (std::abs(reconstructed - magnitude) <= exactTolerance) {
            return finish(numerator, denominator);
        }
    }
}

} // namespace cuexis::chart
