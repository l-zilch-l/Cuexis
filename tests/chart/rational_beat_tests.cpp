#include <cuexis/chart/rational_beat.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

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

TEST_CASE("RationalBeat rejects non-positive denominators", "[chart][beat]") {
    const auto zero = cuexis::chart::RationalBeat::create(1, 0);
    REQUIRE_FALSE(zero.has_value());
    CHECK(zero.error().code() == "chart.beat.invalid_denominator");

    const auto negative = cuexis::chart::RationalBeat::create(1, -2);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().code() == "chart.beat.invalid_denominator");
}

TEST_CASE("Simple beat parsing is exact for integer fraction and decimal text", "[chart][beat]") {
    const auto integer = cuexis::chart::RationalBeat::parseSimple("4");
    const auto fraction = cuexis::chart::RationalBeat::parseSimple("7/4");
    const auto decimal = cuexis::chart::RationalBeat::parseSimple("1.2500");
    const auto negative = cuexis::chart::RationalBeat::parseSimple("-0.125");

    REQUIRE(integer.has_value());
    REQUIRE(fraction.has_value());
    REQUIRE(decimal.has_value());
    REQUIRE(negative.has_value());
    CHECK(integer->toString() == "4/1");
    CHECK(fraction->toString() == "7/4");
    CHECK(decimal->toString() == "5/4");
    CHECK(negative->toString() == "-1/8");
}

TEST_CASE("Simple beat parsing rejects malformed and over-budget values", "[chart][beat]") {
    CHECK_FALSE(cuexis::chart::RationalBeat::parseSimple("1.2/3").has_value());
    CHECK_FALSE(cuexis::chart::RationalBeat::parseSimple("1/0").has_value());
    CHECK_FALSE(cuexis::chart::RationalBeat::parseSimple("1.").has_value());
    CHECK_FALSE(cuexis::chart::RationalBeat::parseSimple(" 1").has_value());

    cuexis::chart::ChartLimits limits;
    limits.maxBeatNumeratorMagnitude = 4;
    const auto limited = cuexis::chart::RationalBeat::parseSimple("5", limits);
    REQUIRE_FALSE(limited.has_value());
    CHECK(limited.error().code() == "chart.beat.limit_exceeded");
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
