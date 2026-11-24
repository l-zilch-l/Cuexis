// T-10 advance contract: value, score, kernel and GP-03 fault/priority matrix.
//
// This case covers the runtime value headers, the prepared identity declarations, the advance value
// contract, the normalized score records and the shared exact integer helpers (GP-05 and GP-04),
// the kernel transaction and the GP-03 settlement fault/priority matrix. The Playback owning DTO
// projection and the session-level recovery archive are not part of this case; the report records
// that remainder and its conditions.

#include <cuexis/judgement/advance.hpp>
#include <cuexis/judgement/advance_kernel.hpp>
#include <cuexis/judgement/gameplay_assembler_identity.hpp>
#include <cuexis/judgement/gameplay_graph_reference.hpp>
#include <cuexis/judgement/normalized_score.hpp>
#include <cuexis/judgement/phase_types.hpp>
#include <cuexis/judgement/prepared_gameplay.hpp>
#include <cuexis/judgement/prepared_gameplay_records.hpp>
#include <cuexis/judgement/prepared_identity.hpp>

#include "advance_testing.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
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
    requirement.atomBindings = {runtime::AtomBinding{"atom.press", "domain.binding.one", "keyboard",
                                                     "channel.zero", runtime::InputAction::press,
                                                     std::nullopt, false}};
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

auto gp02Score() -> PreparedNormalizedScore {
    PreparedNormalizedScore score;
    auto& mode = score.mode;
    mode.id = "hold";
    mode.total = 100;
    mode.mj = 90;
    mode.mc = 10;
    mode.onExhausted = ComboAction::reset;
    mode.comboRewards = {ComboRewardSegment{1, 1, 1}};
    mode.rules = {
        ScoreRuleRow{PhaseKind::head, Outcome::hit, std::nullopt, true, 2, ComboAction::increment},
        ScoreRuleRow{PhaseKind::head, Outcome::miss, std::nullopt, false, 0, ComboAction::keep},
        ScoreRuleRow{PhaseKind::body, Outcome::hit, std::nullopt, true, 2, ComboAction::increment},
        ScoreRuleRow{PhaseKind::body, Outcome::miss, std::nullopt, false, 0, ComboAction::keep},
        ScoreRuleRow{PhaseKind::body, Outcome::hit, std::string{"break"}, true, 2,
                     ComboAction::increment},
        ScoreRuleRow{PhaseKind::body, Outcome::miss, std::string{"break"}, true, 1,
                     ComboAction::reset},
        ScoreRuleRow{PhaseKind::body, Outcome::hit, std::string{"release"}, true, 2,
                     ComboAction::increment},
        ScoreRuleRow{PhaseKind::body, Outcome::miss, std::string{"release"}, true, 1,
                     ComboAction::reset},
        ScoreRuleRow{PhaseKind::tail, Outcome::hit, std::string{"break"}, true, 6,
                     ComboAction::increment},
        ScoreRuleRow{PhaseKind::tail, Outcome::miss, std::string{"break"}, true, 1,
                     ComboAction::reset},
        ScoreRuleRow{PhaseKind::tail, Outcome::hit, std::string{"release"}, true, 6,
                     ComboAction::increment},
        ScoreRuleRow{PhaseKind::tail, Outcome::miss, std::string{"release"}, true, 1,
                     ComboAction::reset},
        ScoreRuleRow{PhaseKind::tail, Outcome::miss, std::nullopt, false, 0, ComboAction::keep},
    };
    PreparedGradeTable table;
    table.id = "main";
    table.exactStart = -154;
    table.exactEnd = 20;
    table.intervals = {PreparedGradeInterval{-154, 0, "break"},
                       PreparedGradeInterval{0, 20, "release"}};
    score.gradeTables = {table};
    score.units = {ScoreUnit{{0}}, ScoreUnit{{1}}, ScoreUnit{{2}}};
    score.basis = ScoreBasis{10, 6};
    return score;
}

auto gp02Requirement(bool shortHold) -> runtime::PreparedRequirement {
    runtime::PreparedRequirement requirement;
    requirement.stableId = StableDeclarationId{"doc.alpha", 77};
    requirement.identity = RequirementIdentity{
        "chart.one", "invocation.one", "module.one", "export.one", {}, "req.hold"};
    requirement.phases = {PhaseDeclaration{PhaseKind::head, 0},
                          PhaseDeclaration{PhaseKind::body, 0},
                          PhaseDeclaration{PhaseKind::tail, 0}};
    const Tick bodyStart{shortHold ? 850 : 900};
    const Tick bodyEnd{shortHold ? 851 : 1000};
    const Tick bodyTarget{shortHold ? 851 : 1000};
    const Tick headTarget{shortHold ? 850 : 900};
    requirement.timing.end = Tick{1040};
    requirement.timing.body = TimeInterval{bodyStart, bodyEnd};
    requirement.timing.successWindows = {
        runtime::SuccessWindow{Tick{850}, Tick{901}, requirement.phases[0]},
        runtime::SuccessWindow{bodyStart, bodyEnd, requirement.phases[1]},
        runtime::SuccessWindow{Tick{1000}, Tick{1010}, requirement.phases[2]},
        runtime::SuccessWindow{Tick{1015}, Tick{1020}, requirement.phases[2]}};
    requirement.timing.phaseTargets = {runtime::PhaseTarget{PhaseKind::head, Tick{headTarget}},
                                       runtime::PhaseTarget{PhaseKind::body, Tick{bodyTarget}},
                                       runtime::PhaseTarget{PhaseKind::tail, Tick{1000}}};
    requirement.deadline = Tick{1040};
    requirement.atomBindings = {
        runtime::AtomBinding{"atom.press", "domain.binding.one", "keyboard", "channel.zero",
                             runtime::InputAction::press, std::nullopt, false},
        runtime::AtomBinding{"atom.release", "domain.binding.one", "keyboard", "channel.zero",
                             runtime::InputAction::release, std::nullopt, false}};
    requirement.measure = {
        runtime::CompiledMeasureComponent{
            PhaseKind::body, FactCategory::holdBody, runtime::GradePresence::declared, {}, 0},
        runtime::CompiledMeasureComponent{
            PhaseKind::tail, FactCategory::holdTail, runtime::GradePresence::declared, {}, 0}};
    return requirement;
}

