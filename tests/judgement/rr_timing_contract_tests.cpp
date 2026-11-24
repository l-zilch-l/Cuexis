//  T-12 candidate timing and author compiler contract.
//
//  Built only in the candidate configuration: the file includes the build-only Chart v5 source
//  values and the compile-side compiler header, neither of which exists in the default build.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/chart/chart_v5_document.hpp>
#include <cuexis/chart/chart_v5_timing.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/judgement/beat_us_compile.hpp>
#include <cuexis/judgement/compiled_gameplay_rows.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/timing/timing_map.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
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
}
