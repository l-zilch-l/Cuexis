//  T-12 candidate timing and author compiler contract.
//
//  Built only in the candidate configuration: the file includes the build-only Chart v5 source
//  values and the compile-side compiler header, neither of which exists in the default build.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/chart/chart_v5_document.hpp>
#include <cuexis/chart/chart_v5_timing.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/judgement/advance.hpp>
#include <cuexis/judgement/advance_kernel.hpp>
#include <cuexis/judgement/beat_us_compile.hpp>
#include <cuexis/judgement/compiled_gameplay_rows.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/judgement/prepared_gameplay.hpp>
#include <cuexis/judgement/prepared_gameplay_records.hpp>
#include <cuexis/judgement/prepared_identity.hpp>
#include <cuexis/timing/timing_map.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using cuexis::chart::ChartTimingSource;
using cuexis::judgement::BeatGameplaySourceView;
using cuexis::judgement::BeatGradeSelection;
using cuexis::judgement::BeatGradeTable;
using cuexis::judgement::BeatRequirementSource;
using cuexis::judgement::BeatWindow;
using cuexis::judgement::CompileCapabilityContext;
using cuexis::judgement::CompiledBeatKeys;
using cuexis::judgement::ContentProfileLimits;
using cuexis::judgement::GameplayCompileContext;
using cuexis::judgement::GameplaySource;
using cuexis::judgement::MatchPolicy;
using cuexis::judgement::MeasureComponentDeclaration;
using cuexis::judgement::MeasureSpecDeclaration;
using cuexis::judgement::PatternDeclaration;
using cuexis::judgement::PatternNodeDeclaration;
using cuexis::judgement::PatternPrimitive;
using cuexis::judgement::PhaseDeclaration;
using cuexis::judgement::PhaseKind;
using cuexis::judgement::RequirementRecord;
using cuexis::judgement::ScoreMode;
using cuexis::judgement::ScoreRuleRow;
using cuexis::timing::RationalBeat;
using cuexis::timing::TimingMap;
using cuexis::timing::TimingSourceView;

auto beat(std::int64_t numerator, std::int64_t denominator) -> RationalBeat {
    auto created = RationalBeat::create(numerator, denominator);
    REQUIRE(created);
    return *created;
}

auto chartBeat(std::int64_t numerator, std::int64_t denominator) -> cuexis::chart::RationalBeat {
    auto created = cuexis::chart::RationalBeat::create(numerator, denominator);
    REQUIRE(created);
    return *created;
}

auto reasonOf(const cuexis::core::Error& error) -> std::string {
    for (const auto& context : error.context()) {
        if (context.key == "reason") {
            return context.value;
        }
    }
    return {};
}

auto fieldPathOf(const cuexis::core::Error& error) -> std::string {
    for (const auto& context : error.context()) {
        if (context.key == "fieldPath") {
            return context.value;
        }
    }
    return {};
}

struct TimingFixture final {
    double defaultBpm{120.0};
    std::vector<cuexis::timing::TempoEvent> tempos;
    std::vector<cuexis::timing::Stop> stops;

    [[nodiscard]] auto view() const -> TimingSourceView {
        return TimingSourceView{defaultBpm, tempos, stops};
    }
};

struct AuthoredFixture final {
    GameplaySource source;
    BeatRequirementSource authored;
    std::vector<PatternDeclaration> patterns;
    std::vector<cuexis::judgement::BeatMeasureDefinition> measures;
    ScoreMode mode;
    cuexis::judgement::LatePolicyParameters latePolicy;
    std::vector<cuexis::judgement::BeatGradeTable> gradeTables;
    std::vector<cuexis::judgement::BeatNoteType> noteTypes;
    std::vector<GameplaySource> sources;
    std::vector<PhaseDeclaration> phases;

    AuthoredFixture() {
        source.form = cuexis::judgement::SourceForm::memory;
        source.provenanceToken = "t-12-test";
        source.document.sourceDocumentId = "doc";
        source.document.chartVersion = 5;
        source.document.gameplayVersion = 3;

        phases.push_back(PhaseDeclaration{PhaseKind::tap, 0});

        RequirementRecord requirement;
        requirement.stableId = cuexis::judgement::StableDeclarationId{"doc", 0};
        requirement.identity = cuexis::judgement::RequirementIdentity{
            "entry", "invocation", "__chart__", "__object__", {}, "local"};
        requirement.phases = phases;
        requirement.pattern =
            PatternDeclaration{"",
                               MatchPolicy::leftmostFirst,
                               PatternNodeDeclaration{PatternPrimitive::instant, {}},
                               {}};
        requirement.measure.components.push_back(
            MeasureComponentDeclaration{PhaseKind::tap, "", {}, std::nullopt});
        requirement.requiresReleaseTailSemantics = false;
        source.document.requirements.push_back(std::move(requirement));

        authored.identity = source.document.requirements.front().identity;
        authored.beats.end = beat(2, 1);
        authored.beats.phaseTargets.push_back({PhaseKind::tap, beat(1, 1)});
        authored.successWindows.push_back(
            BeatWindow{PhaseDeclaration{PhaseKind::tap, 0}, -50.0, 50.0});
        authored.gradeTables.push_back(BeatGradeSelection{PhaseKind::tap, "main"});

        gradeTables.push_back(
            BeatGradeTable{"main", -1.0, 1.0, {{"Good", -50.0, 0.0}, {"Bad", 0.0, 50.0}}});

        mode.id = "mode.test";
        mode.total = 1000;
        mode.mj = 1000;
        mode.mc = 0;
        mode.onExhausted = cuexis::judgement::ComboAction::keep;
        mode.comboRewards.push_back(cuexis::judgement::ComboRewardSegment{1, 0, 0});
        mode.rules.push_back(ScoreRuleRow{PhaseKind::tap, cuexis::judgement::Outcome::miss,
                                          std::nullopt, true, 0,
                                          cuexis::judgement::ComboAction::keep});
        mode.rules.push_back(ScoreRuleRow{PhaseKind::tap, cuexis::judgement::Outcome::hit,
                                          std::string{"Good"}, true, 10,
                                          cuexis::judgement::ComboAction::increment});
        mode.rules.push_back(ScoreRuleRow{PhaseKind::tap, cuexis::judgement::Outcome::hit,
                                          std::string{"Bad"}, true, 5,
                                          cuexis::judgement::ComboAction::keep});

        const auto measured = [](std::int64_t value) {
            return cuexis::judgement::MeasuredParameter<cuexis::judgement::TickSpan>::measured(
                cuexis::judgement::TickSpan{value});
        };
        latePolicy.finalizationWatermark = measured(0);
        latePolicy.maxQueueHop = measured(0);
        latePolicy.windowCloseThreshold = measured(0);
        latePolicy.windowOpenThreshold = measured(0);
        latePolicy.policy = cuexis::judgement::LateEventPolicy::rejectLate;

        sources.push_back(source);
    }

    [[nodiscard]] auto view(const TimingFixture& timing) const -> BeatGameplaySourceView {
        return BeatGameplaySourceView{timing.view(),
                                      sources,
                                      patterns,
                                      measures,
                                      {&authored, 1},
                                      gradeTables,
                                      noteTypes,
                                      mode,
                                      latePolicy,
                                      cuexis::judgement::DeclaredCapabilitySet{},
                                      cuexis::judgement::FeatureClosure{},
                                      cuexis::judgement::ClosureContributions{},
                                      "gameplay.execution.beat-us.v1",
                                      "gameplay.normalization.beat-us.v1",
                                      "coordinator.policy.greedy_v1"};
    }

    [[nodiscard]] auto
    context(const CompileCapabilityContext& capabilities, const ContentProfileLimits& contentLimits,
            const cuexis::judgement::PatternCompileBudget& patternBudget,
            const cuexis::judgement::runtime::PrepareGameplayLimits& prepareLimits) const
        -> GameplayCompileContext {
        return GameplayCompileContext{capabilities, contentLimits, patternBudget, prepareLimits};
    }
};

struct CompileFixture final {
    CompileCapabilityContext capabilities;
    ContentProfileLimits contentLimits;
    cuexis::judgement::PatternCompileBudget patternBudget;
    cuexis::judgement::runtime::PrepareGameplayLimits prepareLimits;
    AuthoredFixture authored;
    TimingFixture timing;

    [[nodiscard]] auto map() const -> cuexis::core::Result<TimingMap> {
        const std::vector<RationalBeat> anchors{
            authored.authored.beats.end, authored.authored.beats.phaseTargets.front().second};
        return TimingMap::compile(timing.view(), anchors);
    }
};

} // namespace

