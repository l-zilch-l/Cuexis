#include "../../engine/judgement/src/execution_testing.hpp"
#include "execution_reference.hpp"
#include "execution_test_fixture.hpp"
#include <cuexis/judgement/publication.hpp>

#include <thread>

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
namespace {
struct PublicationFixture final {
    Fixture base;
    AssemblyRequest assembly{base.request()};
    PublicationFixture(bool realtime = false) {
        base.latePolicy = executionLate();
        executionFields(assembly);
        if (realtime) {
            assembly.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
            assembly.identityDeclarations.engine.executionProfileToken = assembly.executionProfile;
            assembly.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
            assembly.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
        }
    }
    auto configuration(bool fold) -> SessionConfiguration {
        InputMappingProfile mapping{
            "mapping",
            "v1",
            *SourceClass::fromToken("keyboard"),
            {{"domain.binding.one",
              {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
        SessionConfiguration config{OwnedInputMappingProfile{mapping},
                                    OwnedTimebaseProfile{base.timebase},
                                    base.latePolicy,
                                    assembly.identityDeclarations,
                                    "calibration.fixture",
                                    assembly.executionProfile,
                                    *assembly.identityDeclarations.engine.lateAlgorithmToken,
                                    assembly.identityDeclarations.engine.factSemanticRevision};
        if (fold) {
            RulesetDeclaration rules{"cuexis.ruleset.finite",
                                     "1",
                                     "cuexis.finite-fold.1",
                                     "loadout.fixture",
                                     ProgramPolicy::locked,
                                     OutcomeScope::declared,
                                     {{"score", "1", "cuexis.finite-fold.1"},
                                      {"combo", "1", "cuexis.finite-fold.1"},
                                      {"statistics", "1", "cuexis.finite-fold.1"}},
                                     {0, INT64_MIN, INT64_MAX, ArithmeticPolicy::checked, 0, {}},
                                     {},
                                     false,
                                     false};
            for (const auto phase :
                 {PhaseKind::tap, PhaseKind::head, PhaseKind::body, PhaseKind::tail}) {
                rules.score.rules.push_back({phase, Outcome::hit, {}, 2, true});
                rules.score.rules.push_back({phase, Outcome::miss, {}, -1, false});
            }
            auto prepared = prepareRuleset(rules);
            REQUIRE(prepared);
            config.ruleset = *prepared;
        }
        return config;
    }
    auto inputs(bool fold = true) -> RecoveryInputs {
        auto prepared = prepareResolvedGameplay({assembly, {}});
        REQUIRE(prepared);
        return {configuration(fold), *prepared};
    }
    auto session(bool fold = true) -> JudgementSession {
        auto s = JudgementSession::create();
        REQUIRE(s);
        auto dependencies = inputs(fold);
        REQUIRE(s->configure(dependencies.configuration));
        REQUIRE(s->prepare(dependencies.prepared));
        return std::move(*s);
    }
};
auto read(const JudgementSession& s, std::uint64_t from = 0) -> detail::PublicationRead {
    auto scope = detail::PublicationAccess::scope(s);
    REQUIRE(scope);
    auto result = detail::PublicationAccess::readSince(s, *scope, from);
    REQUIRE(result);
    return std::move(*result);
}
} // namespace

TEST_CASE("Publication bridge reads the owning source without copying Fact history",
          "[judgement][publication]") {
    for (const bool fold : {false, true}) {
        PublicationFixture f;
        auto s = f.session(fold);
        REQUIRE(s.advance(Tick{1103}));
        const auto owning = s.query();
        REQUIRE(owning);
        const auto& p = owning->kernelView();
        auto view = read(s);
        auto metadata = view.metadata();
        REQUIRE(metadata);
        const auto& m = metadata->get();
        CHECK(&m.identity == &p.judgementIdentity);
        CHECK(m.runScope == p.runScope);
        CHECK(m.sealedCursor == p.sealedFactCursor);
        CHECK(m.publishableCursor == (fold ? p.fold->factCursor : p.sealedFactCursor));
        CHECK(m.hasFold == fold);
        CHECK(m.mayProject);
        CHECK(m.kernelWorkTick == p.kernelWorkTick);
        CHECK(m.foldWorkTick == (fold ? p.fold->workTick : std::nullopt));
        CHECK(m.processedFrontier == p.processedFrontier);
        REQUIRE(m.publishableCursor > 0);
        for (std::uint64_t ordinal = 0; ordinal < m.publishableCursor; ++ordinal) {
            const auto fact = view.fact(ordinal);
            REQUIRE(fact);
            if (const auto* phase = std::get_if<PhaseOutcomeFact>(&p.facts[ordinal])) {
                CHECK(fact->kind == FactKind::phaseOutcome);
                CHECK(fact->requirement == &phase->requirement);
                CHECK(fact->factId == phase->factId);
                CHECK(fact->phase == phase->phase);
                CHECK(fact->outcome == phase->outcome);
                CHECK(fact->error == phase->error);
            } else {
                const auto& receipt = std::get<ReceiptFact>(p.facts[ordinal]);
                CHECK(fact->kind == receipt.kind);
                CHECK(fact->factId == receipt.factId);
                CHECK_FALSE(fact->requirement);
            }
        }
        CHECK_FALSE(view.fact(m.publishableCursor));
    }
}

TEST_CASE("Publication bridge reads a nonzero exclusive cursor including receipts",
          "[judgement][publication]") {
    PublicationFixture f;
    auto s = f.session();
    REQUIRE(s.advance(Tick{1103}));
    auto p = s.query();
    REQUIRE(p);
    const auto from = p->kernelView().sealedFactCursor;
    REQUIRE(from > 0);
    REQUIRE(s.submit({{ObservationTick{Tick{1200}},
                       {IngressSequence{1},
                        InputAction::press,
                        *ChannelRef::fromToken("lane.one"),
                        "domain.binding.one",
                        {},
                        {false, false, false},
                        ContinuityKind::discrete,
                        {}}}}));
    REQUIRE(s.advance(Tick{1203}));
    auto view = read(s, from);
    auto m = view.metadata();
    REQUIRE(m);
    CHECK_FALSE(view.fact(from - 1));
    REQUIRE(m->get().publishableCursor > from);
    auto fact = view.fact(from);
    REQUIRE(fact);
    CHECK(fact->kind == FactKind::stray);
    CHECK_FALSE(fact->requirement);
}

TEST_CASE("Publication bridge rejects scopes cursors owners and reentry",
          "[judgement][publication]") {
    PublicationFixture f;
    auto s = f.session();
    auto other = f.session();
    auto scope = *detail::PublicationAccess::scope(s);
    CHECK_FALSE(detail::PublicationAccess::readSince(s, {}, 0));
    CHECK_FALSE(
        detail::PublicationAccess::readSince(s, *detail::PublicationAccess::scope(other), 0));
    CHECK_FALSE(detail::PublicationAccess::readSince(s, scope, 1));
    auto view = read(s);
    CHECK_FALSE(detail::PublicationAccess::readSince(s, scope, 0));
    bool wrongScope = false, wrongMetadata = false, wrongFact = false, wrongMutation = false;
    std::thread foreign([&] {
        wrongScope = !detail::PublicationAccess::scope(s);
        wrongMetadata = !view.metadata();
        wrongFact = !view.fact(0);
        wrongMutation = !s.reset();
    });
    foreign.join();
    CHECK(wrongScope);
    CHECK(wrongMetadata);
    CHECK(wrongFact);
    CHECK(wrongMutation);
    CHECK(view.metadata());
    REQUIRE(detail::KernelTestAccess::duringMutation(s, [&] {
        CHECK_FALSE(view.metadata());
        CHECK_FALSE(detail::PublicationAccess::readSince(s, scope, 0));
        CHECK_FALSE(s.reset());
    }));
    CHECK_FALSE(view.metadata());
    CHECK(read(s).metadata());
}

TEST_CASE("Publication bridge invalidates reads on all session mutation paths",
          "[judgement][publication]") {
    PublicationFixture f;
    auto s = f.session();
    auto owning = s.query();
    const auto scope = *detail::PublicationAccess::scope(s);
    auto view = read(s);
    REQUIRE(s.submit({}));
    CHECK_FALSE(view.metadata());
    auto canonical = read(s);
    REQUIRE(s.submitCanonical({}));
    CHECK_FALSE(canonical.metadata());
    auto advanced = read(s);
    REQUIRE(s.advance(Tick{1103}));
    CHECK_FALSE(advanced.fact(0));
    auto controlled = read(s);
    CHECK_FALSE(s.controlRpa({RpaControlKind::pause, Tick{0}, 0, 1, 0, {}}));
    CHECK_FALSE(controlled.metadata());
    auto snapshot = s.snapshot();
    REQUIRE(snapshot);
    auto archive = s.archive();
    REQUIRE(archive);
    auto restored = read(s);
    REQUIRE(s.restore(*snapshot, f.inputs()));
    CHECK_FALSE(restored.metadata());
    CHECK_FALSE(detail::PublicationAccess::readSince(s, scope, 0));
    auto sought = read(s);
    REQUIRE(s.seek(*archive, Tick{1103}));
    CHECK_FALSE(sought.metadata());
    auto reset = read(s);
    REQUIRE(s.reset());
    CHECK_FALSE(reset.metadata());
    CHECK(owning->kernelView().runScope == scope);
    CHECK(owning->kernelView().facts.empty());
    std::optional<detail::PublicationRead> dead;
    {
        auto temporary = f.session();
        dead.emplace(read(temporary));
    }
    CHECK_FALSE(dead->metadata());
    auto replaced = f.session();
    auto replacedView = read(replaced);
    replaced = f.session();
    CHECK_FALSE(replacedView.metadata());
}

TEST_CASE("Publication bridge keeps sealed and Fold fault prefixes distinct",
          "[judgement][publication][fault]") {
    for (const auto point :
         {detail::KernelFailurePoint::beforeSeal, detail::KernelFailurePoint::beforeFold}) {
        PublicationFixture f;
        auto s = f.session();
        REQUIRE(detail::KernelTestAccess::inject(s, {Tick{900}, point, {}, {}, {}}));
        REQUIRE(s.submit({{ObservationTick{Tick{900}},
                           {IngressSequence{1},
                            InputAction::press,
                            *ChannelRef::fromToken("lane.one"),
                            "domain.binding.one",
                            {},
                            {false, false, false},
                            ContinuityKind::discrete,
                            {}}}}));
        CHECK_FALSE(s.advance(Tick{1103}));
        auto owning = s.query();
        REQUIRE(owning);
        auto view = read(s);
        const auto m = view.metadata();
        REQUIRE(m);
        const auto& p = owning->kernelView();
        CHECK_FALSE(m->get().mayProject);
        CHECK(m->get().sealedCursor == p.sealedFactCursor);
        CHECK(m->get().publishableCursor == p.fold->factCursor);
        CHECK(m->get().faultStage == p.faultStage);
        CHECK(m->get().faultDiagnostic == &*p.faultDiagnostic);
        if (point == detail::KernelFailurePoint::beforeFold)
            CHECK(m->get().sealedCursor > m->get().publishableCursor);
    }
}

TEST_CASE("Publication bridge retains empty intervals and successful control cursors",
          "[judgement][publication][control]") {
    PublicationFixture f{true};
    auto s = f.session();
    REQUIRE(s.advance(Tick{1103}));
    auto p = s.query();
    REQUIRE(p);
    const auto from = p->kernelView().fold->factCursor;
    const auto cut = *p->kernelView().processedFrontier;
    auto before = read(s, from);
    CHECK(before.metadata()->get().fromCursor == before.metadata()->get().publishableCursor);
    REQUIRE(s.controlRpa({RpaControlKind::pause, cut, 0, 1, 0, {}}));
    CHECK_FALSE(before.metadata());
    auto paused = read(s, from);
    CHECK(paused.metadata()->get().controlCursor == 1);
    CHECK(paused.metadata()->get().publishableCursor == from);
    REQUIRE(s.controlRpa({RpaControlKind::resume, cut, 0, 1, 0, {}}));
    auto resumed = read(s, from);
    CHECK(resumed.metadata()->get().controlCursor == 2);
    CHECK(resumed.metadata()->get().mayProject);
}

TEST_CASE("Publication bridge refuses projection after Control finalization faults",
          "[judgement][publication][fault]") {
    PublicationFixture f;
    auto s = f.session();
    REQUIRE(s.advance(Tick{1103}));
    const auto before = s.query();
    REQUIRE(before);
    detail::KernelTestControls controls{};
    controls.failAdvanceFinalization = true;
    REQUIRE(detail::KernelTestAccess::inject(s, controls));
    CHECK_FALSE(s.advance(Tick{1203}));
    auto view = read(s);
    auto m = view.metadata();
    REQUIRE(m);
    CHECK(m->get().faultStage == FaultStage::control);
    CHECK_FALSE(m->get().mayProject);
    CHECK(m->get().sealedCursor == before->kernelView().sealedFactCursor);
    CHECK(m->get().publishableCursor == before->kernelView().fold->factCursor);
    CHECK(m->get().foldWorkTick == before->kernelView().fold->workTick);
}
