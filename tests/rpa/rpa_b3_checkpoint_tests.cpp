#include "rpa_b3_fixture.hpp"

// Sealed checkpoint, hint and log base cases.

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
using namespace cuexis::rpa::testing;

TEST_CASE("R3b untrusted substituted partial and multiple hints retain origin behavior",
          "[rpa][r3b][checkpoint][provenance]") {
    Fixture3 f;
    auto source = f.session();
    REQUIRE(source.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(source.advance(Tick{903}));
    REQUIRE(source.advance(Tick{1103}));
    const auto archive = source.archive();
    REQUIRE(archive);
    const auto valid = JudgementSession::checkpoint(*archive, Tick{903});
    REQUIRE(valid);
    auto reference = f.session();
    REQUIRE(reference.seek(*archive, Tick{1103}));
    const ReplayArchive external{
        archive->anchor(), std::make_shared<const RecoveryInputs>(f.configuration(), f.prepared())};
    for (unsigned variant = 0; variant < 6; ++variant) {
        auto hint = *valid;
        if (variant == 0)
            hint.state = std::make_shared<const SnapshotDTO>(*hint.state);
        if (variant == 1)
            hint.horizon = Tick{904};
        if (variant == 2)
            hint.cut.partialHorizon = Tick{903};
        if (variant == 3)
            hint.archive = external;
        if (variant == 4)
            hint = *JudgementSession::checkpoint(*archive, Tick{902});
        const std::array hints{hint, hint};
        auto checked = f.session();
        REQUIRE(checked.seek(variant == 5 ? external : *archive, Tick{1103},
                             replayCutAt(*archive, Tick{1103}), std::span{&hint, 1}));
        CHECK(detail::KernelTestAccess::certifiedPrefixRecords(checked) == 0);
        CHECK(sameSnapshot(reference.snapshot()->state(), checked.snapshot()->state()));
        REQUIRE(checked.seek(*archive, Tick{1103}, replayCutAt(*archive, Tick{1103}), hints));
        CHECK(detail::KernelTestAccess::certifiedPrefixRecords(checked) == 0);
        CHECK(sameSnapshot(reference.snapshot()->state(), checked.snapshot()->state()));
    }
    detail::KernelTestControls failure{};
    failure.failCertifiedRestore = true;
    auto fallback = detail::ExecutionKernel::seekCandidate(*archive, Tick{1103}, &failure, nullptr,
                                                           std::span{&*valid, 1});
    REQUIRE(fallback);
    CHECK((*fallback)->certifiedPrefixRecords() == 0);
    CHECK(sameSnapshot(reference.snapshot()->state(), (*fallback)->snapshot().value()->dto));
    auto partial = f.session();
    REQUIRE(partial.seek(*archive, Tick{950}, ReplayCut{2, Tick{950}}, std::span{&*valid, 1}));
    CHECK(detail::KernelTestAccess::certifiedPrefixRecords(partial) == 2);
    auto partialOrigin = f.session();
    REQUIRE(partialOrigin.seek(*archive, Tick{950}, ReplayCut{2, Tick{950}}));
    CHECK(sameSnapshot(partial.snapshot()->state(), partialOrigin.snapshot()->state()));
}

TEST_CASE("R3b immutable log base preserves large payload and tail boundaries",
          "[rpa][r3b][storage]") {
    Fixture3 f;
    auto source = f.session();
    std::vector<CanonicalInput> batch;
    for (std::uint64_t i = 0; i < 512; ++i)
        batch.push_back(occurrence(900, i, InputAction::press));
    REQUIRE(source.submitCanonical(batch));
    REQUIRE(source.advance(Tick{903}));
    const auto prefix = source.snapshot();
    REQUIRE(prefix);
    auto owner = std::make_shared<const SnapshotDTO>(prefix->state());
    for (unsigned i = 1; i <= 33; ++i)
        REQUIRE(source.advance(Tick{903 + i}));
    const auto archive = source.archive();
    REQUIRE(archive);
    const auto checkpoint = JudgementSession::checkpoint(*archive, Tick{903});
    REQUIRE(checkpoint);
    for (const unsigned count : {0U, 31U, 32U, 33U}) {
        auto base = std::make_shared<detail::ExecutionLogChunk>();
        base->baseState = owner;
        base->baseRecords = 2;
        std::shared_ptr<const detail::ExecutionLogChunk> tail = base;
        for (unsigned i = 0; i < count; ++i) {
            auto next = detail::reserveLogAppend(tail);
            next->records.push_back(archive->data().records[2 + i]);
            tail = std::move(next);
        }
        const auto records = detail::flattenLog(tail);
        CHECK(records.size() == 2 + count);
        CHECK(records.front().batch == batch);
        CHECK(records.front().admission == archive->data().records.front().admission);
        ReplayData expected = archive->data();
        expected.records.resize(2 + count);
        const auto inputs = std::make_shared<const RecoveryInputs>(f.configuration(), f.prepared());
        ReplayArchive actual{
            std::make_shared<const ReplayData>(ReplayData{records, 512, archive->data().identity}),
            inputs};
        ReplayArchive oracle{std::make_shared<const ReplayData>(std::move(expected)), inputs};
        CHECK(sameReplayHistory(actual, oracle));
        auto origin = f.session();
        auto restored = f.session();
        const auto horizon = Tick{903 + count};
        const ReplayCut cut{2 + count, {}};
        REQUIRE(origin.seek(*archive, horizon, cut));
        REQUIRE(restored.seek(*archive, horizon, cut, std::span{&*checkpoint, 1}));
        CHECK(detail::KernelTestAccess::certifiedPrefixRecords(restored) == 2);
        CHECK(sameSnapshot(origin.snapshot()->state(), restored.snapshot()->state()));
        REQUIRE_FALSE(origin.submitCanonical(batch));
        REQUIRE_FALSE(restored.submitCanonical(batch));
        const auto branch = restored.archive();
        REQUIRE(branch);
        CHECK(branch->data().eventCount == 1024);
        CHECK(sameReplayHistory(*branch, *origin.archive()));
    }
}

TEST_CASE("R3b base releases source suffix and retired owning state independently",
          "[rpa][r3b][lifetime]") {
    Fixture3 f;
    auto source = f.session();
    REQUIRE(source.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(source.advance(Tick{903}));
    for (unsigned i = 1; i <= 33; ++i)
        REQUIRE(source.advance(Tick{903 + i}));
    std::optional<ReplayArchive> archive;
    {
        auto value = source.archive();
        REQUIRE(value);
        archive.emplace(std::move(*value));
    }
    std::weak_ptr<const ReplayData> oldSource = archive->anchor();
    std::weak_ptr<const SnapshotDTO> checkpointState;
    auto restored = [&] {
        auto checkpoint = JudgementSession::checkpoint(*archive, Tick{903});
        REQUIRE(checkpoint);
        checkpointState = checkpoint->state;
        auto session = f.session();
        REQUIRE(session.seek(*archive, Tick{904}, replayCutAt(*archive, Tick{904}),
                             std::span{&*checkpoint, 1}));
        return session;
    }();
    CHECK_FALSE(checkpointState.expired());
    REQUIRE(restored.advance(Tick{905}));
    archive.reset();
    REQUIRE(source.reset());
    CHECK(oldSource.expired());
    auto result = restored.query();
    REQUIRE(result);
    auto snapshot = restored.snapshot();
    REQUIRE(snapshot);
    auto branch = restored.archive();
    REQUIRE(branch);
    const auto bytes = encodeSnapshot(*snapshot);
    REQUIRE(bytes);
    REQUIRE(restored.reset());
    CHECK(checkpointState.expired());
    CHECK(result->kernelView().lastAdvanceHorizon == Tick{905});
    CHECK(*encodeSnapshot(*snapshot) == *bytes);
    auto replay = JudgementSession::evaluateReplay(*branch);
    REQUIRE(replay);
    CHECK(sameKernelResult(result->kernelView(), *replay->result));
}

TEST_CASE("R3b restored prefix preserves future commit ID exhaustion",
          "[rpa][r3b][fault][exhaustion]") {
    Fixture3 f;
    f.rules = controlRules(false);
    auto source = f.session();
    REQUIRE(source.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(source.advance(Tick{903}));
    REQUIRE(source.advance(Tick{1103}));
    auto archive = source.archive();
    REQUIRE(archive);
    auto checkpoint = JudgementSession::checkpoint(*archive, Tick{903});
    REQUIRE(checkpoint);
    auto origin = f.session();
    auto restored = f.session();
    REQUIRE(origin.seek(*archive, Tick{903}));
    REQUIRE(restored.seek(*archive, Tick{903}, replayCutAt(*archive, Tick{903}),
                          std::span{&*checkpoint, 1}));
    CHECK(detail::KernelTestAccess::certifiedPrefixRecords(restored) == 2);
    std::optional<cuexis::core::Error> expected;
    std::optional<SnapshotDTO> state;
    for (auto* session : {&origin, &restored}) {
        detail::KernelTestControls ids{};
        ids.nextCommitId = UINT64_MAX;
        REQUIRE(detail::KernelTestAccess::inject(*session, ids));
        REQUIRE(session->controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
        REQUIRE(session->controlRpa({RpaControlKind::resume,
                                     Tick{900},
                                     0,
                                     1,
                                     1,
                                     {{0, occurrence(901, 0, InputAction::release).key}}}));
        REQUIRE(session->submitCanonical({occurrence(1040, 0, InputAction::press)}));
        auto failed = session->advance(Tick{1103});
        REQUIRE_FALSE(failed);
        auto dto = detail::KernelTestAccess::captureDTO(*session);
        REQUIRE(dto);
        if (!expected) {
            expected = failed.error();
            state = *dto;
        }
        CHECK(sameRecoveryError(expected, failed.error()));
        CHECK(sameSnapshot(*state, *dto));
        CHECK(dto->kernel.state == KernelSessionState::Faulted);
    }
}
TEST_CASE("R3 empty RPA finalization preserves private reserve and owning results",
          "[rpa][r3][empty-reserve]") {
    const auto rules = GENERATE(0, 1, 2);
    const auto injectIds = GENERATE(false, true);
    Fixture3 f;
    if (rules)
        f.rules = controlRules(rules == 2);
    auto live = f.session();
    auto reference = f.session();
    // Injection leaves an otherwise identical state and forces the full reserve-copy path.
    REQUIRE(detail::KernelTestAccess::inject(reference, {}));
    const auto equal = [&] {
        auto actual = live.snapshot();
        auto expected = reference.snapshot();
        REQUIRE(actual);
        REQUIRE(expected);
        CHECK(sameSnapshot(actual->state(), expected->state()));
        auto actualBytes = encodeSnapshot(*actual);
        auto expectedBytes = encodeSnapshot(*expected);
        REQUIRE(actualBytes);
        REQUIRE(expectedBytes);
        CHECK(*actualBytes == *expectedBytes);
        auto a = live.archive();
        auto b = reference.archive();
        REQUIRE(a);
        REQUIRE(b);
        CHECK(sameReplayHistory(*a, *b));
        auto archiveBytes = encodeReplay(*a);
        auto referenceBytes = encodeReplay(*b);
        REQUIRE(archiveBytes);
        REQUIRE(referenceBytes);
        CHECK(*archiveBytes == *referenceBytes);
    };
    REQUIRE(live.advance(Tick{1}));
    REQUIRE(reference.advance(Tick{1}));
    auto old = live.snapshot();
    REQUIRE(old);
    auto oldBytes = encodeSnapshot(*old);
    REQUIRE(oldBytes);
    const auto oldUnchanged = [&] {
        auto currentBytes = encodeSnapshot(*old);
        REQUIRE(currentBytes);
        CHECK(*currentBytes == *oldBytes);
    };
    auto oldQuery = live.query();
    REQUIRE(oldQuery);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    rpa::SeekLedger actualLedger{}, referenceLedger{};
    {
        rpa::SeekSinkScope sink{&actualLedger};
        REQUIRE(live.advance(Tick{2}));
    }
    {
        rpa::SeekSinkScope sink{&referenceLedger};
        REQUIRE(reference.advance(Tick{2}));
    }
    const auto index = static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution);
    CHECK(actualLedger.work[index].faultReserveCopies == 0);
    CHECK(referenceLedger.work[index].faultReserveCopies == 1);
#else
    REQUIRE(live.advance(Tick{2}));
    REQUIRE(reference.advance(Tick{2}));
#endif
    equal();
    const std::vector inputs{occurrence(900, 0, InputAction::press),
                             occurrence(1000, 0, InputAction::release)};
    REQUIRE(live.submitCanonical(inputs));
    REQUIRE(reference.submitCanonical(inputs));
    for (const auto h : {3, 903, 1103, 1104, 1105}) {
        REQUIRE(live.advance(Tick{h}));
        REQUIRE(reference.advance(Tick{h}));
        equal();
    }
    for (auto* session : {&live, &reference}) {
        REQUIRE(session->controlRpa({RpaControlKind::pause, Tick{1102}, 0, 1, 0, {}}));
        REQUIRE(session->controlRpa({RpaControlKind::resume, Tick{1102}, 0, 1, 0, {}}));
        REQUIRE(session->advance(Tick{1106}));
    }
    equal();
    CHECK(oldQuery->kernelView().lastAdvanceHorizon == Tick{1});
    oldUnchanged();
    for (auto* session : {&live, &reference}) {
        if (injectIds) {
            detail::KernelTestControls controls{};
            controls.nextCommitId = UINT64_MAX - 1;
            REQUIRE(detail::KernelTestAccess::inject(*session, controls));
            REQUIRE(session->advance(Tick{1107}));
        }
        detail::KernelTestControls failure{};
        failure.failAdvanceFinalization = true;
        REQUIRE(detail::KernelTestAccess::inject(*session, failure));
        REQUIRE_FALSE(session->advance(Tick{1108}));
    }
    auto actual = detail::KernelTestAccess::captureDTO(live);
    auto expected = detail::KernelTestAccess::captureDTO(reference);
    REQUIRE(actual);
    REQUIRE(expected);
    CHECK(sameSnapshot(*actual, *expected));
    CHECK(actual->kernel.state == KernelSessionState::Faulted);
    CHECK(actual->kernel.faultStage == FaultStage::control);
    CHECK(actual->kernel.lastAdvanceHorizon == Tick{injectIds ? 1107 : 1106});
    oldUnchanged();
}

TEST_CASE("R3 reused empty reserve faults from the latest successful horizon",
          "[rpa][r3][empty-reserve]") {
    Fixture3 f;
    auto live = f.session();
    auto reference = f.session();
    REQUIRE(detail::KernelTestAccess::inject(reference, {}));
    for (const auto h : {1, 2}) {
        REQUIRE(live.advance(Tick{h}));
        REQUIRE(reference.advance(Tick{h}));
    }
    // No real Tick may rebuild the optimized reserve before this failure.
    for (auto* session : {&live, &reference}) {
        detail::KernelTestControls failure{};
        failure.failAdvanceFinalization = true;
        REQUIRE(detail::KernelTestAccess::inject(*session, failure));
        REQUIRE_FALSE(session->advance(Tick{3}));
    }
    auto actual = detail::KernelTestAccess::captureDTO(live);
    auto expected = detail::KernelTestAccess::captureDTO(reference);
    REQUIRE(actual);
    REQUIRE(expected);
    CHECK(sameSnapshot(*actual, *expected));
    CHECK(actual->kernel.state == KernelSessionState::Faulted);
    CHECK(actual->kernel.faultStage == FaultStage::control);
    CHECK(actual->kernel.processedFrontier == Tick{-1});
    CHECK(actual->kernel.lastAdvanceHorizon == Tick{2});
    CHECK(actual->kernel.requestedHorizon == Tick{3});
}

TEST_CASE("R3 deterministic rejected admission still branches and recovers",
          "[rpa][r3][recovery][rejection]") {
    Fixture3 f;
    const RecoveryInputs inputs{f.configuration(), f.prepared()};
    auto source = f.session();
    const auto input = occurrence(900, 0, InputAction::press);
    REQUIRE(source.submitCanonical({input}));
    REQUIRE(source.advance(Tick{903}));
    REQUIRE(source.advance(Tick{1103}));
    const auto original = source.archive();
    REQUIRE(original);
    auto sought = f.session();
    REQUIRE(sought.seek(*original, Tick{903}));
    const auto before = view(sought);
    auto rejection = sought.submitCanonical({input});
    REQUIRE_FALSE(rejection);
    CHECK(rejection.error().code() == "judgement.late.duplicate_queue_entry");
    CHECK(sameKernelResult(before, view(sought)));
    const auto branch = sought.archive();
    REQUIRE(branch);
    REQUIRE(branch->data().records.size() == 3);
    CHECK(sameRecoveryError(branch->data().records.back().rejection, rejection.error()));
    CHECK_FALSE(sameReplayHistory(*original, *branch));
    auto snapshot = sought.snapshot();
    REQUIRE(snapshot);
    auto recovered = JudgementSession::recover(*snapshot, inputs);
    REQUIRE(recovered);
    CHECK(sameSnapshot(snapshot->state(), recovered->snapshot()->state()));
    auto bytes = encodeSnapshot(*snapshot);
    REQUIRE(bytes);
    auto decoded = decodeSnapshot(*bytes, inputs);
    REQUIRE(decoded);
    CHECK(sameSnapshot(snapshot->state(), decoded->state()));
    auto evaluation = JudgementSession::evaluateReplay(*branch);
    REQUIRE(evaluation);
    CHECK(sameKernelResult(before, *evaluation->result));
}
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
    REQUIRE_FALSE(r);
    CHECK(r.error().code() == "host.rpa.request_retired");
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
