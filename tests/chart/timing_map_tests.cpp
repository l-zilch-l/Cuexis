#include <cuexis/chart/timing_map.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

TEST_CASE("Fixed TimingMap converts beat chart time and audio offset", "[chart][timing]") {
    const auto map = cuexis::chart::TimingMap::create(120.0, 250.0);
    const auto beat = cuexis::chart::RationalBeat::create(7, 4);
    REQUIRE(map.has_value());
    REQUIRE(beat.has_value());

    CHECK(map->beatToChartTimeMs(*beat) == Catch::Approx(875.0));
    const auto inverse = map->chartTimeMsToBeat(875.0);
    REQUIRE(inverse.has_value());
    CHECK(*inverse == Catch::Approx(1.75));
    const auto chartTime = map->audioTimeMsToChartTimeMs(1125.0);
    REQUIRE(chartTime.has_value());
    CHECK(*chartTime == Catch::Approx(875.0));
    const auto audioTime = map->chartTimeMsToAudioTimeMs(875.0);
    REQUIRE(audioTime.has_value());
    CHECK(*audioTime == Catch::Approx(1125.0));
}

TEST_CASE("TimingMap supports negative beats without frame accumulation", "[chart][timing]") {
    const auto map = cuexis::chart::TimingMap::create(60.0, 0.0);
    const auto beat = cuexis::chart::RationalBeat::create(-3, 2);
    REQUIRE(map.has_value());
    REQUIRE(beat.has_value());
    CHECK(map->beatToChartTimeMs(*beat) == Catch::Approx(-1500.0));
}

TEST_CASE("TimingMap rejects invalid and non-finite values", "[chart][timing]") {
    CHECK_FALSE(cuexis::chart::TimingMap::create(0.0, 0.0).has_value());
    CHECK_FALSE(
        cuexis::chart::TimingMap::create(std::numeric_limits<double>::infinity(), 0.0).has_value());
    CHECK_FALSE(cuexis::chart::TimingMap::create(120.0, std::numeric_limits<double>::quiet_NaN())
                    .has_value());

    const auto map = cuexis::chart::TimingMap::create(120.0, 0.0);
    REQUIRE(map.has_value());
    CHECK_FALSE(map->chartTimeMsToBeat(std::numeric_limits<double>::infinity()).has_value());
}