//  ---------------------------------------------------------------------------------------------
//  T-12 GP-01/02/04: the product -> Prepared bridge and the runtime trace recorder.
//
//  The compile test above stops at `BeatGameplayProduct`. The helpers below publish that product as
//  an owning `runtime::PreparedGameplay` with a dummy timebase / identity and the default prepare
//  limits, then drive the delivered `runtime::AdvanceKernel` through
//  receiveCanonicalBatch -> advance/advanceStopCut. The authored fixtures are source values only;
//  every requirement, window, grade table and scoring unit below comes out of
//  `compileBeatGameplay`.
//  ---------------------------------------------------------------------------------------------

namespace {

namespace rt = cuexis::judgement::runtime;

using cuexis::judgement::BeatGameplayProduct;
using cuexis::judgement::BeatNoteType;
using cuexis::judgement::BeatRequirementSource;
using cuexis::judgement::BeatScoreSelection;
using cuexis::judgement::Outcome;
using cuexis::judgement::PhaseDeclaration;
using cuexis::judgement::PhaseKind;
using cuexis::judgement::RequirementRecord;
using cuexis::judgement::SettlementEvent;
using cuexis::judgement::Tick;
using cuexis::judgement::TimingDirection;

constexpr auto kUngraded = std::numeric_limits<std::uint32_t>::max();

auto queueLatePolicy() -> cuexis::judgement::LatePolicyParameters {
    const auto measured = [](std::int64_t value) {
        return cuexis::judgement::MeasuredParameter<cuexis::judgement::TickSpan>::measured(
            cuexis::judgement::TickSpan{value});
    };
    cuexis::judgement::LatePolicyParameters policy;
    policy.finalizationWatermark = measured(8);
    policy.maxQueueHop = measured(4);
    policy.windowCloseThreshold = measured(3);
    policy.windowOpenThreshold = measured(2);
    policy.policy = cuexis::judgement::LateEventPolicy::queueNextTick;
    return policy;
}

auto tapRule(std::string grade, std::uint32_t judgePoints, cuexis::judgement::ComboAction combo)
    -> cuexis::judgement::ScoreRuleRow {
    return cuexis::judgement::ScoreRuleRow{PhaseKind::tap, Outcome::hit, std::move(grade),
                                           true,           judgePoints,  combo};
}

auto tapMissRule() -> cuexis::judgement::ScoreRuleRow {
    return cuexis::judgement::ScoreRuleRow{PhaseKind::tap,
                                           Outcome::miss,
                                           std::nullopt,
                                           false,
                                           0,
                                           cuexis::judgement::ComboAction::keep};
}

auto scoreMode(std::uint32_t total, std::uint32_t mj, std::uint32_t mc,
               std::vector<cuexis::judgement::ScoreRuleRow> rules) -> cuexis::judgement::ScoreMode {
    cuexis::judgement::ScoreMode mode;
    mode.id = "mode.t12";
    mode.total = total;
    mode.mj = mj;
    mode.mc = mc;
    mode.onExhausted = cuexis::judgement::ComboAction::reset;
    mode.comboRewards.push_back(cuexis::judgement::ComboRewardSegment{1, 1, 1});
    mode.rules = std::move(rules);
    return mode;
}

auto gp01Mode() -> cuexis::judgement::ScoreMode {
    return scoreMode(100, 90, 10,
                     {tapRule("Good", 1, cuexis::judgement::ComboAction::increment),
                      tapRule("Great", 2, cuexis::judgement::ComboAction::increment),
                      tapMissRule()});
}

auto gp01FiveMode() -> cuexis::judgement::ScoreMode {
    return scoreMode(100, 90, 10,
                     {tapRule("G1", 1, cuexis::judgement::ComboAction::increment),
                      tapRule("G2", 2, cuexis::judgement::ComboAction::increment),
                      tapRule("G3", 3, cuexis::judgement::ComboAction::increment),
                      tapRule("G4", 4, cuexis::judgement::ComboAction::increment),
                      tapRule("G5", 5, cuexis::judgement::ComboAction::increment), tapMissRule()});
}

//  The "unselected table" sample: the requirement declares a tap Measure component but selects no
//  table, and the mode carries no qualified rule for the phase, so the phase is not a scoring unit.
auto gp01UnselectedMode() -> cuexis::judgement::ScoreMode {
    return scoreMode(100, 90, 10, {tapMissRule()});
}

auto gp02Mode() -> cuexis::judgement::ScoreMode {
    using cuexis::judgement::ComboAction;
    using cuexis::judgement::ScoreRuleRow;
    std::vector<ScoreRuleRow> rules{
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
    return scoreMode(100, 90, 10, std::move(rules));
}

auto gp04Mode() -> cuexis::judgement::ScoreMode {
    return scoreMode(1000000, 900000, 100000,
                     {tapRule("A", 2, cuexis::judgement::ComboAction::increment),
                      tapRule("B", 5, cuexis::judgement::ComboAction::increment),
                      tapRule("solo", 3, cuexis::judgement::ComboAction::increment),
                      tapMissRule()});
}

auto gp01MainTable() -> cuexis::judgement::BeatGradeTable {
    return cuexis::judgement::BeatGradeTable{
        "main",
        -10.0,
        15.0,
        {{"Good", -50.0, -10.0}, {"Great", -10.0, 10.0}, {"Good", 10.0, 50.0}}};
}

auto gp01FiveTable() -> cuexis::judgement::BeatGradeTable {
    return cuexis::judgement::BeatGradeTable{"five",
                                             -10.0,
                                             10.0,
                                             {{"G1", -50.0, -30.0},
                                              {"G2", -30.0, -10.0},
                                              {"G3", -10.0, 10.0},
                                              {"G4", 10.0, 30.0},
                                              {"G5", 30.0, 50.0}}};
}

auto gp02Table() -> cuexis::judgement::BeatGradeTable {
    //  Author fields are milliseconds; the compile step quantizes them by 1000 into tick (us)
    //  endpoints, so these values become the contract's [-154,0)/[0,20) tick intervals and the
    //  Exact [-10,15) tick band.
    return cuexis::judgement::BeatGradeTable{
        "main", -0.010, 0.015, {{"break", -0.154, 0.0}, {"release", 0.0, 0.020}}};
}

auto gp04Table(std::string id) -> cuexis::judgement::BeatGradeTable {
    const std::string grade = id;
    return cuexis::judgement::BeatGradeTable{std::move(id), 0.0, 1.0, {{grade, 0.0, 1.0}}};
}

struct WindowSpec final {
    PhaseDeclaration phase;
    std::int64_t startTick{};
    std::int64_t endTick{};
};

struct RuntimeRequirementSpec final {
    std::string name;
    std::vector<PhaseDeclaration> phases;
    std::vector<std::pair<PhaseKind, std::int64_t>> targets;
    std::int64_t endTick{};
    std::optional<std::pair<std::int64_t, std::int64_t>> bodyTicks;
    std::vector<WindowSpec> windows;
    std::vector<PhaseKind> measuredPhases;
    std::optional<std::string> noteType;
    std::vector<cuexis::judgement::BeatGradeSelection> gradeSelections;
    std::vector<BeatScoreSelection> scoring;
    std::string channel{"channel.zero"};
    bool press{true};
    bool release{false};
};

//  One authored source document, its typed timing rows and the compile context. The only beat unit
//  is BPM 120 = 500000 us, so an authored beat is `tick/500000`; every boundary used is passed to
//  the timing map as a required anchor.
struct RuntimeAuthorFixture final {
    std::vector<cuexis::judgement::GameplaySource> sources;
    std::vector<BeatRequirementSource> rows;
    std::vector<BeatNoteType> noteTypes;
    std::vector<cuexis::judgement::BeatGradeTable> gradeTables;
    cuexis::judgement::ScoreMode mode;
    cuexis::judgement::LatePolicyParameters latePolicy;
    TimingFixture timing;
    std::vector<RationalBeat> anchors;
    CompileCapabilityContext capabilities;
    ContentProfileLimits contentLimits;
    cuexis::judgement::PatternCompileBudget patternBudget;
    rt::PrepareGameplayLimits prepareLimits;
    std::uint32_t nextOrdinal{0};

    RuntimeAuthorFixture() {
        cuexis::judgement::GameplaySource source;
        source.form = cuexis::judgement::SourceForm::memory;
        source.provenanceToken = "t-12-gp";
        source.document.sourceDocumentId = "doc.t12";
        source.document.chartVersion = 5;
        source.document.gameplayVersion = 3;
        sources.push_back(std::move(source));
        latePolicy = queueLatePolicy();
    }

    auto tickBeat(std::int64_t tick) -> RationalBeat {
        auto created = RationalBeat::create(tick, 500000);
        REQUIRE(created);
        return *created;
    }

    void anchor(std::int64_t tick) {
        anchors.push_back(tickBeat(tick));
    }

    void addRequirement(RuntimeRequirementSpec spec) {
        auto& document = sources.front().document;
        RequirementRecord requirement;
        requirement.stableId = cuexis::judgement::StableDeclarationId{"doc.t12", nextOrdinal++};
        requirement.identity = cuexis::judgement::RequirementIdentity{
            "chart.t12", "invocation.t12", "module.t12", "export.t12", {}, spec.name};
        requirement.phases = spec.phases;
        requirement.requiresReleaseTailSemantics =
            std::any_of(spec.phases.begin(), spec.phases.end(), [](const PhaseDeclaration& phase) {
                return phase.kind == PhaseKind::tail;
            });
        requirement.pattern = cuexis::judgement::PatternDeclaration{
            "",
            cuexis::judgement::MatchPolicy::leftmostFirst,
            cuexis::judgement::PatternNodeDeclaration{cuexis::judgement::PatternPrimitive::instant,
                                                      {}},
            {}};
        for (const auto phase : spec.measuredPhases) {
            requirement.measure.components.push_back(
                cuexis::judgement::MeasureComponentDeclaration{phase, "", {}, std::nullopt});
        }
        const auto binding = [&spec](cuexis::judgement::InputAction action) {
            return cuexis::judgement::AtomBinding{"atom." + std::string{"input"},
                                                  "domain.t12",
                                                  "keyboard",
                                                  spec.channel,
                                                  action,
                                                  std::nullopt,
                                                  false};
        };
        if (spec.press) {
            requirement.atomBindings.push_back(binding(cuexis::judgement::InputAction::press));
        }
        if (spec.release) {
            requirement.atomBindings.push_back(binding(cuexis::judgement::InputAction::release));
        }
        document.requirements.push_back(std::move(requirement));

        anchor(spec.endTick);
        if (spec.bodyTicks) {
            anchor(spec.bodyTicks->first);
            anchor(spec.bodyTicks->second);
        }
        for (const auto& target : spec.targets) {
            anchor(target.second);
        }

        BeatRequirementSource row;
        row.identity = document.requirements.back().identity;
        row.beats.end = tickBeat(spec.endTick);
        if (spec.bodyTicks) {
            row.beats.body = std::pair<RationalBeat, RationalBeat>{
                tickBeat(spec.bodyTicks->first), tickBeat(spec.bodyTicks->second)};
        }
        for (const auto& target : spec.targets) {
            row.beats.phaseTargets.push_back(
                std::pair<PhaseKind, RationalBeat>{target.first, tickBeat(target.second)});
        }
        for (const auto& window : spec.windows) {
            std::optional<std::int64_t> targetTick;
            for (const auto& target : spec.targets) {
                if (target.first == window.phase.kind) {
                    targetTick = target.second;
                }
            }
            REQUIRE(targetTick.has_value());
            row.successWindows.push_back(cuexis::judgement::BeatWindow{
                window.phase, static_cast<double>(window.startTick - *targetTick) / 1000.0,
                static_cast<double>(window.endTick - *targetTick) / 1000.0});
        }
        row.noteType = spec.noteType;
        row.gradeTables = spec.gradeSelections;
        row.scoring = spec.scoring;
        rows.push_back(std::move(row));
    }

    [[nodiscard]] auto view() const -> cuexis::judgement::BeatGameplaySourceView {
        return cuexis::judgement::BeatGameplaySourceView{timing.view(),
                                                         sources,
                                                         {},
                                                         {},
                                                         rows,
                                                         gradeTables,
                                                         noteTypes,
                                                         mode,
                                                         latePolicy,
                                                         cuexis::judgement::DeclaredCapabilitySet{},
                                                         cuexis::judgement::FeatureClosure{},
                                                         cuexis::judgement::ClosureContributions{},
                                                         "gameplay.execution.beat-us.v1",
                                                         "gameplay.normalization.beat-us.v1",
                                                         "coordinator.policy.greedy_v1"};
    }

    [[nodiscard]] auto map() const -> cuexis::core::Result<TimingMap> {
        auto sorted = anchors;
        std::sort(sorted.begin(), sorted.end(),
                  [](const RationalBeat& left, const RationalBeat& right) { return left < right; });
        sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
        return TimingMap::compile(timing.view(), sorted);
    }

    [[nodiscard]] auto compile() const -> cuexis::core::Result<BeatGameplayProduct> {
        auto compiledMap = map();
        if (!compiledMap) {
            return cuexis::core::unexpected(compiledMap.error());
        }
        const auto context = cuexis::judgement::GameplayCompileContext{
            capabilities, contentLimits, patternBudget, prepareLimits};
        return cuexis::judgement::compileBeatGameplay(view(), *compiledMap, context);
    }
};

//  The minimal product -> Prepared bridge: dummy identity declarations, the fixed 1:1 tick
//  timebase, dummy content/prepared/execution tokens and the default prepare limits.
auto prepareProduct(const BeatGameplayProduct& product)
    -> cuexis::core::Result<rt::PreparedGameplay> {
    const rt::PreparedTimebase timebase{"engine.tick.us.v1", 1, 1, Tick{0}};
    cuexis::judgement::PreparedIdentityDeclarations identities;
    identities.engine.judgementSemanticRevision = "judgement.semantics.t12";
    identities.engine.factSemanticRevision = "fact.semantics.t12";
    identities.engine.fixedPointTableId = "fixed-point.t12";
    identities.engine.coordinationPhaseOrderToken = "coordination.t12";
    identities.ruleset.interfaceProjectionToken = "ruleset.t12";
    identities.session.judgementConfigToken = "session.t12";
    const rt::CompiledGameplayView view{
        timebase,          product.latePolicy,       identities,
        "content.t12",     "prepared.t12",           "execution.t12",
        nullptr,           &product.normalizedScore, product.requirements,
        product.resources, product.timers,
    };
    return rt::prepareCompiledGameplay(view, rt::PrepareGameplayLimits{});
}

struct TraceStep final {
    std::vector<SettlementEvent> events;
    rt::AdvanceSummary summary;
};

auto advanceStep(rt::AdvanceKernel& kernel, std::size_t capacity, Tick horizon)
    -> cuexis::core::Result<TraceStep> {
    std::vector<SettlementEvent> slots(capacity);
    auto summary = kernel.advance(horizon, slots);
    if (!summary) {
        return cuexis::core::unexpected(summary.error());
    }
    TraceStep step;
    step.events.assign(slots.begin(),
                       slots.begin() + static_cast<std::ptrdiff_t>(summary->written));
    step.summary = *summary;
    return step;
}

auto stopCutStep(rt::AdvanceKernel& kernel, std::size_t capacity, const rt::StopCut& cut)
    -> cuexis::core::Result<TraceStep> {
    std::vector<SettlementEvent> slots(capacity);
    auto summary = kernel.advanceStopCut(cut, slots);
    if (!summary) {
        return cuexis::core::unexpected(summary.error());
    }
    TraceStep step;
    step.events.assign(slots.begin(),
                       slots.begin() + static_cast<std::ptrdiff_t>(summary->written));
    step.summary = *summary;
    return step;
}

auto tapRecord(std::int64_t tick, std::string channel = "channel.zero",
               rt::InputAction action = rt::InputAction::press) -> rt::TickRecord {
    return rt::TickRecord{Tick{tick},         "domain.t12", "keyboard",
                          std::move(channel), action,       std::nullopt};
}

auto gradeName(const rt::AdvanceKernel& kernel, const SettlementEvent& event) -> std::string {
    if (event.gradeIndex == kUngraded) {
        return {};
    }
    const auto& grades = kernel.dictionary().grades;
    REQUIRE(event.gradeIndex < grades.size());
    return grades[event.gradeIndex];
}

auto directionName(std::optional<TimingDirection> direction) -> std::string {
    if (!direction) {
        return "none";
    }
    switch (*direction) {
    case TimingDirection::early:
        return "early";
    case TimingDirection::exact:
        return "exact";
    case TimingDirection::late:
        return "late";
    }
    return "unknown";
}

void checkEvent(const rt::AdvanceKernel& kernel, const SettlementEvent& event, Tick tick,
                Outcome outcome, std::string_view grade, std::optional<TimingDirection> direction,
                std::optional<std::int64_t> error) {
    CHECK(event.tick == tick);
    CHECK(event.outcome == outcome);
    CHECK(gradeName(kernel, event) == grade);
    CHECK(directionName(event.direction) == directionName(direction));
    if (error) {
        CHECK(event.error.has_value());
        if (event.error.has_value()) {
            CHECK(event.error->value() == *error);
        }
    } else {
        CHECK_FALSE(event.error.has_value());
    }
}

//  The same check without the direction assertion: used where the vector only fixes the grade
//  boundary and the contract's Early/Exact/Late split is asserted elsewhere.
void checkEventCore(const rt::AdvanceKernel& kernel, const SettlementEvent& event, Tick tick,
                    Outcome outcome, std::string_view grade, std::optional<std::int64_t> error) {
    CHECK(event.tick == tick);
    CHECK(event.outcome == outcome);
    CHECK(gradeName(kernel, event) == grade);
    if (error) {
        CHECK(event.error.has_value());
        if (event.error.has_value()) {
            CHECK(event.error->value() == *error);
        }
    } else {
        CHECK_FALSE(event.error.has_value());
    }
}

auto makeGp01Fixture(bool five, bool instanceSelection, bool noteType, bool declareTable,
                     cuexis::judgement::ScoreMode mode) -> RuntimeAuthorFixture {
    RuntimeAuthorFixture fixture;
    fixture.mode = std::move(mode);
    if (declareTable) {
        fixture.gradeTables.push_back(five ? gp01FiveTable() : gp01MainTable());
    }
    if (noteType) {
        fixture.noteTypes.push_back(BeatNoteType{
            "tap", {{PhaseKind::tap, five ? std::string{"five"} : std::string{"main"}}}});
    }
    RuntimeRequirementSpec spec;
    spec.name = "gp01.req";
    spec.phases = {PhaseDeclaration{PhaseKind::tap, 0}};
    spec.targets = {{PhaseKind::tap, 1000000}};
    spec.endTick = 1050000;
    spec.windows = {WindowSpec{PhaseDeclaration{PhaseKind::tap, 0}, 950000, 1050000}};
    if (declareTable) {
        spec.measuredPhases = {PhaseKind::tap};
    }
    if (instanceSelection) {
        spec.gradeSelections = {{PhaseKind::tap, five ? std::string{"five"} : std::string{"main"}}};
    }
    if (noteType) {
        spec.noteType = std::string{"tap"};
    }
    fixture.addRequirement(std::move(spec));
    return fixture;
}

auto makeGp02Fixture(bool shortHold) -> RuntimeAuthorFixture {
    RuntimeAuthorFixture fixture;
    fixture.mode = gp02Mode();
    fixture.gradeTables.push_back(gp02Table());
    const std::int64_t bodyStart = shortHold ? 850 : 900;
    const std::int64_t bodyEnd = shortHold ? 851 : 1000;
    const std::int64_t bodyTarget = shortHold ? 851 : 1000;
    const std::int64_t headTarget = shortHold ? 850 : 900;
    RuntimeRequirementSpec spec;
    spec.name = "gp02.hold";
    spec.phases = {PhaseDeclaration{PhaseKind::head, 0}, PhaseDeclaration{PhaseKind::body, 1},
                   PhaseDeclaration{PhaseKind::tail, 2}};
    spec.targets = {
        {PhaseKind::head, headTarget}, {PhaseKind::body, bodyTarget}, {PhaseKind::tail, 1000}};
    spec.endTick = 1040;
    spec.bodyTicks = std::pair<std::int64_t, std::int64_t>{bodyStart, bodyEnd};
    spec.windows = {
        WindowSpec{PhaseDeclaration{PhaseKind::head, 0}, 850, 901},
        WindowSpec{PhaseDeclaration{PhaseKind::body, 1}, bodyStart, bodyEnd},
        WindowSpec{PhaseDeclaration{PhaseKind::tail, 2}, 1000, 1010},
        WindowSpec{PhaseDeclaration{PhaseKind::tail, 2}, 1015, 1020},
    };
    spec.measuredPhases = {PhaseKind::body, PhaseKind::tail};
    spec.gradeSelections = {{PhaseKind::body, std::string{"main"}},
                            {PhaseKind::tail, std::string{"main"}}};
    spec.release = true;
    fixture.addRequirement(std::move(spec));
    return fixture;
}

auto makeGp04Requirement(RuntimeAuthorFixture& fixture, std::string name, std::string grade,
                         std::string channel, std::int64_t target, std::optional<std::string> unit)
    -> void {
    RuntimeRequirementSpec spec;
    spec.name = std::move(name);
    spec.phases = {PhaseDeclaration{PhaseKind::tap, 0}};
    spec.targets = {{PhaseKind::tap, target}};
    spec.endTick = target + 50;
    spec.windows = {WindowSpec{PhaseDeclaration{PhaseKind::tap, 0}, target - 50, target + 50}};
    spec.measuredPhases = {PhaseKind::tap};
    spec.gradeSelections = {{PhaseKind::tap, std::move(grade)}};
    if (unit) {
        spec.scoring = {{PhaseKind::tap, std::move(unit)}};
    }
    spec.channel = std::move(channel);
    fixture.addRequirement(std::move(spec));
}

auto makeGp04SingletonFixture(std::size_t count) -> RuntimeAuthorFixture {
    RuntimeAuthorFixture fixture;
    fixture.mode = gp04Mode();
    fixture.gradeTables.push_back(gp04Table("A"));
    for (std::size_t index = 0; index < count; ++index) {
        makeGp04Requirement(fixture, "gp04.a." + std::to_string(index), "A",
                            "channel." + std::to_string(index), 1000, std::nullopt);
    }
    return fixture;
}

auto makeGp04GroupFixture(bool stagger) -> RuntimeAuthorFixture {
    RuntimeAuthorFixture fixture;
    fixture.mode = gp04Mode();
    fixture.gradeTables.push_back(gp04Table("A"));
    fixture.gradeTables.push_back(gp04Table("B"));
    fixture.gradeTables.push_back(gp04Table("solo"));
    makeGp04Requirement(fixture, "gp04.a", "A", "channel.0", 1000, std::string{"ab"});
    makeGp04Requirement(fixture, "gp04.b", "B", "channel.1", stagger ? 1100 : 1000,
                        std::string{"ab"});
    makeGp04Requirement(fixture, "gp04.solo", "solo", "channel.2", stagger ? 1100 : 1000,
                        std::string{"solo"});
    return fixture;
}

} // namespace

TEST_CASE("cuexis_rr_timing_contract", "[timing][contract]") {
    SECTION("zero-beat Stop keeps [S, E) and the E sample outside") {
        TimingFixture fixture;
        fixture.stops.push_back(cuexis::timing::Stop{RationalBeat::zero(), 20.0});
        auto map = TimingMap::compile(fixture.view(), {});
        REQUIRE(map);
        CHECK(map->beatToTime(RationalBeat::zero())->value == 0);
        const auto beginning = map->timeToBeat(cuexis::timing::TimeUs{0});
        REQUIRE(beginning);
        CHECK(beginning->inStop);
        REQUIRE(beginning->stopBeat.has_value());
        CHECK(*beginning->stopBeat == RationalBeat::zero());
        const auto end = map->timeToBeat(cuexis::timing::TimeUs{20000});
        REQUIRE(end);
        CHECK_FALSE(end->inStop);
        CHECK(end->beat == 0.0);
        CHECK(end->stopProgress == 0.0);
    }

    SECTION("half-tick boundary rounds to even and moves from 810929 to 810930") {
        TimingFixture fixture;
        fixture.defaultBpm = 60000.0;
        const auto exactBefore = RationalBeat::create(810929, 1000);
        const auto exactAfter = RationalBeat::create(810930, 1000);
        const auto halfBefore = RationalBeat::create(1621859, 2000); // 810929.5 us
        const auto halfEven = RationalBeat::create(1621857, 2000);   // 810928.5 us
        REQUIRE(exactBefore);
        REQUIRE(exactAfter);
        REQUIRE(halfBefore);
        REQUIRE(halfEven);
        const std::vector<RationalBeat> anchors{*exactBefore, *exactAfter, *halfBefore, *halfEven};
        auto map = TimingMap::compile(fixture.view(), anchors);
        REQUIRE(map);
        CHECK(map->beatToTime(*exactBefore)->value == 810929);
        CHECK(map->beatToTime(*exactAfter)->value == 810930);
        CHECK(map->beatToTime(*halfBefore)->value == 810930);
        CHECK(map->beatToTime(*halfEven)->value == 810928);
    }

    SECTION("T-11 Chart timing source compiles into the shared Chart timing") {
        ChartTimingSource source;
        source.offsetMs = 12.5;
        source.defaultBpm = 120.0;
        source.stops.push_back(cuexis::chart::ChartStopSource{chartBeat(0, 1), 20.0});
        const std::vector<RationalBeat> anchors{RationalBeat::zero()};
        auto compiled = cuexis::chart::compileChartTiming(source, anchors);
        REQUIRE(compiled);
        CHECK(compiled->chartOffsetUs == 12500);
        CHECK(compiled->map.beatToTime(RationalBeat::zero())->value == 0);
        CHECK(compiled->map.timeToBeat(cuexis::timing::TimeUs{10000})->inStop);
    }

    SECTION("author compiler produces runtime records, canonical score and basis") {
        CompileFixture fixture;
        auto map = fixture.map();
        REQUIRE(map);
        const auto capabilities = fixture.capabilities;
        const auto contentLimits = fixture.contentLimits;
        const auto patternBudget = fixture.patternBudget;
        const auto prepareLimits = fixture.prepareLimits;
        auto product = cuexis::judgement::compileBeatGameplay(
            fixture.authored.view(fixture.timing), *map,
            fixture.authored.context(capabilities, contentLimits, patternBudget, prepareLimits));
        REQUIRE(product);
        REQUIRE(product->requirements.size() == 1);
        const auto& requirement = product->requirements.front();
        CHECK(requirement.timing.phaseTargets.size() == 1);
        CHECK(requirement.timing.phaseTargets.front().chartTick.value() == 500000);
        REQUIRE(requirement.timing.successWindows.size() == 1);
        CHECK(requirement.timing.successWindows.front().start.value() == 450000);
        CHECK(requirement.timing.successWindows.front().end.value() == 550000);
        CHECK(requirement.deadline.value() == 1000000);
        REQUIRE(requirement.measure.size() == 1);
        REQUIRE(requirement.measure.front().gradeTableIndex.has_value());
        CHECK(*requirement.measure.front().gradeTableIndex == 0);
        CHECK(requirement.expandedPatternNodes.has_value());
        REQUIRE(product->normalizedScore.units.size() == 1);
        CHECK(product->normalizedScore.units.front().phaseIndices == std::vector<std::uint32_t>{0});
        CHECK(product->normalizedScore.gradeTables.front().id == "main");
        CHECK(product->normalizedScore.basis.jMax == 10);
        CHECK(product->normalizedScore.basis.cMax == 0);
        CHECK(product->rows.gpr0.empty());
        CHECK(product->beatKeys.size() == 1);
    }

    SECTION("source selection duplication is rejected before any sorting can hide it") {
        CompileFixture fixture;
        fixture.authored.authored.gradeTables.clear();
        fixture.authored.authored.gradeTables.push_back(BeatGradeSelection{PhaseKind::tap, "main"});
        fixture.authored.authored.gradeTables.push_back(BeatGradeSelection{PhaseKind::tap, "main"});
        auto map = fixture.map();
        REQUIRE(map);
        const auto capabilities = fixture.capabilities;
        const auto contentLimits = fixture.contentLimits;
        const auto patternBudget = fixture.patternBudget;
        const auto prepareLimits = fixture.prepareLimits;
        auto rejected = cuexis::judgement::compileBeatGameplay(
            fixture.authored.view(fixture.timing), *map,
            fixture.authored.context(capabilities, contentLimits, patternBudget, prepareLimits));
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "judgement.execution.relation_invalid");
        CHECK(reasonOf(rejected.error()) == "duplicate_grade_selection");
    }

