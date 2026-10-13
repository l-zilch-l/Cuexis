#include "../../engine/judgement/src/execution_kernel.hpp"
#include "../../engine/judgement/src/execution_testing.hpp"
#include "../../engine/judgement/src/recovery_wire.hpp"
#include "../judgement/execution_reference.hpp"
#include "../judgement/execution_test_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cuexis/judgement/execution_scheduler.hpp>
#include <cuexis/judgement/recovery_codec.hpp>
#include <cuexis/playback/realtime_clock.hpp>
#include <fstream>
#include <iostream>
#include <thread>

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
namespace {
struct Fixture3 {
    Fixture base;
    AssemblyRequest assembly;
    std::optional<RulesetDeclaration> rules;
    Fixture3() : assembly(base.request()) {
        base.latePolicy = executionLate();
        executionFields(assembly);
        assembly.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
        assembly.identityDeclarations.engine.executionProfileToken = assembly.executionProfile;
        assembly.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
        assembly.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
    }
    auto prepared() -> PreparedGameplay {
        auto p = prepareResolvedGameplay({assembly, {}});
        INFO((p ? "" : std::string{p.error().message()}));
        REQUIRE(p);
        return *p;
    }
    auto configuration() -> SessionConfiguration {
        InputMappingProfile mapping{
            "mapping",
            "v1",
            *SourceClass::fromToken("keyboard"),
            {{"domain.binding.one",
              {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
        auto result = SessionConfiguration{OwnedInputMappingProfile{mapping},
                                           OwnedTimebaseProfile{base.timebase},
                                           base.latePolicy,
                                           assembly.identityDeclarations,
                                           "calibration.fixture",
                                           assembly.executionProfile,
                                           "late.window.batch.v1",
                                           "fact.semantic.rpa.v1"};
        if (rules) {
            auto preparedRules = prepareRuleset(*rules);
            REQUIRE(preparedRules);
            result.ruleset = *preparedRules;
        }
        return result;
    }
    auto session() -> JudgementSession {
        auto s = JudgementSession::create();
        REQUIRE(s);
        REQUIRE(s->configure(configuration()));
        REQUIRE(s->prepare(prepared()));
        return std::move(*s);
    }
};
auto occurrence(std::int64_t t, std::uint64_t p, InputAction action,
                std::string channel = "lane.one") -> CanonicalInput {
    return {{ObservationTick{Tick{t}},
             "domain.binding.one",
             "keyboard",
             std::move(channel),
             action,
             {},
             p},
            IngressSequence{p}};
}
auto view(JudgementSession& s) -> KernelProjection {
    auto p = s.query();
    REQUIRE(p);
    return p->kernelView();
}
auto phase(const KernelProjection& p, PhaseKind kind) -> KernelPhaseState {
    auto it = std::find_if(p.phases.begin(), p.phases.end(),
                           [&](const auto& v) { return v.phase == kind; });
    REQUIRE(it != p.phases.end());
    return it->state;
}
constexpr RpaContext scope{1, 1, 1};
auto scheduler(JudgementSession& s, RpaContext context, std::size_t capacity)
    -> ExecutionScheduler {
    auto result = ExecutionScheduler::create(s, context, TickSpan{3}, capacity);
    REQUIRE(result);
    return std::move(*result);
}
auto batch(std::uint64_t seq, std::int64_t tick, std::vector<CanonicalInput> inputs)
    -> CaptureMessage {
    const auto size = static_cast<std::uint64_t>(inputs.size());
    return {scope, seq,
            ClosedCaptureBatch{Tick{tick}, Tick{tick + 1}, seq, 0, 1, size, std::move(inputs)}};
}
} // namespace
TEST_CASE("B3 T2 same Tick press release press preserves occurrence and contact identity",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    const auto first = occurrence(900, 0, InputAction::press);
    REQUIRE(s.submitCanonical(
        {first, occurrence(900, 1, InputAction::release), occurrence(900, 2, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    const auto p = view(s);
    REQUIRE(p.receipts.size() == 3);
    CHECK(p.receipts[0].observationKey == first.key);
    CHECK(p.receipts[1].observationKey.sourceLocalPosition == 1);
    CHECK(p.receipts[2].observationKey.sourceLocalPosition == 2);
    CHECK(phase(p, PhaseKind::head) == KernelPhaseState::Hit);
    CHECK(phase(p, PhaseKind::body) == KernelPhaseState::Miss);
    REQUIRE(p.contacts.size() == 1);
    CHECK(p.contacts[0].startPressKey.sourceLocalPosition == 2);
    auto a = s.archive();
    REQUIRE(a);
    auto e = JudgementSession::evaluateReplay(*a);
    REQUIRE(e);
    REQUIRE(e->result);
    CHECK(sameKernelResult(p, *e->result));
}
TEST_CASE("B3 occurrence duplicate and slot conflict are atomic and distinguish content",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    const auto input = occurrence(900, 0, InputAction::press);
    REQUIRE(s.submitCanonical({input}));
    const auto old = view(s);
    auto duplicate = s.submitCanonical({input});
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().code() == "judgement.late.duplicate_queue_entry");
    auto conflict = s.submitCanonical({occurrence(900, 0, InputAction::release)});
    REQUIRE_FALSE(conflict);
    CHECK(conflict.error().code() == "judgement.input.occurrence_conflict");
    CHECK(sameKernelResult(old, view(s)));
}
TEST_CASE("B3 G1 waits at equal B and retains original horizon across reordered delivery",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    auto r = d.request({scope, 7, 0, Tick{903}});
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::waiting);
    REQUIRE(d.deliver({scope, 1, CapturePrefix{Tick{900}}}));
    CHECK_FALSE(d.confirmedSequence());
    REQUIRE(d.deliver(batch(0, 899, {occurrence(899, 0, InputAction::press)})));
    r = d.poll(7);
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::waiting);
    REQUIRE(d.deliver({scope, 2, CapturePrefix{Tick{901}}}));
    r = d.poll(7);
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::executed);
    REQUIRE(r->result);
    CHECK(r->result->lastAdvanceHorizon == Tick{903});
    const auto before = view(s);
    REQUIRE(d.deliver(batch(0, 899, {occurrence(899, 0, InputAction::press)})));
    CHECK(sameKernelResult(before, view(s)));
    REQUIRE(d.retire(7));
    r = d.request({scope, 7, 0, Tick{903}});
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::retired);
}
TEST_CASE("B3 Q1 K1 invalid control does not cancel and valid control cannot revive advance",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE(d.request({scope, 1, 0, Tick{903}}));
    auto busy = d.request({scope, 2, 1, Tick{904}});
    REQUIRE(busy);
    CHECK(busy->disposition == AdvanceDisposition::busyNotAccepted);
    REQUIRE_FALSE(d.acceptControl({2, 1, 1}, 1));
    auto r = d.poll(1);
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::waiting);
    REQUIRE(d.acceptControl(scope, 1));
    REQUIRE(d.deliver({scope, 0, CapturePrefix{Tick{1000}}}));
    r = d.poll(1);
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::canceledNotExecuted);
    CHECK_FALSE(view(s).lastAdvanceHorizon);
}
TEST_CASE("B3 R2 release preserves head and misses body tail without observation error",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
    const auto paused = view(s);
    const RpaControlRecord resume{RpaControlKind::resume,
                                  Tick{900},
                                  0,
                                  1,
                                  1,
                                  {{0, occurrence(901, 0, InputAction::release).key}}};
    REQUIRE(s.controlRpa(resume));
    const auto p = view(s);
    CHECK(phase(p, PhaseKind::head) == KernelPhaseState::Hit);
    CHECK(phase(p, PhaseKind::body) == KernelPhaseState::Miss);
    CHECK(phase(p, PhaseKind::tail) == KernelPhaseState::Miss);
    CHECK(p.contacts.empty());
    CHECK(p.processedFrontier == paused.processedFrontier);
    CHECK(p.lastAdvanceHorizon == paused.lastAdvanceHorizon);
    for (const auto& fact : p.facts)
        if (const auto* phaseFact = std::get_if<PhaseOutcomeFact>(&fact);
            phaseFact && phaseFact->outcome == Outcome::miss) {
            CHECK_FALSE(phaseFact->error);
            CHECK_FALSE(phaseFact->evidence);
            CHECK_FALSE(phaseFact->grade);
            CHECK(std::holds_alternative<ReconciliationOrigin>(phaseFact->originId));
        }
    REQUIRE(s.controlRpa(resume));
    CHECK(sameKernelResult(p, view(s)));
    auto a = s.archive();
    REQUIRE(a);
    auto e = JudgementSession::evaluateReplay(*a);
    REQUIRE(e);
    CHECK(sameKernelResult(p, *e->result));
    auto snapshot = s.snapshot();
    REQUIRE(snapshot);
    auto bytes = encodeSnapshot(*snapshot);
    REQUIRE(bytes);
    CHECK(std::to_integer<unsigned>((*bytes)[8]) == 3);
    auto decoded = decodeSnapshot(*bytes, {f.configuration(), f.prepared()});
    INFO((decoded ? "" : std::string{decoded.error().message()}));
    REQUIRE(decoded);
    CHECK(sameSnapshot(snapshot->state(), decoded->state()));
    auto restored = JudgementSession::recover(*decoded, {f.configuration(), f.prepared()});
    REQUIRE(restored);
    CHECK(sameKernelResult(p, view(*restored)));
}
TEST_CASE("B3 rev3 replay wire rejects rev1 rev2 and retains all fields", "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    auto a = s.archive();
    REQUIRE(a);
    auto bytes = encodeReplay(*a);
    REQUIRE(bytes);
    auto decoded = decodeReplay(*bytes, {f.configuration(), f.prepared()});
    REQUIRE(decoded);
    CHECK(sameReplayHistory(*a, *decoded));
    for (const auto old : {1, 2}) {
        auto changed = *bytes;
        changed[8] = static_cast<std::byte>(old);
        auto result = decodeReplay(changed, {f.configuration(), f.prepared()});
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "codec.version_unsupported");
    }
    for (std::size_t cut = 0; cut < bytes->size(); ++cut)
        CHECK_FALSE(decodeReplay(std::span{*bytes}.first(cut), {f.configuration(), f.prepared()}));
}
TEST_CASE("B3 A1 final route failure terminates continuation and waiting trace is distinct",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE(d.acceptControl(scope, 0));
    REQUIRE(d.deliver({scope, 0, EndRunEpoch{Tick{900}, 1}}));
    REQUIRE(d.pause());
    const RpaControlRecord resume{RpaControlKind::resume, Tick{900}, 0, 1, 1, {}};
    auto pending = d.resume(resume, {1, 2, 2}, true);
    REQUIRE_FALSE(pending);
    CHECK(pending.error().code() == "host.rpa.control_pending");
    const auto before = view(s);
    auto failed = d.resume({RpaControlKind::resume, Tick{900}, 0, 1, 0, {}}, {1, 2, 2}, true, true);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code() == "host.rpa.resume_aborted");
    CHECK(sameKernelResult(before, view(s)));
    auto again = d.resume({RpaControlKind::resume, Tick{900}, 1, 1, 0, {}}, {1, 2, 3}, true);
    REQUIRE_FALSE(again);
    CHECK(again.error().code() == "host.rpa.resume_aborted");
    CHECK(d.suspended());
}

