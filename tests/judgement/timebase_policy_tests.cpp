#include "timebase_test_fixture.hpp"

// Late policy, window commit, microsecond unit and rational cases.

TEST_CASE("timebase: validatePrepare rejects an invalid profile declaration",
          "[judgement][timebase][s7a2]") {
    const LatePolicyParameters parameters = measuredPolicy();

    SECTION("a missing profile identity or unit token is a structural rejection") {
        TimebaseProfile noIdentity = plainProfile(duration(500000, 1));
        noIdentity.profileId = {};
        const auto identityResult = judgement::validatePrepare(noIdentity, parameters);
        REQUIRE_FALSE(identityResult.has_value());
        CHECK(identityResult.error().code() == "judgement.timebase.profile_invalid");
        CHECK(contextValue(identityResult.error(), "category") == "invalid_relation");
        CHECK(contextValue(identityResult.error(), "field.path") == "profileId");

        TimebaseProfile noUnit = plainProfile(duration(500000, 1));
        noUnit.unitToken = {};
        const auto unitResult = judgement::validatePrepare(noUnit, parameters);
        REQUIRE_FALSE(unitResult.has_value());
        CHECK(unitResult.error().code() == "judgement.timebase.profile_invalid");
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
        CHECK(rejected.error().code() == "judgement.timebase.profile_invalid");
        CHECK(contextValue(rejected.error(), "category") == "invalid_relation");
        CHECK(contextValue(rejected.error(), "severity") == "error");
        CHECK(contextValue(rejected.error(), "faulted") == "false");
        CHECK(contextValue(rejected.error(), "field.section") == "judgement.timebase");
        CHECK(contextValue(rejected.error(), "field.path") == "initialTempo");

        //  The mapper applies the same precondition, so an unvalidated profile cannot map either.
        const auto unmapped = judgement::mapBeatToTick(undeclared, beat(1, 1));
        REQUIRE_FALSE(unmapped.has_value());
        CHECK(unmapped.error().code() == "judgement.timebase.profile_invalid");
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
            CHECK(rejected.error().code() == "judgement.timebase.profile_value_out_of_range");
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
        CHECK(duplicateResult.error().code() == "judgement.timebase.profile_invalid");
        CHECK(contextValue(duplicateResult.error(), "field.path") == "tempoSections");

        TimebaseProfile unordered = plainProfile(duration(100, 1));
        unordered.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(4, 1), .durationPerBeat = duration(200, 1)});
        unordered.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(1, 1), .durationPerBeat = duration(300, 1)});
        const auto unorderedResult = judgement::validatePrepare(unordered, parameters);
        REQUIRE_FALSE(unorderedResult.has_value());
        CHECK(unorderedResult.error().code() == "judgement.timebase.profile_invalid");
        CHECK(contextValue(unorderedResult.error(), "category") == "invalid_relation");
        CHECK(contextValue(unorderedResult.error(), "field.path") == "tempoSections");
    }

    SECTION("a non-positive tempo duration is a value rejection") {
        TimebaseProfile profile = plainProfile(duration(100, 1));
        profile.tempoSections.push_back(
            judgement::TempoSection{.startBeat = beat(2, 1), .durationPerBeat = duration(0, 1)});
        const auto rejected = judgement::validatePrepare(profile, parameters);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "judgement.timebase.profile_value_out_of_range");
        CHECK(contextValue(rejected.error(), "category") == "budget_exceeded");
        CHECK(contextValue(rejected.error(), "field.path") == "tempoSections");
    }

    SECTION("stop sections must be non-degenerate, ordered and non-overlapping") {
        TimebaseProfile degenerate = plainProfile(duration(100, 1));
        degenerate.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(2, 1), .duration = duration(10, 1)});
        const auto degenerateResult = judgement::validatePrepare(degenerate, parameters);
        REQUIRE_FALSE(degenerateResult.has_value());
        CHECK(degenerateResult.error().code() == "judgement.timebase.profile_invalid");
        CHECK(contextValue(degenerateResult.error(), "category") == "invalid_relation");
        CHECK(contextValue(degenerateResult.error(), "field.path") == "stopSections");

        TimebaseProfile reversed = plainProfile(duration(100, 1));
        reversed.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(4, 1), .endBeat = beat(2, 1), .duration = duration(10, 1)});
        const auto reversedResult = judgement::validatePrepare(reversed, parameters);
        REQUIRE_FALSE(reversedResult.has_value());
        CHECK(reversedResult.error().code() == "judgement.timebase.profile_invalid");

        TimebaseProfile overlapping = plainProfile(duration(100, 1));
        overlapping.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(6, 1), .duration = duration(10, 1)});
        overlapping.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(4, 1), .endBeat = beat(8, 1), .duration = duration(10, 1)});
        const auto overlappingResult = judgement::validatePrepare(overlapping, parameters);
        REQUIRE_FALSE(overlappingResult.has_value());
        CHECK(overlappingResult.error().code() == "judgement.timebase.profile_invalid");
        CHECK(contextValue(overlappingResult.error(), "field.path") == "stopSections");

        TimebaseProfile unordered = plainProfile(duration(100, 1));
        unordered.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(6, 1), .endBeat = beat(8, 1), .duration = duration(10, 1)});
        unordered.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(4, 1), .duration = duration(10, 1)});
        const auto unorderedResult = judgement::validatePrepare(unordered, parameters);
        REQUIRE_FALSE(unorderedResult.has_value());
        CHECK(unorderedResult.error().code() == "judgement.timebase.profile_invalid");

        TimebaseProfile nonPositive = plainProfile(duration(100, 1));
        nonPositive.stopSections.push_back(judgement::StopSection{
            .startBeat = beat(2, 1), .endBeat = beat(4, 1), .duration = duration(0, 1)});
        const auto nonPositiveResult = judgement::validatePrepare(nonPositive, parameters);
        REQUIRE_FALSE(nonPositiveResult.has_value());
        CHECK(nonPositiveResult.error().code() == "judgement.timebase.profile_value_out_of_range");
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
            CHECK(rejected.error().code() == "judgement.timebase.commit_not_at_commit_tick");
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
            CHECK(rejected.error().code() == "judgement.timebase.commit_window_invalid");
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
        CHECK(rejected.error().code() == "judgement.timebase.profile_invalid");
        CHECK(contextValue(rejected.error(), "category") == "invalid_relation");
        CHECK(contextValue(rejected.error(), "field.path") == "initialTempo");

        const auto unmapped = judgement::mapBeatToTick(profile, beat(1, 1));
        REQUIRE_FALSE(unmapped.has_value());
        CHECK(unmapped.error().code() == "judgement.timebase.profile_invalid");
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
            CHECK(beatValue.error().code() == "judgement.timebase.rational_invalid");
            CHECK(contextValue(beatValue.error(), "category") == "budget_exceeded");
            CHECK(contextValue(beatValue.error(), "field.section") == "judgement.timebase");
            CHECK(contextValue(beatValue.error(), "field.path") == "rational");

            const auto durationValue = RationalDuration::create(item.first, item.second);
            REQUIRE_FALSE(durationValue.has_value());
            CHECK(durationValue.error().code() == "judgement.timebase.rational_invalid");
            CHECK(contextValue(durationValue.error(), "category") == "budget_exceeded");
            CHECK(contextValue(durationValue.error(), "field.path") == "rational");
        }
    }
}
