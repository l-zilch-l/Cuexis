#include "fold_recovery_fixture.hpp"

using namespace fold_fixture;

namespace {
void deadlineFixture(FoldFixture& f, unsigned count, bool rpa) {
    f.tap();
    auto& a = f.assembly;
    if (rpa) {
        a.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
        a.identityDeclarations.engine.executionProfileToken = a.executionProfile;
        a.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
        a.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
    }
    auto& document = a.sources[0].document;
    const auto prototype = document.requirements[0];
    document.requirements.clear();
    std::erase_if(document.declarations,
                  [](const auto& d) { return d.kind == DeclarationKind::requirement; });
    document.relations[0].members.clear();
    a.contentProfileLimits.maxRequirements = MeasuredParameter<std::uint64_t>::measured(count);
    for (unsigned i = 0; i < count; ++i) {
        auto r = prototype;
        r.stableId.declarationOrdinal = 100 + i;
        r.identity.requirementLocalId = "deadline." + std::to_string(i);
        r.resourceClaims[0].claimPolicy.competition = ClaimPolicyDeclaration::CompetitionKey{-1, i};
        r.timing->end = Tick{8 + i};
        document.declarations.push_back(
            testDeclaration(DeclarationKind::requirement, r.identity.requirementLocalId, 100 + i));
        document.relations[0].members.push_back(r.stableId);
        document.requirements.push_back(std::move(r));
    }
}
auto deadlineConfiguration(FoldFixture& f, bool rpa) -> SessionConfiguration {
    auto c = f.configuration();
    if (rpa) {
        c.executionProfileToken = "gameplay.execution.t4-k4.rpa.v1";
        c.lateAlgorithmToken = "late.window.batch.v1";
        c.factSemanticRevision = "fact.semantic.rpa.v1";
    }
    return c;
}
auto deadlineSession(FoldFixture& f, bool rpa) -> JudgementSession {
    auto session = JudgementSession::create();
    REQUIRE(session);
    REQUIRE(session->configure(deadlineConfiguration(f, rpa)));
    REQUIRE(session->prepare(f.prepared()));
    return std::move(*session);
}
} // namespace

TEST_CASE("Deadline-only ticks preserve full state and bounded transaction work",
          "[kernel][recovery][deadline]") {
    for (const bool rpa : {false, true}) {
        DYNAMIC_SECTION("rpa=" << rpa) {
            FoldFixture f;
            deadlineFixture(f, 64, rpa);
            auto optimized = deadlineSession(f, rpa);
            auto reference = deadlineSession(f, rpa);
            auto work = std::make_shared<detail::KernelWorkObservation>();
            detail::KernelTestControls controls{};
            controls.work = work;
            REQUIRE(detail::KernelTestAccess::inject(optimized, controls));
            controls.work.reset();
            controls.forceReferenceAdvance = true;
            REQUIRE(detail::KernelTestAccess::inject(reference, controls));
            auto before = optimized.query();
            REQUIRE(before);
            REQUIRE(optimized.advance(Tick{1000}));
            REQUIRE(reference.advance(Tick{1000}));
            auto a = optimized.snapshot();
            auto b = reference.snapshot();
            REQUIRE(a);
            REQUIRE(b);
            CHECK(sameSnapshot(a->state(), b->state()));
            CHECK(*encodeSnapshot(*a) == *encodeSnapshot(*b));
            CHECK(*encodeReplay(*optimized.archive()) == *encodeReplay(*reference.archive()));
            CHECK(before->kernelView().facts.empty());
            CHECK_FALSE(before->kernelView().processedFrontier);
            CHECK(work->timers == 256);
            CHECK(work->fullTickTransactions <= 4);
            CHECK(work->idleDeadlineTicks >= 60);
        }
    }
}

TEST_CASE("Idle deadline optimization preserves tick fault and Fold boundaries",
          "[kernel][recovery][deadline]") {
    for (const bool rpa : {false, true})
        for (const auto point :
             {detail::KernelFailurePoint::afterTimers, detail::KernelFailurePoint::afterInput,
              detail::KernelFailurePoint::beforeLedger, detail::KernelFailurePoint::beforeSeal,
              detail::KernelFailurePoint::beforeFold}) {
            DYNAMIC_SECTION("rpa=" << rpa << " point=" << static_cast<unsigned>(point)) {
                FoldFixture f;
                deadlineFixture(f, 32, rpa);
                auto optimized = deadlineSession(f, rpa);
                auto reference = deadlineSession(f, rpa);
                detail::KernelTestControls controls{};
                controls.failedTick = Tick{20};
                controls.failurePoint = point;
                REQUIRE(detail::KernelTestAccess::inject(optimized, controls));
                controls.forceReferenceAdvance = true;
                REQUIRE(detail::KernelTestAccess::inject(reference, controls));
                auto a = optimized.advance(Tick{1000});
                auto b = reference.advance(Tick{1000});
                REQUIRE_FALSE(a);
                REQUIRE_FALSE(b);
                CHECK(a.error().code() == b.error().code());
                auto x = detail::KernelTestAccess::captureDTO(optimized);
                auto y = detail::KernelTestAccess::captureDTO(reference);
                REQUIRE(x);
                REQUIRE(y);
                CHECK(sameSnapshot(*x, *y));
            }
        }
}