auto gp02Prepared(bool shortHold) -> runtime::PreparedGameplay {
    std::vector<runtime::PreparedRequirement> requirements = {gp02Requirement(shortHold)};
    auto score = gp02Score();
    auto policy = runtimePolicy();
    auto identities = runtimeIdentities();
    const runtime::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
    runtime::CompiledGameplayView view{
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
    auto prepared = runtime::prepareCompiledGameplay(view, runtime::PrepareGameplayLimits{});
    REQUIRE(prepared);
    return *prepared;
}

auto gp04Score() -> PreparedNormalizedScore {
    PreparedNormalizedScore score;
    auto& mode = score.mode;
    mode.id = "gp04";
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
    for (const char* id : {"A", "B", "solo"}) {
        PreparedGradeTable table;
        table.id = id;
        table.exactStart = 0;
        table.exactEnd = 1;
        table.intervals = {PreparedGradeInterval{0, 1, id}};
        score.gradeTables.push_back(std::move(table));
    }
    return score;
}

auto gp04Prepared(std::size_t count, bool explicitUnits, bool stagger = false)
    -> runtime::PreparedGameplay {
    std::vector<runtime::PreparedRequirement> requirements;
    requirements.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto target = stagger && index > 0 ? 1100 : 1000;
        auto requirement = tapRequirement(index, target, explicitUnits ? index % 3 : 0);
        requirement.atomBindings.front().channelToken = "channel." + std::to_string(index);
        requirements.push_back(std::move(requirement));
    }
    auto score = gp04Score();
    if (explicitUnits && count == 3) {
        score.units = {ScoreUnit{{0, 1}}, ScoreUnit{{2}}};
        score.basis = ScoreBasis{8, 3};
    } else {
        score.units.clear();
        for (std::size_t index = 0; index < count; ++index) {
            score.units.push_back(ScoreUnit{{static_cast<std::uint32_t>(index)}});
        }
        score.basis = ScoreBasis{2 * count, count * (count + 1) / 2};
    }
    auto policy = runtimePolicy();
    auto identities = runtimeIdentities();
    const runtime::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
    runtime::CompiledGameplayView view{
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
    auto prepared = runtime::prepareCompiledGameplay(view, runtime::PrepareGameplayLimits{});
    REQUIRE(prepared);
    return *prepared;
}

auto gp04Record(std::size_t index) -> runtime::TickRecord {
    return runtime::TickRecord{Tick{1000},
                               "domain.binding.one",
                               "keyboard",
                               "channel." + std::to_string(index),
                               runtime::InputAction::press,
                               std::nullopt};
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
    requirement.atomBindings = {
        runtime::AtomBinding{"atom.press", "domain.binding.one", "keyboard", "channel.zero",
                             runtime::InputAction::press, std::nullopt, false},
        runtime::AtomBinding{"atom.release", "domain.binding.one", "keyboard", "channel.zero",
                             runtime::InputAction::release, std::nullopt, false}};
    return requirement;
}

auto emptyPrepared() -> runtime::PreparedGameplay {
    auto policy = runtimePolicy();
    auto identities = runtimeIdentities();
    const runtime::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
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
    auto prepared = runtime::prepareCompiledGameplay(view, runtime::PrepareGameplayLimits{});
    REQUIRE(prepared);
    return std::move(*prepared);
}

auto sentinelEvent() -> SettlementEvent {
    SettlementEvent event{};
    event.tick = Tick{-777};
    event.phaseIndex = 7;
    event.gradeIndex = 9;
    event.outcome = Outcome::miss;
    event.error = TickDelta{-999};
    event.direction = TimingDirection::late;
    return event;
}

auto isSentinel(const SettlementEvent& event) -> bool {
    return event.tick == Tick{-777} && event.phaseIndex == 7 && event.gradeIndex == 9 &&
           event.outcome == Outcome::miss && event.error.has_value() &&
           event.error->value() == -999 && event.direction == TimingDirection::late;
}

auto runtimeConfiguration(const runtime::PreparedGameplay& prepared)
    -> runtime::SessionConfiguration {
    const auto records = prepared.records();
    runtime::SessionConfiguration configuration;
    configuration.timebase = records.timebase;
    configuration.latePolicy = records.latePolicy;
    configuration.identityDeclarations = prepared.identityDeclarations();
    return configuration;
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
        CHECK(reversed.error().code() == "judgement.timebase.time_reversal");
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
        CHECK(runError.error().code() == "session.mutation_rejected");
        CHECK(contextValue(runError.error(), "fieldPath") == "/run");
        const runtime::StopCut wrongBound{RunKey{9, 2}, runtime::IngressSequence{0}, Tick{100}};
        auto boundError = kernel->advanceStopCut(wrongBound, std::span<SettlementEvent>{});
        REQUIRE_FALSE(boundError);
        CHECK(boundError.error().code() == "judgement.execution.relation_invalid");
        CHECK(contextValue(boundError.error(), "fieldPath") == "/acceptedThrough");
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
        CHECK(closed.error().code() == "judgement.execution.relation_invalid");
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
        CHECK(cutoffError.error().code() == "judgement.timebase.time_reversal");

        auto shortSlots = makeKernel();
        auto slotError = shortSlots.advanceStopCut(
            runtime::StopCut{RunKey{1, 1}, runtime::IngressSequence{0}, Tick{100}},
            std::span<SettlementEvent>{});
        REQUIRE_FALSE(slotError);
        CHECK(slotError.error().code() == "judgement.advance.output_capacity_insufficient");
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
        auto kernel = runtime::AdvanceKernel::prepare(*prepared, RunKey{1, 1});
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

    SECTION("the short Hold settles Tail Miss with the observation error") {
        auto requirement = shortHoldRequirement();
        std::vector<runtime::PreparedRequirement> requirements{requirement};
        auto policy = runtimePolicy();
        auto identities = runtimeIdentities();
        const runtime::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
        auto score = tapUnitScore(requirements, {});
        auto view = singleRequirementView(timebase, requirements, policy, identities, score);
        auto prepared = runtime::prepareCompiledGameplay(view, runtime::PrepareGameplayLimits{});
        REQUIRE(prepared);
        auto kernel = runtime::AdvanceKernel::prepare(*prepared, RunKey{1, 1});
        REQUIRE(kernel);

        auto press =
            kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(850)});
        REQUIRE(press);
        std::vector<SettlementEvent> slots(prepared->settlementCapacity());
        auto first = kernel->advance(Tick{854}, slots);
        REQUIRE(first);
        CHECK(first->settledEvents == 2); // Head Hit and the Body timer Hit
        auto release = kernel->receiveCanonicalBatch(
            std::vector<runtime::TickRecord>{tickRecord(849, runtime::InputAction::release)});
        REQUIRE(release);
        REQUIRE(release->front().forwarded);
        auto second = kernel->advance(Tick{854}, slots);
        REQUIRE(second);
        REQUIRE(second->settledEvents == 1);
        const auto& event = slots.front();
        CHECK(event.tick == Tick{852});
        CHECK(event.phaseIndex == 2);
        CHECK(event.outcome == Outcome::miss);
        REQUIRE(event.error.has_value());
        CHECK(event.error->value() == -151);
        //  This phase selects no grade table, so the real error survives while both the grade and
        //  the direction stay absent (GP-01 "旧未声明表样本无 grade/方向").
        CHECK_FALSE(event.direction.has_value());
    }

    SECTION("the settlement slots reject the whole transaction when too small") {
        auto prepared = preparedTapGameplay();
        auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{2, 2});
        REQUIRE(kernel);
        REQUIRE(kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(1000)}));
        auto rejected = kernel->advance(Tick{1000}, std::span<SettlementEvent>{});
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "judgement.advance.output_capacity_insufficient");
        CHECK_FALSE(kernel->frontier().has_value());
        std::vector<SettlementEvent> slots(1);
        auto accepted = kernel->advance(Tick{1000}, slots);
        REQUIRE(accepted);
        CHECK(accepted->settledEvents == 1);
        CHECK(slots.front().outcome == Outcome::hit);
        CHECK(accepted->j == 2);
        CHECK(accepted->c == 1);
        CHECK(accepted->score == 1000000);
    }

    SECTION("the per-tick settlement is idempotent after it is sealed") {
        auto prepared = preparedTapGameplay();
        auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{3, 3});
        REQUIRE(kernel);
        REQUIRE(kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(1000)}));
        std::vector<SettlementEvent> slots(1);
        REQUIRE(kernel->advance(Tick{1050}, slots));
        auto again = kernel->advance(Tick{1060}, slots);
        REQUIRE(again);
        CHECK(again->settledEvents == 0);
        CHECK(again->j == 2);
    }

    SECTION("the GP-02 Hold vectors settle their phase units") {
        auto settle = [](std::optional<Tick> release) {
            auto prepared = gp02Prepared(false);
            auto kernel = runtime::AdvanceKernel::prepare(std::move(prepared), RunKey{9, 1});
            REQUIRE(kernel);
            std::vector<runtime::TickRecord> batch{tickRecord(900)};
            if (release) {
                batch.push_back(tickRecord(release->value(), runtime::InputAction::release));
            }
            REQUIRE(kernel->receiveCanonicalBatch(batch));
            std::vector<SettlementEvent> slots(3);
            auto advanced = kernel->advance(Tick{1103}, slots);
            REQUIRE(advanced);
            return *advanced;
        };
        const auto first = settle(Tick{950});
        CHECK(first.j == 3);
        CHECK(first.c == 1);
        CHECK(first.score == 29);
        const auto second = settle(Tick{1011});
        CHECK(second.j == 5);
        CHECK(second.c == 3);
        CHECK(second.score == 50);
        const auto third = settle(std::nullopt);
        CHECK(third.j == 4);
        CHECK(third.c == 3);
        CHECK(third.score == 41);
        const auto fourth = settle(Tick{1019});
        CHECK(fourth.j == 10);
        CHECK(fourth.c == 6);
        CHECK(fourth.score == 100);
    }

    SECTION("the GP-04 single-A units scale to 200 and 1000") {
        auto run = [](std::size_t count) {
            auto kernel =
                runtime::AdvanceKernel::prepare(gp04Prepared(count, false), RunKey{10, 1});
            REQUIRE(kernel);
            std::vector<runtime::TickRecord> batch;
            for (std::size_t index = 0; index < count; ++index) {
                batch.push_back(gp04Record(index));
            }
            REQUIRE(kernel->receiveCanonicalBatch(batch));
            std::vector<SettlementEvent> slots(count);
            auto advanced = kernel->advance(Tick{1000}, slots);
            REQUIRE(advanced);
            return *advanced;
        };
        const auto small = run(200);
        CHECK(small.j == 400);
        CHECK(small.c == 20100);
        CHECK(small.score == 1000000);
        CHECK(small.jMax == 400);
        CHECK(small.cMax == 20100);
        CHECK(small.mj == 900000);
        CHECK(small.mc == 100000);
        CHECK(small.combo == 200);
        CHECK(small.maxCombo == 200);
        const auto large = run(1000);
        CHECK(large.j == 2000);
        CHECK(large.c == 500500);
        CHECK(large.score == 1000000);
        CHECK(large.jMax == 2000);
        CHECK(large.cMax == 500500);
        CHECK(large.combo == 1000);
    }

    SECTION("the GP-04 explicit unit shares one award") {
        auto settle = [](std::vector<std::size_t> order) {
            auto kernel = runtime::AdvanceKernel::prepare(gp04Prepared(3, true), RunKey{11, 1});
            REQUIRE(kernel);
            std::vector<runtime::TickRecord> batch;
            for (const auto index : order) {
                batch.push_back(gp04Record(index));
            }
            REQUIRE(kernel->receiveCanonicalBatch(batch));
            std::vector<SettlementEvent> slots(3);
            auto advanced = kernel->advance(Tick{1000}, slots);
            REQUIRE(advanced);
            return *advanced;
        };
        const auto aFirst = settle({0, 1, 2});
        CHECK(aFirst.j == 5);
        CHECK(aFirst.c == 3);
        CHECK(aFirst.score == 662500);
        CHECK(aFirst.jMax == 8);
        CHECK(aFirst.cMax == 3);
        CHECK(aFirst.combo == 2);
        CHECK(aFirst.maxCombo == 2);
        const auto bFirst = settle({1, 0, 2});
        CHECK(bFirst.j == 8);
        CHECK(bFirst.score == 1000000);

        auto kernel = runtime::AdvanceKernel::prepare(gp04Prepared(3, true, true), RunKey{12, 1});
        REQUIRE(kernel);
        std::vector<SettlementEvent> slots(3);
        REQUIRE(kernel->advance(Tick{1050}, slots)); // A's null Miss, no early reset
        const auto late = [](std::size_t index) {
            return runtime::TickRecord{Tick{1100},
                                       "domain.binding.one",
                                       "keyboard",
                                       "channel." + std::to_string(index),
                                       runtime::InputAction::press,
                                       std::nullopt};
        };
        CHECK(kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{late(1), late(2)}));
        auto later = kernel->advance(Tick{1100}, slots);
        REQUIRE(later);
        CHECK(later->j == 8);
        CHECK(later->score == 1000000);
    }

    SECTION("the replay keeps true numbering and settled events after Stop") {
        auto kernel = runtime::AdvanceKernel::prepare(gp02Prepared(false), RunKey{13, 1});
        REQUIRE(kernel);
        REQUIRE(kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{
            tickRecord(900), tickRecord(1019, runtime::InputAction::release)}));
        std::vector<SettlementEvent> slots(3);
        REQUIRE(kernel->advance(Tick{1103}, slots));
        const auto open = kernel->replay();
        REQUIRE(open.records.size() == 2);
        CHECK(open.records[0].sequence == runtime::IngressSequence{0});
        CHECK(open.records[1].sequence == runtime::IngressSequence{1});
        CHECK(open.events.size() == 3);
        CHECK(open.frontier == Tick{1103});
        CHECK(open.j == 10);
        CHECK(open.score == 100);

        auto stopped = runtime::AdvanceKernel::prepare(preparedTapGameplay(), RunKey{14, 1});
        REQUIRE(stopped);
        REQUIRE(stopped->receiveCanonicalBatch(
            std::vector<runtime::TickRecord>{tickRecord(1100), tickRecord(1000)}));
        std::vector<SettlementEvent> stopSlots(3);
        auto cut = stopped->advanceStopCut(
            runtime::StopCut{RunKey{14, 1}, stopped->acceptedIngressThrough(), Tick{1000}},
            stopSlots);
        REQUIRE(cut);
        CHECK(cut->settledEvents == 1);
        const auto replay = stopped->replay();
        REQUIRE(replay.records.size() == 2);
        CHECK(replay.records[0].sequence == runtime::IngressSequence{0});
        CHECK(replay.records[1].sequence == runtime::IngressSequence{1});
        CHECK(replay.records[1].accepted);
        CHECK(replay.events.size() == 1);
        CHECK(replay.frontier == Tick{1000});
        CHECK(stopped->nextSequence() == runtime::IngressSequence{2});
        auto rejected =
            stopped->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(95)});
        REQUIRE_FALSE(rejected);
        CHECK(stopped->replay().records.size() == 2);
        CHECK(stopped->nextSequence() == runtime::IngressSequence{2});
    }

    SECTION("GP-03 the settlement event layout is checked per type") {
        STATIC_REQUIRE(sizeof(Tick) == 8);
        STATIC_REQUIRE(sizeof(TickDelta) == 8);
        STATIC_REQUIRE(sizeof(Outcome) == 1);
        STATIC_REQUIRE(sizeof(TimingDirection) == 1);
        STATIC_REQUIRE(sizeof(std::uint32_t) == 4);
        STATIC_REQUIRE(sizeof(std::optional<TickDelta>) == 16);
        STATIC_REQUIRE(sizeof(std::optional<TimingDirection>) == 2);
        STATIC_REQUIRE(sizeof(SettlementEvent) == 48);
        STATIC_REQUIRE(alignof(SettlementEvent) == 8);
        STATIC_REQUIRE(sizeof(runtime::IngressSequence) == 8);
        STATIC_REQUIRE(sizeof(FaultCode) == 2);
        STATIC_REQUIRE(sizeof(NormalizedScoreResult) == 56);
        CHECK(sizeof(runtime::AdvanceSummary) >= sizeof(SettlementEvent));
    }

    SECTION("GP-03 P-1 slots are refused without touching the buffer, ledger or frontier") {
        auto prepared = gp02Prepared(false);
        REQUIRE(prepared.settlementCapacity() == 3);
        auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{20, 1});
        REQUIRE(kernel);
        REQUIRE(kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)}));
        const auto archiveBefore = kernel->archivedRecords().size();

        std::vector<SettlementEvent> shortSlots(2, sentinelEvent());
        auto refused = kernel->advance(Tick{900}, shortSlots);
        REQUIRE_FALSE(refused);
        CHECK(refused.error().code() == "judgement.advance.output_capacity_insufficient");
        CHECK(contextValue(refused.error(), "fieldPath") == "/slots");
        CHECK(contextValue(refused.error(), "required") == "3");
        CHECK(contextValue(refused.error(), "provided") == "2");
        for (const auto& event : shortSlots) {
            CHECK(isSentinel(event));
        }
        CHECK_FALSE(kernel->frontier().has_value());
        CHECK(kernel->archivedRecords().size() == archiveBefore);
        CHECK(kernel->replay().events.empty());
        CHECK(kernel->nextSequence().value() == 1);

        //  With the exact P the same call returns the original event and leaves the tail of the
        //  buffer at its previous value.
        std::vector<SettlementEvent> slots(3, sentinelEvent());
        auto sealed = kernel->advance(Tick{900}, slots);
        REQUIRE(sealed);
        REQUIRE(sealed->settledEvents == 1);
        CHECK(sealed->status == AdvanceStatus::ongoing);
        CHECK(slots[0].tick == Tick{900});
        CHECK(slots[0].phaseIndex == 0);
        CHECK(slots[0].outcome == Outcome::hit);
        CHECK(isSentinel(slots[1]));
        CHECK(isSentinel(slots[2]));

        auto second = kernel->advance(Tick{1103}, slots);
        REQUIRE(second);
        CHECK(second->settledEvents == 2);
        CHECK(slots[0].phaseIndex == 1); // body timer Hit
        CHECK(slots[0].outcome == Outcome::hit);
        CHECK(slots[1].phaseIndex == 2); // tail timer Miss
        CHECK(slots[1].outcome == Outcome::miss);
    }

    SECTION("GP-03 two sessions with the same epoch key refuse each other's StopCut") {
        auto prepared = gp02Prepared(false);
        auto first = runtime::AdvanceKernel::prepare(prepared, RunKey{21, 3});
        auto second = runtime::AdvanceKernel::prepare(prepared, RunKey{21, 3});
        REQUIRE(first);
        REQUIRE(second);
        REQUIRE(first->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)}));
        REQUIRE(second->receiveCanonicalBatch(std::vector<runtime::TickRecord>{
            tickRecord(900), tickRecord(1019, runtime::InputAction::release)}));
        CHECK(first->acceptedIngressThrough() == runtime::IngressSequence{0});
        CHECK(second->acceptedIngressThrough() == runtime::IngressSequence{1});

        std::vector<SettlementEvent> slots(3, sentinelEvent());
        auto secondRejectsFirst = second->advanceStopCut(
            runtime::StopCut{RunKey{21, 3}, first->acceptedIngressThrough(), Tick{1103}}, slots);
        REQUIRE_FALSE(secondRejectsFirst);
        CHECK(secondRejectsFirst.error().code() == "judgement.execution.relation_invalid");
        CHECK(contextValue(secondRejectsFirst.error(), "fieldPath") == "/acceptedThrough");
        CHECK_FALSE(second->receiveClosed());
        CHECK_FALSE(second->frontier().has_value());
        CHECK(second->nextSequence().value() == 2);
        for (const auto& event : slots) {
            CHECK(isSentinel(event));
        }

        auto firstRejectsSecond = first->advanceStopCut(
            runtime::StopCut{RunKey{21, 3}, second->acceptedIngressThrough(), Tick{1103}}, slots);
        REQUIRE_FALSE(firstRejectsSecond);
        CHECK(contextValue(firstRejectsSecond.error(), "fieldPath") == "/acceptedThrough");
        CHECK_FALSE(first->receiveClosed());

        //  Same epoch, different session scope: advance refuses the foreign key before any state
        //  change. An identical (scope, epoch) pair carries no instance identity in `RunKey`, so
        //  the kernel can only separate sessions whose scope or bound differs.
        auto other = runtime::AdvanceKernel::prepare(prepared, RunKey{31, 3});
        REQUIRE(other);
        std::vector<SettlementEvent> advanceSlots(3, sentinelEvent());
        auto crossAdvance = first->advance(Tick{900}, other->run(), advanceSlots);
        REQUIRE_FALSE(crossAdvance);
        CHECK(crossAdvance.error().code() == "session.mutation_rejected");
        CHECK(contextValue(crossAdvance.error(), "category") == "invalid_relation");
        CHECK(contextValue(crossAdvance.error(), "fieldPath") == "/run");
        CHECK_FALSE(first->frontier().has_value());
        CHECK(first->nextSequence().value() == 1);
        for (const auto& event : advanceSlots) {
            CHECK(isSentinel(event));
        }
        auto reverseAdvance = other->advance(Tick{900}, first->run(), advanceSlots);
        REQUIRE_FALSE(reverseAdvance);
        CHECK(reverseAdvance.error().code() == "session.mutation_rejected");
        CHECK(contextValue(reverseAdvance.error(), "fieldPath") == "/run");
        CHECK_FALSE(other->frontier().has_value());
        CHECK(other->nextSequence().value() == 0);

        //  The StopCut form reports the same wrong run at /run before the bound check.
        auto stopRun = first->advanceStopCut(
            runtime::StopCut{other->run(), first->acceptedIngressThrough(), Tick{1103}}, slots);
        REQUIRE_FALSE(stopRun);
        CHECK(stopRun.error().code() == "session.mutation_rejected");
        CHECK(contextValue(stopRun.error(), "fieldPath") == "/run");
        CHECK_FALSE(first->receiveClosed());
        CHECK_FALSE(first->frontier().has_value());

        auto firstStop = first->advanceStopCut(
            runtime::StopCut{RunKey{21, 3}, first->acceptedIngressThrough(), Tick{1103}}, slots);
        REQUIRE(firstStop);
        CHECK(firstStop->settledEvents == 3);
        auto secondStop = second->advanceStopCut(
            runtime::StopCut{RunKey{21, 3}, second->acceptedIngressThrough(), Tick{1103}}, slots);
        REQUIRE(secondStop);
        CHECK(secondStop->settledEvents == 3);
        CHECK(first->replay().events.size() == 3);
        CHECK(second->replay().events.size() == 3);
        CHECK(first->nextSequence().value() == 1);
        CHECK(second->nextSequence().value() == 2);
    }

    SECTION("GP-03 retained values survive unload and a Retry starts a new epoch") {
        auto prepared = gp02Prepared(false);
        auto dictionary = prepared.settlementDictionary(); // host owning copy before the run
        std::vector<SettlementEvent> slots(3);
        SettlementEvent copiedEvent{};
        runtime::ReplaySnapshot stopped;
        {
            auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{22, 1});
            REQUIRE(kernel);
            REQUIRE(kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{
                tickRecord(900), tickRecord(1019, runtime::InputAction::release)}));
            auto stop = kernel->advanceStopCut(
                runtime::StopCut{RunKey{22, 1}, kernel->acceptedIngressThrough(), Tick{1103}},
                slots);
            REQUIRE(stop);
            REQUIRE(stop->settledEvents == 3);
            copiedEvent = slots.front(); // by value
            stopped = kernel->replay();

            //  The same-run state is not re-emitted: a second Stop is refused and the replay
            //  stays the same owning value.
            auto repeated = kernel->advanceStopCut(
                runtime::StopCut{RunKey{22, 1}, kernel->acceptedIngressThrough(), Tick{1103}},
                slots);
            REQUIRE_FALSE(repeated);
            CHECK(repeated.error().code() == "judgement.execution.relation_invalid");
            CHECK(kernel->replay().events.size() == 3);
        }
        //  The session is unloaded; the host copies stay readable.
        CHECK(dictionary.phases.size() == 3);
        CHECK(dictionary.grades == std::vector<std::string>{"break", "release"});
        CHECK(copiedEvent.tick == Tick{900});
        CHECK(copiedEvent.phaseIndex == 0);
        CHECK(copiedEvent.outcome == Outcome::hit);
        CHECK(stopped.events.size() == 3);
        CHECK(stopped.frontier == Tick{1103});

        //  Retry takes a new epoch and starts from a zero cursor with no re-emission.
        auto retry = runtime::AdvanceKernel::prepare(prepared, RunKey{22, 2});
        REQUIRE(retry);
        CHECK_FALSE(retry->acceptedIngressThrough().has_value());
        CHECK(retry->nextSequence().value() == 0);
        CHECK(retry->replay().records.empty());
        CHECK(retry->replay().events.empty());
        CHECK_FALSE(retry->frontier().has_value());
        auto batch = retry->receiveCanonicalBatch(std::vector<runtime::TickRecord>{
            tickRecord(900), tickRecord(1019, runtime::InputAction::release)});
        REQUIRE(batch);
        CHECK(batch->at(0).sequence.value() == 0);
        CHECK(retry->replay().events.empty());
    }

    SECTION("GP-03 an empty run completes at zero with an empty slot span") {
        auto prepared = emptyPrepared();
        auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{23, 1});
        REQUIRE(kernel);
        CHECK(kernel->settlementCapacity() == 0);
        auto summary = kernel->advance(Tick{0}, std::span<SettlementEvent>{});
        REQUIRE(summary);
        CHECK(summary->settledEvents == 0);
        CHECK(summary->status == AdvanceStatus::completed);
        REQUIRE(summary->normalized.has_value());
        CHECK(summary->normalized->score == 0);
        CHECK(summary->normalized->jMax == 0);
        CHECK(summary->normalized->cMax == 0);
        auto again = kernel->advance(Tick{0}, std::span<SettlementEvent>{});
        REQUIRE(again);
        CHECK(again->status == AdvanceStatus::completed);
        CHECK_FALSE(again->normalized.has_value());
    }

    SECTION("GP-03 the pre-check order reports one stable fault and leaves state unchanged") {
        auto prepared = gp02Prepared(false);
        auto preparedKernel = runtime::AdvanceKernel::prepare(prepared, RunKey{24, 1});
        REQUIRE(preparedKernel);
        REQUIRE(preparedKernel->advance(Tick{1000}));
        std::vector<SettlementEvent> slots(3, sentinelEvent());
        const auto archiveSize = preparedKernel->archivedRecords().size();

        //  A run mismatch beats the horizon reversal.
        auto badRun = preparedKernel->advance(Tick{999}, RunKey{24, 2}, slots);
        REQUIRE_FALSE(badRun);
        CHECK(badRun.error().code() == "session.mutation_rejected");
        CHECK(contextValue(badRun.error(), "fieldPath") == "/run");
        CHECK(preparedKernel->frontier() == Tick{1000});

        //  The reversal beats the short slot buffer.
        auto reversed =
            preparedKernel->advance(Tick{999}, RunKey{24, 1}, std::span<SettlementEvent>{});
        REQUIRE_FALSE(reversed);
        CHECK(reversed.error().code() == "judgement.timebase.time_reversal");
        CHECK(contextValue(reversed.error(), "fieldPath") == "/horizon");

        //  The short slot buffer is the last pre-check; nothing moved.
        std::vector<SettlementEvent> shortSlots(2, sentinelEvent());
        auto shortSlotsResult = preparedKernel->advance(Tick{1000}, RunKey{24, 1}, shortSlots);
        REQUIRE_FALSE(shortSlotsResult);
        CHECK(shortSlotsResult.error().code() == "judgement.advance.output_capacity_insufficient");
        CHECK(contextValue(shortSlotsResult.error(), "fieldPath") == "/slots");
        CHECK(contextValue(shortSlotsResult.error(), "required") == "3");
        CHECK(contextValue(shortSlotsResult.error(), "provided") == "2");
        CHECK(preparedKernel->frontier() == Tick{1000});
        CHECK(preparedKernel->archivedRecords().size() == archiveSize);
        for (const auto& event : shortSlots) {
            CHECK(isSentinel(event));
        }

        //  Unprepared beats a bad run.
        runtime::AdvanceKernel unprepared;
        auto unpreparedBadRun = unprepared.advance(Tick{0}, RunKey{0, 0}, slots);
        REQUIRE_FALSE(unpreparedBadRun);
        CHECK(unpreparedBadRun.error().code() == "session.mutation_rejected");
        CHECK(contextValue(unpreparedBadRun.error(), "fieldPath") == "/session/state");
        CHECK(unprepared.state() == runtime::AdvanceSessionState::unprepared);

        //  A zero run is refused at prepare; the mismatch is refused at advance.
        CHECK_FALSE(runtime::AdvanceKernel::prepare(prepared, RunKey{0, 1}));
        CHECK_FALSE(runtime::AdvanceKernel::prepare(prepared, RunKey{1, 0}));

        //  Wrong thread beats unprepared and a bad run; it is a Result, never an assert.
        std::string threadCode;
        std::string threadPath;
        {
            runtime::AdvanceKernel sharedUnprepared;
            std::vector<SettlementEvent> threadSlots(3, sentinelEvent());
            std::thread worker{[&] {
                auto result = sharedUnprepared.advance(Tick{0}, RunKey{0, 0}, threadSlots);
                threadCode = result ? std::string{"ok"} : std::string{result.error().code()};
                if (!result) {
                    threadPath = contextValue(result.error(), "fieldPath");
                }
            }};
            worker.join();
            CHECK(threadCode == "session.mutation_rejected");
            CHECK(threadPath == "/session/owner");
        }

        //  Wrong reentrancy is refused from inside the transaction, before any state change.
        std::string reentrantCode;
        std::string reentrantPath;
        {
            auto reentrant = runtime::AdvanceKernel::prepare(prepared, RunKey{24, 4});
            REQUIRE(reentrant);
            REQUIRE(reentrant->receiveCanonicalBatch(
                std::vector<runtime::TickRecord>{tickRecord(900)}));
            runtime::detail::AdvanceTestControls controls{};
            controls.point = runtime::detail::AdvanceFaultPoint::reentrantSeal;
            controls.atSeal = [&] {
                auto nested = reentrant->advance(Tick{1000}, RunKey{24, 4}, slots);
                reentrantCode = nested ? std::string{"ok"} : std::string{nested.error().code()};
                if (!nested) {
                    reentrantPath = contextValue(nested.error(), "fieldPath");
                }
            };
            runtime::detail::AdvanceTestAccess::inject(*reentrant, std::move(controls));
            std::vector<SettlementEvent> outerSlots(3);
            auto outer = reentrant->advance(Tick{900}, outerSlots);
            REQUIRE(outer);
            CHECK(reentrantCode == "session.mutation_rejected");
            CHECK(reentrantPath == "/session/owner");
            CHECK(outer->settledEvents == 1);
        }

        //  A faulted session beats the run check.
        auto faulted = runtime::AdvanceKernel::prepare(prepared, RunKey{24, 2});
        REQUIRE(faulted);
        REQUIRE(faulted->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)}));
        runtime::detail::AdvanceTestAccess::inject(
            *faulted, {runtime::detail::AdvanceFaultPoint::beforeSeal});
        std::vector<SettlementEvent> faultSlots(3, sentinelEvent());
        auto fault = faulted->advance(Tick{900}, RunKey{24, 2}, faultSlots);
        REQUIRE(fault);
        REQUIRE(fault->status == AdvanceStatus::faulted);
        auto stateFirst = faulted->advance(Tick{1000}, RunKey{24, 3}, faultSlots);
        REQUIRE_FALSE(stateFirst);
        CHECK(stateFirst.error().code() == "session.mutation_rejected");
        CHECK(contextValue(stateFirst.error(), "fieldPath") == "/session/state");
    }

    SECTION("GP-03 seal-time faults keep the sealed prefix and never deliver a score") {
        auto prepared = gp02Prepared(false);

        //  Fault before the first settlement-bearing tick seal: zero events.
        {
            auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{25, 1});
            REQUIRE(kernel);
            REQUIRE(
                kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)}));
            runtime::detail::AdvanceTestAccess::inject(
                *kernel, {runtime::detail::AdvanceFaultPoint::beforeSeal});
            std::vector<SettlementEvent> slots(3, sentinelEvent());
            auto faulted = kernel->advance(Tick{1103}, slots);
            REQUIRE(faulted);
            CHECK(faulted->status == AdvanceStatus::faulted);
            CHECK(faulted->fault.stage == AdvanceFaultStage::kernel);
            CHECK(faulted->fault.reason == AdvanceFaultReason::invariant);
            CHECK(faulted->written == 0);
            CHECK(faulted->settledEvents == 0);
            CHECK_FALSE(faulted->processedThrough.has_value());
            CHECK_FALSE(faulted->normalized.has_value());
            for (const auto& event : slots) {
                CHECK(isSentinel(event));
            }
            CHECK_FALSE(kernel->frontier().has_value());
            CHECK(kernel->replay().events.empty());
            CHECK(kernel->state() == runtime::AdvanceSessionState::faulted);
            auto later = kernel->advance(Tick{1103}, slots);
            REQUIRE_FALSE(later);
            CHECK(later.error().code() == "session.mutation_rejected");
            CHECK(contextValue(later.error(), "fieldPath") == "/session/state");
        }

        //  Fault before the second settlement-bearing tick seal: the first tick's event is sealed
        //  and delivered, and the faulted call reports that one-slot prefix.
        {
            auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{25, 2});
            REQUIRE(kernel);
            REQUIRE(
                kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)}));
            runtime::detail::AdvanceTestAccess::inject(
                *kernel, {runtime::detail::AdvanceFaultPoint::beforeSeal, 2});
            std::vector<SettlementEvent> slots(3, sentinelEvent());
            auto second = kernel->advance(Tick{1103}, slots);
            REQUIRE(second);
            CHECK(second->status == AdvanceStatus::faulted);
            CHECK(second->written == 1);
            CHECK(second->settledEvents == 1);
            CHECK(second->processedThrough == Tick{900});
            CHECK(slots[0].tick == Tick{900});
            CHECK(slots[0].outcome == Outcome::hit);
            CHECK(isSentinel(slots[1]));
            CHECK(kernel->frontier() == Tick{900});
            REQUIRE(kernel->replay().events.size() == 1);
            CHECK(kernel->replay().events.front().tick == Tick{900});
        }

        //  Fold fault after the second settlement-bearing tick seal: two events are sealed, the
        //  call is faulted and the score payload is empty.
        {
            auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{25, 3});
            REQUIRE(kernel);
            REQUIRE(
                kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)}));
            runtime::detail::AdvanceTestAccess::inject(
                *kernel, {runtime::detail::AdvanceFaultPoint::afterSealFold, 2});
            std::vector<SettlementEvent> slots(3, sentinelEvent());
            auto second = kernel->advance(Tick{1103}, slots);
            REQUIRE(second);
            CHECK(second->status == AdvanceStatus::faulted);
            CHECK(second->fault.stage == AdvanceFaultStage::fold);
            CHECK(second->fault.reason == AdvanceFaultReason::invariant);
            CHECK(second->written == 2);
            CHECK(second->settledEvents == 2);
            CHECK(second->processedThrough == Tick{1000});
            CHECK_FALSE(second->normalized.has_value());
            CHECK(slots[0].tick == Tick{900});
            CHECK(slots[1].tick == Tick{1000});
            CHECK(kernel->frontier() == Tick{1000});
            CHECK(kernel->replay().events.size() == 2);
            CHECK(kernel->state() == runtime::AdvanceSessionState::faulted);
        }

        //  An injected pre-check allocation failure numbers nothing and does not advance.
        {
            auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{25, 4});
            REQUIRE(kernel);
            runtime::detail::AdvanceTestAccess::inject(
                *kernel, {runtime::detail::AdvanceFaultPoint::beforeNumbering});
            auto refused =
                kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)});
            REQUIRE_FALSE(refused);
            CHECK(refused.error().code() == "host.rpa.allocation_failed");
            CHECK_FALSE(kernel->acceptedIngressThrough().has_value());
            CHECK(kernel->nextSequence().value() == 0);
            CHECK(kernel->archivedRecords().empty());
            runtime::detail::AdvanceTestAccess::clear(*kernel);
            auto accepted =
                kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)});
            REQUIRE(accepted);
            CHECK(accepted->front().sequence.value() == 0);
            CHECK(kernel->acceptedIngressThrough() == runtime::IngressSequence{0});
        }
    }

    SECTION("GP-03 a pre-seal allocation failure classifies as resource and freezes") {
        auto prepared = gp02Prepared(false);
        auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{27, 1});
        REQUIRE(kernel);
        REQUIRE(kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)}));
        const auto sequenceBefore = kernel->nextSequence();
        const auto archiveBefore = kernel->archivedRecords().size();
        runtime::detail::AdvanceTestAccess::inject(
            *kernel, {runtime::detail::AdvanceFaultPoint::allocationBeforeSeal});
        std::vector<SettlementEvent> slots(3, sentinelEvent());
        auto summary = kernel->advance(Tick{1103}, slots);
        REQUIRE(summary);
        CHECK(summary->status == AdvanceStatus::faulted);
        CHECK(summary->fault.stage == AdvanceFaultStage::kernel);
        CHECK(summary->fault.reason == AdvanceFaultReason::resource);
        CHECK(summary->written == 0);
        CHECK(summary->settledEvents == 0);
        CHECK_FALSE(summary->processedThrough.has_value());
        CHECK_FALSE(summary->normalized.has_value());
        CHECK_FALSE(kernel->frontier().has_value());
        CHECK(kernel->nextSequence() == sequenceBefore);
        CHECK(kernel->archivedRecords().size() == archiveBefore);
        CHECK(kernel->replay().events.empty());
        for (const auto& event : slots) {
            CHECK(isSentinel(event));
        }
        CHECK(kernel->state() == runtime::AdvanceSessionState::faulted);
        auto later = kernel->advance(Tick{1103}, slots);
        REQUIRE_FALSE(later);
        CHECK(later.error().code() == "session.mutation_rejected");
        CHECK(contextValue(later.error(), "fieldPath") == "/session/state");
    }

    SECTION("GP-03 a pre-seal checked-arithmetic failure classifies as arithmetic") {
        auto prepared = gp02Prepared(false);
        auto kernel = runtime::AdvanceKernel::prepare(prepared, RunKey{28, 1});
        REQUIRE(kernel);
        REQUIRE(kernel->receiveCanonicalBatch(std::vector<runtime::TickRecord>{tickRecord(900)}));
        runtime::detail::AdvanceTestAccess::inject(
            *kernel, {runtime::detail::AdvanceFaultPoint::arithmeticBeforeSeal});
        std::vector<SettlementEvent> slots(3, sentinelEvent());
        auto summary = kernel->advance(Tick{1103}, slots);
        REQUIRE(summary);
        CHECK(summary->status == AdvanceStatus::faulted);
        CHECK(summary->fault.stage == AdvanceFaultStage::kernel);
        CHECK(summary->fault.reason == AdvanceFaultReason::arithmetic);
        CHECK(summary->written == 0);
        CHECK_FALSE(summary->normalized.has_value());
        CHECK_FALSE(kernel->frontier().has_value());
        for (const auto& event : slots) {
            CHECK(isSentinel(event));
        }
        CHECK(kernel->state() == runtime::AdvanceSessionState::faulted);
    }

    SECTION("the runtime SessionConfiguration binds the validated prepared run") {
        auto prepared = gp02Prepared(false);
        auto configuration = runtimeConfiguration(prepared);
        auto kernel = runtime::AdvanceKernel::prepare(configuration, prepared, RunKey{26, 1});
        REQUIRE(kernel);
        CHECK(kernel->run() == RunKey{26, 1});
        CHECK(kernel->settlementCapacity() == 3);
        CHECK(kernel->dictionary().phases.size() == 3);
        CHECK(kernel->dictionary().grades == std::vector<std::string>{"break", "release"});

        auto mutated = configuration;
        mutated.legacyRulesetToken = "legacy.ruleset";
        auto legacy = runtime::AdvanceKernel::prepare(mutated, prepared, RunKey{26, 2});
        REQUIRE_FALSE(legacy);
        CHECK(legacy.error().code() == "judgement.execution.relation_invalid");
        CHECK(contextValue(legacy.error(), "fieldPath") == "/session/ruleset");

        mutated = configuration;
        mutated.timebase.microsecondsNumerator = 2;
        auto timebase = runtime::AdvanceKernel::prepare(mutated, prepared, RunKey{26, 2});
        REQUIRE_FALSE(timebase);
        CHECK(timebase.error().code() == "judgement.execution.relation_invalid");
        CHECK(contextValue(timebase.error(), "fieldPath") == "/timebase");

        mutated = configuration;
        mutated.identityDeclarations.session.judgementConfigToken = "other";
        auto identity = runtime::AdvanceKernel::prepare(mutated, prepared, RunKey{26, 2});
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "playback.gameplay.invalid");
        CHECK(contextValue(identity.error(), "fieldPath") == "/identityDeclarations");

        mutated = configuration;
        mutated.latePolicy.finalizationWatermark =
            runtime::MeasuredParameter<TickSpan>::measured(TickSpan{9});
        auto late = runtime::AdvanceKernel::prepare(mutated, prepared, RunKey{26, 2});
        REQUIRE_FALSE(late);
        CHECK(contextValue(late.error(), "fieldPath") == "/latePolicy");

        mutated = configuration;
        mutated.executionProfileToken = "override";
        auto profile = runtime::AdvanceKernel::prepare(mutated, prepared, RunKey{26, 2});
        REQUIRE_FALSE(profile);
        CHECK(contextValue(profile.error(), "fieldPath") == "/session/executionProfile");

        mutated = configuration;
        mutated.lateAlgorithmToken = "override";
        auto algorithm = runtime::AdvanceKernel::prepare(mutated, prepared, RunKey{26, 2});
        REQUIRE_FALSE(algorithm);
        CHECK(contextValue(algorithm.error(), "fieldPath") == "/session/lateAlgorithm");

        mutated = configuration;
        mutated.factSemanticRevision = "override";
        auto semantics = runtime::AdvanceKernel::prepare(mutated, prepared, RunKey{26, 2});
        REQUIRE_FALSE(semantics);
        CHECK(contextValue(semantics.error(), "fieldPath") == "/session/factSemantic");
    }
}
