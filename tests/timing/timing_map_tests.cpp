#include <cuexis/timing/timing_map.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

using cuexis::timing::BeatSample;
using cuexis::timing::RationalBeat;
using cuexis::timing::Stop;
using cuexis::timing::TempoEvent;
using cuexis::timing::TimeUs;
using cuexis::timing::TimingMap;
using cuexis::timing::TimingSourceView;

namespace {

auto beat(std::int64_t numerator, std::int64_t denominator) -> RationalBeat {
    auto created = RationalBeat::create(numerator, denominator);
    REQUIRE(created);
    return *created;
}

struct SourceFixture final {
    double defaultBpm{120.0};
    std::vector<TempoEvent> tempos;
    std::vector<Stop> stops;

    [[nodiscard]] auto view() const -> TimingSourceView {
        return TimingSourceView{defaultBpm, tempos, stops};
    }
};

} // namespace

TEST_CASE("timing rounds half to even inside the signed range", "[timing][round]") {
    CHECK(cuexis::timing::roundMicroseconds(0.0)->value == 0);
    CHECK(cuexis::timing::roundMicroseconds(0.5)->value == 0);
    CHECK(cuexis::timing::roundMicroseconds(1.5)->value == 2);
    CHECK(cuexis::timing::roundMicroseconds(2.5)->value == 2);
    CHECK(cuexis::timing::roundMicroseconds(-0.5)->value == 0);
    CHECK(cuexis::timing::roundMicroseconds(-1.5)->value == -2);
    CHECK(cuexis::timing::roundMicroseconds(2.4)->value == 2);
    CHECK(cuexis::timing::roundMicroseconds(-2.4)->value == -2);
    CHECK(cuexis::timing::roundMicroseconds(-2.6)->value == -3);

    const auto tooLarge = cuexis::timing::roundMicroseconds(1.0e300);
    REQUIRE_FALSE(tooLarge);
    CHECK(tooLarge.error().code() == "timing.time_range");
    const auto nan = cuexis::timing::roundMicroseconds(std::numeric_limits<double>::quiet_NaN());
    REQUIRE_FALSE(nan);
    CHECK(nan.error().code() == "timing.time_range");
}

TEST_CASE("timing compiles zero beat and returns saved anchors", "[timing][forward]") {
    SourceFixture fixture;
    const std::vector<RationalBeat> required{beat(3, 2)};
    auto map = TimingMap::compile(fixture.view(), required);
    REQUIRE(map);

    CHECK(map->beatToTime(RationalBeat::zero())->value == 0);
    CHECK(map->beatToTime(beat(1, 1))->value == 500000);
    CHECK(map->beatToTime(beat(3, 2))->value == 750000);
    CHECK(map->beatToTime(beat(-1, 2))->value == -250000);

    const auto boundaries = map->boundaries();
    REQUIRE(boundaries.anchors.size() == 2);
    CHECK(boundaries.anchors[0].beat == RationalBeat::zero());
    CHECK(boundaries.anchors[0].at.value == 0);
    CHECK(boundaries.anchors[1].beat == beat(3, 2));
    CHECK(boundaries.anchors[1].at.value == 750000);
    CHECK(boundaries.tempos.empty());
    CHECK(boundaries.stops.empty());
}

