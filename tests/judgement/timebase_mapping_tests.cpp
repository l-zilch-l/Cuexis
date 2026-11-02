#include "timebase_test_fixture.hpp"

// Beat mapping, rounding, tempo, stop, arithmetic and canonical order cases.

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
        CHECK(upward.error().code() == "judgement.timebase.tick_overflow");
        CHECK(contextValue(upward.error(), "category") == "budget_exceeded");
        CHECK(contextValue(upward.error(), "severity") == "error");
        CHECK(contextValue(upward.error(), "faulted") == "false");
        CHECK(contextValue(upward.error(), "field.section") == "judgement.timebase");
        CHECK(contextValue(upward.error(), "field.path") == "tick");

        const auto downward = judgement::offsetTicks(Tick{kInt64Min}, TickSpan{-1});
        REQUIRE_FALSE(downward.has_value());
        CHECK(downward.error().code() == "judgement.timebase.tick_overflow");
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
        CHECK(beyondMaximum.error().code() == "judgement.timebase.tick_overflow");
        CHECK(contextValue(beyondMaximum.error(), "category") == "budget_exceeded");

        const auto beyondMinimum = judgement::differenceTicks(Tick{kInt64Min}, Tick{1});
        REQUIRE_FALSE(beyondMinimum.has_value());
        CHECK(beyondMinimum.error().code() == "judgement.timebase.tick_overflow");

        const auto negation = judgement::differenceTicks(Tick{0}, Tick{kInt64Min});
        REQUIRE_FALSE(negation.has_value());
        CHECK(negation.error().code() == "judgement.timebase.tick_overflow");
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
        CHECK(reversed.error().code() == "judgement.timebase.interval_reversed");
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
        CHECK(doubled.error().code() == "judgement.timebase.tick_overflow");
        CHECK(contextValue(doubled.error(), "category") == "budget_exceeded");
        CHECK(contextValue(doubled.error(), "field.section") == "judgement.timebase");
        CHECK(contextValue(doubled.error(), "field.path") == "beat");

        const auto halved = judgement::mapBeatToTick(profile, beat(3, 2));
        REQUIRE_FALSE(halved.has_value());
        CHECK(halved.error().code() == "judgement.timebase.tick_overflow");

        const TimebaseProfile doubledTempo = plainProfile(duration(2, 1));
        const auto far = judgement::mapBeatToTick(doubledTempo, beat(kInt64Max, 1));
        REQUIRE_FALSE(far.has_value());
        CHECK(far.error().code() == "judgement.timebase.tick_overflow");
    }

    SECTION("an invalid profile is rejected before any division happens") {
        TimebaseProfile noScale = plainProfile(duration(1000, 1));
        noScale.tickScale = duration(0, 1);
        const auto rejected = judgement::mapBeatToTick(noScale, beat(1, 1));
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "judgement.timebase.profile_value_out_of_range");
        CHECK(contextValue(rejected.error(), "category") == "budget_exceeded");
        CHECK(contextValue(rejected.error(), "field.path") == "tickScale");

        TimebaseProfile unordered = plainProfile(duration(1000, 1));
        unordered.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(4, 1), .durationPerBeat = duration(1, 1)});
        unordered.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(2, 1), .durationPerBeat = duration(1, 1)});
        const auto unorderedResult = judgement::mapBeatToTick(unordered, beat(3, 1));
        REQUIRE_FALSE(unorderedResult.has_value());
        CHECK(unorderedResult.error().code() == "judgement.timebase.profile_invalid");
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
    CHECK(rejected.error().code() == "judgement.late.policy_undeclared");
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
        CHECK(rejected.error().code() == "judgement.late.parameter_pending");
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
    CHECK(rejected.error().code() == "judgement.late.policy_undeclared");
    CHECK(contextValue(rejected.error(), "category") == "late_policy_incomplete");
}
