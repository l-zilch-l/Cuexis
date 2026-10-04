//  S7A-2 timebase golden tests (the exact-mapping half of the batch).
//
//  Every expected value below is a hard-coded number, never a relation such as "not equal" or
//  "greater than". The cases are organised by the rulings they pin down:
//
//    1. the exact RationalBeat -> judgementTick mapping (Spec 3.7.2 items 1-3): constant tempo,
//       tempo change, tempo section before the origin, the stop collapse model (a frozen interval
//       plus one jump at endBeat), a stop over the origin, round-half-to-even at both signs,
//       negative beats;
//    2. stable overflow rejection (Spec 3.7.1 item 4) for tick addition, subtraction, interval
//       construction and the mapping itself;
//    3. the canonical same-tick order (tick, originKind, canonicalOrdinal), its invariance under
//    the
//       arrival order of the input sequence, and the collapse-tick collision that order exists for
//       (Spec 3.7.2 item 4);
//    4. validatePrepare's stable rejection of unmeasured late-policy parameters, of a profile that
//       never declared its tempo, and of invalid profiles (Spec 3.7.4 items 1-3, Spec 9.3);
//    5. commitTick window semantics (Spec 0.1, Spec 3.7.5);
//    6. the declared engine.tick.us.v1 unit binding (Spec 3.7.1 items 2-3), including its explicit
//       absent-tempo state and the absence of any implicit zero rational or default origin kind.
//
//  The stop model under test is the round 2 ruling's collapse model: inside [startBeat, endBeat)
//  the mapping is frozen at the exact accumulated time of startBeat (internal rate 0), and the
//  declared duration is added once, at endBeat. The endpoints therefore agree with the rejected
//  proportional model and only the interval interior differs.
//
//  The numeric late-policy values used in the fixtures are test-local presence probes. Their
//  magnitude is deliberately meaningless: the S7A-2 ruling leaves every late-policy number to a
//  later batch, and validatePrepare is only allowed to check that a parameter was measured, not
//  what it measured.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/timebase.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

namespace judgement = cuexis::judgement;

using judgement::CanonicalOrdinal;
using judgement::CommitTick;
using judgement::CommitWindow;
using judgement::JudgementTick;
using judgement::LateEventPolicy;
using judgement::LatePolicyParameters;
using judgement::MeasuredParameter;
using judgement::OriginKind;
using judgement::RationalBeat;
using judgement::RationalDuration;
using judgement::Tick;
using judgement::TickCollisionKey;
using judgement::TickSpan;
using judgement::TimebaseProfile;

constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();

//  -------------------------------------------------------------------------------------------
//  Fixture construction
//  -------------------------------------------------------------------------------------------

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator) -> RationalBeat {
    const auto value = RationalBeat::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto duration(std::int64_t numerator, std::int64_t denominator) -> RationalDuration {
    const auto value = RationalDuration::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

//  The declared engine.tick.us.v1 binding with one explicit positive tempo and no sections.
[[nodiscard]] auto plainProfile(RationalDuration tempo) -> TimebaseProfile {
    TimebaseProfile profile = judgement::makeMicrosecondTimebase();
    profile.initialTempo = tempo;
    return profile;
}

//  All four late-policy parameters measured and a policy declared. The numbers are presence probes.
[[nodiscard]] auto measuredPolicy() -> LatePolicyParameters {
    LatePolicyParameters parameters;
    parameters.finalizationWatermark = MeasuredParameter<TickSpan>::measured(TickSpan{0});
    parameters.maxQueueHop = MeasuredParameter<TickSpan>::measured(TickSpan{1});
    parameters.windowCloseThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{2});
    parameters.windowOpenThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{3});
    parameters.policy = LateEventPolicy::rejectLate;
    return parameters;
}

[[nodiscard]] auto contextValue(const cuexis::core::Error& error, std::string_view key)
    -> std::string {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

//  -------------------------------------------------------------------------------------------
//  Mapping helpers
//  -------------------------------------------------------------------------------------------

struct BeatCase final {
    std::int64_t numerator;
    std::int64_t denominator;
    std::int64_t expectedTick;
};

void checkMapping(const TimebaseProfile& profile, const std::vector<BeatCase>& cases) {
    for (const BeatCase& item : cases) {
        CAPTURE(item.numerator, item.denominator);
        const auto result =
            judgement::mapBeatToTick(profile, beat(item.numerator, item.denominator));
        REQUIRE(result.has_value());
        CHECK(result->tick().value() == item.expectedTick);
    }
}

//  Pins the negative symmetry of the mapping. The precondition is stated because it is a property
//  of the rate profile as well as of the rounding rule: the mapping is odd about the origin only
//  when the accumulated time is odd about it. A tempo change is not, and a stop is not either under
//  the collapse model (its jump is attached to endBeat, and that half-open choice does not mirror),
//  so those profiles carry their own explicit negative expectations in the table instead of being
//  mirrored here. This helper is therefore only called for a constant tempo at origin beat 0.
void checkMirrorSymmetry(const TimebaseProfile& profile, const std::vector<BeatCase>& cases) {
    for (const BeatCase& item : cases) {
        if (item.numerator == 0) {
            continue;
        }
        const auto positive =
            judgement::mapBeatToTick(profile, beat(item.numerator, item.denominator));
        const auto negative =
            judgement::mapBeatToTick(profile, beat(-item.numerator, item.denominator));
        CAPTURE(item.numerator, item.denominator, item.expectedTick);
        REQUIRE(positive.has_value());
        REQUIRE(negative.has_value());
        CHECK(negative->tick().value() == -positive->tick().value());
    }
}

struct KeyCase final {
    std::int64_t tick;
    OriginKind originKind;
    std::uint64_t canonicalOrdinal;
};

[[nodiscard]] auto key(const KeyCase& item) -> TickCollisionKey {
    //  The origin kind is a required constructor argument: a key cannot be built by omitting it,
    //  because no member of TickCollisionKey has a default value.
    return TickCollisionKey{Tick{item.tick}, item.originKind,
                            CanonicalOrdinal{item.canonicalOrdinal}};
}

//  Sorts a set of canonical keys by the canonical order alone, whatever arrival order it was given
//  in. This is the property Spec 3.7.2 item 4 protects: the result depends on the keys, never on
//  the sequence the caller happened to hand over.
[[nodiscard]] auto canonicalSort(const std::vector<KeyCase>& arrivalOrder)
    -> std::vector<TickCollisionKey> {
    std::vector<TickCollisionKey> keys;
    keys.reserve(arrivalOrder.size());
    for (const KeyCase& item : arrivalOrder) {
        keys.push_back(key(item));
    }
    std::sort(keys.begin(), keys.end(), judgement::canonicalOrder);
    return keys;
}

} // namespace

//  ---------------------------------------------------------------------------------------------
//  1. Exact mapping
//  ---------------------------------------------------------------------------------------------