TEST_CASE("B3 T2 complete kernel matches independent source scanning oracle", "[rpa][b3][oracle]") {
    Fixture3 f;
    auto s = f.session();
    std::vector<CanonicalInput> inputs{
        occurrence(900, 0, InputAction::press), occurrence(900, 1, InputAction::release),
        occurrence(900, 2, InputAction::press), occurrence(900, 0, InputAction::press, "lane.two")};
    std::sort(inputs.begin(), inputs.end(),
              [](const auto& a, const auto& b) { return a.key < b.key; });
    std::vector<AdmittedObservation> expectedInputs;
    for (const auto& input : inputs)
        expectedInputs.push_back({input.key, Tick{900}, false, {}, {}});
    auto expected = scanReference(f.prepared(), f.configuration(), expectedInputs, Tick{903});
    expected.kernelWorkTick = Tick{900};
    expected.sealedFactCursor = static_cast<std::uint64_t>(expected.facts.size());
    expected.effectiveFactSemanticRevision = "fact.semantic.rpa.v1";
    expected.rpa.emplace();
    for (const auto& input : inputs)
        expected.rpa->accepted.push_back(input.key);
    expected.rpa->foldBaseTick = Tick{900};
    REQUIRE(s.submitCanonical(inputs));
    REQUIRE(s.advance(Tick{903}));
    CHECK(sameKernelResult(expected, view(s)));
}
TEST_CASE("B3 control termination closes the last batch without fictitious later observation",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE(d.acceptControl(scope, 0));
    auto last = batch(0, 900, {occurrence(900, 0, InputAction::press)});
    auto& closed = std::get<ClosedCaptureBatch>(last.payload);
    closed.observedTick = Tick{900};
    closed.epochTerminated = true;
    closed.pauseEpoch = 1;
    REQUIRE(d.deliver(last));
    CHECK_FALSE(d.verifiedExclusive());
    REQUIRE(d.deliver({scope, 1, EndRunEpoch{Tick{900}, 1}}));
    REQUIRE(d.pause());
    const auto p = view(s);
    CHECK(p.rpa->paused);
    CHECK(phase(p, PhaseKind::head) == KernelPhaseState::Hit);
    CHECK(p.lastAdvanceHorizon == Tick{903});
}
TEST_CASE("B3 fragment gaps cannot authorize a prefix and fragments admit a single batch",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE(d.request({scope, 1, 0, Tick{903}}));
    auto first = batch(0, 900, {occurrence(900, 0, InputAction::press)});
    auto second = batch(1, 900, {occurrence(900, 1, InputAction::release)});
    auto& a = std::get<ClosedCaptureBatch>(first.payload);
    auto& b = std::get<ClosedCaptureBatch>(second.payload);
    a.fragmentCount = b.fragmentCount = 2;
    a.totalMembers = b.totalMembers = 2;
    b.batchId = a.batchId;
    b.fragmentIndex = 1;
    REQUIRE(d.deliver(second));
    CHECK(d.admissions().empty());
    REQUIRE(d.deliver(first));
    REQUIRE(d.admissions().size() == 1);
    REQUIRE(d.deliver({scope, 2, CapturePrefix{Tick{901}}}));
    auto r = d.poll(1);
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::executed);
    REQUIRE(r->result);
    CHECK(r->result->receipts.size() == 2);
}
TEST_CASE("B3 same H with different control cuts yields different reachable snapshots",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    const auto beforeCut = s.archive();
    REQUIRE(beforeCut);
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{900},
                          0,
                          1,
                          1,
                          {{0, occurrence(901, 0, InputAction::release).key}}}));
    const auto archive = s.archive();
    REQUIRE(archive);
    const auto after = view(s);
    auto a = f.session();
    REQUIRE(a.seek(*archive, Tick{903}, ReplayCut{2, {}}, {}));
    const auto earlier = view(a);
    auto b = f.session();
    REQUIRE(b.seek(*archive, Tick{903}, ReplayCut{4, {}}, {}));
    CHECK(sameKernelResult(after, view(b)));
    CHECK_FALSE(sameKernelResult(earlier, after));
    CHECK(earlier.lastAdvanceHorizon == after.lastAdvanceHorizon);
    auto checkpoint = JudgementSession::checkpoint(*archive, Tick{903});
    REQUIRE(checkpoint);
    CHECK(checkpoint->archive.data().identity == archive->data().identity);
}
TEST_CASE("B3 future press release association interrupts only its actual paused contact",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.advance(Tick{902}));
    REQUIRE(s.submitCanonical({occurrence(898, 0, InputAction::press)}));
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{899}, 0, 1, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{899},
                          0,
                          1,
                          2,
                          {{0, occurrence(901, 0, InputAction::release).key},
                           {1, occurrence(902, 0, InputAction::press).key}}}));
    REQUIRE(s.advance(Tick{904}));
    const auto p = view(s);
    CHECK(phase(p, PhaseKind::head) == KernelPhaseState::Hit);
    CHECK(phase(p, PhaseKind::body) == KernelPhaseState::Miss);
    CHECK(p.contacts.empty());
    CHECK(p.rpa->deferred.empty());
    REQUIRE(p.rpa->quarantine.size() == 1);
    auto archive = s.archive();
    REQUIRE(archive);
    auto e = JudgementSession::evaluateReplay(*archive);
    REQUIRE(e);
    CHECK(sameKernelResult(p, *e->result));
}
TEST_CASE("B3 occurrence golden wire includes signed Tick and maximum source position",
          "[rpa][b3][wire]") {
    CanonicalObservationKey key{ObservationTick{Tick{-1}}, "d", "s",       "c",
                                InputAction::press,        {},  UINT64_MAX};
    cuexis::judgement::detail::wire::Writer writer;
    writer.revision = 3;
    writer(key);
    constexpr std::string_view golden = "ffffffffffffffff010000000000000064010000000000000073010000"
                                        "000000000063000001ffffffffffffffff";
    std::string hex;
    constexpr char digits[] = "0123456789abcdef";
    for (const auto value : writer.bytes) {
        const auto b = std::to_integer<unsigned>(value);
        hex.push_back(digits[b >> 4]);
        hex.push_back(digits[b & 15]);
    }
    CHECK(hex == golden);
}

