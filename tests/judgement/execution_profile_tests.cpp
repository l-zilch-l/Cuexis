#include "../../engine/judgement/src/ingress_transaction.hpp"
#include "execution_test_fixture.hpp"

#include <limits>

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;

TEST_CASE("Execution late gate has independently derived admission boundaries",
          "[execution][late]") {
    auto policy = executionLate();
    const std::array<std::pair<std::int64_t, std::optional<std::int64_t>>, 5> golden{
        {{102, 100}, {103, 101}, {106, 104}, {107, {}}, {108, {}}}};
    for (auto [horizon, dispatch] : golden) {
        auto result = routeExecutionObservation(policy, Tick{100}, Tick{horizon});
        REQUIRE(static_cast<bool>(result) == dispatch.has_value());
        if (dispatch) {
            CHECK(result->dispatchTick == Tick{*dispatch});
            CHECK(result->wasForwarded == (*dispatch != 100));
        }
    }
    CHECK(routeExecutionObservation(policy, Tick{-100}, Tick{-97})->dispatchTick == Tick{-99});
    CHECK_FALSE(routeExecutionObservation(policy, Tick{100}, Tick{103}, true));
    CHECK_FALSE(routeExecutionObservation(policy, Tick{INT64_MAX}, {}));
    CHECK_FALSE(routeExecutionObservation(policy, Tick{INT64_MIN}, {}));
    CHECK_FALSE(routeExecutionObservation(executionLate(LateEventPolicy::rejectLate), Tick{100},
                                          Tick{103}));
}

TEST_CASE("Execution late parameters enforce measured relationships",
          "[execution][late][negative]") {
    auto p = executionLate();
    SECTION("negative open") {
        p.windowOpenThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{-1});
    }
    SECTION("close equal final") {
        p.windowCloseThreshold = p.finalizationWatermark;
    }
    SECTION("zero queue hop") {
        p.maxQueueHop = MeasuredParameter<TickSpan>::measured(TickSpan{0});
    }
    SECTION("hop greater than span") {
        p.maxQueueHop = MeasuredParameter<TickSpan>::measured(TickSpan{6});
    }
    SECTION("reject with hop") {
        p.policy = LateEventPolicy::rejectLate;
    }
    SECTION("pending") {
        p.windowOpenThreshold = MeasuredParameter<TickSpan>::pendingMeasurement();
    }
    SECTION("missing") {
        p.policy.reset();
    }
    const auto result = validateExecutionLateParameters(p);
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == "judgement.s7a4.late.parameters_invalid");
}

TEST_CASE("Executable prepare owns program targets declarations and stable timers",
          "[execution][prepare]") {
    Fixture fixture;
    fixture.latePolicy = executionLate();
    auto assembly = fixture.request();
    executionFields(assembly);
    auto prepared = prepareResolvedGameplay({assembly, {}});
    INFO((prepared ? "" : std::string{prepared.error().message()}));
    REQUIRE(prepared);
    CHECK(prepared->requirements().front().deadline == Tick{1040});
    CHECK(prepared->requirements().front().executionProgram.has_value());
    CHECK(prepared->identityDeclarations() == assembly.identityDeclarations);
    CHECK(prepared->timers().size() == 9);
    auto changed = assembly;
    changed.sources[0].document.requirements[0].timing->phaseTargets[0].chartTick = Tick{899};
    auto second = prepareResolvedGameplay({changed, {}});
    REQUIRE(second);
    CHECK(prepared->assembled().chart != second->assembled().chart);
    CHECK_FALSE(semanticDiff(prepared->assembled().graph, second->assembled().graph).empty());
}

