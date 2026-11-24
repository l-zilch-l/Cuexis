#include <cuexis/timing/rational_beat.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>

using cuexis::timing::addRationalBeats;
using cuexis::timing::divideRationalBeats;
using cuexis::timing::floorRationalBeats;
using cuexis::timing::multiplyRationalBeats;
using cuexis::timing::RationalBeat;
using cuexis::timing::subtractRationalBeats;

namespace {

auto beat(std::int64_t numerator, std::int64_t denominator) -> RationalBeat {
    auto created = RationalBeat::create(numerator, denominator);
    REQUIRE(created);
    return *created;
}

} // namespace

TEST_CASE("timing RationalBeat reduces and fixes zero", "[timing][rational]") {
    const auto half = beat(2, 4);
    CHECK(half.numerator() == 1);
    CHECK(half.denominator() == 2);
    CHECK(half.toString() == "1/2");
    const auto zero = beat(0, 7);
    CHECK(zero.numerator() == 0);
    CHECK(zero.denominator() == 1);
    CHECK(RationalBeat::zero() == zero);
    CHECK(RationalBeat::one().numerator() == 1);
}

TEST_CASE("timing RationalBeat rejects a non-positive denominator", "[timing][rational]") {
    const auto zero = RationalBeat::create(1, 0);
    REQUIRE_FALSE(zero);
    CHECK(zero.error().code() == "timing.beat.invalid_denominator");
    const auto negative = RationalBeat::create(1, -2);
    REQUIRE_FALSE(negative);
    CHECK(negative.error().code() == "timing.beat.invalid_denominator");
}

TEST_CASE("timing RationalBeat ordering is exact", "[timing][rational]") {
    CHECK(beat(1, 2) < beat(2, 3));
    CHECK(beat(-1, 2) < RationalBeat::zero());
    CHECK(beat(-2, 3) < beat(-1, 2));
    CHECK(beat(2, 4) == beat(1, 2));
    CHECK(beat(std::numeric_limits<std::int64_t>::max(), 1) > beat(1, 1));
}

TEST_CASE("timing RationalBeat arithmetic is checked", "[timing][rational]") {
    const auto sum = addRationalBeats(beat(1, 3), beat(1, 6));
    REQUIRE(sum);
    CHECK(*sum == beat(1, 2));
    const auto difference = subtractRationalBeats(beat(1, 2), beat(1, 3));
    REQUIRE(difference);
    CHECK(*difference == beat(1, 6));
    const auto product = multiplyRationalBeats(beat(2, 3), beat(3, 4));
    REQUIRE(product);
    CHECK(*product == beat(1, 2));
    const auto quotient = divideRationalBeats(beat(1, 2), beat(1, 4));
    REQUIRE(quotient);
    CHECK(*quotient == beat(2, 1));
    const auto floored = floorRationalBeats(beat(-1, 2));
    REQUIRE(floored);
    CHECK(*floored == -1);

    const auto overflow =
        addRationalBeats(beat(std::numeric_limits<std::int64_t>::max(), 1), beat(1, 1));
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error().code() == "timing.beat.out_of_range");
    const auto divisionByZero = divideRationalBeats(beat(1, 1), RationalBeat::zero());
    REQUIRE_FALSE(divisionByZero);
    CHECK(divisionByZero.error().code() == "timing.beat.invalid_denominator");
}
