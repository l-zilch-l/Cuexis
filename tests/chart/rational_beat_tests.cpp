#include <cuexis/chart/rational_beat.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>

TEST_CASE("RationalBeat normalizes signs, factors, and zero", "[chart][beat]") {
    const auto reduced = cuexis::chart::RationalBeat::create(14, 8);
    REQUIRE(reduced.has_value());
    CHECK(reduced->numerator() == 7);
    CHECK(reduced->denominator() == 4);

    const auto negative = cuexis::chart::RationalBeat::create(-9, 6);
    REQUIRE(negative.has_value());
    CHECK(negative->numerator() == -3);
    CHECK(negative->denominator() == 2);

    const auto zero = cuexis::chart::RationalBeat::create(0, 999);
    REQUIRE(zero.has_value());
    CHECK(zero->numerator() == 0);
    CHECK(zero->denominator() == 1);
}

TEST_CASE("RationalBeat zero and one are canonical 0/1 and 1/1", "[chart][beat]") {
    STATIC_CHECK(cuexis::chart::RationalBeat::zero().numerator() == 0);
    STATIC_CHECK(cuexis::chart::RationalBeat::zero().denominator() == 1);
    STATIC_CHECK(cuexis::chart::RationalBeat::one().numerator() == 1);
    STATIC_CHECK(cuexis::chart::RationalBeat::one().denominator() == 1);

    const auto createdZero = cuexis::chart::RationalBeat::create(0, 1);
    const auto createdOne = cuexis::chart::RationalBeat::create(1, 1);
    REQUIRE(createdZero.has_value());
    REQUIRE(createdOne.has_value());
    CHECK(cuexis::chart::RationalBeat::zero() == *createdZero);
    CHECK(cuexis::chart::RationalBeat::one() == *createdOne);
}

TEST_CASE("RationalBeat rejects non-positive denominators", "[chart][beat]") {
    const auto zero = cuexis::chart::RationalBeat::create(1, 0);
    REQUIRE_FALSE(zero.has_value());
    CHECK(zero.error().code() == "chart.beat.invalid_denominator");

    const auto negative = cuexis::chart::RationalBeat::create(1, -2);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().code() == "chart.beat.invalid_denominator");
}

TEST_CASE("RationalBeat ordering avoids cross-multiplication overflow", "[chart][beat]") {
    const auto left = cuexis::chart::RationalBeat::create(INT64_MAX - 1, INT64_MAX);
    const auto right = cuexis::chart::RationalBeat::create(INT64_MAX - 2, INT64_MAX - 1);
    REQUIRE(left.has_value());
    REQUIRE(right.has_value());
    CHECK(*left > *right);

    const auto negativeLeft = cuexis::chart::RationalBeat::create(-(INT64_MAX - 1), INT64_MAX);
    const auto negativeRight = cuexis::chart::RationalBeat::create(-(INT64_MAX - 2), INT64_MAX - 1);
    REQUIRE(negativeLeft.has_value());
    REQUIRE(negativeRight.has_value());
    CHECK(*negativeLeft < *negativeRight);
}

TEST_CASE("RationalBeat multiply divide subtract and floor stay exact", "[chart][beat]") {
    const auto left = cuexis::chart::RationalBeat::create(3, 2);
    const auto right = cuexis::chart::RationalBeat::create(4, 3);
    REQUIRE(left.has_value());
    REQUIRE(right.has_value());

    const auto product = cuexis::chart::multiplyRationalBeats(*left, *right);
    REQUIRE(product.has_value());
    CHECK(product->toString() == "2/1");

    const auto quotient = cuexis::chart::divideRationalBeats(*left, *right);
    REQUIRE(quotient.has_value());
    CHECK(quotient->toString() == "9/8");

    const auto difference = cuexis::chart::subtractRationalBeats(*left, *right);
    REQUIRE(difference.has_value());
    CHECK(difference->toString() == "1/6");

    const auto negative = cuexis::chart::RationalBeat::create(-5, 2);
    REQUIRE(negative.has_value());
    const auto floored = cuexis::chart::floorRationalBeats(*negative);
    REQUIRE(floored.has_value());
    CHECK(*floored == -3);

    const auto zeroDivisor = cuexis::chart::RationalBeat::create(0, 1);
    REQUIRE(zeroDivisor.has_value());
    const auto dividedByZero = cuexis::chart::divideRationalBeats(*left, *zeroDivisor);
    REQUIRE_FALSE(dividedByZero.has_value());
    CHECK(dividedByZero.error().code() == "chart.beat.invalid_denominator");
}

TEST_CASE("approximateRationalBeat reconstructs exact simple fractions", "[chart][beat][s4-d]") {
    const auto half = cuexis::chart::approximateRationalBeat(0.5);
    REQUIRE(half.has_value());
    CHECK(half->toString() == "1/2");

    const auto third = cuexis::chart::approximateRationalBeat(1.0 / 3.0);
    REQUIRE(third.has_value());
    CHECK(third->toString() == "1/3");

    const auto negative = cuexis::chart::approximateRationalBeat(-1.5);
    REQUIRE(negative.has_value());
    CHECK(negative->toString() == "-3/2");

    const auto zero = cuexis::chart::approximateRationalBeat(0.0);
    REQUIRE(zero.has_value());
    CHECK(zero->toString() == "0/1");

    const auto bounded = cuexis::chart::approximateRationalBeat(0.1, 10);
    REQUIRE(bounded.has_value());
    CHECK(bounded->denominator() <= 10);
    CHECK(bounded->toDouble() == Catch::Approx(0.1).margin(1.0e-6));

    const auto invalid =
        cuexis::chart::approximateRationalBeat(std::numeric_limits<double>::infinity());
    REQUIRE_FALSE(invalid.has_value());
    CHECK(invalid.error().code() == "chart.beat.non_finite");
}