TEST_CASE("timebase: beat mapping under a constant tempo", "[judgement][timebase][s7a2]") {
    //  initialTempo = 500000/1 microseconds per beat; origin beat = 0.
    const TimebaseProfile profile = plainProfile(duration(500000, 1));
    const std::vector<BeatCase> cases{
        {0, 1, 0},        {1, 1, 500000},   {1, 2, 250000},    {3, 2, 750000},
        {2, 1, 1000000},  {1, 3, 166667},   {7, 3, 1166667},   {-1, 1, -500000},
        {-3, 2, -750000}, {-1, 3, -166667}, {-7, 3, -1166667},
    };
    checkMapping(profile, cases);
    checkMirrorSymmetry(profile, cases);
}

TEST_CASE("timebase: round half to even with negative symmetry", "[judgement][timebase][s7a2]") {
    //  initialTempo = 1/1, so the exact tick value equals the beat and every half lands on a tie.
    const TimebaseProfile profile = plainProfile(duration(1, 1));
    const std::vector<BeatCase> cases{
        {0, 1, 0},
        //  Positive ties: the even neighbour wins, so 3/2 -> 2 and 5/2 -> 2, not 1 and 3.
        {1, 2, 0},
        {3, 2, 2},
        {5, 2, 2},
        {7, 2, 4},
        {9, 2, 4},
        //  Negative ties: the same even side, mirrored, so the rule is symmetric.
        {-1, 2, 0},
        {-3, 2, -2},
        {-5, 2, -2},
        {-7, 2, -4},
        //  Non-tie values on both signs.
        {1, 1, 1},
        {-1, 1, -1},
        {1, 3, 0},
        {2, 3, 1},
        {4, 3, 1},
        {5, 3, 2},
        {-1, 3, 0},
        {-2, 3, -1},
        {-4, 3, -1},
        {-5, 3, -2},
    };
    checkMapping(profile, cases);
    checkMirrorSymmetry(profile, cases);

    SECTION("a collapsed stop is not odd about the origin, so both signs are written out") {
        //  Two mirrored stops, each freezing 2 beats and adding 4 ticks: [-2, 0) and [0, 2), with
        //  initialTempo 1/1. The rejected proportional model made this rate profile even about the
        //  origin, so the mapping was odd; the collapse model is not, because the jump belongs to
        //  endBeat and that half-open choice does not mirror. The values are therefore pinned
        //  explicitly on both signs instead of being derived by mirroring.
        //
        //  Derivation (positive side): inside [0, 2) the rate is 0 and no stop ends, so the
        //  accumulated time is exactly 0 and tick(b) = 0 for b in [0, 2); tick(2) = 0 + 4 = 4
        //  (the jump at endBeat 2 is in (0, 2]); tick(5/2) = 1/2 + 4 = 4.5 -> ties to even -> 4;
        //  tick(3) = 1 + 4 = 5.
        //  Derivation (negative side): for b in (-2, 0) the rate is 0 but endBeat 0 lies in
        //  (b, 0], so the accumulated time is 0 + 4 and tick(b) = -4, including at b = -2 where the
        //  integral is still 0. For b = -5/2 the integral is 1/2 (one half beat of tempo before the
        //  stop starts at -2) plus the same jump 4, so the exact value is 4.5 and the tie rounds to
        //  the even side, tick(-5/2) = -4. For b = -3 the integral is 1 and the jump 4 gives -5.
        TimebaseProfile mirrored = plainProfile(duration(1, 1));
        mirrored.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(-2, 1), .endBeat = beat(0, 1), .duration = duration(4, 1)});
        mirrored.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(0, 1), .endBeat = beat(2, 1), .duration = duration(4, 1)});
        const std::vector<BeatCase> mirroredCases{
            {1, 2, 0},   {1, 1, 0},   {3, 2, 0},   {2, 1, 4},   {5, 2, 4},   {3, 1, 5},
            {-1, 2, -4}, {-1, 1, -4}, {-3, 2, -4}, {-2, 1, -4}, {-5, 2, -4}, {-3, 1, -5},
        };
        checkMapping(mirrored, mirroredCases);
    }
}

TEST_CASE("timebase: a tempo change only changes the beat to tick ratio",
          "[judgement][timebase][s7a2]") {
    //  initialTempo 100/1 before beat 2; 200/1 from beat 2 onwards. The rate is not even about the
    //  origin, so the negative side is written out explicitly: a beat before the origin only
    //  integrates the rates that lie between it and the origin.
    TimebaseProfile profile = plainProfile(duration(100, 1));
    profile.tempoSections.push_back(
        judgement::TempoSection{.startBeat = beat(2, 1), .durationPerBeat = duration(200, 1)});

    const std::vector<BeatCase> cases{
        {0, 1, 0},   {1, 1, 100},   {3, 2, 150},   {2, 1, 200},   {5, 2, 300},   {3, 1, 400},
        {7, 2, 500}, {-1, 1, -100}, {-3, 2, -150}, {-5, 2, -250}, {-3, 1, -300},
    };
    checkMapping(profile, cases);
}

TEST_CASE("timebase: origin beat and sections before the origin", "[judgement][timebase][s7a2]") {
    SECTION("origin beat after the first section") {
        //  origin beat 1, initialTempo 100/1, 200/1 from beat 2 onwards.
        TimebaseProfile profile = plainProfile(duration(100, 1));
        profile.originBeat = beat(1, 1);
        profile.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(2, 1), .durationPerBeat = duration(200, 1)});
        const std::vector<BeatCase> cases{
            {1, 1, 0},   {3, 2, 50},  {2, 1, 100},  {3, 1, 300},
            {4, 1, 500}, {1, 2, -50}, {0, 1, -100}, {-1, 1, -200},
        };
        checkMapping(profile, cases);
    }

    SECTION("a section declared before the origin still sets the tempo in force there") {
        //  origin beat 2, initialTempo 100/1, 50/1 from beat 1, 200/1 from beat 3.
        TimebaseProfile profile = plainProfile(duration(100, 1));
        profile.originBeat = beat(2, 1);
        profile.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(1, 1), .durationPerBeat = duration(50, 1)});
        profile.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(3, 1), .durationPerBeat = duration(200, 1)});
        const std::vector<BeatCase> cases{
            {2, 1, 0},   {5, 2, 25},  {3, 1, 50},   {7, 2, 150},
            {4, 1, 250}, {1, 1, -50}, {0, 1, -150}, {5, 1, 450},
        };
        checkMapping(profile, cases);
    }
}

