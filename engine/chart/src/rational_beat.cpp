#include <cuexis/chart/rational_beat.hpp>

#include <cuexis/core/error.hpp>

#include <charconv>
#include <limits>
#include <numeric>
#include <string>
#include <system_error>
#include <utility>

namespace cuexis::chart {
namespace {

[[nodiscard]] constexpr auto magnitude(std::int64_t value) noexcept -> std::uint64_t {
    if (value >= 0) {
        return static_cast<std::uint64_t>(value);
    }
    return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

[[nodiscard]] auto parseUnsigned(std::string_view text, const char* part)
    -> core::Result<std::uint64_t> {
    if (text.empty()) {
        return core::unexpected(
            core::Error{"chart.beat.invalid", "Beat contains an empty part"}.withContext("part",
                                                                                         part));
    }

    std::uint64_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (result.ec == std::errc::result_out_of_range) {
        return core::unexpected(
            core::Error{"chart.beat.out_of_range", "Beat integer is outside the supported range"}
                .withContext("part", part));
    }
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return core::unexpected(
            core::Error{"chart.beat.invalid", "Beat contains non-decimal text"}.withContext("part",
                                                                                            part));
    }
    return value;
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

[[nodiscard]] auto enforceLimits(RationalBeat beat, const ChartLimits& limits)
    -> core::Result<RationalBeat> {
    const auto maxMagnitude = limits.maxBeatNumeratorMagnitude;
    if (maxMagnitude < 0 || limits.maxBeatDenominator <= 0) {
        return core::unexpected(
            core::Error{"chart.limits.invalid", "Beat limits must be positive"});
    }
    if (magnitude(beat.numerator()) > static_cast<std::uint64_t>(maxMagnitude) ||
        beat.denominator() > limits.maxBeatDenominator) {
        return core::unexpected(
            core::Error{"chart.beat.limit_exceeded", "Beat exceeds the configured chart limits"}
                .withContext("numerator", std::to_string(beat.numerator()))
                .withContext("denominator", std::to_string(beat.denominator())));
    }
    return beat;
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

auto RationalBeat::parseSimple(std::string_view text, const ChartLimits& limits)
    -> core::Result<RationalBeat> {
    if (text.empty() || text.size() > limits.maxSimpleBeatBytes) {
        return core::unexpected(
            core::Error{"chart.beat.invalid_length", "Simple beat text has an invalid length"}
                .withContext("length", std::to_string(text.size())));
    }

    bool negative = false;
    if (text.front() == '+' || text.front() == '-') {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty()) {
        return core::unexpected(core::Error{"chart.beat.invalid", "Beat has no digits"});
    }

    std::uint64_t unsignedNumerator{};
    std::uint64_t unsignedDenominator{1};
    const auto slash = text.find('/');
    const auto decimal = text.find('.');
    if (slash != std::string_view::npos && decimal != std::string_view::npos) {
        return core::unexpected(
            core::Error{"chart.beat.invalid", "Beat cannot contain both a fraction and decimal"});
    }

    if (slash != std::string_view::npos) {
        if (text.find('/', slash + 1) != std::string_view::npos) {
            return core::unexpected(
                core::Error{"chart.beat.invalid", "Beat contains more than one fraction slash"});
        }
        auto numerator = parseUnsigned(text.substr(0, slash), "numerator");
        auto denominator = parseUnsigned(text.substr(slash + 1), "denominator");
        if (!numerator) {
            return core::unexpected(std::move(numerator.error()));
        }
        if (!denominator) {
            return core::unexpected(std::move(denominator.error()));
        }
        if (*denominator == 0) {
            return core::unexpected(
                core::Error{"chart.beat.invalid_denominator", "Beat denominator must be positive"});
        }
        unsignedNumerator = *numerator;
        unsignedDenominator = *denominator;
    } else if (decimal != std::string_view::npos) {
        if (text.find('.', decimal + 1) != std::string_view::npos || decimal == 0 ||
            decimal + 1 == text.size()) {
            return core::unexpected(
                core::Error{"chart.beat.invalid", "Beat decimal has an invalid shape"});
        }
        const auto wholeText = text.substr(0, decimal);
        const auto fractionText = text.substr(decimal + 1);
        auto whole = parseUnsigned(wholeText, "whole");
        auto fraction = parseUnsigned(fractionText, "fraction");
        if (!whole) {
            return core::unexpected(std::move(whole.error()));
        }
        if (!fraction) {
            return core::unexpected(std::move(fraction.error()));
        }

        for (std::size_t index = 0; index < fractionText.size(); ++index) {
            if (unsignedDenominator >
                static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / 10U) {
                return core::unexpected(
                    core::Error{"chart.beat.out_of_range", "Beat decimal precision is too large"});
            }
            unsignedDenominator *= 10U;
        }
        if (*whole >
            (std::numeric_limits<std::uint64_t>::max() - *fraction) / unsignedDenominator) {
            return core::unexpected(
                core::Error{"chart.beat.out_of_range", "Beat decimal is outside supported range"});
        }
        unsignedNumerator = (*whole * unsignedDenominator) + *fraction;
    } else {
        auto numerator = parseUnsigned(text, "numerator");
        if (!numerator) {
            return core::unexpected(std::move(numerator.error()));
        }
        unsignedNumerator = *numerator;
    }

    if (unsignedDenominator >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return core::unexpected(core::Error{"chart.beat.out_of_range",
                                            "Beat denominator is outside signed 64-bit range"});
    }
    auto numerator = signedValue(unsignedNumerator, negative);
    if (!numerator) {
        return core::unexpected(std::move(numerator.error()));
    }
    auto beat = create(*numerator, static_cast<std::int64_t>(unsignedDenominator));
    if (!beat) {
        return core::unexpected(std::move(beat.error()));
    }
    return enforceLimits(*beat, limits);
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

} // namespace cuexis::chart
