#include <cuexis/playback/runtime_timeline.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("RuntimeTimeline applies offset and freezes delta outside Playing",
          "[playback][timeline]") {
    auto timeline = cuexis::playback::RuntimeTimeline::create(125.0);
    REQUIRE(timeline.has_value());

    const auto first = timeline->advance({1000.0, cuexis::audio::PlaybackState::Playing, 0});
    REQUIRE(first.has_value());
    CHECK(first->chartTimeMs == Catch::Approx(875.0));
    CHECK(first->simulationDeltaTimeMs == Catch::Approx(0.0));

    const auto second = timeline->advance({1016.0, cuexis::audio::PlaybackState::Playing, 0});
    REQUIRE(second.has_value());
    CHECK(second->simulationDeltaTimeMs == Catch::Approx(16.0));

    const auto paused = timeline->advance({1016.0, cuexis::audio::PlaybackState::Paused, 0});
    REQUIRE(paused.has_value());
    CHECK(paused->simulationDeltaTimeMs == Catch::Approx(0.0));
    const auto resumed = timeline->advance({1020.0, cuexis::audio::PlaybackState::Playing, 0});
    REQUIRE(resumed.has_value());
    CHECK(resumed->simulationDeltaTimeMs == Catch::Approx(0.0));
}

TEST_CASE("RuntimeTimeline maps source discontinuities to session segments",
          "[playback][timeline][seek]") {
    auto timeline = cuexis::playback::RuntimeTimeline::create(0.0);
    REQUIRE(timeline.has_value());
    REQUIRE(timeline->advance({500.0, cuexis::audio::PlaybackState::Playing, 7}).has_value());

    const auto seek = timeline->advance({100.0, cuexis::audio::PlaybackState::Playing, 8});
    REQUIRE(seek.has_value());
    CHECK(seek->timeDiscontinuityId == 1);
    CHECK(seek->simulationDeltaTimeMs == Catch::Approx(0.0));

    const auto regression = timeline->advance({99.0, cuexis::audio::PlaybackState::Playing, 8});
    REQUIRE_FALSE(regression.has_value());
    CHECK(regression.error().code() == "playback.timeline.segment_regressed");

    REQUIRE(timeline->reset(-50.0).has_value());
    const auto reset = timeline->advance({100.0, cuexis::audio::PlaybackState::Paused, 8});
    REQUIRE(reset.has_value());
    CHECK(reset->chartTimeMs == Catch::Approx(150.0));
    CHECK(reset->timeDiscontinuityId == 2);
}

TEST_CASE("ChartClock produces the shared source-time contract", "[playback][timeline][clock]") {
    cuexis::playback::ChartClock clock{75.0};
    const auto first = clock.sample(25.0);
    REQUIRE(first.has_value());
    CHECK(first->positionMs == Catch::Approx(100.0));
    CHECK(first->discontinuityId == 0);

    clock.markDiscontinuity();
    const auto seek = clock.sample(0.0, cuexis::audio::PlaybackState::Paused);
    REQUIRE(seek.has_value());
    CHECK(seek->positionMs == Catch::Approx(75.0));
    CHECK(seek->state == cuexis::audio::PlaybackState::Paused);
    CHECK(seek->discontinuityId == 1);

    cuexis::playback::ChartClock preRollClock{0.0};
    const auto preRoll = preRollClock.sample(-25.0);
    REQUIRE(preRoll.has_value());
    CHECK(preRoll->positionMs == Catch::Approx(-25.0));

    cuexis::playback::ChartClock negativeOffsetClock{-50.0};
    const auto negativeOffset = negativeOffsetClock.sample(25.0);
    REQUIRE(negativeOffset.has_value());
    CHECK(negativeOffset->positionMs == Catch::Approx(-25.0));

    auto negativeOffsetTimeline = cuexis::playback::RuntimeTimeline::create(-50.0);
    REQUIRE(negativeOffsetTimeline.has_value());
    const auto restoredChartTime = negativeOffsetTimeline->advance(*negativeOffset);
    REQUIRE(restoredChartTime.has_value());
    CHECK(restoredChartTime->chartTimeMs == Catch::Approx(25.0));
}