TEST_CASE("timebase: a stop freezes its interval and adds its duration at endBeat",
          "[judgement][timebase][s7a2]") {
    SECTION("stop after the origin") {
        //  initialTempo 100/1; the stop [2, 4) freezes the mapping and declares 1000 ticks.
        //
        //  The accumulated time at a beat b is the exact integral of the rate from the origin, plus
        //  every stop duration whose endBeat lies in (origin, b]. Inside [2, 4) the rate is 0, so
        //  the integral is 100 * 2 = 200 for every b in that interval and the interval maps onto
        //  the single tick 200 (the "non-injective interior" the ruling accepts). At b = 4 the same
        //  integral 200 gets the jump 1000 once, so tick(4) = 1200 = tick(2) + 1000. At b = 5 the
        //  integral is 100 * 2 + 0 * 2 + 100 * 1 = 300 and the jump is still inside (0, 5], so
        //  tick(5) = 1300. The endpoints are the same numbers the proportional model produced; only
        //  2.5, 3 and 3.5 changed from 450, 700 and 950 to the frozen 200.
        TimebaseProfile profile = plainProfile(duration(100, 1));
        profile.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(4, 1), .duration = duration(1000, 1)});
        const std::vector<BeatCase> cases{
            {1, 1, 100},       {2, 1, 200},   {5, 2, 200},   {3, 1, 200},   {7, 2, 200},
            {3999, 1000, 200}, {4, 1, 1200},  {9, 2, 1250},  {5, 1, 1300},  {6, 1, 1400},
            {1, 2, 50},        {-1, 1, -100}, {-5, 2, -250}, {-3, 1, -300}, {-9, 2, -450},
        };
        checkMapping(profile, cases);
    }

    SECTION("a stop on the negative side of the origin uses the same rule") {
        //  initialTempo 1/1; the stop [-4, -2) freezes the mapping and declares 10 ticks.
        //
        //  Derivation. For a beat b left of the origin the accumulated time is the negated integral
        //  of the rate over [b, 0) plus the stops whose endBeat lies in (b, 0], and the negation
        //  happens in the exact domain before the single rounding step. Inside [-4, -2) the rate is
        //  0, so the integral from any b in [-4, -2) to 0 is 0 * 2 + 1 * 2 = 2; endBeat -2 is in
        //  (b, 0] for every such b, so the jump 10 is added as well and tick(b) = -(2 + 10) = -12.
        //  At b = -2 exactly the integral is still 2 but the jump at -2 is no longer strictly after
        //  b, so tick(-2) = -2, which is exactly tick(-4) + 10. At b = -5 the integral is
        //  1 + 0 * 2 + 1 * 2 = 3 and the jump is still inside (-5, 0], so tick(-5) = -13.
        //  The rejected proportional model gave -7, -12 and -13 here; only the interior moved.
        TimebaseProfile profile = plainProfile(duration(1, 1));
        profile.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(-4, 1), .endBeat = beat(-2, 1), .duration = duration(10, 1)});
        const std::vector<BeatCase> cases{
            {1, 1, 1},    {3, 1, 3},    {-1, 1, -1},  {-2, 1, -2},  {-3, 1, -12},
            {-7, 2, -12}, {-4, 1, -12}, {-9, 2, -12}, {-5, 1, -13}, {-3, 2, -2},
        };
        checkMapping(profile, cases);
    }

    SECTION("the frozen interior and the one jump after it") {
        //  initialTempo 1/1; [1, 3) freezes the mapping at the exact accumulated time 1 and adds
        //  its 3 ticks at beat 3. Every beat of [1, 3) maps to tick 1, beat 3 maps to 4 = 1 + 3,
        //  and the tie at 7/2 is now a tie of the *post-stop* value: 1 + 1/2 + 3 = 4.5 -> even side
        //  -> 4.
        TimebaseProfile profile = plainProfile(duration(1, 1));
        profile.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(1, 1), .endBeat = beat(3, 1), .duration = duration(3, 1)});
        const std::vector<BeatCase> cases{
            {0, 1, 0}, {1, 2, 0}, {1, 1, 1},  {2, 1, 1},   {5, 2, 1},   {3, 1, 4},
            {7, 2, 4}, {4, 1, 5}, {-1, 2, 0}, {-2, 1, -2}, {-5, 2, -2}, {-4, 1, -4},
        };
        checkMapping(profile, cases);
    }

    SECTION("the stop duration joins the exact value before the single rounding step") {
        //  initialTempo 1/1; [1/2, 3) freezes the mapping at the exact accumulated time 1/2 and
        //  adds 3 ticks at beat 3. The frozen interior therefore rounds 1/2 to 0 (ties to even),
        //  and tick(3) rounds the exact value 1/2 + 3 = 7/2 to 4. Rounding the frozen tick first
        //  and then adding the duration would give 0 + 3 = 3 instead; the model adds the jump to
        //  the exact accumulated time and rounds once, the same way tempo and negative beats are
        //  rounded, so 4 is the value under test.
        TimebaseProfile profile = plainProfile(duration(1, 1));
        profile.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(1, 2), .endBeat = beat(3, 1), .duration = duration(3, 1)});
        const std::vector<BeatCase> cases{
            {0, 1, 0}, {1, 2, 0}, {1, 1, 0}, {5, 2, 0}, {3, 1, 4}, {4, 1, 4}, {5, 1, 6},
        };
        checkMapping(profile, cases);
    }

    SECTION("stop starting at the origin beat") {
        //  [0, 2) freezes the mapping at tick 0 and adds its 7 ticks at beat 2. A beat before the
        //  origin lies left of the stop and the jump at beat 2 is not in (beat, 0], so it
        //  integrates the initial tempo of 1/beat instead of reaching the stop.
        TimebaseProfile profile = plainProfile(duration(1, 1));
        profile.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(0, 1), .endBeat = beat(2, 1), .duration = duration(7, 1)});
        const std::vector<BeatCase> cases{
            {0, 1, 0}, {1, 1, 0},   {3, 2, 0},   {1999, 1000, 0}, {2, 1, 7},
            {3, 1, 8}, {-1, 1, -1}, {-3, 2, -2}, {-2, 1, -2},
        };
        checkMapping(profile, cases);
    }
}

//  ---------------------------------------------------------------------------------------------
//  2. Stable overflow rejection
//  ---------------------------------------------------------------------------------------------

TEST_CASE("timebase: tick addition rejects overflow stably", "[judgement][timebase][s7a2]") {
    SECTION("accepted boundaries") {
        const auto minimum = judgement::offsetTicks(Tick{kInt64Min}, TickSpan{0});
        REQUIRE(minimum.has_value());
        CHECK(minimum->value() == kInt64Min);

        const auto maximum = judgement::offsetTicks(Tick{kInt64Max}, TickSpan{0});
        REQUIRE(maximum.has_value());
        CHECK(maximum->value() == kInt64Max);

        const auto lastStep = judgement::offsetTicks(Tick{kInt64Max - 1}, TickSpan{1});
        REQUIRE(lastStep.has_value());
        CHECK(lastStep->value() == kInt64Max);

        const auto ordinary = judgement::offsetTicks(Tick{123}, TickSpan{-23});
        REQUIRE(ordinary.has_value());
        CHECK(ordinary->value() == 100);
    }

    SECTION("rejected displacements") {
        const auto upward = judgement::offsetTicks(Tick{kInt64Max}, TickSpan{1});
        REQUIRE_FALSE(upward.has_value());
        CHECK(upward.error().code() == "judgement.s7a2.timebase.tick_overflow");
        CHECK(contextValue(upward.error(), "category") == "budget_exceeded");
        CHECK(contextValue(upward.error(), "severity") == "error");
        CHECK(contextValue(upward.error(), "faulted") == "false");
        CHECK(contextValue(upward.error(), "field.section") == "judgement.timebase");
        CHECK(contextValue(upward.error(), "field.path") == "tick");

        const auto downward = judgement::offsetTicks(Tick{kInt64Min}, TickSpan{-1});
        REQUIRE_FALSE(downward.has_value());
        CHECK(downward.error().code() == "judgement.s7a2.timebase.tick_overflow");
        CHECK(contextValue(downward.error(), "category") == "budget_exceeded");
    }
}

