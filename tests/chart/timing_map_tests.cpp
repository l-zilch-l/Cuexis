#include <cuexis/chart/timing_map.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator = 1)
    -> cuexis::chart::RationalBeat {
    return *cuexis::chart::RationalBeat::create(numerator, denominator);
}

} // namespace

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

TEST_CASE("TimingMap integrates Tempo Events and performs fixed-budget inversion",
          "[chart][timing][v3]") {
    const std::vector events{cuexis::chart::TempoEvent{
        .startBeat = beat(0),
        .durationBeats = beat(4),
        .startBpm = 120.0,
        .endBpm = 240.0,
        .startSlope = 1.0,
        .endSlope = 1.0,
    }};
    const auto map = cuexis::chart::TimingMap::create(60.0, 25.0, events, {});
    REQUIRE(map.has_value());

    const double expectedEnd = 2000.0 * std::log(2.0);
    CHECK(map->beatToChartTimeMs(beat(4)) == Catch::Approx(expectedEnd).margin(1e-8));
    CHECK(map->beatToChartTimeMs(beat(2)) == Catch::Approx(2000.0 * std::log(1.5)).margin(1e-8));
    const auto inverse = map->chartTimeMsToBeat(expectedEnd);
    REQUIRE(inverse.has_value());
    CHECK(*inverse == Catch::Approx(4.0).margin(1e-10));
}

TEST_CASE("TimingMap exposes Stop half-open boundaries", "[chart][timing][v3]") {
    const std::vector stops{cuexis::chart::TimingStop{.beat = beat(2), .durationMs = 500.0}};
    const auto map = cuexis::chart::TimingMap::create(120.0, 0.0, {}, stops);
    REQUIRE(map.has_value());
    CHECK(map->beatToChartTimeMs(beat(2)) == Catch::Approx(1000.0));
    CHECK(map->beatToChartTimeMs(beat(3)) == Catch::Approx(2000.0));

    const auto start = map->sampleChartTimeMs(1000.0);
    const auto middle = map->sampleChartTimeMs(1250.0);
    const auto end = map->sampleChartTimeMs(1500.0);
    REQUIRE(start.has_value());
    REQUIRE(middle.has_value());
    REQUIRE(end.has_value());
    CHECK(start->beat == Catch::Approx(2.0));
    CHECK(start->inStop);
    CHECK(start->stopProgress == Catch::Approx(0.0));
    CHECK(middle->beat == Catch::Approx(2.0));
    CHECK(middle->inStop);
    CHECK(middle->stopProgress == Catch::Approx(0.5));
    CHECK(end->beat == Catch::Approx(2.0));
    CHECK_FALSE(end->inStop);
}

TEST_CASE("Negative Stops retain Beat zero as chart time zero", "[chart][timing][v3]") {
    const std::vector stops{cuexis::chart::TimingStop{.beat = beat(-1), .durationMs = 250.0}};
    const auto map = cuexis::chart::TimingMap::create(60.0, 0.0, {}, stops);
    REQUIRE(map.has_value());
    CHECK(map->beatToChartTimeMs(beat(0)) == Catch::Approx(0.0));
    CHECK(map->beatToChartTimeMs(beat(-1)) == Catch::Approx(-1250.0));
    const auto during = map->sampleChartTimeMs(-1125.0);
    REQUIRE(during.has_value());
    CHECK(during->beat == Catch::Approx(-1.0));
    CHECK(during->inStop);
    CHECK(during->stopProgress == Catch::Approx(0.5));
}

TEST_CASE("TimingMap validates Tempo Event and Stop conflicts", "[chart][timing][v3]") {
    const std::vector overlapping{
        cuexis::chart::TempoEvent{beat(0), beat(2), 120.0, 180.0, 1.0, 1.0},
        cuexis::chart::TempoEvent{beat(1), beat(2), 180.0, 120.0, 1.0, 1.0},
    };
    CHECK_FALSE(cuexis::chart::TimingMap::create(120.0, 0.0, overlapping, {}).has_value());

    const std::vector duplicateStops{
        cuexis::chart::TimingStop{beat(1), 100.0},
        cuexis::chart::TimingStop{beat(1), 200.0},
    };
    CHECK_FALSE(cuexis::chart::TimingMap::create(120.0, 0.0, {}, duplicateStops).has_value());
}

TEST_CASE("TimingMap meets the full-range integration error budget deterministically",
          "[chart][timing][v3][limits]") {
    const std::vector events{cuexis::chart::TempoEvent{
        .startBeat = beat(0),
        .durationBeats = beat(4096),
        .startBpm = 1.0,
        .endBpm = 65536.0,
        .startSlope = 1.0,
        .endSlope = 1.0,
    }};
    const auto map = cuexis::chart::TimingMap::create(1.0, 0.0, events, {});
    REQUIRE(map.has_value());
    const double exactEndMs = 4096.0 * 60000.0 * std::log(65536.0) / 65535.0;
    CHECK(map->beatToChartTimeMs(beat(4096)) == Catch::Approx(exactEndMs).margin(0.05));
    const auto inverse = map->chartTimeMsToBeat(exactEndMs);
    REQUIRE(inverse.has_value());
    CHECK(*inverse == Catch::Approx(4096.0).margin(1e-6));
}

TEST_CASE("TimingMap enforces the fixed Tempo Event and Stop budgets",
          "[chart][timing][v3][limits]") {
    const cuexis::chart::TempoEvent event{
        .startBeat = beat(0), .durationBeats = beat(0), .startBpm = 120.0, .endBpm = 120.0};
    const cuexis::chart::TimingStop stop{.beat = beat(0), .durationMs = 1.0};
    const std::vector tooManyEvents(4097, event);
    const std::vector tooManyStops(4097, stop);
    CHECK_FALSE(cuexis::chart::TimingMap::create(120.0, 0.0, tooManyEvents, {}).has_value());
    CHECK_FALSE(cuexis::chart::TimingMap::create(120.0, 0.0, {}, tooManyStops).has_value());
}
