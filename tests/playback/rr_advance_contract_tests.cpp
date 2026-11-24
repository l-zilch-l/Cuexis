// T-10 advance contract, value and score part.
//
// This case currently covers the runtime value headers, the prepared identity declarations, the
// advance value contract, the normalized score records and the shared exact integer helpers (GP-05
// and the computable halves of GP-04). The kernel transaction itself - GP-02, GP-03 and CLK-02 - is
// not implemented in this round; the report records that remainder and its conditions instead of
// asserting a result the tree cannot produce.

#include <cuexis/judgement/advance.hpp>
#include <cuexis/judgement/gameplay_assembler_identity.hpp>
#include <cuexis/judgement/gameplay_graph_reference.hpp>
#include <cuexis/judgement/normalized_score.hpp>
#include <cuexis/judgement/phase_types.hpp>
#include <cuexis/judgement/prepared_identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using namespace cuexis::judgement;

auto errorCode(const cuexis::core::Result<void>& result) -> std::string {
    REQUIRE_FALSE(result);
    return std::string{result.error().code()};
}

auto scoreMode() -> ScoreMode {
    ScoreMode mode;
    mode.id = "standard";
    mode.total = 1000000;
    mode.mj = 900000;
    mode.mc = 100000;
    mode.onExhausted = ComboAction::reset;
    mode.comboRewards = {ComboRewardSegment{1, 1, 1}};
    mode.rules = {
        ScoreRuleRow{PhaseKind::tap, Outcome::hit, std::string{"A"}, true, 2,
                     ComboAction::increment},
        ScoreRuleRow{PhaseKind::tap, Outcome::hit, std::string{"B"}, true, 5,
                     ComboAction::increment},
        ScoreRuleRow{PhaseKind::tap, Outcome::hit, std::string{"solo"}, true, 3,
                     ComboAction::increment},
        ScoreRuleRow{PhaseKind::tap, Outcome::miss, std::nullopt, false, 0, ComboAction::keep},
    };
    return mode;
}

auto preparedScore() -> PreparedNormalizedScore {
    PreparedNormalizedScore score;
    score.mode = scoreMode();
    for (const char* id : {"A", "B", "solo"}) {
        PreparedGradeTable table;
        table.id = id;
        table.exactStart = 0;
        table.exactEnd = 1;
        table.intervals = {PreparedGradeInterval{0, 1, id}};
        score.gradeTables.push_back(std::move(table));
    }
    score.units = {ScoreUnit{{0, 1}}, ScoreUnit{{2}}};
    score.basis = ScoreBasis{8, 3};
    return score;
}

} // namespace