TEST_CASE("timing keeps a Stop half open and reports its end outside", "[timing][stop]") {
    SourceFixture fixture;
    fixture.stops.push_back(Stop{RationalBeat::zero(), 20.0});
    auto map = TimingMap::compile(fixture.view(), {});
    REQUIRE(map);

    CHECK(map->beatToTime(RationalBeat::zero())->value == 0);
    CHECK(map->beatToTime(beat(1, 2))->value == 270000);
    CHECK(map->beatToTime(beat(1, 1))->value == 520000);

    const auto boundaries = map->boundaries();
    REQUIRE(boundaries.stops.size() == 1);
    CHECK(boundaries.stops[0].start.value == 0);
    CHECK(boundaries.stops[0].end.value == 20000);

    const auto inside = map->timeToBeat(TimeUs{9999});
    REQUIRE(inside);
    CHECK(inside->inStop);
    REQUIRE(inside->stopBeat.has_value());
    CHECK(*inside->stopBeat == RationalBeat::zero());
    CHECK(inside->stopProgress > 0.49);
    CHECK(inside->stopProgress < 0.50);

    const auto end = map->timeToBeat(TimeUs{20000});
    REQUIRE(end);
    CHECK_FALSE(end->inStop);
    REQUIRE(end->stopBeat.has_value());
    CHECK(*end->stopBeat == RationalBeat::zero());
    CHECK(end->stopProgress == 0.0);
    CHECK(end->beat == 0.0);

    const auto after = map->timeToBeat(TimeUs{20001});
    REQUIRE(after);
    CHECK_FALSE(after->inStop);
    CHECK_FALSE(after->stopBeat.has_value());
    CHECK(after->beat > 0.0);
    CHECK(after->beat < 0.00001);

    const auto reverse = map->timeToBeat(TimeUs{270000});
    REQUIRE(reverse);
    CHECK_FALSE(reverse->inStop);
    CHECK(std::abs(reverse->beat - 0.5) < 1.0e-9);
}

TEST_CASE("timing tempo boundaries and anchors share the discrete table", "[timing][tempo]") {
    SourceFixture fixture;
    fixture.tempos.push_back(TempoEvent{beat(2, 1), beat(2, 1), 60.0, 60.0, 0.0, 0.0});
    auto map = TimingMap::compile(fixture.view(), {});
    REQUIRE(map);
    CHECK(map->beatToTime(beat(2, 1))->value == 1000000);
    CHECK(map->beatToTime(beat(3, 1))->value == 2000000);
    CHECK(map->beatToTime(beat(4, 1))->value == 3000000);
    CHECK(map->beatToTime(beat(5, 1))->value == 4000000);

    const auto boundaries = map->boundaries();
    REQUIRE(boundaries.tempos.size() == 2);
    CHECK(boundaries.tempos[0].beat == beat(2, 1));
    CHECK(boundaries.tempos[0].at.value == 1000000);
    CHECK(boundaries.tempos[1].beat == beat(4, 1));
    CHECK(boundaries.tempos[1].at.value == 3000000);
    REQUIRE(boundaries.anchors.size() == 3);
}

TEST_CASE("timing matchesSource compares the normalized source values", "[timing][source]") {
    SourceFixture fixture;
    auto map = TimingMap::compile(fixture.view(), {});
    REQUIRE(map);
    CHECK(map->matchesSource(fixture.view()));

    auto changed = fixture;
    changed.defaultBpm = 121.0;
    CHECK_FALSE(map->matchesSource(changed.view()));

    SourceFixture withTempo;
    withTempo.tempos.push_back(TempoEvent{beat(1, 1), beat(1, 1), 120.0, 120.0, 0.0, 0.0});
    auto tempoMap = TimingMap::compile(withTempo.view(), {});
    REQUIRE(tempoMap);
    CHECK(tempoMap->matchesSource(withTempo.view()));
    auto reordered = withTempo;
    reordered.tempos.push_back(TempoEvent{beat(0, 1), beat(1, 2), 90.0, 90.0, 0.0, 0.0});
    CHECK_FALSE(tempoMap->matchesSource(reordered.view()));
}