TEST_CASE("timebase: tick difference rejects only a real overflow", "[judgement][timebase][s7a2]") {
    SECTION("difference inside the frozen range, including INT64_MIN operands") {
        const auto zero = judgement::differenceTicks(Tick{kInt64Min}, Tick{kInt64Min});
        REQUIRE(zero.has_value());
        CHECK(zero->value() == 0);

        const auto minimum = judgement::differenceTicks(Tick{kInt64Min}, Tick{0});
        REQUIRE(minimum.has_value());
        CHECK(minimum->value() == kInt64Min);

        const auto maximum = judgement::differenceTicks(Tick{-1}, Tick{kInt64Min});
        REQUIRE(maximum.has_value());
        CHECK(maximum->value() == kInt64Max);

        const auto ordinary = judgement::differenceTicks(Tick{5}, Tick{9});
        REQUIRE(ordinary.has_value());
        CHECK(ordinary->value() == -4);
    }

    SECTION("rejected differences") {
        const auto beyondMaximum = judgement::differenceTicks(Tick{kInt64Max}, Tick{-1});
        REQUIRE_FALSE(beyondMaximum.has_value());
        CHECK(beyondMaximum.error().code() == "judgement.s7a2.timebase.tick_overflow");
        CHECK(contextValue(beyondMaximum.error(), "category") == "budget_exceeded");

        const auto beyondMinimum = judgement::differenceTicks(Tick{kInt64Min}, Tick{1});
        REQUIRE_FALSE(beyondMinimum.has_value());
        CHECK(beyondMinimum.error().code() == "judgement.s7a2.timebase.tick_overflow");

        const auto negation = judgement::differenceTicks(Tick{0}, Tick{kInt64Min});
        REQUIRE_FALSE(negation.has_value());
        CHECK(negation.error().code() == "judgement.s7a2.timebase.tick_overflow");
    }
}

TEST_CASE("timebase: interval construction rejects a reversed end", "[judgement][timebase][s7a2]") {
    SECTION("accepted intervals") {
        const auto empty = judgement::makeInterval(Tick{5}, Tick{5});
        REQUIRE(empty.has_value());
        CHECK(empty->start == Tick{5});
        CHECK(empty->end == Tick{5});

        const auto spanning = judgement::makeInterval(Tick{-2}, Tick{3});
        REQUIRE(spanning.has_value());
        CHECK(spanning->start == Tick{-2});
        CHECK(spanning->end == Tick{3});
    }

    SECTION("rejected interval") {
        const auto reversed = judgement::makeInterval(Tick{5}, Tick{4});
        REQUIRE_FALSE(reversed.has_value());
        CHECK(reversed.error().code() == "judgement.s7a2.timebase.interval_reversed");
        CHECK(contextValue(reversed.error(), "category") == "invalid_relation");
        CHECK(contextValue(reversed.error(), "field.path") == "interval");
    }
}

TEST_CASE("timebase: the mapping rejects a value it cannot hold exactly",
          "[judgement][timebase][s7a2]") {
    SECTION("an exactly representable extreme is accepted") {
        const TimebaseProfile profile = plainProfile(duration(kInt64Max, 1));
        const auto maximum = judgement::mapBeatToTick(profile, beat(1, 1));
        REQUIRE(maximum.has_value());
        CHECK(maximum->tick().value() == kInt64Max);

        const auto minimum = judgement::mapBeatToTick(profile, beat(-1, 1));
        REQUIRE(minimum.has_value());
        CHECK(minimum->tick().value() == -kInt64Max);
    }

    SECTION("intermediate products that leave the range are rejected") {
        const TimebaseProfile profile = plainProfile(duration(kInt64Max, 1));

        const auto doubled = judgement::mapBeatToTick(profile, beat(2, 1));
        REQUIRE_FALSE(doubled.has_value());
        CHECK(doubled.error().code() == "judgement.s7a2.timebase.tick_overflow");
        CHECK(contextValue(doubled.error(), "category") == "budget_exceeded");
        CHECK(contextValue(doubled.error(), "field.section") == "judgement.timebase");
        CHECK(contextValue(doubled.error(), "field.path") == "beat");

        const auto halved = judgement::mapBeatToTick(profile, beat(3, 2));
        REQUIRE_FALSE(halved.has_value());
        CHECK(halved.error().code() == "judgement.s7a2.timebase.tick_overflow");

        const TimebaseProfile doubledTempo = plainProfile(duration(2, 1));
        const auto far = judgement::mapBeatToTick(doubledTempo, beat(kInt64Max, 1));
        REQUIRE_FALSE(far.has_value());
        CHECK(far.error().code() == "judgement.s7a2.timebase.tick_overflow");
    }

    SECTION("an invalid profile is rejected before any division happens") {
        TimebaseProfile noScale = plainProfile(duration(1000, 1));
        noScale.tickScale = duration(0, 1);
        const auto rejected = judgement::mapBeatToTick(noScale, beat(1, 1));
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "judgement.s7a2.timebase.profile_value_out_of_range");
        CHECK(contextValue(rejected.error(), "category") == "budget_exceeded");
        CHECK(contextValue(rejected.error(), "field.path") == "tickScale");

        TimebaseProfile unordered = plainProfile(duration(1000, 1));
        unordered.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(4, 1), .durationPerBeat = duration(1, 1)});
        unordered.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(2, 1), .durationPerBeat = duration(1, 1)});
        const auto unorderedResult = judgement::mapBeatToTick(unordered, beat(3, 1));
        REQUIRE_FALSE(unorderedResult.has_value());
        CHECK(unorderedResult.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(unorderedResult.error(), "category") == "invalid_relation");
    }
}

//  ---------------------------------------------------------------------------------------------
//  3. Canonical same-tick order
//  ---------------------------------------------------------------------------------------------