    SECTION("canonical ordering accepts reordered source values") {
        CompileFixture fixture;
        fixture.authored.gradeTables.clear();
        fixture.authored.gradeTables.push_back(cuexis::judgement::BeatGradeTable{
            "zeta", -2.0, 2.0, {{"B", -2.0, 0.0}, {"A", 0.0, 2.0}}});
        fixture.authored.gradeTables.push_back(
            cuexis::judgement::BeatGradeTable{"alpha", -1.0, 1.0, {{"D", -1.0, 1.0}}});
        fixture.authored.authored.gradeTables.clear();
        fixture.authored.authored.gradeTables.push_back(
            BeatGradeSelection{PhaseKind::tap, "alpha"});
        fixture.authored.mode.rules.clear();
        fixture.authored.mode.rules.push_back(
            ScoreRuleRow{PhaseKind::tap, cuexis::judgement::Outcome::hit, std::string{"D"}, true, 7,
                         cuexis::judgement::ComboAction::increment});
        fixture.authored.mode.rules.push_back(
            ScoreRuleRow{PhaseKind::tap, cuexis::judgement::Outcome::hit, std::string{"A"}, true, 6,
                         cuexis::judgement::ComboAction::keep});
        fixture.authored.mode.rules.push_back(
            ScoreRuleRow{PhaseKind::tap, cuexis::judgement::Outcome::hit, std::string{"B"}, true, 4,
                         cuexis::judgement::ComboAction::keep});
        fixture.authored.mode.rules.push_back(
            ScoreRuleRow{PhaseKind::tap, cuexis::judgement::Outcome::miss, std::nullopt, true, 0,
                         cuexis::judgement::ComboAction::keep});
        auto map = fixture.map();
        REQUIRE(map);
        const auto capabilities = fixture.capabilities;
        const auto contentLimits = fixture.contentLimits;
        const auto patternBudget = fixture.patternBudget;
        const auto prepareLimits = fixture.prepareLimits;
        auto product = cuexis::judgement::compileBeatGameplay(
            fixture.authored.view(fixture.timing), *map,
            fixture.authored.context(capabilities, contentLimits, patternBudget, prepareLimits));
        REQUIRE(product);
        REQUIRE(product->normalizedScore.gradeTables.size() == 2);
        CHECK(product->normalizedScore.gradeTables[0].id == "alpha");
        CHECK(product->normalizedScore.gradeTables[1].id == "zeta");
        REQUIRE(product->normalizedScore.gradeTables[1].intervals.size() == 2);
        CHECK(product->normalizedScore.gradeTables[1].intervals[0].grade == "B");
        CHECK(product->normalizedScore.gradeTables[1].intervals[1].grade == "A");
        REQUIRE(product->normalizedScore.mode.rules.size() == 4);
        CHECK(product->normalizedScore.mode.rules.front().phase == PhaseKind::tap);
    }