TEST_CASE("Executable prepare rejects incomplete and inconsistent phase binding fields atomically",
          "[execution][prepare][negative][atomic]") {
    Fixture fixture;
    fixture.latePolicy = executionLate();
    auto assembly = fixture.request();
    executionFields(assembly);
    PreparedGameplayPublication live;
    REQUIRE(prepareResolvedInto(live, {assembly, {}}));
    auto identity = live.active()->assembled().prepared;
    auto& r = assembly.sources[0].document.requirements[0];
    SECTION("no target") {
        r.timing->phaseTargets.clear();
    }
    SECTION("duplicate target") {
        r.timing->phaseTargets.push_back(r.timing->phaseTargets[0]);
    }
    SECTION("target outside") {
        r.timing->phaseTargets[0].chartTick = Tick{800};
    }
    SECTION("body target") {
        r.timing->phaseTargets[1].chartTick = Tick{999};
    }
    SECTION("phase ordinal collision") {
        r.phases[2].declarationOrdinal = 2;
    }
    SECTION("missing binding") {
        r.atomBindings.erase(r.atomBindings.begin());
    }
    SECTION("binding duplicate") {
        r.atomBindings.push_back(r.atomBindings[0]);
    }
    SECTION("unknown action") {
        r.atomBindings[0].action = static_cast<InputAction>(255);
    }
    SECTION("head release ending") {
        r.atomBindings[0].action = InputAction::release;
    }
    SECTION("head epsilon") {
        r.pattern.root = {PatternPrimitive::instant, {}, {}, {}, {}};
    }
    SECTION("extra independent pair") {
        r.independentCompetition = {0, 1};
    }
    SECTION("body not coverage") {
        r.timing->successWindows[1].end = Tick{999};
    }
    SECTION("tail flag") {
        r.requiresReleaseTailSemantics = false;
    }
    SECTION("timer overflow") {
        r.timing->end = Tick{INT64_MAX};
    }
    CHECK_FALSE(prepareResolvedInto(live, {assembly, {}}));
    CHECK(live.active()->assembled().prepared == identity);
}