namespace {
auto controlRules(bool hooks = false) -> RulesetDeclaration {
    RulesetDeclaration d{"cuexis.ruleset.finite",
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
    for (auto kind : {PhaseKind::head, PhaseKind::body, PhaseKind::tail}) {
        d.score.rules.push_back({kind, Outcome::hit, {}, 2, true});
        d.score.rules.push_back({kind, Outcome::miss, {}, -1, false});
    }
    if (hooks) {
        d.modules.push_back({"hook", "1", "cuexis.finite-fold.1"});
        d.hooks.push_back(
            {"fold.bonus.u64", HookConsumer::fold, CombineOperator::maximum, {"hook"}, 3, false});
    }
    return d;
}
} // namespace
TEST_CASE("B3 same q control suffix recomputes Fold and emits one hook at q plus one",
          "[rpa][b3][fold]") {
    Fixture3 f;
    f.rules = controlRules(true);
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{900},
                          0,
                          1,
                          1,
                          {{0, occurrence(901, 0, InputAction::release).key}}}));
    auto p = view(s);
    REQUIRE(p.fold);
    CHECK(p.fold->score == 0);
    CHECK(p.fold->factCursor == p.sealedFactCursor);
    REQUIRE(p.fold->produced.size() == 1);
    CHECK(p.fold->produced[0].visibleTick == Tick{901});
    auto a = s.archive();
    REQUIRE(a);
    auto e = JudgementSession::evaluateReplay(*a);
    REQUIRE(e);
    CHECK(sameKernelResult(p, *e->result));
}
TEST_CASE("B3 resume allocation rejection preserves paused kernel and Fold failure preserves seal",
          "[rpa][b3][fault]") {
    for (const auto sealed : {false, true}) {
        Fixture3 f;
        f.rules = controlRules();
        auto s = f.session();
        auto d = scheduler(s, scope, 32);
        REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
        REQUIRE(d.acceptControl(scope, 0));
        REQUIRE(d.deliver({scope, 0, EndRunEpoch{Tick{900}, 1}}));
        REQUIRE(d.pause());
        const auto before = view(s);
        detail::KernelTestControls controls{};
        controls.failedTick = Tick{900};
        controls.failurePoint = sealed ? detail::KernelFailurePoint::beforeFold
                                       : detail::KernelFailurePoint::beforeSeal;
        REQUIRE(detail::KernelTestAccess::inject(s, controls));
        auto failed = d.resume({RpaControlKind::resume,
                                Tick{900},
                                0,
                                1,
                                1,
                                {{0, occurrence(901, 0, InputAction::release).key}}},
                               {1, 2, 2}, true);
        REQUIRE_FALSE(failed);
        const auto p = view(s);
        if (!sealed) {
            CHECK(sameKernelResult(before, p));
            CHECK(d.suspended());
        } else {
            CHECK(p.state == KernelSessionState::Faulted);
            CHECK(p.faultStage == FaultStage::fold);
            CHECK(p.sealedFactCursor == 3);
            REQUIRE(p.fold);
            CHECK(p.fold->factCursor == 1);
            CHECK_FALSE(s.snapshot());
            auto dto = detail::KernelTestAccess::captureDTO(s);
            REQUIRE(dto);
            auto snapshot =
                detail::KernelTestAccess::snapshotValue(*dto, {f.configuration(), f.prepared()});
            auto restored = JudgementSession::recover(snapshot, {f.configuration(), f.prepared()});
            REQUIRE(restored);
            CHECK(sameKernelResult(p, view(*restored)));
            auto archive = s.archive();
            REQUIRE(archive);
            auto evaluation = detail::ExecutionKernel::evaluate(*archive, &controls);
            REQUIRE(evaluation);
            CHECK(sameKernelResult(p, *evaluation->result));
        }
    }
}
TEST_CASE("B3 clock invalid cancels wait and epoch Tick overflow cannot fabricate drain",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE_FALSE(d.request({scope, 0, 0, Tick{INT64_MIN}}));
    CHECK_FALSE(d.poll(0));
    REQUIRE(d.request({scope, 1, 0, Tick{903}}));
    REQUIRE(d.clockValid(false));
    REQUIRE(d.deliver({scope, 0, CapturePrefix{Tick{1000}}}));
    auto r = d.poll(1);
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::clockInvalidNotExecuted);
    CHECK_FALSE(view(s).lastAdvanceHorizon);
    auto other = f.session();
    auto overflow = scheduler(other, scope, 32);
    REQUIRE(overflow.acceptControl(scope, 0));
    REQUIRE(overflow.deliver({scope, 0, EndRunEpoch{Tick{INT64_MAX}, 1}}));
    REQUIRE_FALSE(overflow.pause());
    CHECK_FALSE(view(other).lastAdvanceHorizon);
}
TEST_CASE("B3 real monotonic clock measures H and frontier lag separately",
          "[rpa][b3][measurement]") {
    using namespace cuexis::playback;
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 128);
    auto bridge = RealtimeClock::create({1000000, 100, 128, "", true},
                                        {{0, 1}, {0, 1}, {0, 1}, {1, 1}}, "steady.fixture", 1);
    REQUIRE(bridge);
    const auto origin = std::chrono::steady_clock::now();
    auto now = [&] {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - origin)
            .count();
    };
    ClockObservation observation{"steady.fixture",
                                 1,
                                 1,
                                 0,
                                 1000000,
                                 0,
                                 ClockFramePosition{0, 1000000},
                                 ClockFramePosition{0, 1000000},
                                 {},
                                 ClockPlaybackState::Playing,
                                 ClockQuality::Exact,
                                 false,
                                 false,
                                 false};
    std::uint64_t publication = 0;
    auto sampleClock = [&]() {
        const auto stamp = now();
        observation.sampleMonotonicUs = stamp;
        observation.validUntilMonotonicUs = stamp + 1000000;
        observation.source = ClockFramePosition{stamp, 1000000};
        observation.presented = observation.source;
        observation.publicationSequence = publication++;
        return bridge->sample(observation, stamp);
    };
    std::vector<std::int64_t> horizonLag, frontierLag;
    for (std::uint64_t i = 0; i < 32; ++i) {
        const auto sampled = sampleClock();
        REQUIRE(sampled);
        REQUIRE(d.deliver({scope, i, CapturePrefix{Tick{sampled->horizon + 1}}}));
        auto result = d.request({scope, i, i, Tick{sampled->horizon}});
        REQUIRE(result);
        REQUIRE(result->disposition == AdvanceDisposition::executed);
        const auto measured = sampleClock();
        REQUIRE(measured);
        const auto p = view(s);
        REQUIRE(p.lastAdvanceHorizon);
        REQUIRE(p.processedFrontier);
        horizonLag.push_back(measured->horizon - p.lastAdvanceHorizon->value());
        frontierLag.push_back(measured->horizon - 3 - p.processedFrontier->value());
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    auto statistics = [](auto values) {
        std::sort(values.begin(), values.end());
        return std::vector<std::int64_t>{values[15], values[30], values[31], values.back()};
    };
    const auto h = statistics(horizonLag), frontier = statistics(frontierLag);
    const auto json = std::string{"{\"testOnly\":true,\"samples\":32,\"unit\":\"us\",\"clock\":"
                                  "\"std::chrono::steady_clock\",\"H_lag_p50_p95_p99_max\":["} +
                      std::to_string(h[0]) + "," + std::to_string(h[1]) + "," +
                      std::to_string(h[2]) + "," + std::to_string(h[3]) +
                      "],\"frontier_lag_p50_p95_p99_max\":[" + std::to_string(frontier[0]) + "," +
                      std::to_string(frontier[1]) + "," + std::to_string(frontier[2]) + "," +
                      std::to_string(frontier[3]) + "]}";
    std::cout << json << '\n';
    if (const auto* destination = std::getenv("CUEXIS_B3_MEASUREMENT_FILE")) {
        std::ofstream output{destination};
        REQUIRE(output);
        output << json << '\n';
        REQUIRE(output);
    }
}