TEST_CASE("timebase: canonical order is the key, never the arrival order",
          "[judgement][timebase][s7a2]") {
    const std::vector<KeyCase> canonical{
        {10, OriginKind::observation, 0},  {10, OriginKind::observation, 3},
        {10, OriginKind::timer, 0},        {10, OriginKind::coordination, 1},
        {10, OriginKind::coordination, 2}, {10, OriginKind::correction, 0},
        {11, OriginKind::observation, 0},
    };
    const std::vector<TickCollisionKey> expected = canonicalSort(canonical);

    SECTION("the declared origin kind priority is observation, timer, coordination, correction") {
        CHECK(judgement::canonicalOrder(key({10, OriginKind::observation, 99}),
                                        key({10, OriginKind::timer, 0})));
        CHECK(judgement::canonicalOrder(key({10, OriginKind::timer, 99}),
                                        key({10, OriginKind::coordination, 0})));
        CHECK(judgement::canonicalOrder(key({10, OriginKind::coordination, 99}),
                                        key({10, OriginKind::correction, 0})));
        CHECK_FALSE(judgement::canonicalOrder(key({10, OriginKind::correction, 0}),
                                              key({10, OriginKind::observation, 99})));
        //  Always a strict order, never an equivalence.
        CHECK_FALSE(judgement::canonicalOrder(key({10, OriginKind::timer, 4}),
                                              key({10, OriginKind::timer, 4})));
    }

    SECTION("the tick dominates the origin kind and the ordinal") {
        CHECK(judgement::canonicalOrder(key({10, OriginKind::correction, 9999}),
                                        key({11, OriginKind::observation, 0})));
        CHECK_FALSE(judgement::canonicalOrder(key({11, OriginKind::observation, 0}),
                                              key({10, OriginKind::correction, 9999})));
    }

    SECTION("the ordinal breaks a tie inside one tick and one origin kind") {
        CHECK(judgement::canonicalOrder(key({10, OriginKind::observation, 3}),
                                        key({10, OriginKind::observation, 4})));
        CHECK_FALSE(judgement::canonicalOrder(key({10, OriginKind::observation, 4}),
                                              key({10, OriginKind::observation, 3})));
    }

    SECTION("every arrival order of the same set produces the same canonical sequence") {
        const std::vector<KeyCase> reversed(canonical.rbegin(), canonical.rend());
        const std::vector<KeyCase> interleaved{
            canonical[3], canonical[6], canonical[0], canonical[5],
            canonical[1], canonical[4], canonical[2],
        };
        const std::vector<KeyCase> rotated{
            canonical[4], canonical[5], canonical[6], canonical[0],
            canonical[1], canonical[2], canonical[3],
        };
        CHECK(canonicalSort(reversed) == expected);
        CHECK(canonicalSort(interleaved) == expected);
        CHECK(canonicalSort(rotated) == expected);
    }

    SECTION("the canonical sequence is pinned to concrete keys") {
        REQUIRE(expected.size() == canonical.size());
        CHECK(expected[0] == key({10, OriginKind::observation, 0}));
        CHECK(expected[1] == key({10, OriginKind::observation, 3}));
        CHECK(expected[2] == key({10, OriginKind::timer, 0}));
        CHECK(expected[3] == key({10, OriginKind::coordination, 1}));
        CHECK(expected[4] == key({10, OriginKind::coordination, 2}));
        CHECK(expected[5] == key({10, OriginKind::correction, 0}));
        CHECK(expected[6] == key({11, OriginKind::observation, 0}));
    }
}

//  The risk item of the collapse model: several distinct beats inside one stop map onto one tick,
//  so a set of collided items has to be ordered by the canonical key alone. The keys carry no beat,
//  and the expected sequence is never derived from the order the items were produced in.
TEST_CASE("timebase: beats collapsed onto one stop tick keep the canonical tie order",
          "[judgement][timebase][s7a2]") {
    //  initialTempo 100/1 with a stop that freezes [2, 4) and adds 1000 ticks at beat 4.
    TimebaseProfile profile = plainProfile(duration(100, 1));
    profile.stopSections.push_back(judgement::StopSection{
        .startBeat = beat(2, 1), .endBeat = beat(4, 1), .duration = duration(1000, 1)});

    SECTION("six distinct beats inside the stop collapse onto judgementTick 200") {
        const std::vector<std::pair<std::int64_t, std::int64_t>> collapsed{
            {2, 1}, {5, 2}, {3, 1}, {7, 2}, {3999, 1000}, {3333, 1000},
        };
        for (const auto& item : collapsed) {
            CAPTURE(item.first, item.second);
            const auto mapped = judgement::mapBeatToTick(profile, beat(item.first, item.second));
            REQUIRE(mapped.has_value());
            CHECK(mapped->tick() == Tick{200});
        }
    }

    SECTION("the canonical ordinal decides the order inside the collided tick") {
        const std::vector<KeyCase> ordinalOrder{
            {200, OriginKind::observation, 0},
            {200, OriginKind::observation, 7},
            {200, OriginKind::observation, 9},
        };
        const std::vector<TickCollisionKey> expected = canonicalSort(ordinalOrder);
        //  Three arrival orders of the same three collided observations: the result is the same
        //  sequence every time, and that sequence is the canonical ordinal order 0, 7, 9 rather
        //  than the order the observations arrived in.
        const std::vector<KeyCase> rotated{ordinalOrder[2], ordinalOrder[0], ordinalOrder[1]};
        const std::vector<KeyCase> reversed(ordinalOrder.rbegin(), ordinalOrder.rend());
        CHECK(canonicalSort(rotated) == expected);
        CHECK(canonicalSort(reversed) == expected);
        REQUIRE(expected.size() == 3);
        CHECK(expected[0] == key({200, OriginKind::observation, 0}));
        CHECK(expected[1] == key({200, OriginKind::observation, 7}));
        CHECK(expected[2] == key({200, OriginKind::observation, 9}));
    }

    SECTION("the origin kind still dominates the ordinal at the collided tick") {
        CHECK(judgement::canonicalOrder(key({200, OriginKind::observation, 9999}),
                                        key({200, OriginKind::timer, 0})));
        CHECK(judgement::canonicalOrder(key({200, OriginKind::timer, 9999}),
                                        key({200, OriginKind::coordination, 0})));
        CHECK(judgement::canonicalOrder(key({200, OriginKind::coordination, 9999}),
                                        key({200, OriginKind::correction, 0})));
        CHECK_FALSE(judgement::canonicalOrder(key({200, OriginKind::correction, 0}),
                                              key({200, OriginKind::observation, 9999})));
    }

    SECTION("the tick that follows the jump is ordered after every collided item") {
        //  A fact at the post-stop tick 1200 sorts after every fact at the frozen tick 200, however
        //  high the collided origin kind and ordinal are.
        CHECK(judgement::canonicalOrder(key({200, OriginKind::correction, 9999}),
                                        key({1200, OriginKind::observation, 0})));
        CHECK_FALSE(judgement::canonicalOrder(key({1200, OriginKind::observation, 0}),
                                              key({200, OriginKind::correction, 9999})));
    }
}

//  ---------------------------------------------------------------------------------------------
//  4. validatePrepare
//  ---------------------------------------------------------------------------------------------

TEST_CASE("timebase: validatePrepare accepts a fully measured declaration",
          "[judgement][timebase][s7a2]") {
    const TimebaseProfile profile = plainProfile(duration(500000, 1));
    const auto accepted = judgement::validatePrepare(profile, measuredPolicy());
    CHECK(accepted.has_value());
}

TEST_CASE("timebase: validatePrepare rejects a policy that was never declared",
          "[judgement][timebase][s7a2]") {
    const TimebaseProfile profile = plainProfile(duration(500000, 1));
    LatePolicyParameters parameters = measuredPolicy();
    parameters.policy = std::nullopt;

    const auto rejected = judgement::validatePrepare(profile, parameters);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "judgement.s7a2.late.policy_undeclared");
    CHECK(contextValue(rejected.error(), "category") == "late_policy_incomplete");
    CHECK(contextValue(rejected.error(), "severity") == "error");
    CHECK(contextValue(rejected.error(), "faulted") == "false");
    CHECK(contextValue(rejected.error(), "field.section") == "judgement.timebase");
    CHECK(contextValue(rejected.error(), "field.path") == "latePolicy");
}

