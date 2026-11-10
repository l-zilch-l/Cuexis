#include "execution_kernel_test_fixture.hpp"

// Head, tail, owner, timer and matcher phase cases.

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
    CHECK(conflict.error().code() == "judgement.late.rejected");
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
        CHECK(failed.error().code() == "judgement.kernel.transaction_failed");
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
