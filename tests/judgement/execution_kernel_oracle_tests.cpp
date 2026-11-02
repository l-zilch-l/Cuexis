#include "execution_kernel_test_fixture.hpp"

// Reset, invariant, trace-oracle and containment cases.

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
    CHECK(rejected.error().code() == "judgement.late.dispatch_collision");
    CHECK(session.query()->kernelView().facts == before->kernelView().facts);
    auto admitted = session.submit({input(901, 1, InputAction::press)});
    REQUIRE(admitted);
    CHECK(admitted->front().dispatchTick == Tick{901});
    CHECK_FALSE(admitted->front().wasForwarded);
    auto duplicate = session.submit({input(901, 9, InputAction::press)});
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().code() == "judgement.late.duplicate_queue_entry");
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
    CHECK(rejected.error().code() == "judgement.declaration.structurally_incomplete");
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
        CHECK(configured.error().code() == "judgement.execution.profile_unsupported");
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
    CHECK(collision.error().code() == "judgement.execution.relation_invalid");
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