TEST_CASE("Idle deadlines preserve split advances hooks inputs and recovery bytes",
          "[kernel][recovery][deadline]") {
    for (const bool rpa : {false, true})
        for (const bool hooks : {false, true}) {
            DYNAMIC_SECTION("rpa=" << rpa << " hooks=" << hooks) {
                FoldFixture f{hooks};
                deadlineFixture(f, 32, rpa);
                auto optimized = deadlineSession(f, rpa);
                auto reference = deadlineSession(f, rpa);
                detail::KernelTestControls controls{};
                controls.forceReferenceAdvance = true;
                REQUIRE(detail::KernelTestAccess::inject(reference, controls));
                REQUIRE(optimized.advance(Tick{3}));
                REQUIRE(reference.advance(Tick{3}));
                auto retained = optimized.query();
                REQUIRE(retained);
                if (!rpa) {
                    REQUIRE(optimized.submit({raw(5, 1)}));
                    REQUIRE(reference.submit({raw(5, 1)}));
                }
                REQUIRE(optimized.advance(Tick{1000}));
                REQUIRE(reference.advance(Tick{1000}));
                CHECK(retained->kernelView().facts.empty());
                CHECK(retained->kernelView().processedFrontier == Tick{0});
                auto a = optimized.snapshot();
                auto b = reference.snapshot();
                REQUIRE(a);
                REQUIRE(b);
                CHECK(sameSnapshot(a->state(), b->state()));
                auto encoded = encodeSnapshot(*a);
                REQUIRE(encoded);
                CHECK(*encoded == *encodeSnapshot(*b));
                const RecoveryInputs dependencies{deadlineConfiguration(f, rpa), f.prepared()};
                auto decoded = decodeSnapshot(*encoded, dependencies);
                REQUIRE(decoded);
                auto recovered = JudgementSession::recover(*decoded, dependencies);
                REQUIRE(recovered);
                CHECK(sameSnapshot(a->state(), recovered->snapshot()->state()));
                auto archive = optimized.archive();
                REQUIRE(archive);
                auto evaluated = JudgementSession::evaluateReplay(*archive);
                REQUIRE(evaluated);
                CHECK(sameKernelResult(a->state().kernel, *evaluated->result));
                REQUIRE(optimized.seek(*archive, Tick{1000}));
                CHECK(sameSnapshot(a->state(), optimized.snapshot()->state()));
            }
        }
}

TEST_CASE("Idle deadlines retain finalization duplicate phase register and signal behavior",
          "[kernel][recovery][deadline]") {
    for (const bool rpa : {false, true})
        for (unsigned scenario = 0; scenario < 4; ++scenario) {
            DYNAMIC_SECTION("rpa=" << rpa << " scenario=" << scenario) {
                FoldFixture f;
                deadlineFixture(f, 32, rpa);
                auto optimized = deadlineSession(f, rpa);
                auto reference = deadlineSession(f, rpa);
                detail::KernelTestControls controls{};
                if (scenario == 0)
                    controls.failAdvanceFinalization = true;
                if (scenario == 1)
                    controls.duplicatePhaseTick = Tick{20};
                if (scenario == 2) {
                    controls.invalidRegisterTick = Tick{20};
                    controls.registerFault = detail::RegisterFault::secondExclusive;
                }
                if (scenario == 3)
                    controls.signalTicks = {Tick{20}};
                REQUIRE(detail::KernelTestAccess::inject(optimized, controls));
                controls.forceReferenceAdvance = true;
                REQUIRE(detail::KernelTestAccess::inject(reference, controls));
                const auto a = optimized.advance(Tick{1000});
                const auto b = reference.advance(Tick{1000});
                CHECK(a.has_value() == b.has_value());
                CHECK(a.has_value() == (scenario == 3));
                const auto x = detail::KernelTestAccess::captureDTO(optimized);
                const auto y = detail::KernelTestAccess::captureDTO(reference);
                REQUIRE(x);
                REQUIRE(y);
                CHECK(sameSnapshot(*x, *y));
                CHECK(detail::KernelTestAccess::visibleSignalCount(optimized, Tick{21}) ==
                      detail::KernelTestAccess::visibleSignalCount(reference, Tick{21}));
            }
        }
}
