#include "rpa_b1_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/playback/realtime_clock.hpp>
#include <limits>

using namespace cuexis::playback;
namespace {
auto profile() -> SchedulingProfile {
    return {100000, 100, 64, "", true};
}
auto offsets() -> ClockMappingConfiguration {
    return {{1000, 1}, {500, 1}, {250, 1}, {1, 1}};
}
auto observation(std::int64_t frames, std::int64_t rate) -> ClockObservation {
    return {"fixture.monotonic",
            1,
            1,
            0,
            100000,
            10,
            ClockFramePosition{frames, rate},
            ClockFramePosition{frames, rate},
            ClockFramePosition{480, 48000},
            ClockPlaybackState::Playing,
            ClockQuality::Estimated,
            false,
            false,
            false};
}
auto fixtureClock(ClockMappingConfiguration c = offsets()) -> RealtimeClock {
    auto result = RealtimeClock::create(profile(), c, "fixture.monotonic", 1);
    REQUIRE(result);
    return std::move(*result);
}
} // namespace
TEST_CASE("B3 clock calibration identity binds reduced judgement mapping and excludes display lead",
          "[rpa][b3][clock]") {
    const auto baseline = RealtimeClock::calibrationIdentity(offsets());
    REQUIRE(baseline);
    CHECK(*baseline == "gameplay.clock-mapping.v1;A=1000/1;I=500/1;rate=1/1");
    const auto equivalent =
        RealtimeClock::calibrationIdentity({{2000, 2}, {1500, 3}, {999, 7}, {2, 2}});
    REQUIRE(equivalent);
    CHECK(*equivalent == *baseline);
    for (const auto field : {0, 1, 2}) {
        auto changed = offsets();
        if (field == 0)
            changed.timingOffsetUs = {1001, 1};
        if (field == 1)
            changed.inputCompensationUs = {501, 1};
        if (field == 2)
            changed.fixedRate = {2, 1};
        const auto identity = RealtimeClock::calibrationIdentity(changed);
        REQUIRE(identity);
        CHECK(*identity != *baseline);
    }
    for (const auto field : {0, 1, 2, 3}) {
        auto invalid = offsets();
        if (field == 0)
            invalid.timingOffsetUs.denominator = 0;
        if (field == 1)
            invalid.inputCompensationUs.denominator = -1;
        if (field == 2)
            invalid.displayLeadUs.denominator = 0;
        if (field == 3)
            invalid.fixedRate.denominator = 0;
        CHECK_FALSE(RealtimeClock::calibrationIdentity(invalid));
    }
    auto stopped = offsets();
    stopped.fixedRate.numerator = 0;
    CHECK_FALSE(RealtimeClock::calibrationIdentity(stopped));
}
TEST_CASE("B3 clock maps 44.1 and 48 kHz with independent offset oracle", "[rpa][b3][clock]") {
    for (const auto rate : {44100, 48000}) {
        auto bridge = fixtureClock();
        auto result = bridge.sample(observation(rate / 100, rate), 0);
        REQUIRE(result);
        const auto oracle = cuexis::rpa::testing::mapTime({rate / 100, rate, 1000, 500, 250});
        REQUIRE(oracle);
        CHECK(result->observationTick == oracle->observationTick);
        CHECK(result->horizon == oracle->horizonTick);
        CHECK(result->presentationTick == oracle->presentationTick);
        CHECK(result->observationTick == 8500);
        CHECK(result->horizon == 9000);
        CHECK(result->presentationTick == 9250);
    }
}
TEST_CASE("B3 clock rounds once with negative ties and rejects arithmetic overflow",
          "[rpa][b3][clock]") {
    for (const auto numerator : {-5, -3, 3, 5}) {
        auto bridge = fixtureClock({{0, 1}, {-numerator, 2}, {numerator, 2}, {1, 1}});
        auto result = bridge.sample(observation(0, 48000), 0);
        REQUIRE(result);
        const auto expected = cuexis::rpa::testing::roundHalfEven({numerator, 2});
        REQUIRE(expected);
        CHECK(result->observationTick == *expected);
        CHECK(result->presentationTick == *expected);
        CHECK(result->horizon == 0);
    }
    auto bridge = fixtureClock({{0, 1}, {0, 1}, {0, 1}, {1, 1}});
    auto result = bridge.sample(observation(INT64_MAX, 1), 0);
    REQUIRE_FALSE(result);
    CHECK_FALSE(bridge.permitsAdvance());
}
TEST_CASE("B3 cached sample keeps timestamp and invalid clock needs explicit new segment",
          "[rpa][b3][clock]") {
    auto bridge = fixtureClock();
    auto sample = observation(441, 44100);
    REQUIRE(bridge.sample(sample, 0));
    REQUIRE(bridge.sample(sample, 100000));
    auto expired = bridge.sample(sample, 100001);
    REQUIRE_FALSE(expired);
    CHECK_FALSE(bridge.permitsAdvance());
    auto restamped = sample;
    restamped.sampleMonotonicUs = 100001;
    restamped.validUntilMonotonicUs = 200001;
    REQUIRE_FALSE(bridge.sample(restamped, 100001));
    sample.segment = 2;
    sample.publicationSequence = 0;
    sample.sampleMonotonicUs = 100001;
    sample.validUntilMonotonicUs = 200001;
    REQUIRE(bridge.startSegment(2, sample, 100001));
    CHECK(bridge.permitsAdvance());
    sample.underrun = true;
    sample.publicationSequence = 1;
    REQUIRE_FALSE(bridge.sample(sample, 100002));
    CHECK_FALSE(bridge.permitsAdvance());
}
TEST_CASE("B3 clock rejects domain and quality changes instead of advancing silent frames",
          "[rpa][b3][clock]") {
    for (const auto mode : {0, 1, 2, 3, 4, 5}) {
        auto bridge = fixtureClock();
        auto sample = observation(0, 48000);
        if (mode == 0)
            sample.monotonicDomain = "another.clock";
        if (mode == 1)
            sample.quality = ClockQuality::Unknown;
        if (mode == 2)
            sample.deviceChanged = true;
        if (mode == 3)
            sample.rateChanged = true;
        if (mode == 4)
            sample.presented.reset();
        if (mode == 5)
            sample.errorBoundUs = 101;
        REQUIRE_FALSE(bridge.sample(sample, 0));
        CHECK_FALSE(bridge.permitsAdvance());
    }
    auto incomplete = profile();
    incomplete.observationValidityUs.reset();
    auto result = RealtimeClock::create(incomplete, offsets(), "fixture.monotonic", 1);
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == "host.rpa.profile_incomplete");
}
TEST_CASE("B3 explicit EOF tail remains continuous and excludes paused wall time",
          "[rpa][b3][clock]") {
    auto bridge = fixtureClock({{0, 1}, {0, 1}, {0, 1}, {1, 1}});
    auto eof = observation(480, 48000);
    eof.playback = ClockPlaybackState::EndOfStream;
    auto transition = bridge.startTail(eof, 0, 2, {{1, 1}, 20000, 100, "", true}, 25000, 3);
    REQUIRE(transition);
    CHECK(transition->terminalHorizon == 25003);
    auto sample = bridge.sampleTail(1000);
    REQUIRE(sample);
    CHECK(sample->horizon == 11000);
    REQUIRE(bridge.pauseTail(1000));
    REQUIRE_FALSE(bridge.sampleTail(2000));
    REQUIRE(bridge.resumeTail(1000000, 3));
    sample = bridge.sampleTail(1001000);
    REQUIRE(sample);
    CHECK(sample->horizon == 12000);
    auto terminal = bridge.sampleTail(1014003);
    REQUIRE(terminal);
    CHECK(terminal->horizon == 25003);
    auto after = bridge.sampleTail(1014004);
    REQUIRE(after);
    CHECK(after->horizon == 25003);
    CHECK(bridge.permitsAdvance());
}

TEST_CASE("B3 EOF final horizon stays reachable when fixed rate skips its exact Tick",
          "[rpa][b3][clock]") {
    auto bridge = fixtureClock({{0, 1}, {0, 1}, {0, 1}, {1, 1}});
    auto eof = observation(0, 48000);
    eof.playback = ClockPlaybackState::EndOfStream;
    REQUIRE(bridge.startTail(eof, 0, 2, {{2, 1}, 20, 100, "", true}, 20, 3));
    auto final = bridge.sampleTail(12);
    REQUIRE(final);
    CHECK(final->horizon == 23);
    CHECK(final->publicationSequence == 1);
    CHECK(final->observationTick == 23);
    CHECK(final->presentationTick == 23);
    auto later = bridge.sampleTail(1000);
    REQUIRE(later);
    CHECK(later->horizon == 23);
}