TEST_CASE(
    "B3 incomplete staged fragment remains unacknowledged and detects conflicting retransmission",
    "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    auto first = batch(0, 900, {occurrence(900, 0, InputAction::press)});
    auto& fragment = std::get<ClosedCaptureBatch>(first.payload);
    fragment.fragmentCount = 2;
    fragment.totalMembers = 2;
    REQUIRE(d.deliver(first));
    CHECK_FALSE(d.confirmedSequence());
    REQUIRE(d.deliver(first));
    fragment.members[0].key.action = InputAction::release;
    auto conflict = d.deliver(first);
    REQUIRE_FALSE(conflict);
    CHECK(conflict.error().code() == "host.rpa.message_conflict");
    CHECK(d.suspended());
}
TEST_CASE("B3 repeated Pause Resume includes prior actual trace releases", "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{900},
                          0,
                          1,
                          1,
                          {{0, occurrence(901, 0, InputAction::release).key}}}));
    REQUIRE(s.advance(Tick{905}));
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{902}, 1, 2, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{902},
                          1,
                          2,
                          1,
                          {{0, occurrence(903, 0, InputAction::press).key}}}));
    CHECK(view(s).rpa->quarantine.size() == 1);
    auto archive = s.archive();
    REQUIRE(archive);
    auto evaluated = JudgementSession::evaluateReplay(*archive);
    REQUIRE(evaluated);
    CHECK(sameKernelResult(view(s), *evaluated->result));
}