TEST_CASE("cuexis_rr_advance_contract", "[playback][judgement][rr][advance]") {
    SECTION("the advance value contract carries the frozen vocabulary and by-value results") {
        STATIC_REQUIRE(std::is_same_v<RunKey, cuexis::judgement::RunKey>);
        STATIC_REQUIRE(static_cast<int>(AdvanceStatus::ongoing) == 0);
        STATIC_REQUIRE(static_cast<int>(AdvanceStatus::completed) == 1);
        STATIC_REQUIRE(static_cast<int>(AdvanceStatus::faulted) == 2);
        STATIC_REQUIRE(static_cast<int>(AdvanceFaultStage::none) == 0);
        STATIC_REQUIRE(static_cast<int>(AdvanceFaultStage::control) == 3);
        STATIC_REQUIRE(static_cast<int>(AdvanceFaultReason::invariant) == 3);
        STATIC_REQUIRE(static_cast<int>(TimingDirection::early) == 0);
        STATIC_REQUIRE(static_cast<int>(TimingDirection::exact) == 1);
        STATIC_REQUIRE(static_cast<int>(TimingDirection::late) == 2);

        RunKey run{7, 1};
        CHECK(run == RunKey{7, 1});
        CHECK(run != RunKey{8, 1});

        NormalizedScoreResult score{5, 8, 3, 3, 900000, 100000, 662500, 3, 3};
        AdvanceOutcome outcome;
        outcome.run = run;
        outcome.written = 1;
        outcome.processedThrough = Tick{120};
        outcome.status = AdvanceStatus::faulted;
        outcome.fault = FaultCode{AdvanceFaultStage::fold, AdvanceFaultReason::invariant};
        outcome.score = score;
        CHECK(outcome.run == run);
        CHECK(outcome.written == 1);
        CHECK(outcome.processedThrough == Tick{120});
        CHECK(outcome.fault.stage == AdvanceFaultStage::fold);
        CHECK(outcome.score->score == 662500);
        CHECK_FALSE(outcome.score->jMax == 0);

        SettlementEvent event;
        event.tick = Tick{100};
        event.phaseIndex = 2;
        event.gradeIndex = std::numeric_limits<std::uint32_t>::max();
        event.outcome = Outcome::miss;
        event.error = TickDelta{11};
        event.direction = TimingDirection::late;
        CHECK(event.tick == Tick{100});
        CHECK(event.phaseIndex == 2);
        CHECK(event.gradeIndex == std::numeric_limits<std::uint32_t>::max());
        CHECK(event.outcome == Outcome::miss);
        CHECK(event.error.value() == TickDelta{11});
        CHECK(event.direction.value() == TimingDirection::late);

        //  The dictionary is an owning value: a copy taken before the source is destroyed keeps its
        //  phases and grades readable.
        SettlementDictionary dictionary;
        dictionary.phases.push_back(SettlementPhase{
            RequirementIdentity{"chart", "invocation", "module", "export", {}, "req"},
            PhaseKind::tap});
        dictionary.grades = {"Good", "Great"};
        auto owned = dictionary;
        dictionary.phases.clear();
        dictionary.grades.clear();
        CHECK(owned.phases.size() == 1);
        CHECK(owned.phases.front().requirement.requirementLocalId == "req");
        CHECK(owned.grades.size() == 2);
    }

    SECTION("the runtime value headers carry the sole complete definitions") {
        // The author-facing headers re-include the runtime definitions, so the compile side and
        // the runtime records share one type identity instead of two spelled-alike copies.
        STATIC_REQUIRE(std::is_same_v<StableDeclarationId, cuexis::judgement::StableDeclarationId>);
        STATIC_REQUIRE(std::is_same_v<PreparedIdentityDeclarations,
                                      cuexis::judgement::PreparedIdentityDeclarations>);
        STATIC_REQUIRE(std::is_same_v<decltype(PreparedIdentityDeclarations{}.engine),
                                      EngineIdentityDeclaration>);
        STATIC_REQUIRE(std::is_same_v<decltype(PreparedIdentityDeclarations{}.ruleset),
                                      RulesetIdentityDeclaration>);
        STATIC_REQUIRE(std::is_same_v<decltype(PreparedIdentityDeclarations{}.session),
                                      SessionIdentityDeclaration>);

        StableDeclarationId first{"doc.alpha", 4};
        StableDeclarationId second{"doc.alpha", 5};
        CHECK(first != second);
        CHECK(first < second);
        CHECK(first == StableDeclarationId{"doc.alpha", 4});

        PreparedIdentityDeclarations declarations;
        declarations.engine.judgementSemanticRevision = "judgement.t4-k4.v1";
        declarations.engine.factSemanticRevision = "fact.semantic.phase-local.v1";
        declarations.engine.fixedPointTableId = "fixed-point.none.v1";
        declarations.engine.coordinationPhaseOrderToken = "coordination.six-eight.t4-k4.v1";
        declarations.ruleset.interfaceProjectionToken = "ruleset.one";
        declarations.ruleset.moduleOrder = {"score", "combo", "statistics"};
        declarations.ruleset.buildHash = "build.fixture";
        declarations.session.loadoutToken = "loadout.fixture";
        declarations.session.defaultGraceSourceToken = "grace.source.one";
        declarations.session.normalizationProfileToken = "normalization.one";
        declarations.session.judgementConfigToken = "judgement.config.one";
        auto copy = declarations;
        CHECK(copy == declarations);
        copy.session.loadoutToken = "loadout.other";
        CHECK(copy != declarations);
        CHECK(declarations.engine.executionProfileToken == std::nullopt);
    }

    SECTION("prepared normalized score validation accepts the canonical mode") {
        auto score = preparedScore();
        CHECK(validatePreparedNormalizedScore(score).has_value());

        auto invalid = score;
        invalid.mode.total = 999999;
        CHECK(errorCode(validatePreparedNormalizedScore(invalid)) ==
              "judgement.execution.relation_invalid");

        invalid = score;
        invalid.mode.onExhausted = ComboAction::increment;
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.mode.rules[1] = invalid.mode.rules[0];
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.mode.rules[3].judgePoints = 1;
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.mode.comboRewards.front().from = 2;
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.mode.comboRewards = {ComboRewardSegment{1, 5, 0}, ComboRewardSegment{2, 4, 0}};
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.mode.comboRewards = {ComboRewardSegment{1, 1, 0}, ComboRewardSegment{2, 1, 0}};
        CHECK(validatePreparedNormalizedScore(invalid).has_value());

        invalid = score;
        invalid.gradeTables[1].id = "A";
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.gradeTables[0].exactStart = 1;
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.gradeTables[0].intervals.front().end = 0;
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.gradeTables[0].intervals.push_back(PreparedGradeInterval{2, 3, "A"});
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.units.front().phaseIndices.clear();
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.units = {ScoreUnit{{2}}, ScoreUnit{{0, 1}}};
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.units = {ScoreUnit{{0, 1}}, ScoreUnit{{1}}};
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));

        invalid = score;
        invalid.units = {ScoreUnit{{1, 0}}};
        CHECK_FALSE(validatePreparedNormalizedScore(invalid));
    }

    SECTION("combo reward segments and the combo basis follow GP-04 and GP-05") {
        auto mode = scoreMode();
        CHECK(deriveComboBasis(mode, 0).value() == 0);
        CHECK(deriveComboBasis(mode, 200).value() == 20100);
        CHECK(deriveComboBasis(mode, 1000).value() == 500500);
        CHECK(deriveComboBasis(mode, 3).value() == 6);

        //  `{1,1,1}, {2,2,0}` gives rewards 1, 2, 2, 2; four units sum to 7.
        mode.comboRewards = {ComboRewardSegment{1, 1, 1}, ComboRewardSegment{2, 2, 0}};
        CHECK(deriveComboBasis(mode, 4).value() == 7);
        CHECK(comboReward(mode, 2).value() == 2);
        CHECK(comboReward(mode, 5).value() == 2);

        //  A decreasing boundary is rejected by the structural validation, not silently clamped.
        mode.comboRewards = {ComboRewardSegment{1, 2, 0}, ComboRewardSegment{2, 1, 0}};
        auto decreasing = preparedScore();
        decreasing.mode = mode;
        CHECK_FALSE(validatePreparedNormalizedScore(decreasing));
        mode.comboRewards = {ComboRewardSegment{1, 1, 0}, ComboRewardSegment{2, 0, 0}};
        auto score = preparedScore();
        score.mode = mode;
        CHECK_FALSE(validatePreparedNormalizedScore(score));

        //  `reward = UINT64_MAX / 2` twice is representable; a third unit overflows.
        mode = scoreMode();
        mode.comboRewards = {
            ComboRewardSegment{1, std::numeric_limits<std::uint64_t>::max() / 2U, 0}};
        CHECK(deriveComboBasis(mode, 2).value() == std::numeric_limits<std::uint64_t>::max() - 1U);
        CHECK_FALSE(deriveComboBasis(mode, 3));
    }

    SECTION("normalized score output follows the exact integer rounding of GP-05") {
        //  (mj, mc; J/Jmax, C/Cmax) -> total.
        CHECK(normalizedScoreOutput(1, 1, 2, 1, 2, 1, 2).value() == 1);
        CHECK(normalizedScoreOutput(1, 0, 1, 1, 2, 0, 0).value() == 0);
        CHECK(normalizedScoreOutput(3, 0, 3, 1, 2, 0, 0).value() == 2);
        CHECK(normalizedScoreOutput(5, 0, 5, 1, 2, 0, 0).value() == 2);
        CHECK(normalizedScoreOutput(7, 0, 7, 1, 2, 0, 0).value() == 4);
        CHECK(normalizedScoreOutput(0, 0, 0, 0, 0, 0, 0).value() == 0);

        //  Exact rationals that do not land on a half.
        CHECK(normalizedScoreOutput(1, 0, 1, 1, 3, 0, 0).value() == 0);
        CHECK(normalizedScoreOutput(2, 0, 2, 2, 3, 0, 0).value() == 1);
        CHECK(normalizedScoreOutput(900000, 100000, 1000000, 5, 8, 3, 3).value() == 662500);
        CHECK(normalizedScoreOutput(900000, 100000, 1000000, 8, 8, 3, 3).value() == 1000000);

        //  The widest vector: a 160-bit numerator and a 128-bit denominator.
        const std::uint64_t wide = std::numeric_limits<std::uint64_t>::max();
        const std::uint32_t total = std::numeric_limits<std::uint32_t>::max();
        CHECK(normalizedScoreOutput(total - 1U, 1U, total, wide - 1U, wide, wide - 2U, wide)
                  .value() == total);

        //  Rejections: split, enabled zero denominator, numerator above denominator.
        CHECK_FALSE(normalizedScoreOutput(1, 0, 2, 1, 2, 0, 0));
        CHECK_FALSE(normalizedScoreOutput(1, 0, 1, 0, 0, 0, 0));
        CHECK_FALSE(normalizedScoreOutput(0, 1, 1, 0, 0, 0, 0));
        CHECK_FALSE(normalizedScoreOutput(1, 0, 1, 3, 2, 0, 0));
        CHECK_FALSE(normalizedScoreOutput(0, 1, 1, 0, 0, 3, 2));
    }

    SECTION("the prepared grade table answers half-open intervals") {
        PreparedGradeTable table;
        table.id = "main";
        table.exactStart = -10;
        table.exactEnd = 15;
        table.intervals = {PreparedGradeInterval{-50, -10, "Good"},
                           PreparedGradeInterval{-10, 10, "Great"},
                           PreparedGradeInterval{10, 50, "Good"}};
        CHECK(preparedGradeAt(table, -51) == nullptr);
        CHECK(*preparedGradeAt(table, -50) == "Good");
        CHECK(*preparedGradeAt(table, -11) == "Good");
        CHECK(*preparedGradeAt(table, -10) == "Great");
        CHECK(*preparedGradeAt(table, 9) == "Great");
        CHECK(*preparedGradeAt(table, 10) == "Good");
        CHECK(*preparedGradeAt(table, 49) == "Good");
        CHECK(preparedGradeAt(table, 50) == nullptr);
    }
}
