#include <cuexis/chart/rational_beat.hpp>

#include <cuexis/core/error.hpp>

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

} // namespace cuexis::chart
