// T-10 advance contract, value and score part.
//
// This case currently covers the runtime value headers, the prepared identity declarations, the
// advance value contract, the normalized score records and the shared exact integer helpers (GP-05
// and the computable halves of GP-04). The kernel transaction itself - GP-02, GP-03 and CLK-02 - is
// not implemented in this round; the report records that remainder and its conditions instead of
// asserting a result the tree cannot produce.

#include <cuexis/judgement/advance.hpp>
#include <cuexis/judgement/advance_kernel.hpp>
#include <cuexis/judgement/gameplay_assembler_identity.hpp>
#include <cuexis/judgement/gameplay_graph_reference.hpp>
#include <cuexis/judgement/normalized_score.hpp>
#include <cuexis/judgement/phase_types.hpp>
#include <cuexis/judgement/prepared_gameplay.hpp>
#include <cuexis/judgement/prepared_gameplay_records.hpp>
#include <cuexis/judgement/prepared_identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
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

auto runtimePolicy() -> runtime::LatePolicyParameters {
    runtime::LatePolicyParameters policy;
    policy.finalizationWatermark = runtime::MeasuredParameter<TickSpan>::measured(TickSpan{8});
    policy.maxQueueHop = runtime::MeasuredParameter<TickSpan>::measured(TickSpan{4});
    policy.windowCloseThreshold = runtime::MeasuredParameter<TickSpan>::measured(TickSpan{3});
    policy.windowOpenThreshold = runtime::MeasuredParameter<TickSpan>::measured(TickSpan{2});
    policy.policy = runtime::LateEventPolicy::queueNextTick;
    return policy;
}

auto runtimeIdentities() -> PreparedIdentityDeclarations {
    PreparedIdentityDeclarations identities;
    identities.engine.judgementSemanticRevision = "judgement.t4-k4.v1";
    identities.engine.factSemanticRevision = "fact.semantic.phase-local.v1";
    identities.engine.fixedPointTableId = "fixed-point.none.v1";
    identities.engine.coordinationPhaseOrderToken = "coordination.six-eight.t4-k4.v1";
    identities.ruleset.interfaceProjectionToken = "ruleset.one";
    identities.session.judgementConfigToken = "judgement.config.one";
    return identities;
}

auto tapRequirement(std::size_t index, std::int64_t target,
                    std::optional<std::uint32_t> gradeTableIndex, std::int64_t windowRadius = 50)
    -> runtime::PreparedRequirement {
    runtime::PreparedRequirement requirement;
    requirement.stableId = StableDeclarationId{"doc.alpha", static_cast<std::uint32_t>(index)};
    requirement.identity = RequirementIdentity{"chart.one",  "invocation.one",
                                               "module.one", "export.one",
                                               {},           "req." + std::to_string(index)};
    requirement.phases = {PhaseDeclaration{PhaseKind::tap, 0}};
    requirement.timing.end = Tick{target + windowRadius};
    requirement.timing.successWindows = {runtime::SuccessWindow{
        Tick{target - windowRadius}, Tick{target + windowRadius}, requirement.phases.front()}};
    requirement.timing.phaseTargets = {runtime::PhaseTarget{PhaseKind::tap, Tick{target}}};
    requirement.deadline = Tick{target + windowRadius};
    if (gradeTableIndex) {
        requirement.measure.push_back(
            runtime::CompiledMeasureComponent{PhaseKind::tap,
                                              FactCategory::tap,
                                              runtime::GradePresence::declared,
                                              {},
                                              *gradeTableIndex});
    }
    return requirement;
}

auto tapUnitScore(std::vector<runtime::PreparedRequirement>& requirements,
                  std::vector<ScoreUnit> units) -> PreparedNormalizedScore {
    auto score = preparedScore();
    score.units = std::move(units);
    score.basis = ScoreBasis{0, 0};
    (void)requirements;
    return score;
}