TEST_CASE("timebase: validatePrepare rejects every parameter left pending measurement",
          "[judgement][timebase][s7a2]") {
    const TimebaseProfile profile = plainProfile(duration(500000, 1));

    struct PendingCase final {
        MeasuredParameter<TickSpan> LatePolicyParameters::* member;
        std::string_view expectedPath;
    };
    const std::vector<PendingCase> cases{
        {&LatePolicyParameters::finalizationWatermark, "latePolicy.finalizationWatermark"},
        {&LatePolicyParameters::maxQueueHop, "latePolicy.maxQueueHop"},
        {&LatePolicyParameters::windowCloseThreshold, "latePolicy.windowCloseThreshold"},
        {&LatePolicyParameters::windowOpenThreshold, "latePolicy.windowOpenThreshold"},
    };

    for (const PendingCase& item : cases) {
        LatePolicyParameters parameters = measuredPolicy();
        parameters.*(item.member) = MeasuredParameter<TickSpan>::pendingMeasurement();
        CAPTURE(item.expectedPath);
        const auto rejected = judgement::validatePrepare(profile, parameters);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "judgement.s7a2.late.parameter_pending");
        CHECK(contextValue(rejected.error(), "category") == "late_policy_incomplete");
        CHECK(contextValue(rejected.error(), "field.path") == std::string{item.expectedPath});
    }
}

TEST_CASE("timebase: a default-constructed parameter bundle is entirely unmeasured",
          "[judgement][timebase][s7a2]") {
    const TimebaseProfile profile = plainProfile(duration(500000, 1));
    const LatePolicyParameters defaults;
    CHECK(defaults.finalizationWatermark.isPendingMeasurement());
    CHECK(defaults.maxQueueHop.isPendingMeasurement());
    CHECK(defaults.windowCloseThreshold.isPendingMeasurement());
    CHECK(defaults.windowOpenThreshold.isPendingMeasurement());
    CHECK_FALSE(defaults.policy.has_value());

    const auto rejected = judgement::validatePrepare(profile, defaults);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "judgement.s7a2.late.policy_undeclared");
    CHECK(contextValue(rejected.error(), "category") == "late_policy_incomplete");
}

TEST_CASE("timebase: validatePrepare rejects an invalid profile declaration",
          "[judgement][timebase][s7a2]") {
    const LatePolicyParameters parameters = measuredPolicy();

    SECTION("a missing profile identity or unit token is a structural rejection") {
        TimebaseProfile noIdentity = plainProfile(duration(500000, 1));
        noIdentity.profileId = {};
        const auto identityResult = judgement::validatePrepare(noIdentity, parameters);
        REQUIRE_FALSE(identityResult.has_value());
        CHECK(identityResult.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(identityResult.error(), "category") == "invalid_relation");
        CHECK(contextValue(identityResult.error(), "field.path") == "profileId");

        TimebaseProfile noUnit = plainProfile(duration(500000, 1));
        noUnit.unitToken = {};
        const auto unitResult = judgement::validatePrepare(noUnit, parameters);
        REQUIRE_FALSE(unitResult.has_value());
        CHECK(unitResult.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(unitResult.error(), "category") == "invalid_relation");
        CHECK(contextValue(unitResult.error(), "field.path") == "unitToken");
    }

    SECTION("a profile that never declared a tempo is a structural rejection") {
        //  makeMicrosecondTimebase names every declared field but deliberately leaves the tempo in
        //  the absent state. There is no value to range-check, so this is the declaration-side
        //  category (invalid_relation) on the initialTempo path, not a budget_exceeded value.
        const TimebaseProfile undeclared = judgement::makeMicrosecondTimebase();
        REQUIRE_FALSE(undeclared.initialTempo.has_value());

        const auto rejected = judgement::validatePrepare(undeclared, parameters);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(rejected.error(), "category") == "invalid_relation");
        CHECK(contextValue(rejected.error(), "severity") == "error");
        CHECK(contextValue(rejected.error(), "faulted") == "false");
        CHECK(contextValue(rejected.error(), "field.section") == "judgement.timebase");
        CHECK(contextValue(rejected.error(), "field.path") == "initialTempo");

        //  The mapper applies the same precondition, so an unvalidated profile cannot map either.
        const auto unmapped = judgement::mapBeatToTick(undeclared, beat(1, 1));
        REQUIRE_FALSE(unmapped.has_value());
        CHECK(unmapped.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(unmapped.error(), "category") == "invalid_relation");
        CHECK(contextValue(unmapped.error(), "field.path") == "initialTempo");
    }

    SECTION("a non-positive scale or tempo is a value rejection") {
        struct ValueCase final {
            RationalDuration scale;
            RationalDuration tempo;
            std::string_view expectedPath;
        };
        const std::vector<ValueCase> cases{
            {duration(0, 1), duration(500000, 1), "tickScale"},
            {duration(-1, 2), duration(500000, 1), "tickScale"},
            {duration(1, 1), duration(0, 1), "initialTempo"},
            {duration(1, 1), duration(-3, 4), "initialTempo"},
        };
        for (const ValueCase& item : cases) {
            TimebaseProfile profile = plainProfile(item.tempo);
            profile.tickScale = item.scale;
            CAPTURE(item.expectedPath);
            const auto rejected = judgement::validatePrepare(profile, parameters);
            REQUIRE_FALSE(rejected.has_value());
            CHECK(rejected.error().code() == "judgement.s7a2.timebase.profile_value_out_of_range");
            CHECK(contextValue(rejected.error(), "category") == "budget_exceeded");
            CHECK(contextValue(rejected.error(), "field.path") == std::string{item.expectedPath});
        }
    }

    SECTION("an unordered or duplicated tempo section is a structural rejection") {
        TimebaseProfile duplicated = plainProfile(duration(100, 1));
        duplicated.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(2, 1), .durationPerBeat = duration(200, 1)});
        duplicated.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(2, 1), .durationPerBeat = duration(300, 1)});
        const auto duplicateResult = judgement::validatePrepare(duplicated, parameters);
        REQUIRE_FALSE(duplicateResult.has_value());
        CHECK(duplicateResult.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(duplicateResult.error(), "field.path") == "tempoSections");

        TimebaseProfile unordered = plainProfile(duration(100, 1));
        unordered.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(4, 1), .durationPerBeat = duration(200, 1)});
        unordered.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(1, 1), .durationPerBeat = duration(300, 1)});
        const auto unorderedResult = judgement::validatePrepare(unordered, parameters);
        REQUIRE_FALSE(unorderedResult.has_value());
        CHECK(unorderedResult.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(unorderedResult.error(), "category") == "invalid_relation");
        CHECK(contextValue(unorderedResult.error(), "field.path") == "tempoSections");
    }

    SECTION("a non-positive tempo duration is a value rejection") {
        TimebaseProfile profile = plainProfile(duration(100, 1));
        profile.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(2, 1), .durationPerBeat = duration(0, 1)});
        const auto rejected = judgement::validatePrepare(profile, parameters);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "judgement.s7a2.timebase.profile_value_out_of_range");
        CHECK(contextValue(rejected.error(), "category") == "budget_exceeded");
        CHECK(contextValue(rejected.error(), "field.path") == "tempoSections");
    }

    SECTION("stop sections must be non-degenerate, ordered and non-overlapping") {
        TimebaseProfile degenerate = plainProfile(duration(100, 1));
        degenerate.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(2, 1), .duration = duration(10, 1)});
        const auto degenerateResult = judgement::validatePrepare(degenerate, parameters);
        REQUIRE_FALSE(degenerateResult.has_value());
        CHECK(degenerateResult.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(degenerateResult.error(), "category") == "invalid_relation");
        CHECK(contextValue(degenerateResult.error(), "field.path") == "stopSections");

        TimebaseProfile reversed = plainProfile(duration(100, 1));
        reversed.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(4, 1), .endBeat = beat(2, 1), .duration = duration(10, 1)});
        const auto reversedResult = judgement::validatePrepare(reversed, parameters);
        REQUIRE_FALSE(reversedResult.has_value());
        CHECK(reversedResult.error().code() == "judgement.s7a2.timebase.profile_invalid");

        TimebaseProfile overlapping = plainProfile(duration(100, 1));
        overlapping.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(6, 1), .duration = duration(10, 1)});
        overlapping.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(4, 1), .endBeat = beat(8, 1), .duration = duration(10, 1)});
        const auto overlappingResult = judgement::validatePrepare(overlapping, parameters);
        REQUIRE_FALSE(overlappingResult.has_value());
        CHECK(overlappingResult.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(overlappingResult.error(), "field.path") == "stopSections");

        TimebaseProfile unordered = plainProfile(duration(100, 1));
        unordered.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(6, 1), .endBeat = beat(8, 1), .duration = duration(10, 1)});
        unordered.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(4, 1), .duration = duration(10, 1)});
        const auto unorderedResult = judgement::validatePrepare(unordered, parameters);
        REQUIRE_FALSE(unorderedResult.has_value());
        CHECK(unorderedResult.error().code() == "judgement.s7a2.timebase.profile_invalid");

        TimebaseProfile nonPositive = plainProfile(duration(100, 1));
        nonPositive.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(4, 1), .duration = duration(0, 1)});
        const auto nonPositiveResult = judgement::validatePrepare(nonPositive, parameters);
        REQUIRE_FALSE(nonPositiveResult.has_value());
        CHECK(nonPositiveResult.error().code() ==
              "judgement.s7a2.timebase.profile_value_out_of_range");
        CHECK(contextValue(nonPositiveResult.error(), "category") == "budget_exceeded");
        CHECK(contextValue(nonPositiveResult.error(), "field.path") == "stopSections");
    }

    SECTION("a valid profile with measured parameters is accepted with sections present") {
        TimebaseProfile profile = plainProfile(duration(500000, 1));
        profile.tempoSections.push_back(judgement::TempoSection{
            .startBeat = beat(4, 1), .durationPerBeat = duration(250000, 1)});
        profile.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(8, 1), .endBeat = beat(9, 1), .duration = duration(100000, 1)});
        const auto accepted = judgement::validatePrepare(profile, parameters);
        CHECK(accepted.has_value());
    }
}