TEST_CASE("B3 different original Ticks forwarded to one dispatch retain canonical occurrence order",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.advance(Tick{902}));
    REQUIRE(s.submitCanonical({occurrence(898, 0, InputAction::press)}));
    REQUIRE(s.submitCanonical({occurrence(897, 0, InputAction::release)}));
    REQUIRE(s.advance(Tick{903}));
    auto p = view(s);
    REQUIRE(p.receipts.size() == 2);
    CHECK(p.receipts[0].observationKey.observationTick.tick() == Tick{897});
    CHECK(p.receipts[1].observationKey.observationTick.tick() == Tick{898});
    CHECK(p.receipts[0].dispatchTick == p.receipts[1].dispatchTick);
    CHECK(phase(p, PhaseKind::head) == KernelPhaseState::Hit);
    auto archive = s.archive();
    REQUIRE(archive);
    auto e = JudgementSession::evaluateReplay(*archive);
    REQUIRE(e);
    CHECK(sameKernelResult(p, *e->result));
}
TEST_CASE("B3 R2 without declared tail never creates a tail phase", "[rpa][b3]") {
    Fixture3 f;
    auto& requirement = f.assembly.sources[0].document.requirements[0];
    requirement.phases.pop_back();
    requirement.measure.components.pop_back();
    requirement.requiresReleaseTailSemantics = false;
    requirement.atomBindings.pop_back();
    auto& timing = *requirement.timing;
    timing.successWindows.erase(
        std::remove_if(timing.successWindows.begin(), timing.successWindows.end(),
                       [](const auto& w) { return w.phase.kind == PhaseKind::tail; }),
        timing.successWindows.end());
    timing.phaseTargets.erase(
        std::remove_if(timing.phaseTargets.begin(), timing.phaseTargets.end(),
                       [](const auto& a) { return a.phase == PhaseKind::tail; }),
        timing.phaseTargets.end());
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{900},
                          0,
                          1,
                          1,
                          {{0, occurrence(901, 0, InputAction::release).key}}}));
    auto p = view(s);
    CHECK(phase(p, PhaseKind::body) == KernelPhaseState::Miss);
    CHECK(std::none_of(p.phases.begin(), p.phases.end(),
                       [](const auto& v) { return v.phase == PhaseKind::tail; }));
}