TEST_CASE("timing fromCompiled rebuilds the same discrete map", "[timing][compiled]") {
    SourceFixture fixture;
    fixture.stops.push_back(Stop{RationalBeat::zero(), 20.0});
    const std::vector<RationalBeat> required{beat(1, 2), beat(7, 4)};
    auto compiled = TimingMap::compile(fixture.view(), required);
    REQUIRE(compiled);
    const auto boundaries = compiled->boundaries();
    auto rebuilt = TimingMap::fromCompiled(fixture.view(), boundaries, required);
    REQUIRE(rebuilt);
    CHECK(rebuilt->matchesSource(fixture.view()));
    CHECK(rebuilt->beatToTime(RationalBeat::zero())->value == 0);
    CHECK(rebuilt->beatToTime(beat(1, 2))->value == compiled->beatToTime(beat(1, 2))->value);
    CHECK(rebuilt->beatToTime(beat(2, 1))->value == compiled->beatToTime(beat(2, 1))->value);
    const auto inside = rebuilt->timeToBeat(TimeUs{5000});
    REQUIRE(inside);
    CHECK(inside->inStop);
}

namespace {

auto reasonOf(const cuexis::core::Error& error) -> std::string {
    for (const auto& context : error.context()) {
        if (context.key == "reason") {
            return context.value;
        }
    }
    return {};
}

} // namespace

TEST_CASE("timing fromCompiled rejects missing, regressed and inconsistent tables",
          "[timing][compiled][negative]") {
    SourceFixture fixture;
    fixture.stops.push_back(Stop{RationalBeat::zero(), 20.0});
    const std::vector<RationalBeat> required{beat(1, 2)};
    auto compiled = TimingMap::compile(fixture.view(), required);
    REQUIRE(compiled);
    const auto boundaries = compiled->boundaries();

    std::vector<cuexis::timing::AnchoredBeat> anchors(boundaries.anchors.begin(),
                                                      boundaries.anchors.end());
    REQUIRE(anchors.size() >= 2);

    auto missingAnchors = anchors;
    missingAnchors.pop_back();
    auto missing = TimingMap::fromCompiled(
        fixture.view(),
        cuexis::timing::TimingBoundariesView{boundaries.tempos, boundaries.stops, missingAnchors},
        required);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code() == "judgement.execution.relation_invalid");
    CHECK(reasonOf(missing.error()) == "anchor_key_mismatch");

    auto regressedAnchors = anchors;
    regressedAnchors[1].at = TimeUs{-1};
    auto regressed = TimingMap::fromCompiled(
        fixture.view(),
        cuexis::timing::TimingBoundariesView{boundaries.tempos, boundaries.stops, regressedAnchors},
        required);
    REQUIRE_FALSE(regressed);
    CHECK(reasonOf(regressed.error()) == "tick_regression");

    auto insideStopAnchors = anchors;
    REQUIRE(insideStopAnchors.size() >= 2);
    insideStopAnchors[1].at = TimeUs{10000};
    auto insideStop =
        TimingMap::fromCompiled(fixture.view(),
                                cuexis::timing::TimingBoundariesView{
                                    boundaries.tempos, boundaries.stops, insideStopAnchors},
                                required);
    REQUIRE_FALSE(insideStop);
    CHECK(reasonOf(insideStop.error()) == "stop_bounds");

    SourceFixture tempoFixture;
    tempoFixture.tempos.push_back(TempoEvent{beat(1, 1), beat(1, 1), 120.0, 120.0, 0.0, 0.0});
    auto tempoMap = TimingMap::compile(tempoFixture.view(), {});
    REQUIRE(tempoMap);
    const auto tempoBoundaries = tempoMap->boundaries();
    std::vector<cuexis::timing::TempoBoundary> tempos(tempoBoundaries.tempos.begin(),
                                                      tempoBoundaries.tempos.end());
    REQUIRE(!tempos.empty());
    tempos[0].at = TimeUs{tempos[0].at.value + 1};
    auto shifted =
        TimingMap::fromCompiled(tempoFixture.view(),
                                cuexis::timing::TimingBoundariesView{tempos, tempoBoundaries.stops,
                                                                     tempoBoundaries.anchors},
                                {});
    REQUIRE_FALSE(shifted);
    CHECK(reasonOf(shifted.error()) == "tick_mismatch");
}