//  ---------------------------------------------------------------------------------------------
//  5. commitTick and the commit window
//  ---------------------------------------------------------------------------------------------

TEST_CASE("timebase: facts commit only at the window commit tick", "[judgement][timebase][s7a2]") {
    const CommitWindow window{
        .openTick = Tick{10}, .commitTick = CommitTick{Tick{12}}, .closeTick = Tick{20}};

    SECTION("the commit tick inside the window is accepted") {
        CHECK(judgement::commitAt(Tick{12}, window).has_value());
    }

    SECTION("every other tick is rejected") {
        const std::vector<std::int64_t> rejectedTicks{11, 10, 13, 20, 9, 21, 0, -5};
        for (const std::int64_t value : rejectedTicks) {
            CAPTURE(value);
            const auto rejected = judgement::commitAt(Tick{value}, window);
            REQUIRE_FALSE(rejected.has_value());
            CHECK(rejected.error().code() == "judgement.s7a2.timebase.commit_not_at_commit_tick");
            CHECK(contextValue(rejected.error(), "category") == "invalid_relation");
            CHECK(contextValue(rejected.error(), "severity") == "error");
            CHECK(contextValue(rejected.error(), "faulted") == "false");
            CHECK(contextValue(rejected.error(), "field.section") == "judgement.timebase");
            CHECK(contextValue(rejected.error(), "field.path") == "commitWindow");
        }
    }

    SECTION("a commit tick on a window boundary is accepted because the window is closed") {
        const CommitWindow atOpen{
            .openTick = Tick{10}, .commitTick = CommitTick{Tick{10}}, .closeTick = Tick{20}};
        CHECK(judgement::commitAt(Tick{10}, atOpen).has_value());
        CHECK_FALSE(judgement::commitAt(Tick{11}, atOpen).has_value());

        const CommitWindow atClose{
            .openTick = Tick{10}, .commitTick = CommitTick{Tick{20}}, .closeTick = Tick{20}};
        CHECK(judgement::commitAt(Tick{20}, atClose).has_value());
        CHECK_FALSE(judgement::commitAt(Tick{19}, atClose).has_value());
    }

    SECTION("a window whose boundaries contradict its commit tick is rejected as a window") {
        const std::vector<CommitWindow> malformed{
            {.openTick = Tick{20}, .commitTick = CommitTick{Tick{12}}, .closeTick = Tick{10}},
            {.openTick = Tick{10}, .commitTick = CommitTick{Tick{25}}, .closeTick = Tick{20}},
            {.openTick = Tick{10}, .commitTick = CommitTick{Tick{5}}, .closeTick = Tick{20}},
        };
        for (const CommitWindow& item : malformed) {
            CAPTURE(item.openTick.value(), item.commitTick.tick().value(), item.closeTick.value());
            const auto rejected = judgement::commitAt(item.commitTick.tick(), item);
            REQUIRE_FALSE(rejected.has_value());
            CHECK(rejected.error().code() == "judgement.s7a2.timebase.commit_window_invalid");
            CHECK(contextValue(rejected.error(), "category") == "invalid_relation");
            CHECK(contextValue(rejected.error(), "field.path") == "commitWindow");
        }
    }
}

//  ---------------------------------------------------------------------------------------------
//  6. The declared engine.tick.us.v1 unit binding
//  ---------------------------------------------------------------------------------------------