TEST_CASE("Ingress journals own subjects and publish only after full reservation",
          "[execution][ingress][atomic][ownership]") {
    auto mapping = InputMappingProfile{
        "mapping",
        "v1",
        *SourceClass::fromToken("keyboard"),
        {{"domain", {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
    SessionIngressState live;
    const auto input = [](std::int64_t tick, std::uint64_t sequence, InputAction action) {
        return ClockedIngress{ObservationTick{Tick{tick}},
                              {IngressSequence{sequence},
                               action,
                               *ChannelRef::fromToken("lane"),
                               "domain",
                               {0, 0, 0, 0},
                               {false, false, false},
                               ContinuityKind::discrete,
                               {}}};
    };
    std::vector<ClockedIngress> batch{input(11, 1, InputAction::release),
                                      input(10, 2, InputAction::press)};
    auto journal = detail::prepareIngressBatch(live, mapping, batch);
    REQUIRE(journal);
    CHECK(live.lastObservedTick() == nullptr);
    CHECK(journal->entries[0].observation.observationTick == ObservationTick{Tick{10}});
    CHECK(journal->entries[0].observation.observationId == ObservationId{0});
    CHECK(journal->entries[1].observation.observationId == ObservationId{1});
    REQUIRE(detail::reserveIngressBatch(live, *journal));
    detail::commitIngressBatch(live, std::move(*journal));
    CHECK(*live.lastObservedTick() == ObservationTick{Tick{11}});
    auto duplicatedSequence = input(12, 1, InputAction::press);
    CHECK_FALSE(detail::prepareIngressBatch(live, mapping, std::span{&duplicatedSequence, 1}));
    auto reversed = input(9, 3, InputAction::press);
    CHECK_FALSE(detail::prepareIngressBatch(live, mapping, std::span{&reversed, 1}));
    CHECK(*live.lastObservedTick() == ObservationTick{Tick{11}});
    live.reset();
    std::string channel(80, 'x');
    auto first = input(10, 8, InputAction::press);
    first.declaration.channel = *ChannelRef::fromToken(channel);
    auto owned = detail::prepareIngressBatch(live, mapping, std::span{&first, 1});
    REQUIRE(owned);
    REQUIRE(detail::reserveIngressBatch(live, *owned));
    detail::commitIngressBatch(live, std::move(*owned));
    channel.assign(80, 'y');
    const std::string original(80, 'x');
    first.declaration.channel = *ChannelRef::fromToken(original);
    first.declaration.ingressSequence = IngressSequence{9};
    auto duplicateSubject = detail::prepareIngressBatch(live, mapping, std::span{&first, 1});
    REQUIRE_FALSE(duplicateSubject);
    CHECK(duplicateSubject.error().code() == "judgement.s7a2.late.duplicate_queue_entry");
}

TEST_CASE("Every execution field reaches both canonical identity projections",
          "[execution][identity][mutation]") {
    Fixture f;
    f.latePolicy = executionLate();
    auto assembly = f.request();
    executionFields(assembly);
    auto prepared = prepareResolvedGameplay({assembly, {}});
    REQUIRE(prepared);
    auto graph = prepared->assembled().graph;
    const auto originalChart = makeChartIdentity(graph);
    const auto originalContent = makeContentIdentity(graph);
    auto& r = graph.requirements[0];
    SECTION("execution token") {
        graph.executionProfile += ".changed";
    }
    SECTION("normalization") {
        graph.normalizationProfileToken += ".changed";
    }
    SECTION("coordinator") {
        graph.coordinatorPolicyToken += ".changed";
    }
    SECTION("target kind") {
        r.timing->phaseTargets[0].phase = PhaseKind::tap;
    }
    SECTION("target tick") {
        r.timing->phaseTargets[0].chartTick = Tick{899};
    }
    SECTION("atom") {
        r.atomBindings[0].atomRef += ".changed";
    }
    SECTION("domain") {
        r.atomBindings[0].domainToken += ".changed";
    }
    SECTION("source") {
        r.atomBindings[0].sourceClass += ".changed";
    }
    SECTION("channel") {
        r.atomBindings[0].channelToken += ".changed";
    }
    SECTION("action") {
        r.atomBindings[0].action = InputAction::update;
    }
    SECTION("range presence") {
        r.atomBindings[0].amountRange = AmountMatchRange{-1, 1};
    }
    SECTION("range minimum") {
        r.atomBindings[0].amountRange = AmountMatchRange{-2, 1};
    }
    SECTION("range maximum") {
        r.atomBindings[0].amountRange = AmountMatchRange{-1, 2};
    }
    SECTION("tail only") {
        r.atomBindings[0].tailOnly = true;
    }
    SECTION("independent presence") {
        r.independentCompetition = {-1, 7};
    }
    SECTION("independent priority") {
        r.independentCompetition = {-2, 7};
    }
    SECTION("independent rank") {
        r.independentCompetition = {-1, 8};
    }
    CHECK(makeChartIdentity(graph) != originalChart);
    CHECK(makeContentIdentity(graph) != originalContent);
    CHECK_FALSE(semanticDiff(prepared->assembled().graph, graph).empty());
}

TEST_CASE("Late parameter invalidity and same Tick precedence never publish ingress",
          "[execution][late][atomic]") {
    auto mapping = InputMappingProfile{
        "mapping",
        "v1",
        *SourceClass::fromToken("keyboard"),
        {{"domain", {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
    SessionIngressState live;
    ClockedIngress a{ObservationTick{Tick{-10}},
                     {IngressSequence{1},
                      InputAction::press,
                      *ChannelRef::fromToken("lane"),
                      "domain",
                      {0, 0, 0, 0},
                      {false, false, false},
                      ContinuityKind::discrete,
                      {}}};
    auto b = a;
    b.declaration.ingressSequence = IngressSequence{2};
    b.declaration.action = InputAction::release;
    auto distinct = detail::prepareIngressBatch(live, mapping, std::vector<ClockedIngress>{a, b});
    REQUIRE_FALSE(distinct);
    CHECK(distinct.error().code() == "judgement.s7a2.input.same_tick_collision");
    CHECK(live.lastObservedTick() == nullptr);
    auto duplicate = a;
    duplicate.declaration.ingressSequence = IngressSequence{2};
    auto same =
        detail::prepareIngressBatch(live, mapping, std::vector<ClockedIngress>{a, duplicate});
    REQUIRE_FALSE(same);
    CHECK(same.error().code() == "judgement.s7a2.late.duplicate_queue_entry");
    b.declaration.ingressSequence = IngressSequence{1};
    auto sequence = detail::prepareIngressBatch(live, mapping, std::vector<ClockedIngress>{a, b});
    REQUIRE_FALSE(sequence);
    CHECK(sequence.error().code() == "judgement.s7a2.input.ingress_sequence_duplicate");
    auto p = executionLate();
    SECTION("negative open") {
        p.windowOpenThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{-1});
    }
    SECTION("close equals final") {
        p.windowCloseThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{8});
    }
    SECTION("zero queue hop") {
        p.maxQueueHop = MeasuredParameter<TickSpan>::measured(TickSpan{0});
    }
    SECTION("queue too far") {
        p.maxQueueHop = MeasuredParameter<TickSpan>::measured(TickSpan{6});
    }
    SECTION("reject with hop") {
        p.policy = LateEventPolicy::rejectLate;
    }
    SECTION("pending") {
        p.finalizationWatermark = {};
    }
    SECTION("mode absent") {
        p.policy.reset();
    }
    auto valid = validateExecutionLateParameters(p);
    REQUIRE_FALSE(valid);
    CHECK(contextValue(valid.error(), "category") == "late_policy_incomplete");
    CHECK(live.lastObservedTick() == nullptr);
}