auto contextValue(const cuexis::core::Error& error, std::string_view key) -> std::string {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

auto singleRequirementView(const runtime::PreparedTimebase& timebase,
                           std::vector<runtime::PreparedRequirement>& requirements,
                           runtime::LatePolicyParameters& policy,
                           PreparedIdentityDeclarations& identities, PreparedNormalizedScore& score)
    -> runtime::CompiledGameplayView {
    return runtime::CompiledGameplayView{
        timebase,
        policy,
        identities,
        "content.one",
        "prepared.one",
        "execution.one",
        nullptr,
        &score,
        requirements,
        std::span<const runtime::PreparedResource>{},
        std::span<const runtime::PreparedTimerKey>{},
    };
}

auto preparedTapGameplay() -> runtime::PreparedGameplay {
    std::vector<runtime::PreparedRequirement> requirements = {tapRequirement(0, 1000, 0)};
    auto score = tapUnitScore(requirements, {ScoreUnit{{0}}});
    score.basis = ScoreBasis{2, 1};
    auto policy = runtimePolicy();
    auto identities = runtimeIdentities();
    const runtime::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
    auto view = singleRequirementView(timebase, requirements, policy, identities, score);
    auto prepared = runtime::prepareCompiledGameplay(view, runtime::PrepareGameplayLimits{});
    REQUIRE(prepared);
    return std::move(*prepared);
}

auto tickRecord(std::int64_t tick, runtime::InputAction action = runtime::InputAction::press)
    -> runtime::TickRecord {
    return runtime::TickRecord{Tick{tick}, "domain.binding.one", "keyboard", "channel.zero",
                               action,     std::nullopt};
}

auto shortHoldRequirement() -> runtime::PreparedRequirement {
    runtime::PreparedRequirement requirement;
    requirement.stableId = StableDeclarationId{"doc.hold", 0};
    requirement.identity = RequirementIdentity{
        "chart.hold", "invocation.hold", "module.hold", "export.hold", {}, "hold.short"};
    requirement.phases = {PhaseDeclaration{PhaseKind::head, 0},
                          PhaseDeclaration{PhaseKind::body, 1},
                          PhaseDeclaration{PhaseKind::tail, 2}};
    requirement.timing.end = Tick{1040};
    requirement.timing.successWindows = {
        runtime::SuccessWindow{Tick{850}, Tick{901}, requirement.phases[0]},
        runtime::SuccessWindow{Tick{850}, Tick{851}, requirement.phases[1]},
        runtime::SuccessWindow{Tick{1000}, Tick{1010}, requirement.phases[2]},
        runtime::SuccessWindow{Tick{1015}, Tick{1020}, requirement.phases[2]}};
    requirement.timing.body = TimeInterval{Tick{850}, Tick{851}};
    requirement.timing.phaseTargets = {runtime::PhaseTarget{PhaseKind::head, Tick{850}},
                                       runtime::PhaseTarget{PhaseKind::body, Tick{851}},
                                       runtime::PhaseTarget{PhaseKind::tail, Tick{1000}}};
    requirement.deadline = Tick{1040};
    return requirement;
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

    SECTION("the fixed full-mark basis follows GP-04") {
        auto policy = runtimePolicy();

        //  200 and 1000 single-A units give (400, 20100) and (2000, 500500).
        {
            std::vector<runtime::PreparedRequirement> requirements;
            std::vector<ScoreUnit> units;
            for (std::size_t index = 0; index < 200; ++index) {
                requirements.push_back(tapRequirement(index, 1000, 0));
                units.push_back(ScoreUnit{{static_cast<std::uint32_t>(index)}});
            }
            auto score = tapUnitScore(requirements, std::move(units));
            auto basis = runtime::deriveScoreBasis(requirements, policy, score);
            REQUIRE(basis);
            CHECK(basis->jMax == 400);
            CHECK(basis->cMax == 20100);
        }
        {
            std::vector<runtime::PreparedRequirement> requirements;
            std::vector<ScoreUnit> units;
            for (std::size_t index = 0; index < 1000; ++index) {
                requirements.push_back(tapRequirement(index, 1000, 0));
                units.push_back(ScoreUnit{{static_cast<std::uint32_t>(index)}});
            }
            auto score = tapUnitScore(requirements, std::move(units));
            auto basis = runtime::deriveScoreBasis(requirements, policy, score);
            REQUIRE(basis);
            CHECK(basis->jMax == 2000);
            CHECK(basis->cMax == 500500);
        }

        //  A/B share one explicit unit; solo is its own unit: Jmax=8, Cmax=3.
        {
            std::vector<runtime::PreparedRequirement> requirements = {
                tapRequirement(0, 1000, 0), tapRequirement(1, 2000, 1), tapRequirement(2, 3000, 2)};
            auto score = tapUnitScore(requirements, {ScoreUnit{{0, 1}}, ScoreUnit{{2}}});
            auto basis = runtime::deriveScoreBasis(requirements, policy, score);
            REQUIRE(basis);
            CHECK(basis->jMax == 8);
            CHECK(basis->cMax == 3);
            CHECK(normalizedScoreOutput(900000, 100000, 1000000, 8, 8, 3, 3).value() == 1000000);
            CHECK(normalizedScoreOutput(900000, 100000, 1000000, 5, 8, 3, 3).value() == 662500);
        }

        //  A unit with only non-qualifying keys is rejected with the precise reason.
        {
            std::vector<runtime::PreparedRequirement> requirements = {
                tapRequirement(0, 1000, std::nullopt)};
            requirements.front().timing.successWindows.clear();
            auto score = tapUnitScore(requirements, {ScoreUnit{{0}}});
            auto basis = runtime::deriveScoreBasis(requirements, policy, score);
            REQUIRE_FALSE(basis);
            CHECK(contextValue(basis.error(), "reason") == "score_unit_unqualified");
        }

        //  A local key with no exact rule is rejected.
        {
            std::vector<runtime::PreparedRequirement> requirements = {tapRequirement(0, 1000, 0)};
            auto score = tapUnitScore(requirements, {ScoreUnit{{0}}});
            score.mode.rules = {ScoreRuleRow{PhaseKind::tap, Outcome::miss, std::nullopt, false, 0,
                                             ComboAction::keep}};
            auto basis = runtime::deriveScoreBasis(requirements, policy, score);
            REQUIRE_FALSE(basis);
            CHECK(contextValue(basis.error(), "reason") == "score_rule_missing");
        }

        //  ObserveOnly requirements cannot join a unit.
        {
            std::vector<runtime::PreparedRequirement> requirements = {tapRequirement(0, 1000, 0)};
            runtime::ResourceClaimDeclaration claim;
            claim.intent = runtime::ResourceClaimIntent::observe;
            requirements.front().resourceClaims = {claim};
            auto score = tapUnitScore(requirements, {ScoreUnit{{0}}});
            auto basis = runtime::deriveScoreBasis(requirements, policy, score);
            REQUIRE_FALSE(basis);
            CHECK(contextValue(basis.error(), "reason") == "score_observe_only_unit");
        }

        //  Structural unit errors are refused before any key derivation.
        {
            std::vector<runtime::PreparedRequirement> requirements = {tapRequirement(0, 1000, 0)};
            auto score = tapUnitScore(requirements, {ScoreUnit{{0}}, ScoreUnit{{0}}});
            CHECK_FALSE(runtime::deriveScoreBasis(requirements, policy, score));
            auto outOfRange = tapUnitScore(requirements, {ScoreUnit{{5}}});
            CHECK_FALSE(runtime::deriveScoreBasis(requirements, policy, outOfRange));
        }
    }

    SECTION("the release observation envelope follows GP-02") {
        //  Body release: earliest dispatch 850 minus hop 4 gives the -154 body error at target
        //  1000.
        auto body = runtime::releaseObservationDomain(850, 1000, 4);
        REQUIRE(body);
        CHECK(body->start == 846);
        CHECK(body->end == 1000);
        CHECK(846 - 1000 == -154);

        //  Tail release: the last window closes at 1020, which is +20 at tail target 1000.
        auto tail = runtime::releaseObservationDomain(1000, 1020, 4);
        REQUIRE(tail);
        CHECK(tail->start == 996);
        CHECK(tail->end == 1020);
        CHECK(1020 - 1000 == 20);

        CHECK_FALSE(runtime::releaseObservationDomain(1000, 1000, 4));
        CHECK_FALSE(runtime::releaseObservationDomain(1001, 1000, 4));
        CHECK(runtime::releaseObservationDomain(850, 1000, 0)->start == 850);
        const auto clamped =
            runtime::releaseObservationDomain(std::numeric_limits<std::int64_t>::min(), 0, 4);
        REQUIRE(clamped);
        CHECK(clamped->start == std::numeric_limits<std::int64_t>::min());
        CHECK(clamped->end == 0);
    }

    SECTION("prepared storage owns the records and rejects an inconsistent basis") {
        auto policy = runtimePolicy();
        auto identities = runtimeIdentities();
        const runtime::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
        std::optional<runtime::PreparedGameplay> prepared;
        {
            std::vector<runtime::PreparedRequirement> requirements = {tapRequirement(0, 1000, 0)};
            auto score = tapUnitScore(requirements, {ScoreUnit{{0}}});
            score.basis = ScoreBasis{2, 1};
            runtime::PrepareGameplayLimits limits{};
            auto view = singleRequirementView(timebase, requirements, policy, identities, score);
            auto result = runtime::prepareCompiledGameplay(view, limits);
            REQUIRE(result);
            prepared = std::move(*result);
        }
        //  The source vectors are gone; the owning handle still serves every value.
        CHECK(prepared->requirements().size() == 1);
        CHECK(prepared->resources().empty());
        CHECK(prepared->timers().empty());
        CHECK(prepared->identity() == "prepared.one");
        CHECK(prepared->settlementCapacity() == 1);
        const auto& dictionary = prepared->settlementDictionary();
        REQUIRE(dictionary.phases.size() == 1);
        CHECK(dictionary.phases.front().phase == PhaseKind::tap);
        CHECK(dictionary.phases.front().requirement.requirementLocalId == "req.0");
        CHECK(dictionary.grades == std::vector<std::string>{"A", "B", "solo"});
        const auto records = prepared->records();
        CHECK(records.normalizedScore != nullptr);
        CHECK(records.contentIdentity == "content.one");
        CHECK(records.preparedIdentity == "prepared.one");
        CHECK(records.requirements.size() == 1);
        CHECK(prepared->admitsSuccess(0, Tick{1000}));
        CHECK(prepared->admitsSuccess(0, Tick{1049}));
        CHECK_FALSE(prepared->admitsSuccess(0, Tick{1050}));
        CHECK_FALSE(prepared->admitsSuccess(1, Tick{1000}));

        //  A tampered basis is rejected with the contract field path.
        {
            std::vector<runtime::PreparedRequirement> requirements = {tapRequirement(0, 1000, 0)};
            auto score = tapUnitScore(requirements, {ScoreUnit{{0}}});
            score.basis = ScoreBasis{3, 1};
            runtime::PrepareGameplayLimits limits{};
            auto view = singleRequirementView(timebase, requirements, policy, identities, score);
            auto result = runtime::prepareCompiledGameplay(view, limits);
            REQUIRE_FALSE(result);
            CHECK(result.error().code() == "judgement.execution.relation_invalid");
            CHECK(contextValue(result.error(), "reason") == "basis_mismatch");
            CHECK(contextValue(result.error(), "fieldPath") == "/scoreBasis/jMax");
        }

        //  A measured limit is enforced; the previous handle is not touched by a failure.
        {
            std::vector<runtime::PreparedRequirement> requirements = {tapRequirement(0, 1000, 0),
                                                                      tapRequirement(1, 2000, 1)};
            auto score = tapUnitScore(requirements, {ScoreUnit{{0}}, ScoreUnit{{1}}});
            score.basis = ScoreBasis{4, 3};
            runtime::PrepareGameplayLimits limits{};
            limits.maxRequirements = runtime::MeasuredParameter<std::uint64_t>::measured(1);
            auto view = singleRequirementView(timebase, requirements, policy, identities, score);
            auto result = runtime::prepareCompiledGameplay(view, limits);
            REQUIRE_FALSE(result);
            CHECK(result.error().code() == "capability.budget_insufficient");
            CHECK(contextValue(result.error(), "fieldPath") == "/limits/maxRequirements");
            CHECK(prepared->settlementCapacity() == 1);
        }

        //  Cross-reference and shape rejections.
        {
            std::vector<runtime::PreparedRequirement> requirements = {tapRequirement(0, 1000, 0)};
            auto score = tapUnitScore(requirements, {ScoreUnit{{0}}});
            score.basis = ScoreBasis{2, 1};
            runtime::PrepareGameplayLimits limits{};
            runtime::PreparedTimerKey timer;
            timer.requirement = requirements.front().identity;
            timer.requirement.requirementLocalId = "unknown";
            timer.phase = PhaseKind::tap;
            auto view = singleRequirementView(timebase, requirements, policy, identities, score);
            view.timers = std::span<const runtime::PreparedTimerKey>{&timer, 1};
            CHECK_FALSE(runtime::prepareCompiledGameplay(view, limits));
        }

        //  An empty view prepares with capacity zero.
        {
            runtime::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
            runtime::CompiledGameplayView view{timebase,
                                               policy,
                                               identities,
                                               "",
                                               "",
                                               "",
                                               nullptr,
                                               nullptr,
                                               std::span<const runtime::PreparedRequirement>{},
                                               std::span<const runtime::PreparedResource>{},
                                               std::span<const runtime::PreparedTimerKey>{}};
            runtime::PrepareGameplayLimits limits{};
            auto empty = runtime::prepareCompiledGameplay(view, limits);
            REQUIRE(empty);
            CHECK(empty->settlementCapacity() == 0);
            CHECK(empty->settlementDictionary().phases.empty());
        }
    }

    SECTION("the runtime advance kernel numbers before late and keeps the cursor") {
        auto prepared = preparedTapGameplay();
        auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{7, 1});
        REQUIRE(kernel);
        CHECK_FALSE(kernel->acceptedIngressThrough().has_value());
        CHECK(kernel->nextSequence().value() == 0);

        REQUIRE(kernel->advance(Tick{100}));
        auto batch = std::vector<runtime::TickRecord>{tickRecord(94), tickRecord(10)};
        auto receipts = kernel->receiveCanonicalBatch(batch);
        REQUIRE(receipts);
        REQUIRE(receipts->size() == 2);
        CHECK(receipts->at(0).sequence.value() == 0);
        CHECK(receipts->at(0).accepted);
        CHECK(receipts->at(0).forwarded);
        CHECK(receipts->at(0).dispatchTick == Tick{98});
        CHECK(receipts->at(1).sequence.value() == 1);
        CHECK_FALSE(receipts->at(1).accepted);
        CHECK(receipts->at(1).rejectionCode == "judgement.late.rejected");
        CHECK(kernel->acceptedIngressThrough()->value() == 1);
        CHECK(kernel->nextSequence().value() == 2);
        CHECK(kernel->archivedRecords().size() == 2);

        //  A backward horizon keeps frontier and records unchanged.
        auto reversed = kernel->advance(Tick{99});
        REQUIRE_FALSE(reversed);
        CHECK(reversed.error().code() == "judgement.execution.horizon_reversed");
        CHECK(kernel->frontier() == Tick{100});
        CHECK(kernel->archivedRecords().size() == 2);
    }

    SECTION("CLK-02 Stop locks the prefix and drains only records at or before cutoff") {
        auto prepared = preparedTapGameplay();
        auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{9, 2});
        REQUIRE(kernel);
        auto batch = std::vector<runtime::TickRecord>{tickRecord(105), tickRecord(90)};
        auto receipts = kernel->receiveCanonicalBatch(batch);
        REQUIRE(receipts);
        CHECK(receipts->at(0).sequence.value() == 0);
        CHECK(receipts->at(1).sequence.value() == 1);
        CHECK(kernel->acceptedIngressThrough()->value() == 1);

        //  The first-fault order is run, bound, cutoff, slots.
        const runtime::StopCut wrongRun{RunKey{9, 3}, runtime::IngressSequence{0}, Tick{100}};
        auto runError = kernel->advanceStopCut(wrongRun, std::span<SettlementEvent>{});
        REQUIRE_FALSE(runError);
        CHECK(runError.error().code() == "judgement.stop_cut.run_mismatch");
        const runtime::StopCut wrongBound{RunKey{9, 2}, runtime::IngressSequence{0}, Tick{100}};
        auto boundError = kernel->advanceStopCut(wrongBound, std::span<SettlementEvent>{});
        REQUIRE_FALSE(boundError);
        CHECK(boundError.error().code() == "judgement.stop_cut.bound_mismatch");
        CHECK_FALSE(kernel->receiveClosed());
        CHECK(kernel->acceptedIngressThrough()->value() == 1);

        std::vector<SettlementEvent> slots(prepared.settlementCapacity());
        auto stop = kernel->advanceStopCut(
            runtime::StopCut{RunKey{9, 2}, runtime::IngressSequence{1}, Tick{100}}, slots);
        REQUIRE(stop);
        CHECK(stop->processedThrough == Tick{100});
        CHECK(stop->processedRecords == 1);
        CHECK(kernel->receiveClosed());
        CHECK(kernel->frontier() == Tick{100});

        const auto sequenceAfterStop = kernel->nextSequence().value();
        auto closed =
            kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(95)});
        REQUIRE_FALSE(closed);
        CHECK(closed.error().code() == "judgement.stop_cut.closed");
        CHECK(kernel->nextSequence().value() == sequenceAfterStop);
        CHECK(kernel->archivedRecords().size() == 2);
    }

    SECTION("a StopCut validates cutoff and slots after run and bound") {
        auto prepared = preparedTapGameplay();
        auto makeKernel = [&] {
            auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{1, 1});
            REQUIRE(kernel);
            REQUIRE(kernel->advance(Tick{100}));
            auto receipts =
                kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(99)});
            REQUIRE(receipts);
            return std::move(*kernel);
        };

        auto reversed = makeKernel();
        auto cutoffError = reversed.advanceStopCut(
            runtime::StopCut{RunKey{1, 1}, runtime::IngressSequence{0}, Tick{99}},
            std::span<SettlementEvent>{});
        REQUIRE_FALSE(cutoffError);
        CHECK(cutoffError.error().code() == "judgement.stop_cut.cutoff_reversed");

        auto shortSlots = makeKernel();
        auto slotError = shortSlots.advanceStopCut(
            runtime::StopCut{RunKey{1, 1}, runtime::IngressSequence{0}, Tick{100}},
            std::span<SettlementEvent>{});
        REQUIRE_FALSE(slotError);
        CHECK(slotError.error().code() == "capability.budget_insufficient");
        CHECK_FALSE(shortSlots.receiveClosed());
    }

    SECTION("the GP-02 short Hold shape prepares on the runtime records") {
        auto requirement = shortHoldRequirement();
        std::vector<runtime::PreparedRequirement> requirements{requirement};
        auto policy = runtimePolicy();
        auto identities = runtimeIdentities();
        const runtime::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
        auto score = tapUnitScore(requirements, {});
        auto view = singleRequirementView(timebase, requirements, policy, identities, score);
        auto prepared = runtime::prepareCompiledGameplay(view, runtime::PrepareGameplayLimits{});
        REQUIRE(prepared);
        CHECK(prepared->requirements().front().timing.body->end == Tick{851});
        auto kernel = runtime::AdvanceKernel::prepare(*prepared);
        REQUIRE(kernel);
        CHECK(kernel->nextSequence().value() == 0);
        CHECK((runtime::releaseObservationDomain(850, 851, 4)->end == 851));

        //  press@850 -> advance(854) -> a late release observation 849 routes to dispatch 852.
        REQUIRE(kernel->advance(Tick{854}));
        auto receipts = kernel->receiveCanonicalBatch(
            std::vector<runtime::TickRecord>{tickRecord(849, runtime::InputAction::release)});
        REQUIRE(receipts);
        REQUIRE(receipts->size() == 1);
        CHECK(receipts->front().forwarded);
        CHECK(receipts->front().dispatchTick == Tick{852});
        CHECK(receipts->front().accepted);
    }
}