TEST_CASE("timebase: the microsecond profile declares its unit explicitly",
          "[judgement][timebase][s7a2]") {
    const TimebaseProfile profile = judgement::makeMicrosecondTimebase();

    CHECK(profile.profileId == "engine.tick.us.v1");
    CHECK(profile.unitToken == "us");
    CHECK(profile.tickScale.numerator() == 1);
    CHECK(profile.tickScale.denominator() == 1);
    CHECK(profile.originBeat.numerator() == 0);
    CHECK(profile.originBeat.denominator() == 1);
    CHECK(profile.tempoSections.empty());
    CHECK(profile.stopSections.empty());

    SECTION("the declaration carries no invented tempo, so it is not prepare-valid on its own") {
        //  Absent, not zero: there is no numeric tempo at all, so no consumer can read one the
        //  profile never declared. The rejection is a structural one (the declaration is
        //  incomplete), which is a different token and category from a declared non-positive value.
        CHECK_FALSE(profile.initialTempo.has_value());

        const auto rejected = judgement::validatePrepare(profile, measuredPolicy());
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(rejected.error(), "category") == "invalid_relation");
        CHECK(contextValue(rejected.error(), "field.path") == "initialTempo");

        const auto unmapped = judgement::mapBeatToTick(profile, beat(1, 1));
        REQUIRE_FALSE(unmapped.has_value());
        CHECK(unmapped.error().code() == "judgement.s7a2.timebase.profile_invalid");
        CHECK(contextValue(unmapped.error(), "category") == "invalid_relation");
    }

    SECTION("declaring a tempo makes the same declaration prepare-valid") {
        TimebaseProfile declared = profile;
        declared.initialTempo = duration(500000, 1);
        REQUIRE(declared.initialTempo.has_value());
        CHECK(judgement::validatePrepare(declared, measuredPolicy()).has_value());

        //  One tick is one microsecond, so a 120 BPM tempo of 500000 us per beat maps beat 1 to
        //  500000 ticks. That is the unit declaration being observable, not a budget constant.
        const auto mapped = judgement::mapBeatToTick(declared, beat(1, 1));
        REQUIRE(mapped.has_value());
        CHECK(mapped->tick().value() == 500000);
    }
}

//  ---------------------------------------------------------------------------------------------
//  7. Checked rational construction
//  ---------------------------------------------------------------------------------------------

TEST_CASE("timebase: exact rationals are reduced, signed and checked",
          "[judgement][timebase][s7a2]") {
    SECTION("reduction and sign canonicalisation") {
        const auto half = RationalBeat::create(2, 4);
        REQUIRE(half.has_value());
        CHECK(half->numerator() == 1);
        CHECK(half->denominator() == 2);

        const auto negative = RationalBeat::create(1, -2);
        REQUIRE(negative.has_value());
        CHECK(negative->numerator() == -1);
        CHECK(negative->denominator() == 2);

        const auto sameByReduction = RationalBeat::create(-3, 6);
        REQUIRE(sameByReduction.has_value());
        CHECK(*sameByReduction == *negative);
        CHECK(sameByReduction->numerator() == -1);
        CHECK(sameByReduction->denominator() == 2);
        CHECK_FALSE(*negative == *half);

        const auto whole = RationalBeat::create(4, -2);
        REQUIRE(whole.has_value());
        CHECK(whole->numerator() == -2);
        CHECK(whole->denominator() == 1);

        const auto zero = RationalBeat::create(0, 7);
        REQUIRE(zero.has_value());
        CHECK(zero->numerator() == 0);
        CHECK(zero->denominator() == 1);
        CHECK(zero->isZero());

        const auto third = RationalBeat::create(6, 3);
        REQUIRE(third.has_value());
        CHECK(third->numerator() == 2);
        CHECK(third->denominator() == 1);

        const auto durationHalf = RationalDuration::create(3, -6);
        REQUIRE(durationHalf.has_value());
        CHECK(durationHalf->numerator() == -1);
        CHECK(durationHalf->denominator() == 2);

        const auto durationHalfPositive = RationalDuration::create(5, 10);
        const auto durationHalfReference = RationalDuration::create(1, 2);
        REQUIRE(durationHalfPositive.has_value());
        REQUIRE(durationHalfReference.has_value());
        CHECK(*durationHalfPositive == *durationHalfReference);
    }

    SECTION("zero exists only as an explicit declaration, never as a default") {
        //  The zero beat is still available, but only through the checked factory, so a zero can
        //  never be a value the type handed out on its own.
        const auto explicitZero = RationalBeat::create(0, 1);
        REQUIRE(explicitZero.has_value());
        CHECK(explicitZero->numerator() == 0);
        CHECK(explicitZero->denominator() == 1);
        CHECK(explicitZero->isZero());
        const auto reducedZero = RationalBeat::create(0, 7);
        REQUIRE(reducedZero.has_value());
        CHECK(*explicitZero == *reducedZero);
        CHECK(reducedZero->denominator() == 1);

        const auto explicitZeroDuration = RationalDuration::create(0, 1);
        REQUIRE(explicitZeroDuration.has_value());
        CHECK(explicitZeroDuration->numerator() == 0);
        CHECK(explicitZeroDuration->denominator() == 1);

        //  The compile-time half of the same contract. "Cannot be default-constructed" is a
        //  property of the type, so it is asserted where the compiler can enforce it: a zero
        //  rational, a default origin kind and an all-zero TimebaseProfile must all be
        //  unrepresentable. These static assertions fail the build if a default constructor is ever
        //  reintroduced; the runtime checks below only repeat the verdict so it appears in the test
        //  log.
        static_assert(!std::is_default_constructible_v<RationalBeat>);
        static_assert(!std::is_default_constructible_v<RationalDuration>);
        static_assert(!std::is_default_constructible_v<TickCollisionKey>);
        static_assert(!std::is_default_constructible_v<TimebaseProfile>);
        static_assert(
            std::is_constructible_v<TickCollisionKey, Tick, OriginKind, CanonicalOrdinal>);
        static_assert(!std::is_constructible_v<TickCollisionKey, Tick, OriginKind>);

        CHECK_FALSE(std::is_default_constructible_v<RationalBeat>);
        CHECK_FALSE(std::is_default_constructible_v<RationalDuration>);
        CHECK_FALSE(std::is_default_constructible_v<TickCollisionKey>);
        CHECK_FALSE(std::is_default_constructible_v<TimebaseProfile>);
        CHECK(std::is_constructible_v<TickCollisionKey, Tick, OriginKind, CanonicalOrdinal>);
        CHECK_FALSE(std::is_constructible_v<TickCollisionKey, Tick, OriginKind>);
    }

    SECTION("unrepresentable rationals are rejected") {
        const std::vector<std::pair<std::int64_t, std::int64_t>> rejected{
            {1, 0}, {-1, 0}, {0, 0}, {1, kInt64Min}, {kInt64Min, 1}, {kInt64Min, -1},
        };
        for (const auto& item : rejected) {
            CAPTURE(item.first, item.second);
            const auto beatValue = RationalBeat::create(item.first, item.second);
            REQUIRE_FALSE(beatValue.has_value());
            CHECK(beatValue.error().code() == "judgement.s7a2.timebase.rational_invalid");
            CHECK(contextValue(beatValue.error(), "category") == "budget_exceeded");
            CHECK(contextValue(beatValue.error(), "field.section") == "judgement.timebase");
            CHECK(contextValue(beatValue.error(), "field.path") == "rational");

            const auto durationValue = RationalDuration::create(item.first, item.second);
            REQUIRE_FALSE(durationValue.has_value());
            CHECK(durationValue.error().code() == "judgement.s7a2.timebase.rational_invalid");
            CHECK(contextValue(durationValue.error(), "category") == "budget_exceeded");
            CHECK(contextValue(durationValue.error(), "field.path") == "rational");
        }
    }
}
