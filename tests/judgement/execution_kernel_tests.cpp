#include "../../engine/judgement/src/execution_testing.hpp"
#include "../../engine/judgement/src/pattern_dfa.hpp"
#include "execution_reference.hpp"
#include "execution_test_fixture.hpp"
#include <algorithm>
#include <cuexis/judgement/judgement_session.hpp>
#include <sstream>

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;

namespace {
struct KernelFixture {
    Fixture base;
    AssemblyRequest assembly;
    KernelFixture() : assembly(base.request()) {
        base.latePolicy = executionLate();
        executionFields(assembly);
    }
    auto prepared() -> PreparedGameplay {
        auto result = prepareResolvedGameplay({assembly, {}});
        INFO((result ? "" : std::string{result.error().message()}));
        REQUIRE(result);
        return *result;
    }
    auto configuration() -> SessionConfiguration {
        InputMappingProfile mapping{
            "mapping",
            "v1",
            *SourceClass::fromToken("keyboard"),
            {{"domain.binding.one",
              {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
        return {OwnedInputMappingProfile{mapping},
                OwnedTimebaseProfile{base.timebase},
                base.latePolicy,
                assembly.identityDeclarations,
                "calibration.fixture",
                assembly.executionProfile,
                "late.window.logical.v1",
                "fact.semantic.phase-local.v1"};
    }
    auto session() -> JudgementSession {
        auto result = JudgementSession::create();
        REQUIRE(result);
        REQUIRE(result->configure(configuration()));
        REQUIRE(result->prepare(prepared()));
        return std::move(*result);
    }
};
auto input(std::int64_t tick, std::uint64_t sequence, InputAction action) -> ClockedIngress {
    return {ObservationTick{Tick{tick}},
            {IngressSequence{sequence},
             action,
             *ChannelRef::fromToken("lane.one"),
             "domain.binding.one",
             {0, 0, 0, 0},
             {false, false, false},
             ContinuityKind::discrete,
             {}}};
}
auto state(const KernelProjection& p, PhaseKind phase) -> KernelPhaseState {
    auto found = std::find_if(p.phases.begin(), p.phases.end(),
                              [phase](const auto& v) { return v.phase == phase; });
    REQUIRE(found != p.phases.end());
    return found->state;
}
void tap(RequirementRecord& r) {
    r.phases = {{PhaseKind::tap, 1}};
    r.requiresReleaseTailSemantics = false;
    r.timing = RequirementRecord::Timing{Tick{940},
                                         {{Tick{850}, Tick{920}, {PhaseKind::tap, 1}}},
                                         {},
                                         {{PhaseKind::tap, Tick{900}}}};
    r.measure.components = {{PhaseKind::tap, "tap", {}}};
    r.atomBindings.resize(1);
    r.preparedGrace = PreparedGrace{TickSpan{0}};
}
} // namespace

TEST_CASE("T4 session head coverage and owner tail seal immutable projections",
          "[execution][kernel][t4]") {
    KernelFixture f;
    auto session = f.session();
    auto dormant = session.query();
    REQUIRE(dormant);
    CHECK(state(dormant->kernelView(), PhaseKind::head) == KernelPhaseState::Dormant);
    auto receipt =
        session.submit({input(900, 1, InputAction::press), input(1000, 2, InputAction::release)});
    REQUIRE(receipt);
    CHECK(receipt->size() == 2);
    CHECK(session.query()->kernelView().receipts.empty());
    auto result = session.advance(Tick{1003});
    INFO((result ? "" : std::string{result.error().message()}));
    REQUIRE(result);
    const auto& p = result->kernelView();
    CHECK(p.processedFrontier == Tick{1000});
    CHECK(p.lastAdvanceHorizon == Tick{1003});
    CHECK(state(p, PhaseKind::head) == KernelPhaseState::Hit);
    CHECK(state(p, PhaseKind::body) == KernelPhaseState::Hit);
    CHECK(state(p, PhaseKind::tail) == KernelPhaseState::Hit);
    REQUIRE(p.facts.size() == 3);
    const auto& head = std::get<PhaseOutcomeFact>(p.facts[0]);
    const auto& tail = std::get<PhaseOutcomeFact>(p.facts[1]);
    const auto& body = std::get<PhaseOutcomeFact>(p.facts[2]);
    CHECK(head.phase == PhaseKind::head);
    CHECK(head.error == TickDelta{0});
    CHECK(head.commitId == 0);
    CHECK(tail.phase == PhaseKind::tail);
    CHECK(tail.commitId == 1);
    CHECK(std::holds_alternative<ObservationOrigin>(tail.originId));
    CHECK(body.phase == PhaseKind::body);
    CHECK(body.commitId == 2);
    CHECK_FALSE(body.error);
    CHECK(std::holds_alternative<TimerOrigin>(body.originId));
    REQUIRE(p.receipts.size() == 2);
    CHECK(p.receipts[1].consideredCount == 1);
    CHECK(p.receipts[1].consumedBy.has_value());
    CHECK(p.contacts.empty());
    CHECK(p.ownership.empty());
    CHECK(std::holds_alternative<Free>(p.resources[0].state));
    REQUIRE(session.advance(Tick{1100}));
    CHECK(session.query()->kernelView().facts.size() == 3);
    REQUIRE(session.reset());
    CHECK_FALSE(session.hasPreparedState());
    CHECK(result->kernelView().facts.size() == 3);
    CHECK(state(dormant->kernelView(), PhaseKind::head) == KernelPhaseState::Dormant);
}

TEST_CASE("T4 owner early release consumes and propagates exactly once",
          "[execution][kernel][release]") {
    KernelFixture f;
    auto session = f.session();
    REQUIRE(
        session.submit({input(900, 1, InputAction::press), input(950, 2, InputAction::release)}));
    auto result = session.advance(Tick{1100});
    REQUIRE(result);
    const auto& p = result->kernelView();
    REQUIRE(p.facts.size() == 3);
    const auto& body = std::get<PhaseOutcomeFact>(p.facts[1]);
    const auto& tail = std::get<PhaseOutcomeFact>(p.facts[2]);
    CHECK(body.outcome == Outcome::miss);
    CHECK(body.error == TickDelta{-50});
    CHECK_FALSE(tail.error);
    CHECK(body.commitId == tail.commitId);
    CHECK(body.factId.localOrdinal == 0);
    CHECK(tail.factId.localOrdinal == 1);
    CHECK(p.receipts[1].consumedBy.has_value());
    CHECK(p.ownership.empty());
}

TEST_CASE("T4 unpressed head close propagates no-observation Miss without duplicates",
          "[execution][kernel][timer]") {
    KernelFixture f;
    auto session = f.session();
    bool late = false;
    SECTION("no head observation") {}
    SECTION("head press after b0 and at close cannot patch dependencies") {
        late = true;
        REQUIRE(session.submit(
            {input(901, 1, InputAction::press), input(902, 2, InputAction::release)}));
    }
    auto result = session.advance(Tick{1100});
    REQUIRE(result);
    const auto& p = result->kernelView();
    CHECK(p.facts.size() == (late ? 5 : 3));
    std::size_t outcomes = 0;
    for (const auto& fact : p.facts) {
        if (const auto* outcome = std::get_if<PhaseOutcomeFact>(&fact)) {
            ++outcomes;
            CHECK(outcome->outcome == Outcome::miss);
            CHECK_FALSE(outcome->error);
            CHECK_FALSE(outcome->evidence);
            CHECK(outcome->commitId == 0);
            CHECK(outcome->commitTick == Tick{901});
        }
    }
    CHECK(outcomes == 3);
    CHECK(p.ownership.empty());
    CHECK(p.contacts.empty());
    for (const auto& receipt : p.receipts) {
        CHECK(receipt.consideredCount == 0);
        CHECK_FALSE(receipt.consumedBy);
    }
}

TEST_CASE("Session identity includes real mapping and configuration matching is atomic",
          "[execution][kernel][identity]") {
    KernelFixture f;
    auto first = f.session();
    auto config = f.configuration();
    InputMappingProfile mapping{
        "mapping",
        "v2",
        *SourceClass::fromToken("keyboard"),
        {{"domain.binding.one", {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
    config.inputMapping = OwnedInputMappingProfile{mapping};
    auto second = JudgementSession::create();
    REQUIRE(second);
    REQUIRE(second->configure(config));
    REQUIRE(second->prepare(f.prepared()));
    CHECK_FALSE(first.query()->kernelView().judgementIdentity ==
                second->query()->kernelView().judgementIdentity);
    auto mismatch = f.configuration();
    mismatch.identityDeclarations.ruleset.buildHash = "different.ruleset";
    auto third = JudgementSession::create();
    REQUIRE(third);
    REQUIRE(third->configure(mismatch));
    CHECK_FALSE(third->prepare(f.prepared()));
    CHECK_FALSE(third->hasPreparedState());
}

TEST_CASE("T4 no-tail Hold ends coverage with physical contact still active",
          "[execution][kernel][no-tail]") {
    KernelFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    r.phases.pop_back();
    r.requiresReleaseTailSemantics = false;
    r.measure.components.pop_back();
    r.timing->successWindows.pop_back();
    r.timing->phaseTargets.pop_back();
    r.atomBindings.pop_back();
    auto session = f.session();
    REQUIRE(session.submit({input(900, 1, InputAction::press)}));
    auto covered = session.advance(Tick{1003});
    REQUIRE(covered);
    CHECK(covered->kernelView().facts.size() == 2);
    CHECK(covered->kernelView().contacts.size() == 1);
    CHECK(covered->kernelView().ownership.empty());
    auto conflict = session.submit({input(999, 2, InputAction::release)});
    REQUIRE_FALSE(conflict);
    CHECK(conflict.error().code() == "judgement.s7a4.late.rejected");
    REQUIRE(session.submit({input(1001, 2, InputAction::release)}));
    REQUIRE(session.advance(Tick{1004}));
    CHECK(session.query()->kernelView().contacts.empty());
    CHECK(session.query()->kernelView().receipts.back().consideredCount == 0);
}

TEST_CASE("Observer tap records progress and consumeEmpty without phase outcomes",
          "[execution][kernel][observe]") {
    KernelFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    tap(r);
    r.resourceClaims[0].intent = ResourceClaimIntent::observe;
    r.resourceClaims[0].claimPolicy.competition.reset();
    r.resourceClaims[0].claimPolicy.claimKeyToken.clear();
    auto session = f.session();
    REQUIRE(session.submit({input(900, 1, InputAction::press)}));
    auto result = session.advance(Tick{1003});
    REQUIRE(result);
    const auto& p = result->kernelView();
    CHECK(state(p, PhaseKind::tap) == KernelPhaseState::Observed);
    REQUIRE(p.facts.size() == 1);
    CHECK(std::get<ReceiptFact>(p.facts[0]).kind == FactKind::consumeEmpty);
    REQUIRE(p.observers.size() == 1);
    CHECK(p.observers[0].accepted);
    CHECK(p.observers[0].progressed);
    CHECK_FALSE(p.receipts[0].consumedBy);
    CHECK(p.receipts[0].consideredCount == 1);
    CHECK(std::holds_alternative<Free>(p.resources[0].state));
}

TEST_CASE("K4 priority wins fanout one and terminal resources retain considered losers",
          "[execution][kernel][k4]") {
    KernelFixture f;
    auto& source = f.assembly.sources[0].document;
    tap(source.requirements[0]);
    source.resources[0].terminalAfterTermination = true;
    auto contender = source.requirements[0];
    contender.identity.requirementLocalId = "requirement.two";
    contender.stableId.declarationOrdinal = 5;
    contender.resourceClaims[0].claimPolicy.competition = {-2, 8};
    SECTION("lower priority") {}
    SECTION("same priority smaller rank") {
        contender.resourceClaims[0].claimPolicy.competition = {-1, 6};
    }
    source.requirements.push_back(contender);
    source.declarations.push_back(
        testDeclaration(DeclarationKind::requirement, "requirement.two", 5));
    source.relations[0].members.push_back(contender.stableId);
    auto session = f.session();
    REQUIRE(session.submit({input(900, 1, InputAction::press), input(901, 2, InputAction::press)}));
    auto result = session.advance(Tick{910});
    REQUIRE(result);
    const auto& p = result->kernelView();
    REQUIRE(p.receipts.size() == 2);
    CHECK(p.receipts[0].consideredCount == 2);
    CHECK(p.receipts[0].consumedBy == contender.identity);
    CHECK(p.receipts[1].consideredCount == 1);
    CHECK_FALSE(p.receipts[1].consumedBy);
    CHECK(std::holds_alternative<Terminal>(p.resources[0].state));
    CHECK(std::get<ReceiptFact>(p.facts[1]).kind == FactKind::consumeEmpty);
}

TEST_CASE("Tick failure injection preserves sealed prefix cursors ownership receipts and IDs",
          "[execution][kernel][failure][atomic]") {
    for (const auto point :
         {detail::KernelFailurePoint::afterTimers, detail::KernelFailurePoint::afterInput,
          detail::KernelFailurePoint::beforeLedger, detail::KernelFailurePoint::beforeSeal}) {
        KernelFixture f;
        auto session = f.session();
        REQUIRE(session.submit(
            {input(900, 1, InputAction::press), input(1000, 2, InputAction::release)}));
        auto prefix = session.advance(Tick{903});
        REQUIRE(prefix);
        REQUIRE(detail::KernelTestAccess::inject(session, {Tick{1000}, point, {}, {}}));
        auto failed = session.advance(Tick{1003});
        REQUIRE_FALSE(failed);
        CHECK(failed.error().code() == "judgement.s7a4.kernel.transaction_failed");
        auto projection = session.query();
        REQUIRE(projection);
        const auto& p = projection->kernelView();
        CHECK(p.state == KernelSessionState::Faulted);
        CHECK(p.processedFrontier == Tick{999});
        CHECK(p.lastAdvanceHorizon == Tick{903});
        CHECK(p.facts.size() == 1);
        CHECK(p.receipts.size() == 1);
        CHECK(p.ownership.size() == 1);
        CHECK(p.contacts.size() == 1);
        CHECK(state(p, PhaseKind::body) == KernelPhaseState::Pending);
        CHECK(std::holds_alternative<Held>(p.resources[0].state));
        CHECK_FALSE(session.submit({}));
        CHECK_FALSE(session.advance(Tick{1100}));
        REQUIRE(session.reset());
        CHECK(prefix->kernelView().facts.size() == 1);
        CHECK(projection->kernelView().state == KernelSessionState::Faulted);
    }
    KernelFixture minimum;
    auto& r = minimum.assembly.sources[0].document.requirements[0];
    tap(r);
    r.timing =
        RequirementRecord::Timing{Tick{INT64_MIN + 20},
                                  {{Tick{INT64_MIN}, Tick{INT64_MIN + 10}, {PhaseKind::tap, 1}}},
                                  {},
                                  {{PhaseKind::tap, Tick{INT64_MIN}}}};
    minimum.base.latePolicy.windowOpenThreshold =
        MeasuredParameter<TickSpan>::measured(TickSpan{0});
    minimum.base.latePolicy.windowCloseThreshold =
        MeasuredParameter<TickSpan>::measured(TickSpan{0});
    auto firstTick = minimum.session();
    REQUIRE(firstTick.submit({input(INT64_MIN, 1, InputAction::press)}));
    REQUIRE(detail::KernelTestAccess::inject(
        firstTick, {Tick{INT64_MIN}, detail::KernelFailurePoint::afterTimers, {}, {}}));
    REQUIRE_FALSE(firstTick.advance(Tick{INT64_MIN}));
    auto p = firstTick.query();
    REQUIRE(p);
    CHECK(p->kernelView().state == KernelSessionState::Faulted);
    CHECK_FALSE(p->kernelView().processedFrontier);
    CHECK_FALSE(p->kernelView().lastAdvanceHorizon);
    CHECK(p->kernelView().failedTick == Tick{INT64_MIN});
    CHECK(p->kernelView().requestedHorizon == Tick{INT64_MIN});
    CHECK(p->kernelView().facts.empty());
    CHECK(p->kernelView().receipts.empty());
    CHECK(p->kernelView().contacts.empty());
}

TEST_CASE("Commit MAX is valid and the next nonempty Tick faults without wrapping",
          "[execution][kernel][ids]") {
    KernelFixture f;
    auto session = f.session();
    REQUIRE(
        session.submit({input(900, 1, InputAction::press), input(1000, 2, InputAction::release)}));
    REQUIRE(detail::KernelTestAccess::inject(
        session, {{}, detail::KernelFailurePoint::beforeSeal, UINT64_MAX, {}}));
    auto failed = session.advance(Tick{1100});
    REQUIRE_FALSE(failed);
    auto result = session.query();
    REQUIRE(result);
    const auto& p = result->kernelView();
    REQUIRE(p.facts.size() == 1);
    CHECK(std::get<PhaseOutcomeFact>(p.facts[0]).commitId == UINT64_MAX);
    CHECK(p.processedFrontier == Tick{999});
    CHECK(state(p, PhaseKind::body) == KernelPhaseState::Pending);
}

TEST_CASE("Internal committed signals first become visible on the next Tick",
          "[execution][kernel][signal]") {
    KernelFixture f;
    auto session = f.session();
    REQUIRE(session.submit({input(900, 1, InputAction::press)}));
    REQUIRE(detail::KernelTestAccess::inject(
        session, {{}, detail::KernelFailurePoint::beforeSeal, {}, {Tick{900}}}));
    REQUIRE(session.advance(Tick{903}));
    CHECK(detail::KernelTestAccess::visibleSignalCount(session, Tick{900}) == 0);
    CHECK(detail::KernelTestAccess::visibleSignalCount(session, Tick{901}) == 1);
    REQUIRE(session.advance(Tick{904}));
    CHECK(detail::KernelTestAccess::visibleSignalCount(session, Tick{901}) == 1);
}

TEST_CASE("S2 and independent S1 scanning agree on every full committed prefix",
          "[execution][kernel][oracle][differential]") {
    for (unsigned scenario = 0; scenario < 16; ++scenario) {
        KernelFixture f;
        if (scenario == 5) {
            tap(f.assembly.sources[0].document.requirements[0]);
        }
        if (scenario == 6) {
            auto& r = f.assembly.sources[0].document.requirements[0];
            tap(r);
            r.resourceClaims[0].intent = ResourceClaimIntent::observe;
            r.resourceClaims[0].claimPolicy.competition.reset();
            r.resourceClaims[0].claimPolicy.claimKeyToken.clear();
        }
        if (scenario >= 12) {
            auto& r = f.assembly.sources[0].document.requirements[0];
            r.preparedGrace = PreparedGrace{TickSpan{0}};
            r.timing->body->end = Tick{990};
            r.timing->successWindows[1].end = Tick{990};
            r.timing->successWindows[2].start = Tick{990};
            r.timing->successWindows[2].end = Tick{1000};
            r.timing->phaseTargets[1].chartTick = Tick{990};
            r.timing->phaseTargets[2].chartTick = Tick{990};
        }
        auto session = f.session();
        std::vector<ClockedIngress> traffic;
        if (scenario != 0) {
            traffic.push_back(input(900, 1, InputAction::press));
        }
        if (scenario == 1) {
            traffic.push_back(input(1000, 2, InputAction::release));
        }
        if (scenario == 2) {
            traffic.push_back(input(950, 2, InputAction::release));
        }
        if (scenario == 3) {
            traffic.push_back(input(1020, 2, InputAction::release));
        }
        if (scenario == 4) {
            traffic.push_back(input(950, 2, InputAction::update));
            traffic.push_back(input(1001, 3, InputAction::release));
        }
        if (scenario >= 7 && scenario < 12) {
            constexpr std::int64_t releases[]{999, 1000, 1019, 1020, 1040};
            traffic.push_back(input(releases[scenario - 7], 2, InputAction::release));
        }
        if (scenario >= 12) {
            constexpr std::int64_t releases[]{989, 990, 999, 1000};
            traffic.push_back(input(releases[scenario - 12], 2, InputAction::release));
        }
        auto admitted = session.submit(traffic);
        REQUIRE(admitted);
        std::vector<AdmittedObservation> trace;
        for (const auto& r : *admitted) {
            trace.push_back({r.observationKey, r.dispatchTick, r.wasForwarded, r.admissionFrontier,
                             r.admissionHorizon});
        }
        const auto prepared = f.prepared();
        const auto configuration = f.configuration();
        for (const auto tick :
             {849, 850, 899, 900, 901, 989, 990, 999, 1000, 1001, 1019, 1020, 1040, 1100}) {
            INFO(scenario);
            INFO(tick);
            auto production = session.advance(Tick{tick + 3});
            REQUIRE(production);
            const auto reference = scanReference(prepared, configuration, trace, Tick{tick + 3});
            const auto& p = production->kernelView();
            CHECK(p.phases == reference.phases);
            CHECK(p.resources == reference.resources);
            CHECK(p.contacts == reference.contacts);
            CHECK(p.ownership == reference.ownership);
            CHECK(p.observers == reference.observers);
            CHECK(p.receipts == reference.receipts);
            CHECK(p.facts == reference.facts);
            CHECK(p.processedFrontier == reference.processedFrontier);
            CHECK(p.lastAdvanceHorizon == reference.lastAdvanceHorizon);
            CHECK(p.judgementIdentity == reference.judgementIdentity);
        }
    }
}

TEST_CASE("L2 independent integer table agrees with L1 including signed endpoints",
          "[execution][late][oracle]") {
    for (const auto mode : {LateEventPolicy::rejectLate, LateEventPolicy::queueNextTick}) {
        const auto policy = executionLate(mode);
        for (std::int64_t t = -20; t <= 20; ++t) {
            for (std::int64_t a = -30; a <= 30; ++a) {
                const auto reference = referenceRoute(policy, Tick{t}, Tick{a});
                const auto production = routeExecutionObservation(policy, Tick{t}, Tick{a});
                REQUIRE(production.has_value() == reference.has_value());
                if (reference) {
                    CHECK(production->dispatchTick == *reference);
                }
            }
        }
        for (const auto t :
             {INT64_MIN, INT64_MIN + 1, INT64_MIN + 2, INT64_MAX - 8, INT64_MAX - 7, INT64_MAX}) {
            const auto reference = referenceRoute(policy, Tick{t}, {});
            const auto production = routeExecutionObservation(policy, Tick{t}, {});
            CHECK(production.has_value() == reference.has_value());
        }
    }
}

TEST_CASE("Manual T4 release goldens cover half-open tail and grace zero",
          "[execution][kernel][golden][t4]") {
    for (const auto graceZero : {false, true}) {
        const std::vector<std::int64_t> releases =
            graceZero ? std::vector<std::int64_t>{989, 990, 999, 1000}
                      : std::vector<std::int64_t>{999, 1000, 1019, 1020, 1040};
        for (const auto release : releases) {
            KernelFixture f;
            auto& r = f.assembly.sources[0].document.requirements[0];
            if (graceZero) {
                r.preparedGrace = PreparedGrace{TickSpan{0}};
                r.timing->body->end = Tick{990};
                r.timing->successWindows[1].end = Tick{990};
                r.timing->successWindows[2].start = Tick{990};
                r.timing->successWindows[2].end = Tick{1000};
                r.timing->phaseTargets[1].chartTick = Tick{990};
                r.timing->phaseTargets[2].chartTick = Tick{990};
            }
            auto session = f.session();
            REQUIRE(session.submit(
                {input(900, 1, InputAction::press), input(release, 2, InputAction::release)}));
            auto p = session.advance(Tick{1103});
            REQUIRE(p);
            const auto bodyEnd = graceZero ? 990 : 1000;
            const auto tailEnd = graceZero ? 1000 : 1020;
            CHECK(state(p->kernelView(), PhaseKind::body) ==
                  (release < bodyEnd ? KernelPhaseState::Miss : KernelPhaseState::Hit));
            CHECK(state(p->kernelView(), PhaseKind::tail) ==
                  (release >= bodyEnd && release < tailEnd ? KernelPhaseState::Hit
                                                           : KernelPhaseState::Miss));
            CHECK(p->kernelView().ownership.empty());
            for (auto phase : {PhaseKind::head, PhaseKind::body, PhaseKind::tail}) {
                CHECK(std::count_if(p->kernelView().facts.begin(), p->kernelView().facts.end(),
                                    [phase](const auto& fact) {
                                        const auto* outcome = std::get_if<PhaseOutcomeFact>(&fact);
                                        return outcome && outcome->phase == phase;
                                    }) == 1);
            }
        }
    }
}

TEST_CASE("Head prefix progress can precede the final new press",
          "[execution][kernel][pattern][head]") {
    KernelFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    r.pattern.root = {PatternPrimitive::sequence,
                      {{PatternPrimitive::atom, {}, "prefix", {}, {}}, r.pattern.root},
                      {},
                      {},
                      {}};
    r.patternArmRefs = {"prefix", "atom.one"};
    r.maxArmElements = MeasuredParameter<std::uint64_t>::measured(2);
    r.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(2);
    r.atomBindings.push_back(
        {"prefix", "domain.binding.one", "keyboard", "lane.one", InputAction::update, {}, false});
    bool repeatedPress = false;
    SECTION("fresh final press") {}
    SECTION("repeat press cannot acquire head ownership") {
        repeatedPress = true;
    }
    auto session = f.session();
    std::vector<ClockedIngress> traffic;
    if (repeatedPress) {
        traffic.push_back(input(880, 1, InputAction::press));
    }
    traffic.push_back(input(890, repeatedPress ? 2 : 1, InputAction::update));
    traffic.push_back(input(900, repeatedPress ? 3 : 2, InputAction::press));
    traffic.push_back(input(1000, repeatedPress ? 4 : 3, InputAction::release));
    REQUIRE(session.submit(traffic));
    auto p = session.advance(Tick{1103});
    REQUIRE(p);
    CHECK(state(p->kernelView(), PhaseKind::head) ==
          (repeatedPress ? KernelPhaseState::Miss : KernelPhaseState::Hit));
    CHECK(state(p->kernelView(), PhaseKind::body) ==
          (repeatedPress ? KernelPhaseState::Miss : KernelPhaseState::Hit));
    CHECK(p->kernelView().receipts[repeatedPress ? 1 : 0].consumedBy == r.identity);
    const auto found = std::find_if(p->kernelView().facts.begin(), p->kernelView().facts.end(),
                                    [](const auto& fact) {
                                        const auto* v = std::get_if<PhaseOutcomeFact>(&fact);
                                        return v && v->phase == PhaseKind::head;
                                    });
    REQUIRE(found != p->kernelView().facts.end());
    const auto& head = std::get<PhaseOutcomeFact>(*found);
    if (repeatedPress) {
        CHECK_FALSE(head.evidence);
        CHECK(head.commitTick == Tick{901});
        CHECK_FALSE(p->kernelView().receipts[2].consumedBy);
    } else {
        REQUIRE(head.evidence);
        CHECK(head.evidence->action == InputAction::press);
        CHECK(head.evidence->observationTick == ObservationTick{Tick{900}});
    }
}

TEST_CASE("Epsilon is attempted once and observer timeout never emits outcomes",
          "[execution][kernel][epsilon][observer]") {
    for (bool observe : {false, true}) {
        KernelFixture f;
        auto& r = f.assembly.sources[0].document.requirements[0];
        tap(r);
        r.pattern.root = {PatternPrimitive::instant, {}, {}, {}, {}};
        r.patternArmRefs.clear();
        r.atomBindings.clear();
        if (observe) {
            r.resourceClaims[0].intent = ResourceClaimIntent::observe;
            r.resourceClaims[0].claimPolicy.competition.reset();
            r.resourceClaims[0].claimPolicy.claimKeyToken.clear();
        }
        auto session = f.session();
        auto p = session.advance(Tick{1103});
        REQUIRE(p);
        CHECK(state(p->kernelView(), PhaseKind::tap) ==
              (observe ? KernelPhaseState::Observed : KernelPhaseState::Hit));
        CHECK(p->kernelView().facts.size() == (observe ? 0 : 1));
        CHECK(p->kernelView().observers.size() == (observe ? 1 : 0));
        auto again = session.advance(Tick{1104});
        REQUIRE(again);
        CHECK(again->kernelView().facts == p->kernelView().facts);
    }
}

TEST_CASE("Reset makes a new run anchor while old projections stay readable",
          "[execution][kernel][ownership][run]") {
    KernelFixture f;
    auto session = f.session();
    auto first = session.query();
    REQUIRE(first);
    const auto anchor = first->kernelView().runScope;
    REQUIRE(anchor);
    REQUIRE(session.reset());
    REQUIRE(session.configure(f.configuration()));
    REQUIRE(session.prepare(f.prepared()));
    auto second = session.query();
    REQUIRE(second);
    CHECK(second->kernelView().runScope != anchor);
    CHECK(second->kernelView().judgementIdentity == first->kernelView().judgementIdentity);
    CHECK(first->kernelView().runScope == anchor);
}

TEST_CASE("Draft invariant signal and subtraction failures preserve exact prefix",
          "[execution][kernel][failure][overflow]") {
    KernelFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    tap(r);
    detail::KernelTestControls controls{{}, detail::KernelFailurePoint::beforeSeal, {}, {}, {}};
    Tick horizon{903};
    SECTION("duplicate phase") {
        controls.duplicatePhaseTick = Tick{900};
    }
    SECTION("signal overflow") {
        f.base.latePolicy.windowCloseThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{0});
        controls.signalTicks = {Tick{INT64_MAX}};
        horizon = Tick{INT64_MAX};
    }
    SECTION("outcome subtraction overflow") {
        r.timing->successWindows[0].start = Tick{INT64_MIN + 2};
        r.timing->successWindows[0].end = Tick{INT64_MAX - 8};
        r.timing->phaseTargets[0].chartTick = Tick{INT64_MIN + 2};
        r.timing->end = Tick{INT64_MAX - 8};
        horizon = Tick{13};
    }
    auto session = f.session();
    const auto observation = horizon == Tick{13} ? 10 : 900;
    REQUIRE(session.submit({input(observation, 1, InputAction::press)}));
    REQUIRE(detail::KernelTestAccess::inject(session, controls));
    auto failed = session.advance(horizon);
    REQUIRE_FALSE(failed);
    auto p = session.query();
    REQUIRE(p);
    CHECK(p->kernelView().state == KernelSessionState::Faulted);
    REQUIRE(p->kernelView().faultDiagnostic);
    CHECK(p->kernelView().failedTick.has_value());
    CHECK(p->kernelView().requestedHorizon == horizon);
    if (horizon == Tick{INT64_MAX}) {
        CHECK(p->kernelView().facts.size() == 1);
    } else {
        CHECK(p->kernelView().facts.empty());
        CHECK(p->kernelView().contacts.empty());
        CHECK(p->kernelView().receipts.empty());
    }
}

TEST_CASE("Late dispatch collisions reject whole admission without poisoning sequences",
          "[execution][kernel][late][atomic]") {
    KernelFixture f;
    auto session = f.session();
    REQUIRE(session.advance(Tick{903}));
    // At A=903, original 900 forwards to 901; original 901 remains native.
    auto before = session.query();
    REQUIRE(before);
    auto rejected =
        session.submit({input(900, 1, InputAction::press), input(901, 2, InputAction::release)});
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "judgement.s7a4.late.dispatch_collision");
    CHECK(session.query()->kernelView().facts == before->kernelView().facts);
    auto admitted = session.submit({input(901, 1, InputAction::press)});
    REQUIRE(admitted);
    CHECK(admitted->front().dispatchTick == Tick{901});
    CHECK_FALSE(admitted->front().wasForwarded);
    auto duplicate = session.submit({input(901, 9, InputAction::press)});
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().code() == "judgement.s7a2.late.duplicate_queue_entry");
}

TEST_CASE("Matcher branches advance one step retain state across gaps and ignore dead transitions",
          "[execution][kernel][pattern][state]") {
    KernelFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    tap(r);
    r.pattern.root = {
        PatternPrimitive::sequence,
        {{PatternPrimitive::atom, {}, "a", {}, {}}, {PatternPrimitive::atom, {}, "b", {}, {}}},
        {},
        {},
        {}};
    r.patternArmRefs = {"a", "b"};
    r.maxArmElements = MeasuredParameter<std::uint64_t>::measured(2);
    r.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(2);
    r.atomBindings = {
        {"a", "domain.binding.one", "keyboard", "lane.one", InputAction::press, {}, false},
        {"b", "domain.binding.one", "keyboard", "lane.one", InputAction::update, {}, false}};
    r.timing->successWindows = {{Tick{850}, Tick{880}, {PhaseKind::tap, 1}},
                                {Tick{900}, Tick{920}, {PhaseKind::tap, 1}}};
    auto session = f.session();
    REQUIRE(
        session.submit({input(860, 1, InputAction::press), input(870, 2, InputAction::press),
                        input(890, 3, InputAction::update), input(900, 4, InputAction::update)}));
    auto p = session.advance(Tick{923});
    REQUIRE(p);
    REQUIRE(p->kernelView().receipts.size() == 4);
    CHECK(p->kernelView().receipts[0].consumedBy == r.identity);
    CHECK(p->kernelView().receipts[1].consideredCount == 1);
    CHECK_FALSE(p->kernelView().receipts[1].consumedBy);
    CHECK(p->kernelView().receipts[2].consideredCount == 0);
    CHECK_FALSE(p->kernelView().receipts[2].consumedBy);
    CHECK(p->kernelView().receipts[3].consumedBy == r.identity);
    CHECK(state(p->kernelView(), PhaseKind::tap) == KernelPhaseState::Hit);
    CHECK(std::get<ReceiptFact>(p->kernelView().facts[0]).kind == FactKind::consumeEmpty);
    CHECK(std::get<ReceiptFact>(p->kernelView().facts[1]).kind == FactKind::stray);
}

TEST_CASE("Multiple atom matches are parallel alternatives and consume one step",
          "[execution][kernel][pattern][branches]") {
    KernelFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    tap(r);
    r.pattern.root = {
        PatternPrimitive::sequence,
        {{PatternPrimitive::atom, {}, "a", {}, {}}, {PatternPrimitive::atom, {}, "b", {}, {}}},
        {},
        {},
        {}};
    r.patternArmRefs = {"a", "b"};
    r.maxArmElements = MeasuredParameter<std::uint64_t>::measured(2);
    r.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(2);
    r.atomBindings = {
        {"a", "domain.binding.one", "keyboard", "lane.one", InputAction::press, {}, false},
        {"b", "domain.binding.one", "keyboard", "lane.one", InputAction::press, {}, false}};
    auto session = f.session();
    REQUIRE(session.submit({input(860, 1, InputAction::press), input(900, 2, InputAction::press)}));
    auto first = session.advance(Tick{863});
    REQUIRE(first);
    CHECK(state(first->kernelView(), PhaseKind::tap) == KernelPhaseState::Pending);
    CHECK(first->kernelView().receipts[0].consideredCount == 1);
    CHECK(first->kernelView().facts.empty());
    auto second = session.advance(Tick{903});
    REQUIRE(second);
    CHECK(state(second->kernelView(), PhaseKind::tap) == KernelPhaseState::Hit);
    CHECK(second->kernelView().receipts[1].consideredCount == 1);
}

TEST_CASE("Physical repeated and orphan events preserve contact handles independently of outcomes",
          "[execution][kernel][contact]") {
    KernelFixture f;
    auto session = f.session();
    REQUIRE(
        session.submit({input(900, 1, InputAction::press), input(901, 2, InputAction::press),
                        input(902, 3, InputAction::update), input(1000, 4, InputAction::release),
                        input(1001, 5, InputAction::release), input(1002, 6, InputAction::press)}));
    auto first = session.advance(Tick{903});
    REQUIRE(first);
    REQUIRE(first->kernelView().contacts.size() == 1);
    const auto handle = first->kernelView().contacts[0];
    auto repeats = session.advance(Tick{905});
    REQUIRE(repeats);
    CHECK(repeats->kernelView().contacts[0] == handle);
    CHECK_FALSE(repeats->kernelView().receipts[2].consumedBy);
    auto last = session.advance(Tick{1005});
    REQUIRE(last);
    REQUIRE(last->kernelView().contacts.size() == 1);
    CHECK(last->kernelView().contacts[0] != handle);
    CHECK(last->kernelView().ownership.empty());
    CHECK(last->kernelView().contacts[0].startPressKey.observationTick ==
          ObservationTick{Tick{1002}});
}

TEST_CASE("Owner control precedes free resource contenders and global fanout stays one",
          "[execution][kernel][k4][owner]") {
    KernelFixture f;
    auto& source = f.assembly.sources[0].document;
    auto contender = source.requirements[0];
    tap(contender);
    contender.identity.requirementLocalId = "contender";
    contender.stableId.declarationOrdinal = 5;
    contender.timing->end = Tick{1100};
    contender.timing->successWindows[0] = {Tick{850}, Tick{1090}, {PhaseKind::tap, 1}};
    contender.atomBindings[0].action = InputAction::release;
    auto resource = source.resources[0];
    resource.ref.resourceId = "aaa.free";
    source.resources.push_back(resource);
    contender.resourceClaims[0].resourceRef = resource.ref;
    contender.resourceClaims[0].claimPolicy.competition = {-100, 0};
    source.requirements.push_back(contender);
    source.declarations.push_back(testDeclaration(DeclarationKind::requirement, "contender", 5));
    source.declarations.push_back(testDeclaration(DeclarationKind::resourceRecord, "aaa.free", 6));
    auto session = f.session();
    REQUIRE(
        session.submit({input(900, 1, InputAction::press), input(950, 2, InputAction::release)}));
    auto p = session.advance(Tick{953});
    REQUIRE(p);
    CHECK(p->kernelView().receipts[1].consumedBy == source.requirements[0].identity);
    auto contenderPhase =
        std::find_if(p->kernelView().phases.begin(), p->kernelView().phases.end(),
                     [&](const auto& phase) { return phase.requirement == contender.identity; });
    REQUIRE(contenderPhase != p->kernelView().phases.end());
    CHECK(contenderPhase->state == KernelPhaseState::Pending);
    CHECK(p->kernelView().receipts[1].consideredCount == 2);
}

TEST_CASE("Blocked epsilon is never retried but later explicit input can still match",
          "[execution][kernel][epsilon][blocked]") {
    for (bool laterInput : {false, true}) {
        KernelFixture f;
        auto& source = f.assembly.sources[0].document;
        auto r = source.requirements[0];
        tap(r);
        r.identity.requirementLocalId = "epsilon";
        r.stableId.declarationOrdinal = 5;
        r.pattern.root = {PatternPrimitive::choice,
                          {{PatternPrimitive::instant, {}, {}, {}, {}}, r.pattern.root},
                          {},
                          {},
                          {}};
        r.patternArmRefs = {"atom.one"};
        r.atomBindings[0].channelToken = "lane.two";
        r.timing->end = Tick{1040};
        r.timing->phaseTargets[0].chartTick = Tick{1010};
        r.timing->successWindows = {{Tick{910}, Tick{920}, {PhaseKind::tap, 1}},
                                    {Tick{1001}, Tick{1020}, {PhaseKind::tap, 1}}};
        r.resourceClaims[0].claimPolicy.competition = {0, 8};
        source.requirements.push_back(r);
        source.declarations.push_back(testDeclaration(DeclarationKind::requirement, "epsilon", 5));
        auto session = f.session();
        std::vector<ClockedIngress> traffic{input(900, 1, InputAction::press),
                                            input(1000, 2, InputAction::release)};
        if (laterInput) {
            auto event = input(1005, 3, InputAction::press);
            event.declaration.channel = *ChannelRef::fromToken("lane.two");
            traffic.push_back(event);
        }
        REQUIRE(session.submit(traffic));
        auto p = session.advance(Tick{1103});
        REQUIRE(p);
        CHECK(state(p->kernelView(), PhaseKind::tap) ==
              (laterInput ? KernelPhaseState::Hit : KernelPhaseState::Miss));
        const auto fact = std::find_if(p->kernelView().facts.begin(), p->kernelView().facts.end(),
                                       [](const auto& fact) {
                                           const auto* v = std::get_if<PhaseOutcomeFact>(&fact);
                                           return v && v->phase == PhaseKind::tap;
                                       });
        REQUIRE(fact != p->kernelView().facts.end());
        CHECK(std::get<PhaseOutcomeFact>(*fact).commitTick == Tick{laterInput ? 1005 : 1020});
    }
}

TEST_CASE("Observer dead transition and expiry create records without phase outcomes",
          "[execution][kernel][observer][expiry]") {
    KernelFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    tap(r);
    r.pattern.root = {PatternPrimitive::sequence,
                      {r.pattern.root, {PatternPrimitive::atom, {}, "b", {}, {}}},
                      {},
                      {},
                      {}};
    r.patternArmRefs = {"atom.one", "b"};
    r.maxArmElements = MeasuredParameter<std::uint64_t>::measured(2);
    r.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(2);
    r.atomBindings.push_back(
        {"b", "domain.binding.one", "keyboard", "lane.one", InputAction::update, {}, false});
    r.resourceClaims[0].intent = ResourceClaimIntent::observe;
    r.resourceClaims[0].claimPolicy.competition.reset();
    r.resourceClaims[0].claimPolicy.claimKeyToken.clear();
    auto session = f.session();
    REQUIRE(session.submit({input(860, 1, InputAction::press), input(870, 2, InputAction::press)}));
    auto p = session.advance(Tick{1103});
    REQUIRE(p);
    CHECK(state(p->kernelView(), PhaseKind::tap) == KernelPhaseState::Expired);
    REQUIRE(p->kernelView().observers.size() == 2);
    CHECK(p->kernelView().observers[0].progressed);
    CHECK_FALSE(p->kernelView().observers[1].progressed);
    CHECK(std::none_of(p->kernelView().facts.begin(), p->kernelView().facts.end(),
                       [](const auto& f) { return std::holds_alternative<PhaseOutcomeFact>(f); }));
    REQUIRE(p->kernelView().facts.size() == 2);
    CHECK(std::get<ReceiptFact>(p->kernelView().facts[0]).kind == FactKind::consumeEmpty);
}

TEST_CASE("Fixed seed admission traces match S1 and one-shot versus segmented advance",
          "[execution][kernel][oracle][random]") {
    constexpr std::uint64_t seed = 740304;
    for (unsigned scenario = 0; scenario < 24; ++scenario) {
        KernelFixture f;
        if (scenario % 3 == 0) {
            tap(f.assembly.sources[0].document.requirements[0]);
        }
        if (scenario % 4 == 1 || scenario % 4 == 2) {
            auto& source = f.assembly.sources[0].document;
            auto contender = source.requirements[0];
            contender.identity.requirementLocalId = "random.contender";
            contender.stableId.declarationOrdinal = 5;
            contender.resourceClaims[0].claimPolicy.competition = {scenario % 4 == 1 ? -2 : -1, 8};
            source.requirements.push_back(contender);
            source.declarations.push_back(
                testDeclaration(DeclarationKind::requirement, "random.contender", 5));
            source.relations[0].members.push_back(contender.stableId);
        }
        auto batchSession = f.session();
        auto segmented = f.session();
        std::uint64_t generator = seed + scenario;
        std::vector<ClockedIngress> traffic;
        for (std::uint64_t i = 0; i < 40; ++i) {
            generator = generator * 6364136223846793005ULL + 1442695040888963407ULL;
            auto event = input(840 + static_cast<std::int64_t>(i) * 5, i + 1,
                               static_cast<InputAction>((generator >> 32) % 3));
            if ((generator & 7) == 0) {
                event.declaration.channel = *ChannelRef::fromToken("unmatched.channel");
            }
            traffic.push_back(event);
        }
        auto admitted = batchSession.submit(traffic);
        REQUIRE(admitted);
        REQUIRE(segmented.submit(traffic));
        std::vector<AdmittedObservation> trace;
        for (const auto& a : *admitted) {
            trace.push_back({a.observationKey, a.dispatchTick, a.wasForwarded, a.admissionFrontier,
                             a.admissionHorizon});
        }
        const auto prepared = f.prepared();
        const auto configuration = f.configuration();
        for (auto tick : {850, 875, 900, 925, 950, 975, 1000, 1020, 1040, 1100}) {
            INFO(seed);
            INFO(scenario);
            INFO(tick);
            auto p = segmented.advance(Tick{tick + 3});
            REQUIRE(p);
            auto ref = scanReference(prepared, configuration, trace, Tick{tick + 3});
            const auto differs = [](const KernelProjection& a, const KernelProjection& b) {
                return a.phases != b.phases || a.resources != b.resources ||
                       a.contacts != b.contacts || a.ownership != b.ownership ||
                       a.receipts != b.receipts || a.facts != b.facts;
            };
            if (differs(p->kernelView(), ref)) {
                // Deterministic deletion minimization retains the failing prefix and reproduces
                // admission for each candidate. It runs only on a mismatch, outside the kernel.
                auto minimal = traffic;
                const auto reproduces = [&](const std::vector<ClockedIngress>& candidate) {
                    auto trial = f.session();
                    auto admissions = trial.submit(candidate);
                    if (!admissions) {
                        return false;
                    }
                    std::vector<AdmittedObservation> trialTrace;
                    for (const auto& a : *admissions) {
                        trialTrace.push_back({a.observationKey, a.dispatchTick, a.wasForwarded,
                                              a.admissionFrontier, a.admissionHorizon});
                    }
                    auto actual = trial.advance(Tick{tick + 3});
                    if (!actual) {
                        return false;
                    }
                    return differs(actual->kernelView(), scanReference(prepared, configuration,
                                                                       trialTrace, Tick{tick + 3}));
                };
                for (std::size_t index = 0; index < minimal.size();) {
                    auto candidate = minimal;
                    candidate.erase(candidate.begin() + static_cast<std::ptrdiff_t>(index));
                    if (reproduces(candidate)) {
                        minimal = std::move(candidate);
                        index = 0;
                    } else {
                        ++index;
                    }
                }
                std::ostringstream description;
                description << "minimal trace: seed=" << seed << " scenario=" << scenario
                            << " horizon=" << tick + 3 << " count=" << minimal.size();
                for (const auto& event : minimal) {
                    description << " [tick=" << event.observationTick.tick().value()
                                << " channel=" << event.declaration.channel.token()
                                << " action=" << static_cast<unsigned>(event.declaration.action)
                                << "]";
                }
                description << "; structural diff: phases="
                            << (p->kernelView().phases != ref.phases)
                            << " resources=" << (p->kernelView().resources != ref.resources)
                            << " contacts=" << (p->kernelView().contacts != ref.contacts)
                            << " ownership=" << (p->kernelView().ownership != ref.ownership)
                            << " receipts=" << (p->kernelView().receipts != ref.receipts)
                            << " facts=" << (p->kernelView().facts != ref.facts);
                FAIL(description.str());
            }
            CHECK(p->kernelView().phases == ref.phases);
            CHECK(p->kernelView().resources == ref.resources);
            CHECK(p->kernelView().contacts == ref.contacts);
            CHECK(p->kernelView().ownership == ref.ownership);
            CHECK(p->kernelView().receipts == ref.receipts);
            CHECK(p->kernelView().facts == ref.facts);
        }
        auto once = batchSession.advance(Tick{1103});
        REQUIRE(once);
        auto split = segmented.query();
        REQUIRE(split);
        CHECK(once->kernelView().facts == split->kernelView().facts);
        CHECK(once->kernelView().receipts == split->kernelView().receipts);
        CHECK(once->kernelView().contacts == split->kernelView().contacts);
        CHECK(once->kernelView().resources == split->kernelView().resources);
    }
}

TEST_CASE("Forwarded early release at bodyEnd uses timer-after state and never rewrites coverage",
          "[execution][kernel][late][oracle]") {
    KernelFixture f;
    auto session = f.session();
    auto first = session.submit({input(900, 1, InputAction::press)});
    REQUIRE(first);
    REQUIRE(session.advance(Tick{1002})); // P=999; bodyEnd has not sealed.
    auto release = session.submit({input(999, 2, InputAction::release)});
    REQUIRE(release);
    REQUIRE(release->size() == 1);
    CHECK(release->front().wasForwarded);
    CHECK(release->front().dispatchTick == Tick{1000});
    std::vector<AdmittedObservation> trace;
    for (const auto& a : *first) {
        trace.push_back({a.observationKey, a.dispatchTick, a.wasForwarded, a.admissionFrontier,
                         a.admissionHorizon});
    }
    for (const auto& a : *release) {
        trace.push_back({a.observationKey, a.dispatchTick, a.wasForwarded, a.admissionFrontier,
                         a.admissionHorizon});
    }
    auto p = session.advance(Tick{1103});
    REQUIRE(p);
    auto ref = scanReference(f.prepared(), f.configuration(), trace, Tick{1103});
    CHECK(p->kernelView().phases == ref.phases);
    CHECK(p->kernelView().resources == ref.resources);
    CHECK(p->kernelView().receipts == ref.receipts);
    CHECK(p->kernelView().facts == ref.facts);
    CHECK(state(p->kernelView(), PhaseKind::body) == KernelPhaseState::Miss);
    CHECK(state(p->kernelView(), PhaseKind::tail) == KernelPhaseState::Miss);
    CHECK_FALSE(p->kernelView().receipts.back().consumedBy);
    const auto& body = std::get<PhaseOutcomeFact>(p->kernelView().facts[1]);
    CHECK(body.commitTick == Tick{1000});
    CHECK_FALSE(body.error);
    CHECK(std::holds_alternative<TimerOrigin>(body.originId));
}

TEST_CASE("Automaton physical sizes reject overflow before allocation",
          "[execution][pattern][overflow]") {
    CHECK(detail::checkedDfaTableEntries(3, 7) == 21);
    CHECK(detail::checkedDfaTableEntries(0, SIZE_MAX) == 0);
    CHECK(detail::checkedDfaStateSum(3, 7) == 10);
    CHECK_THROWS_AS(detail::checkedDfaTableEntries(SIZE_MAX, 2), std::length_error);
    CHECK_THROWS_AS(detail::checkedDfaStateSum(SIZE_MAX, 1), std::length_error);
    CHECK_THROWS_AS(detail::checkedDfaStateSum(1, SIZE_MAX), std::length_error);
}

TEST_CASE("Prefix complement materializes live self loops while containment rejects unbounded "
          "publication",
          "[execution][pattern][loop][containment]") {
    KernelFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    tap(r);
    r.pattern.root = {PatternPrimitive::complement, {r.pattern.root}, {}, {}, {}};
    auto compiled = compilePattern(r.pattern);
    REQUIRE(compiled);
    auto program = compiled->executionProgram();
    REQUIRE(program);
    bool liveLoop = false;
    const auto symbols = program->atomRefs.size();
    for (std::size_t state = 0; state < program->accepting.size(); ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            liveLoop |=
                program->live[state] && program->transitions[state * symbols + symbol] == state;
        }
    }
    CHECK(liveLoop);
    auto rejected = prepareResolvedGameplay({f.assembly, {}});
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "judgement.s7a3.declaration.structurally_incomplete");
}

TEST_CASE("Held leases cannot be preempted by a higher priority later candidate",
          "[execution][kernel][k4][held]") {
    KernelFixture f;
    auto& source = f.assembly.sources[0].document;
    auto contender = source.requirements[0];
    tap(contender);
    contender.identity.requirementLocalId = "later";
    contender.stableId.declarationOrdinal = 5;
    contender.timing->end = Tick{1040};
    contender.timing->successWindows[0] = {Tick{950}, Tick{990}, {PhaseKind::tap, 1}};
    contender.timing->phaseTargets[0].chartTick = Tick{950};
    contender.resourceClaims[0].claimPolicy.competition = {-100, 0};
    source.requirements.push_back(contender);
    source.declarations.push_back(testDeclaration(DeclarationKind::requirement, "later", 5));
    source.relations[0].members.push_back(contender.stableId);
    auto session = f.session();
    REQUIRE(session.submit({input(900, 1, InputAction::press), input(950, 2, InputAction::press)}));
    auto p = session.advance(Tick{953});
    REQUIRE(p);
    REQUIRE(p->kernelView().ownership.size() == 1);
    CHECK(p->kernelView().ownership[0].requirement == source.requirements[0].identity);
    CHECK_FALSE(p->kernelView().receipts.back().consumedBy);
    CHECK(p->kernelView().receipts.back().consideredCount == 1);
}

TEST_CASE("Calibration and complete configuration mismatch preserve prepare publication",
          "[execution][kernel][configuration]") {
    KernelFixture f;
    auto first = f.session();
    auto c = f.configuration();
    c.calibrationIdentityToken = "calibration.changed";
    auto second = JudgementSession::create();
    REQUIRE(second);
    REQUIRE(second->configure(c));
    REQUIRE(second->prepare(f.prepared()));
    CHECK(first.query()->kernelView().judgementIdentity !=
          second->query()->kernelView().judgementIdentity);
    auto mismatch = f.configuration();
    SECTION("timebase") {
        auto t = f.base.timebase;
        t.originBeat = makeBeat(1, 1);
        mismatch.timebase = OwnedTimebaseProfile{t};
    }
    SECTION("late") {
        mismatch.latePolicy.windowOpenThreshold =
            MeasuredParameter<TickSpan>::measured(TickSpan{1});
    }
    SECTION("ruleset") {
        mismatch.identityDeclarations.ruleset.buildHash += ".changed";
    }
    SECTION("engine") {
        mismatch.identityDeclarations.engine.fixedPointTableId += ".changed";
    }
    auto third = JudgementSession::create();
    REQUIRE(third);
    auto configured = third->configure(mismatch);
    if (configured) {
        CHECK_FALSE(third->prepare(f.prepared()));
    } else {
        CHECK(configured.error().code() == "judgement.s7a4.execution.profile_unsupported");
        CHECK(contextValue(configured.error(), "category") == "capability_disabled");
    }
    CHECK_FALSE(third->hasPreparedState());
}

TEST_CASE("Virtual independent competition has one winner and rejects pair collisions",
          "[execution][kernel][k4][independent]") {
    KernelFixture f;
    auto& source = f.assembly.sources[0].document;
    tap(source.requirements[0]);
    source.requirements[0].resourceClaims.clear();
    source.requirements[0].independentCompetition = {0, 7};
    source.relations.clear();
    source.resources.clear();
    std::erase_if(source.declarations,
                  [](const auto& d) { return d.kind == DeclarationKind::resourceRecord; });
    auto contender = source.requirements[0];
    contender.identity.requirementLocalId = "independent.two";
    contender.stableId.declarationOrdinal = 5;
    contender.independentCompetition = {-1, 8};
    source.requirements.push_back(contender);
    source.declarations.push_back(
        testDeclaration(DeclarationKind::requirement, "independent.two", 5));
    auto session = f.session();
    REQUIRE(session.submit({input(900, 1, InputAction::press)}));
    auto p = session.advance(Tick{903});
    REQUIRE(p);
    CHECK(p->kernelView().resources.empty());
    REQUIRE(p->kernelView().receipts.size() == 1);
    CHECK(p->kernelView().receipts[0].consideredCount == 2);
    CHECK(p->kernelView().receipts[0].consumedBy == contender.identity);
    REQUIRE(p->kernelView().facts.size() == 1);
    source.requirements[1].independentCompetition = source.requirements[0].independentCompetition;
    auto collision = prepareResolvedGameplay({f.assembly, {}});
    REQUIRE_FALSE(collision);
    CHECK(collision.error().code() == "judgement.s7a4.execution.relation_invalid");
}

TEST_CASE("Contact subjects include source class even with identical channel and press time",
          "[execution][kernel][contact][source]") {
    KernelFixture keyboard;
    auto first = keyboard.session();
    REQUIRE(first.submit({input(900, 1, InputAction::press)}));
    auto a = first.advance(Tick{903});
    REQUIRE(a);
    REQUIRE(a->kernelView().contacts.size() == 1);
    KernelFixture controller;
    for (auto& binding : controller.assembly.sources[0].document.requirements[0].atomBindings) {
        binding.sourceClass = "controller";
    }
    auto c = controller.configuration();
    InputMappingProfile mapping{
        "mapping",
        "v1",
        *SourceClass::fromToken("controller"),
        {{"domain.binding.one", {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
    c.inputMapping = OwnedInputMappingProfile{mapping};
    auto second = JudgementSession::create();
    REQUIRE(second);
    REQUIRE(second->configure(c));
    REQUIRE(second->prepare(controller.prepared()));
    REQUIRE(second->submit({input(900, 1, InputAction::press)}));
    auto b = second->advance(Tick{903});
    REQUIRE(b);
    REQUIRE(b->kernelView().contacts.size() == 1);
    CHECK(a->kernelView().contacts[0] != b->kernelView().contacts[0]);
    CHECK(a->kernelView().contacts[0].startPressKey.sourceClass == "keyboard");
    CHECK(b->kernelView().contacts[0].startPressKey.sourceClass == "controller");
}
