#include "rpa_b3_fixture.hpp"

// Clock, scheduler and content-bound cases.

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
using namespace cuexis::rpa::testing;

TEST_CASE("RPA F3 kernel history measures recovery and Replay at declared content bounds",
          "[rpa][b3][measurement][budget]") {
    const auto count = GENERATE(std::size_t{1}, std::size_t{256});
    Fixture3 f;
    auto& document = f.assembly.sources[0].document;
    const auto prototype = document.requirements[0];
    document.requirements.clear();
    std::erase_if(document.declarations, [](const auto& declaration) {
        return declaration.kind == DeclarationKind::requirement;
    });
    document.relations[0].members.clear();
    f.assembly.contentProfileLimits.maxRequirements =
        MeasuredParameter<std::uint64_t>::measured(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto requirement = prototype;
        requirement.stableId.declarationOrdinal = static_cast<std::uint32_t>(100 + i);
        requirement.identity.requirementLocalId = "budget." + std::to_string(i);
        requirement.resourceClaims[0].claimPolicy.competition =
            ClaimPolicyDeclaration::CompetitionKey{-1, static_cast<std::uint64_t>(i)};
        document.declarations.push_back(testDeclaration(DeclarationKind::requirement,
                                                        requirement.identity.requirementLocalId,
                                                        requirement.stableId.declarationOrdinal));
        document.relations[0].members.push_back(requirement.stableId);
        document.requirements.push_back(std::move(requirement));
    }
    const auto elapsed = [](auto start) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now() - start)
            .count();
    };
    auto start = std::chrono::steady_clock::now();
    const auto prepared = f.prepared();
    auto configuration = f.configuration();
    auto s = JudgementSession::create();
    REQUIRE(s);
    REQUIRE(s->configure(configuration));
    REQUIRE(s->prepare(prepared));
    const auto prepareNs = elapsed(start);
    start = std::chrono::steady_clock::now();
    REQUIRE(s->advance(Tick{1100}));
    const auto timerBurstNs = elapsed(start);
    const auto expected = view(*s);
    std::vector<std::int64_t> steadyNs;
    for (std::int64_t i = 1; i <= 32; ++i) {
        start = std::chrono::steady_clock::now();
        REQUIRE(s->advance(Tick{1100 + i}));
        steadyNs.push_back(elapsed(start));
    }
    const auto final = view(*s);
    CHECK(final.facts.size() == expected.facts.size());
    start = std::chrono::steady_clock::now();
    auto snapshot = s->snapshot();
    REQUIRE(snapshot);
    auto snapshotBytes = encodeSnapshot(*snapshot);
    REQUIRE(snapshotBytes);
    const auto snapshotNs = elapsed(start);
    start = std::chrono::steady_clock::now();
    auto decoded = decodeSnapshot(*snapshotBytes, {configuration, prepared});
    REQUIRE(decoded);
    auto recovered = JudgementSession::recover(*decoded, {configuration, prepared});
    REQUIRE(recovered);
    const auto restoreNs = elapsed(start);
    CHECK(sameKernelResult(final, view(*recovered)));
    start = std::chrono::steady_clock::now();
    auto archive = s->archive();
    REQUIRE(archive);
    auto replayBytes = encodeReplay(*archive);
    REQUIRE(replayBytes);
    const auto archiveNs = elapsed(start);
    start = std::chrono::steady_clock::now();
    auto replay = decodeReplay(*replayBytes, {configuration, prepared});
    REQUIRE(replay);
    auto evaluation = JudgementSession::evaluateReplay(*replay);
    REQUIRE(evaluation);
    const auto replayNs = elapsed(start);
    CHECK(sameKernelResult(final, *evaluation->result));
    auto seekSession = JudgementSession::create();
    REQUIRE(seekSession);
    REQUIRE(seekSession->configure(configuration));
    REQUIRE(seekSession->prepare(prepared));
    const auto cut = replayCutAt(*replay, Tick{1132});
    start = std::chrono::steady_clock::now();
    REQUIRE(seekSession->seek(*replay, Tick{1132}, cut));
    const auto seekNs = elapsed(start);
    CHECK(sameKernelResult(final, view(*seekSession)));
    std::sort(steadyNs.begin(), steadyNs.end());
    std::cout << "{\"event\":\"rpa.f3.kernel_budget\",\"test_only\":true,\"requirements\":" << count
              << ",\"phases_per_requirement\":3,\"facts\":" << final.facts.size()
              << ",\"prepare_ns\":" << prepareNs << ",\"timer_burst_ns\":" << timerBurstNs
              << ",\"steady_samples\":32,\"steady_p50_ns\":" << steadyNs[15]
              << ",\"steady_p95_ns\":" << steadyNs[30] << ",\"steady_max_ns\":" << steadyNs.back()
              << ",\"snapshot_encode_ns\":" << snapshotNs
              << ",\"snapshot_bytes\":" << snapshotBytes->size()
              << ",\"snapshot_decode_recover_ns\":" << restoreNs
              << ",\"archive_encode_ns\":" << archiveNs
              << ",\"replay_bytes\":" << replayBytes->size()
              << ",\"replay_decode_evaluate_ns\":" << replayNs << ",\"kernel_seek_ns\":" << seekNs
              << ",\"replay_records\":" << replay->data().records.size()
              << ",\"prepare_peak_heap_bytes\":null,\"production_max_seek_latency_ns\":null}\n";
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