TEST_CASE("B3 pause release at body end and inside tail window preserves sealed outcomes",
          "[rpa][b3]") {
    for (const auto q : {999, 1000, 1020}) {
        Fixture3 f;
        auto s = f.session();
        REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
        REQUIRE(s.advance(Tick{q + 3}));
        const auto before = view(s);
        REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{q}, 0, 1, 0, {}}));
        REQUIRE(s.controlRpa({RpaControlKind::resume,
                              Tick{q},
                              0,
                              1,
                              1,
                              {{0, occurrence(q + 1, 0, InputAction::release).key}}}));
        const auto after = view(s);
        CHECK(phase(after, PhaseKind::head) == KernelPhaseState::Hit);
        CHECK(phase(after, PhaseKind::body) ==
              (q >= 1000 ? KernelPhaseState::Hit : KernelPhaseState::Miss));
        CHECK(phase(after, PhaseKind::tail) == KernelPhaseState::Miss);
        if (q >= 1020)
            CHECK(after.facts == before.facts);
        auto archive = s.archive();
        REQUIRE(archive);
        auto e = JudgementSession::evaluateReplay(*archive);
        REQUIRE(e);
        CHECK(sameKernelResult(after, *e->result));
    }
}

TEST_CASE("B3 control and ControlDrain record golden fix rev3 field order", "[rpa][b3][wire]") {
    const RpaControlRecord control{
        RpaControlKind::pause, Tick{-1}, UINT64_MAX - 1, UINT64_MAX, 0, {}};
    detail::wire::Writer writer;
    writer.revision = 3;
    writer(control);
    auto hex = [](const auto& bytes) {
        std::string result;
        constexpr char digits[] = "0123456789abcdef";
        for (const auto value : bytes) {
            const auto b = std::to_integer<unsigned>(value);
            result.push_back(digits[b >> 4]);
            result.push_back(digits[b & 15]);
        }
        return result;
    };
    constexpr std::string_view golden = "00"
                                        "ffffffffffffffff"
                                        "feffffffffffffff"
                                        "ffffffffffffffff"
                                        "0000000000000000"
                                        "0000000000000000";
    CHECK(hex(writer.bytes) == golden);
    detail::wire::Writer record;
    record.revision = 3;
    record(ReplayRecord{{}, {}, {}, {}, control, {}, false});
    const auto expected =
        std::string{"00000000000000000000000000000000000001"} + std::string{golden} + "0000";
    CHECK(hex(record.bytes) == expected);
    detail::wire::Writer drain;
    drain.revision = 3;
    drain(ReplayRecord{{}, {}, Tick{-1}, {}, {}, {}, true});
    CHECK(hex(drain.bytes) == "0000000000000000000000000000000001ffffffffffffffff00000001");
}