    SECTION("window phase ordinal mismatch is rejected with the source field path") {
        CompileFixture fixture;
        auto mistyped = fixture.authored.authored;
        mistyped.successWindows.front().phase = PhaseDeclaration{PhaseKind::tap, 2};
        auto map = fixture.map();
        REQUIRE(map);
        const auto capabilities = fixture.capabilities;
        const auto contentLimits = fixture.contentLimits;
        const auto patternBudget = fixture.patternBudget;
        const auto prepareLimits = fixture.prepareLimits;
        std::vector<GameplaySource> sources{fixture.authored.source};
        std::vector<BeatRequirementSource> authoredRows{mistyped};
        const auto view = BeatGameplaySourceView{fixture.timing.view(),
                                                 sources,
                                                 {},
                                                 {},
                                                 authoredRows,
                                                 fixture.authored.gradeTables,
                                                 {},
                                                 fixture.authored.mode,
                                                 fixture.authored.latePolicy,
                                                 cuexis::judgement::DeclaredCapabilitySet{},
                                                 cuexis::judgement::FeatureClosure{},
                                                 cuexis::judgement::ClosureContributions{},
                                                 "gameplay.execution.beat-us.v1",
                                                 "gameplay.normalization.beat-us.v1",
                                                 "coordinator.policy.greedy_v1"};
        auto rejected = cuexis::judgement::compileBeatGameplay(
            view, *map,
            fixture.authored.context(capabilities, contentLimits, patternBudget, prepareLimits));
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "judgement.execution.relation_invalid");
        CHECK(reasonOf(rejected.error()) == "invalid_relation");
        CHECK(fieldPathOf(rejected.error()) ==
              "/gameplay/requirements/0/timing/successWindows/0/phase");
    }

    SECTION("a mismatched timing source is rejected before any row is produced") {
        CompileFixture fixture;
        auto map = fixture.map();
        REQUIRE(map);
        auto changed = fixture.timing;
        changed.defaultBpm = 60.0;
        const auto capabilities = fixture.capabilities;
        const auto contentLimits = fixture.contentLimits;
        const auto patternBudget = fixture.patternBudget;
        const auto prepareLimits = fixture.prepareLimits;
        auto rejected = cuexis::judgement::compileBeatGameplay(
            fixture.authored.view(changed), *map,
            fixture.authored.context(capabilities, contentLimits, patternBudget, prepareLimits));
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "judgement.execution.relation_invalid");
        CHECK(reasonOf(rejected.error()) == "timing_source_mismatch");
        CHECK(fieldPathOf(rejected.error()) == "/timing");
    }

    SECTION("quantized interval collapse and exact-band loss are rejected") {
        CompileFixture fixture;
        fixture.authored.gradeTables.clear();
        fixture.authored.gradeTables.push_back(
            BeatGradeTable{"main", -0.0001, 0.0001, {{"A", 0.0001, 0.0004}}});
        auto map = fixture.map();
        REQUIRE(map);
        const auto capabilities = fixture.capabilities;
        const auto contentLimits = fixture.contentLimits;
        const auto patternBudget = fixture.patternBudget;
        const auto prepareLimits = fixture.prepareLimits;
        auto rejected = cuexis::judgement::compileBeatGameplay(
            fixture.authored.view(fixture.timing), *map,
            fixture.authored.context(capabilities, contentLimits, patternBudget, prepareLimits));
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "judgement.execution.relation_invalid");
        CHECK(reasonOf(rejected.error()) == "exact_band");

        fixture.authored.gradeTables.clear();
        fixture.authored.gradeTables.push_back(
            BeatGradeTable{"main", -1.0, 1.0, {{"A", 0.0001, 0.0004}}});
        auto collapsed = cuexis::judgement::compileBeatGameplay(
            fixture.authored.view(fixture.timing), *map,
            fixture.authored.context(capabilities, contentLimits, patternBudget, prepareLimits));
        REQUIRE_FALSE(collapsed);
        CHECK(reasonOf(collapsed.error()) == "interval_collapsed");
    }

    SECTION("the product to Prepared bridge keeps the compiled records and the score basis") {
        auto fixture = makeGp01Fixture(false, false, true, true, gp01Mode());
        auto product = fixture.compile();
        REQUIRE(product);
        CHECK(product->rows.gph0.empty());
        CHECK(product->rows.gpr0.empty());
        CHECK(product->rows.gpd0Patterns.empty());
        REQUIRE(product->normalizedScore.units.size() == 1);
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        CHECK(prepared->requirements().size() == product->requirements.size());
        CHECK(prepared->timers().size() == product->timers.size());
        CHECK(prepared->settlementCapacity() == 1);
        CHECK(prepared->identity() == "prepared.t12");
        REQUIRE(prepared->records().normalizedScore != nullptr);
        CHECK(prepared->records().normalizedScore->basis.jMax == 2);
        CHECK(prepared->records().normalizedScore->basis.cMax == 1);
        CHECK(prepared->records().timebase.identity == "engine.tick.us.v1");
        CHECK(prepared->identityDeclarations().ruleset.interfaceProjectionToken == "ruleset.t12");
    }

    SECTION("GP-01 two grades settle Error/Exact/Late with real errors and 55/100") {
        auto fixture = makeGp01Fixture(false, false, true, true, gp01Mode());
        auto product = fixture.compile();
        REQUIRE(product);
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        const std::vector<std::int64_t> errors{-50000, -10001, -10000, 0,     8000,
                                               9999,   10000,  14999,  15000, 49999};
        const std::vector<std::string> grades{"Good",  "Good", "Great", "Great", "Great",
                                              "Great", "Good", "Good",  "Good",  "Good"};
        const std::vector<TimingDirection> directions{
            TimingDirection::early, TimingDirection::early, TimingDirection::exact,
            TimingDirection::exact, TimingDirection::exact, TimingDirection::exact,
            TimingDirection::exact, TimingDirection::exact, TimingDirection::late,
            TimingDirection::late};
        for (std::size_t index = 0; index < errors.size(); ++index) {
            auto kernel = rt::AdvanceKernel::prepare(
                *prepared, cuexis::judgement::RunKey{1, static_cast<std::uint64_t>(index + 1)});
            REQUIRE(kernel);
            auto receipt = kernel->receiveCanonicalBatch(
                std::vector<rt::TickRecord>{tapRecord(1000000 + errors[index])});
            REQUIRE(receipt);
            REQUIRE(receipt->size() == 1);
            CHECK(receipt->front().accepted);
            auto step = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050003});
            REQUIRE(step);
            REQUIRE(step->events.size() == 1);
            checkEvent(*kernel, step->events.front(), Tick{1000000 + errors[index]}, Outcome::hit,
                       grades[index], directions[index], errors[index]);
            const bool great = grades[index] == "Great";
            CHECK(step->summary.j == (great ? 2 : 1));
            CHECK(step->summary.c == 1);
            CHECK(step->summary.jMax == 2);
            CHECK(step->summary.cMax == 1);
            CHECK(step->summary.score == (great ? 100U : 55U));
        }
    }

    SECTION("GP-01 five tables score 28/46/64/82/100 and keep the previous grade at left-1") {
        auto fixture = makeGp01Fixture(true, true, false, true, gp01FiveMode());
        auto product = fixture.compile();
        REQUIRE(product);
        REQUIRE(product->normalizedScore.basis.jMax == 5);
        REQUIRE(product->normalizedScore.basis.cMax == 1);
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        const std::vector<std::int64_t> lefts{-50000, -30000, -10000, 10000, 30000};
        const std::vector<std::string> names{"G1", "G2", "G3", "G4", "G5"};
        const std::vector<std::uint32_t> scores{28, 46, 64, 82, 100};
        for (std::size_t index = 0; index < lefts.size(); ++index) {
            auto kernel = rt::AdvanceKernel::prepare(
                *prepared, cuexis::judgement::RunKey{2, static_cast<std::uint64_t>(index + 1)});
            REQUIRE(kernel);
            REQUIRE(kernel->receiveCanonicalBatch(
                std::vector<rt::TickRecord>{tapRecord(1000000 + lefts[index])}));
            auto step = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050003});
            REQUIRE(step);
            REQUIRE(step->events.size() == 1);
            checkEventCore(*kernel, step->events.front(), Tick{1000000 + lefts[index]},
                           Outcome::hit, names[index], lefts[index]);
            CHECK(step->summary.j == static_cast<std::int64_t>(index + 1));
            CHECK(step->summary.score == scores[index]);
        }
        for (std::size_t index = 1; index < lefts.size(); ++index) {
            auto kernel = rt::AdvanceKernel::prepare(
                *prepared, cuexis::judgement::RunKey{3, static_cast<std::uint64_t>(index)});
            REQUIRE(kernel);
            REQUIRE(kernel->receiveCanonicalBatch(
                std::vector<rt::TickRecord>{tapRecord(1000000 + lefts[index] - 1)}));
            auto step = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050003});
            REQUIRE(step);
            REQUIRE(step->events.size() == 1);
            CHECK(gradeName(*kernel, step->events.front()) == names[index - 1]);
            REQUIRE(step->events.front().error.has_value());
            CHECK(step->events.front().error->value() == lefts[index] - 1);
        }
    }

    SECTION("GP-01 the note type defaults to main and an instance selection overrides five") {
        auto defaultFixture = makeGp01Fixture(false, false, true, true, gp01Mode());
        auto defaultProduct = defaultFixture.compile();
        REQUIRE(defaultProduct);
        auto defaultPrepared = prepareProduct(*defaultProduct);
        REQUIRE(defaultPrepared);
        auto defaultKernel =
            rt::AdvanceKernel::prepare(*defaultPrepared, cuexis::judgement::RunKey{4, 1});
        REQUIRE(defaultKernel);
        REQUIRE(
            defaultKernel->receiveCanonicalBatch(std::vector<rt::TickRecord>{tapRecord(970000)}));
        auto defaultStep =
            advanceStep(*defaultKernel, defaultPrepared->settlementCapacity(), Tick{1050003});
        REQUIRE(defaultStep);
        REQUIRE(defaultStep->events.size() == 1);
        checkEvent(*defaultKernel, defaultStep->events.front(), Tick{970000}, Outcome::hit, "Good",
                   TimingDirection::early, -30000);
        CHECK(defaultStep->summary.score == 55U);

        auto overrideFixture = makeGp01Fixture(true, true, true, true, gp01FiveMode());
        overrideFixture.gradeTables.push_back(gp01MainTable());
        auto overrideProduct = overrideFixture.compile();
        REQUIRE(overrideProduct);
        auto overridePrepared = prepareProduct(*overrideProduct);
        REQUIRE(overridePrepared);
        auto overrideKernel =
            rt::AdvanceKernel::prepare(*overridePrepared, cuexis::judgement::RunKey{4, 2});
        REQUIRE(overrideKernel);
        REQUIRE(
            overrideKernel->receiveCanonicalBatch(std::vector<rt::TickRecord>{tapRecord(970000)}));
        auto overrideStep =
            advanceStep(*overrideKernel, overridePrepared->settlementCapacity(), Tick{1050003});
        REQUIRE(overrideStep);
        REQUIRE(overrideStep->events.size() == 1);
        checkEvent(*overrideKernel, overrideStep->events.front(), Tick{970000}, Outcome::hit, "G2",
                   TimingDirection::early, -30000);
        CHECK(overrideStep->summary.score == 46U);
    }

    SECTION("GP-01 an unselected table keeps the real error but no grade or direction") {
        auto fixture = makeGp01Fixture(false, false, false, false, gp01UnselectedMode());
        auto product = fixture.compile();
        REQUIRE(product);
        REQUIRE(product->normalizedScore.units.empty());
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        auto kernel = rt::AdvanceKernel::prepare(*prepared, cuexis::judgement::RunKey{5, 1});
        REQUIRE(kernel);
        REQUIRE(kernel->receiveCanonicalBatch(std::vector<rt::TickRecord>{tapRecord(970000)}));
        auto step = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050003});
        REQUIRE(step);
        REQUIRE(step->events.size() == 1);
        const auto& event = step->events.front();
        CHECK(event.tick == Tick{970000});
        CHECK(event.outcome == Outcome::hit);
        REQUIRE(event.error.has_value());
        CHECK(event.error->value() == -30000);
        CHECK(event.gradeIndex == kUngraded);
        CHECK(directionName(event.direction) == "none");
        CHECK(step->summary.j == 0);
        CHECK(step->summary.c == 0);
        CHECK(step->summary.score == 0U);
    }

    SECTION("GP-01 the deadline Miss waits for the close threshold and never repeats") {
        auto run = [](bool withPress) {
            auto fixture = makeGp01Fixture(false, false, true, true, gp01Mode());
            auto product = fixture.compile();
            REQUIRE(product);
            auto prepared = prepareProduct(*product);
            REQUIRE(prepared);
            auto kernel = rt::AdvanceKernel::prepare(
                *prepared, cuexis::judgement::RunKey{6, withPress ? 1U : 2U});
            REQUIRE(kernel);
            if (withPress) {
                auto receipt =
                    kernel->receiveCanonicalBatch(std::vector<rt::TickRecord>{tapRecord(1050000)});
                REQUIRE(receipt);
                CHECK(receipt->front().accepted);
            }
            auto before = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050002});
            REQUIRE(before);
            CHECK(before->events.empty());
            CHECK(before->summary.score == 0U);
            auto at = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050003});
            REQUIRE(at);
            REQUIRE(at->events.size() == 1);
            checkEvent(*kernel, at->events.front(), Tick{1050000}, Outcome::miss, "", std::nullopt,
                       std::nullopt);
            CHECK(at->events.front().gradeIndex == kUngraded);
            CHECK(at->summary.j == 0);
            CHECK(at->summary.c == 0);
            CHECK(at->summary.score == 0U);
            auto again = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050004});
            REQUIRE(again);
            CHECK(again->events.empty());
        };
        run(false);
        run(true);
    }

    SECTION("GP-02 the compiled Hold settles the four release branches") {
        auto fixture = makeGp02Fixture(false);
        auto product = fixture.compile();
        REQUIRE(product);
        REQUIRE(product->requirements.size() == 1);
        REQUIRE(product->normalizedScore.basis.jMax == 10);
        REQUIRE(product->normalizedScore.basis.cMax == 6);
        REQUIRE(product->normalizedScore.units.size() == 3);
        REQUIRE(product->requirements.front().measure.size() == 2);
        REQUIRE(product->requirements.front().measure.front().gradeTableIndex.has_value());
        CHECK(product->requirements.front().deadline == Tick{1040});
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        REQUIRE(prepared->settlementCapacity() == 3);

        const auto settle = [&prepared](std::optional<std::int64_t> release, std::uint64_t epoch) {
            auto kernel =
                rt::AdvanceKernel::prepare(*prepared, cuexis::judgement::RunKey{7, epoch});
            REQUIRE(kernel);
            std::vector<rt::TickRecord> batch{tapRecord(900)};
            if (release) {
                batch.push_back(tapRecord(*release, "channel.zero", rt::InputAction::release));
            }
            REQUIRE(kernel->receiveCanonicalBatch(batch));
            auto step = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1103});
            REQUIRE(step);
            return std::pair<rt::AdvanceKernel, TraceStep>{std::move(*kernel), std::move(*step)};
        };

        auto first = settle(950, 1);
        REQUIRE(first.second.events.size() == 3);
        checkEvent(first.first, first.second.events[0], Tick{900}, Outcome::hit, "", std::nullopt,
                   std::nullopt);
        checkEvent(first.first, first.second.events[1], Tick{950}, Outcome::miss, "break",
                   TimingDirection::early, -50);
        checkEvent(first.first, first.second.events[2], Tick{950}, Outcome::miss, "", std::nullopt,
                   std::nullopt);
        CHECK(first.second.summary.j == 3);
        CHECK(first.second.summary.c == 1);
        CHECK(first.second.summary.score == 29U);

        auto second = settle(1011, 2);
        REQUIRE(second.second.events.size() == 3);
        checkEvent(second.first, second.second.events[0], Tick{900}, Outcome::hit, "", std::nullopt,
                   std::nullopt);
        checkEvent(second.first, second.second.events[1], Tick{1000}, Outcome::hit, "",
                   std::nullopt, std::nullopt);
        checkEvent(second.first, second.second.events[2], Tick{1011}, Outcome::miss, "release",
                   TimingDirection::exact, 11);
        CHECK(second.second.summary.j == 5);
        CHECK(second.second.summary.c == 3);
        CHECK(second.second.summary.score == 50U);
        CHECK(second.second.summary.jMax == 10);
        CHECK(second.second.summary.cMax == 6);

        for (const auto release :
             {std::optional<std::int64_t>{}, std::optional<std::int64_t>{1020}}) {
            auto third = settle(release, release ? 4 : 3);
            REQUIRE(third.second.events.size() == 3);
            checkEvent(third.first, third.second.events[0], Tick{900}, Outcome::hit, "",
                       std::nullopt, std::nullopt);
            checkEvent(third.first, third.second.events[1], Tick{1000}, Outcome::hit, "",
                       std::nullopt, std::nullopt);
            checkEvent(third.first, third.second.events[2], Tick{1020}, Outcome::miss, "",
                       std::nullopt, std::nullopt);
            CHECK(third.second.events[2].gradeIndex == kUngraded);
            CHECK(third.second.summary.j == 4);
            CHECK(third.second.summary.c == 3);
            CHECK(third.second.summary.score == 41U);
        }

        auto fourth = settle(1019, 5);
        REQUIRE(fourth.second.events.size() == 3);
        checkEvent(fourth.first, fourth.second.events[0], Tick{900}, Outcome::hit, "", std::nullopt,
                   std::nullopt);
        checkEvent(fourth.first, fourth.second.events[1], Tick{1000}, Outcome::hit, "",
                   std::nullopt, std::nullopt);
        checkEvent(fourth.first, fourth.second.events[2], Tick{1019}, Outcome::hit, "release",
                   TimingDirection::late, 19);
        CHECK(fourth.second.summary.j == 10);
        CHECK(fourth.second.summary.c == 6);
        CHECK(fourth.second.summary.score == 100U);
    }

    SECTION("GP-02 a release observed one tick before the frontier forwards to dispatch 851") {
        auto fixture = makeGp02Fixture(false);
        auto product = fixture.compile();
        REQUIRE(product);
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        auto kernel = rt::AdvanceKernel::prepare(*prepared, cuexis::judgement::RunKey{8, 1});
        REQUIRE(kernel);
        std::vector<rt::TickRecord> press{tapRecord(850)};
        REQUIRE(kernel->receiveCanonicalBatch(press));
        auto head = advanceStep(*kernel, prepared->settlementCapacity(), Tick{853});
        REQUIRE(head);
        REQUIRE(head->events.size() == 1);
        checkEvent(*kernel, head->events.front(), Tick{850}, Outcome::hit, "", std::nullopt,
                   std::nullopt);

        std::vector<rt::TickRecord> release{
            tapRecord(849, "channel.zero", rt::InputAction::release)};
        auto receipt = kernel->receiveCanonicalBatch(release);
        REQUIRE(receipt);
        REQUIRE(receipt->size() == 1);
        CHECK(receipt->front().accepted);
        CHECK(receipt->front().forwarded);
        CHECK(receipt->front().dispatchTick == Tick{851});
        auto late = advanceStep(*kernel, prepared->settlementCapacity(), Tick{854});
        REQUIRE(late);
        REQUIRE(late->events.size() == 2);
        checkEvent(*kernel, late->events.front(), Tick{851}, Outcome::miss, "break",
                   TimingDirection::early, -151);
        checkEvent(*kernel, late->events[1], Tick{851}, Outcome::miss, "", std::nullopt,
                   std::nullopt);
    }

    SECTION("GP-02 the short Hold settles Tail Miss with the observation error") {
        auto fixture = makeGp02Fixture(true);
        auto product = fixture.compile();
        REQUIRE(product);
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        REQUIRE(prepared->settlementCapacity() == 3);
        auto kernel = rt::AdvanceKernel::prepare(*prepared, cuexis::judgement::RunKey{9, 1});
        REQUIRE(kernel);
        std::vector<rt::TickRecord> press{tapRecord(850)};
        REQUIRE(kernel->receiveCanonicalBatch(press));
        auto head = advanceStep(*kernel, prepared->settlementCapacity(), Tick{854});
        REQUIRE(head);
        REQUIRE(head->events.size() == 2);
        checkEvent(*kernel, head->events[0], Tick{850}, Outcome::hit, "", std::nullopt,
                   std::nullopt);
        checkEvent(*kernel, head->events[1], Tick{851}, Outcome::hit, "", std::nullopt,
                   std::nullopt);

        std::vector<rt::TickRecord> release{
            tapRecord(849, "channel.zero", rt::InputAction::release)};
        auto receipt = kernel->receiveCanonicalBatch(release);
        REQUIRE(receipt);
        REQUIRE(receipt->size() == 1);
        CHECK(receipt->front().forwarded);
        CHECK(receipt->front().dispatchTick == Tick{852});
        auto late = advanceStep(*kernel, prepared->settlementCapacity(), Tick{854});
        REQUIRE(late);
        REQUIRE(late->events.size() == 1);
        checkEvent(*kernel, late->events.front(), Tick{852}, Outcome::miss, "break",
                   TimingDirection::early, -151);
    }

    SECTION("GP-04 200 and 1000 single-A units keep (400,20100) and (2000,500500)") {
        for (const std::size_t count : {std::size_t{200}, std::size_t{1000}}) {
            auto fixture = makeGp04SingletonFixture(count);
            auto product = fixture.compile();
            REQUIRE(product);
            REQUIRE(product->normalizedScore.units.size() == count);
            REQUIRE(product->normalizedScore.basis.jMax == 2 * count);
            REQUIRE(product->normalizedScore.basis.cMax == count * (count + 1) / 2);
            auto prepared = prepareProduct(*product);
            REQUIRE(prepared);
            REQUIRE(prepared->settlementCapacity() == count);
            auto kernel = rt::AdvanceKernel::prepare(
                *prepared, cuexis::judgement::RunKey{10, static_cast<std::uint64_t>(count)});
            REQUIRE(kernel);
            std::vector<rt::TickRecord> batch;
            batch.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                batch.push_back(tapRecord(1000, "channel." + std::to_string(index)));
            }
            auto receipt = kernel->receiveCanonicalBatch(batch);
            REQUIRE(receipt);
            REQUIRE(receipt->size() == count);
            REQUIRE(std::all_of(receipt->begin(), receipt->end(),
                                [](const auto& entry) { return entry.accepted; }));

            //  The 200-requirement run uses the Stop entry; the 1000-requirement run uses advance.
            std::optional<TraceStep> step;
            if (count == 200) {
                rt::StopCut cut;
                cut.run = kernel->run();
                cut.acceptedThrough = receipt->back().sequence;
                cut.cutoff = Tick{1050};
                auto stopped = stopCutStep(*kernel, prepared->settlementCapacity(), cut);
                REQUIRE(stopped);
                step = std::move(*stopped);
            } else {
                auto advanced = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050});
                REQUIRE(advanced);
                step = std::move(*advanced);
            }
            REQUIRE(step->events.size() == count);
            checkEvent(*kernel, step->events.front(), Tick{1000}, Outcome::hit, "A",
                       TimingDirection::exact, 0);
            checkEvent(*kernel, step->events.back(), Tick{1000}, Outcome::hit, "A",
                       TimingDirection::exact, 0);
            CHECK(step->summary.j == static_cast<std::int64_t>(2 * count));
            CHECK(step->summary.c == static_cast<std::int64_t>(count * (count + 1) / 2));
            CHECK(step->summary.jMax == 2 * count);
            CHECK(step->summary.cMax == count * (count + 1) / 2);
            CHECK(step->summary.mj == 900000U);
            CHECK(step->summary.mc == 100000U);
            CHECK(step->summary.score == 1000000U);
            CHECK(step->summary.combo == count);
            CHECK(step->summary.maxCombo == count);
            if (count == 200) {
                CHECK(step->summary.status == cuexis::judgement::AdvanceStatus::completed);
                CHECK(step->summary.normalized.has_value());
            }
        }
    }

    SECTION("GP-04 the explicit A/B unit and solo settle 662500 and B first settles 1000000") {
        auto fixture = makeGp04GroupFixture(false);
        auto product = fixture.compile();
        REQUIRE(product);
        REQUIRE(product->normalizedScore.units.size() == 2);
        REQUIRE(product->normalizedScore.units[0].phaseIndices == std::vector<std::uint32_t>{0, 1});
        REQUIRE(product->normalizedScore.units[1].phaseIndices == std::vector<std::uint32_t>{2});
        REQUIRE(product->normalizedScore.basis.jMax == 8);
        REQUIRE(product->normalizedScore.basis.cMax == 3);
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        REQUIRE(prepared->settlementCapacity() == 3);

        const auto settle = [&prepared](const std::vector<std::size_t>& order, std::uint64_t epoch,
                                        bool stop) {
            auto kernel =
                rt::AdvanceKernel::prepare(*prepared, cuexis::judgement::RunKey{11, epoch});
            REQUIRE(kernel);
            std::vector<rt::TickRecord> batch;
            batch.reserve(order.size());
            for (const auto index : order) {
                batch.push_back(tapRecord(1000, "channel." + std::to_string(index)));
            }
            auto receipt = kernel->receiveCanonicalBatch(batch);
            REQUIRE(receipt);
            std::optional<TraceStep> step;
            if (stop) {
                rt::StopCut cut;
                cut.run = kernel->run();
                cut.acceptedThrough = receipt->back().sequence;
                cut.cutoff = Tick{1050};
                auto stopped = stopCutStep(*kernel, prepared->settlementCapacity(), cut);
                REQUIRE(stopped);
                step = std::move(*stopped);
            } else {
                auto advanced = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1050});
                REQUIRE(advanced);
                step = std::move(*advanced);
            }
            return std::pair<rt::AdvanceKernel, TraceStep>{std::move(*kernel), std::move(*step)};
        };

        auto aFirst = settle({0, 1, 2}, 1, true);
        REQUIRE(aFirst.second.events.size() == 3);
        CHECK(gradeName(aFirst.first, aFirst.second.events[0]) == "A");
        CHECK(gradeName(aFirst.first, aFirst.second.events[1]) == "B");
        CHECK(gradeName(aFirst.first, aFirst.second.events[2]) == "solo");
        CHECK(aFirst.second.summary.j == 5);
        CHECK(aFirst.second.summary.c == 3);
        CHECK(aFirst.second.summary.score == 662500U);
        CHECK(aFirst.second.summary.jMax == 8);
        CHECK(aFirst.second.summary.cMax == 3);
        CHECK(aFirst.second.summary.combo == 2);
        CHECK(aFirst.second.summary.maxCombo == 2);
        CHECK(aFirst.second.summary.status == cuexis::judgement::AdvanceStatus::completed);

        auto bFirst = settle({1, 0, 2}, 2, false);
        REQUIRE(bFirst.second.events.size() == 3);
        CHECK(bFirst.second.summary.j == 8);
        CHECK(bFirst.second.summary.c == 3);
        CHECK(bFirst.second.summary.score == 1000000U);
    }

    SECTION("GP-04 an A Miss does not reset early and B still scores the unit") {
        auto fixture = makeGp04GroupFixture(true);
        auto product = fixture.compile();
        REQUIRE(product);
        REQUIRE(product->normalizedScore.basis.jMax == 8);
        auto prepared = prepareProduct(*product);
        REQUIRE(prepared);
        auto kernel = rt::AdvanceKernel::prepare(*prepared, cuexis::judgement::RunKey{12, 1});
        REQUIRE(kernel);

        //  W=3: the ordinary advance seals through horizon-3, so the 1050 Miss first becomes
        //  final at 1053 while its event tick stays the 1050 deadline.
        auto missed = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1053});
        REQUIRE(missed);
        REQUIRE(missed->events.size() == 1);
        checkEvent(*kernel, missed->events.front(), Tick{1050}, Outcome::miss, "", std::nullopt,
                   std::nullopt);
        CHECK(missed->summary.j == 0);
        CHECK(missed->summary.c == 0);
        CHECK(missed->summary.combo == 0);

        std::vector<rt::TickRecord> batch{tapRecord(1100, "channel.1"),
                                          tapRecord(1100, "channel.2")};
        REQUIRE(kernel->receiveCanonicalBatch(batch));
        auto later = advanceStep(*kernel, prepared->settlementCapacity(), Tick{1100});
        REQUIRE(later);
        REQUIRE(later->events.size() == 2);
        CHECK(gradeName(*kernel, later->events[0]) == "B");
        CHECK(gradeName(*kernel, later->events[1]) == "solo");
        CHECK(later->summary.j == 8);
        CHECK(later->summary.c == 3);
        CHECK(later->summary.score == 1000000U);
    }
}