TEST_CASE("B3 EOF clock drains remaining actual timer obligations through verified prefixes",
          "[rpa][b3][clock]") {
    using namespace cuexis::playback;
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 64);
    auto bridge = RealtimeClock::create({1000, 100, 64, "", true}, {{0, 1}, {0, 1}, {0, 1}, {1, 1}},
                                        "tail.fixture", 1);
    REQUIRE(bridge);
    ClockObservation eof{"tail.fixture",
                         1,
                         1,
                         0,
                         1000,
                         0,
                         ClockFramePosition{500, 1000000},
                         ClockFramePosition{500, 1000000},
                         {},
                         ClockPlaybackState::EndOfStream,
                         ClockQuality::Exact,
                         false,
                         false,
                         false};
    REQUIRE(bridge->startTail(eof, 0, 2, {{1, 1}, 1020, 100, "", true}, 1040, 3));
    const auto initial = view(s);
    CHECK(initial.facts.empty());
    const auto mid = bridge->sampleTail(100);
    REQUIRE(mid);
    REQUIRE(d.deliver({scope, 0, CapturePrefix{Tick{mid->observationTick}}}));
    REQUIRE(d.request({scope, 1, 0, Tick{mid->horizon}}));
    CHECK(view(s).facts.empty());
    const auto final = bridge->sampleTail(600);
    REQUIRE(final);
    CHECK(final->horizon == 1043);
    REQUIRE(d.deliver({scope, 1, CapturePrefix{Tick{final->observationTick}}}));
    REQUIRE(d.request({scope, 2, 1, Tick{final->horizon}}));
    const auto result = view(s);
    CHECK(result.lastAdvanceHorizon == Tick{1043});
    CHECK(phase(result, PhaseKind::head) == KernelPhaseState::Miss);
    CHECK(phase(result, PhaseKind::body) == KernelPhaseState::Miss);
    CHECK(phase(result, PhaseKind::tail) == KernelPhaseState::Miss);
    auto archive = s.archive();
    REQUIRE(archive);
    auto e = JudgementSession::evaluateReplay(*archive);
    REQUIRE(e);
    CHECK(sameKernelResult(result, *e->result));
}

TEST_CASE("B3 resumed clock below ControlDrain waits before request acceptance", "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE(d.acceptControl(scope, 0));
    REQUIRE(d.deliver({scope, 0, EndRunEpoch{Tick{900}, 1}}));
    REQUIRE(d.pause());
    REQUIRE(view(s).lastAdvanceHorizon == Tick{903});
    const RpaContext resumed{1, 2, 2};
    REQUIRE(d.resume({RpaControlKind::resume, Tick{900}, 0, 1, 0, {}}, resumed, true));
    REQUIRE(d.deliver({resumed, 0, CapturePrefix{Tick{902}}}));
    const auto before = view(s);
    auto early = d.request({resumed, 10, 1, Tick{901}});
    REQUIRE_FALSE(early);
    CHECK(early.error().code() == "host.rpa.control_pending");
    CHECK_FALSE(d.poll(10));
    CHECK_FALSE(d.suspended());
    CHECK(sameKernelResult(before, view(s)));
    // The refused request consumes neither the identity nor the command ordinal.
    REQUIRE(d.deliver({resumed, 1, CapturePrefix{Tick{904}}}));
    auto ready = d.request({resumed, 10, 1, Tick{904}});
    REQUIRE(ready);
    CHECK(ready->disposition == AdvanceDisposition::executed);
    REQUIRE(ready->result);
    CHECK(ready->result->lastAdvanceHorizon == Tick{904});
    auto archive = s.archive();
    REQUIRE(archive);
    auto replay = JudgementSession::evaluateReplay(*archive);
    REQUIRE(replay);
    REQUIRE(replay->result);
    CHECK(sameKernelResult(view(s), *replay->result));
}
TEST_CASE("B3 scheduler factory rejects invalid configuration without exceptions", "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    CHECK_FALSE(ExecutionScheduler::create(s, scope, TickSpan{-1}, 32));
    CHECK_FALSE(ExecutionScheduler::create(s, {0, 1, 1}, TickSpan{3}, 32));
    CHECK_FALSE(ExecutionScheduler::create(s, {1, 0, 1}, TickSpan{3}, 32));
    CHECK_FALSE(ExecutionScheduler::create(s, {1, 1, 0}, TickSpan{3}, 32));
    CHECK_FALSE(ExecutionScheduler::create(s, scope, TickSpan{3}, 0));
    auto empty = JudgementSession::create();
    REQUIRE(empty);
    CHECK_FALSE(ExecutionScheduler::create(*empty, scope, TickSpan{3}, 32));
}
