#include "rpa_b3_fixture.hpp"

// RPA recovery wire, cache and prefix recovery cases.

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
using namespace cuexis::rpa::testing;

TEST_CASE("Recovery wire cached sequence bytes remain exact after growth and detached mutation",
          "[rpa][wire][redesign]") {
    detail::SharedSequence<std::uint64_t> values;
    for (std::uint64_t i = 0; i < 257; ++i)
        values.push_back(i * i);
    detail::wire::Writer cached, reference;
    cached(values);
    reference(values.materialize());
    for (unsigned i = 0; i < 31; ++i) {
        cached(values);
        reference(values.materialize());
    }
    values.update(128, [](auto& value) { value = UINT64_MAX; });
    cached(values);
    reference(values.materialize());
    values.push_back(12);
    cached(values);
    reference(values.materialize());
    CHECK(cached.bytes == reference.bytes);
    detail::SharedSequence<std::uint64_t> independent;
    for (const auto value : values)
        independent.push_back(value);
    cached(independent);
    reference(independent.materialize());
    CHECK(cached.bytes == reference.bytes);
    detail::wire::Writer measuring{true};
    measuring(values, values, independent);
    detail::wire::Writer materialized;
    materialized(values.materialize(), values.materialize(), independent.materialize());
    CHECK(measuring.bytes.empty());
    CHECK(measuring.size() == materialized.bytes.size());
    // Writer is reused by malformed-wire tests. Its public output can be cleared or edited.
    cached.bytes.clear();
    cached(values);
    detail::wire::Writer fresh;
    fresh(values.materialize());
    CHECK(cached.bytes == fresh.bytes);
    std::fill(cached.bytes.begin(), cached.bytes.end(), std::byte{0xff});
    cached(values);
    CHECK(std::equal(fresh.bytes.begin(), fresh.bytes.end(),
                     cached.bytes.begin() + static_cast<std::ptrdiff_t>(fresh.bytes.size())));
    detail::wire::Writer bounded, uncached, boundedMeasure{true};
    for (std::uint64_t i = 0; i < 160; ++i) {
        detail::SharedSequence<std::uint64_t> distinct{i, i + 1};
        bounded(distinct, distinct);
        boundedMeasure(distinct, distinct);
        uncached(distinct.materialize(), distinct.materialize());
    }
    CHECK(bounded.bytes == uncached.bytes);
    CHECK(boundedMeasure.size() == uncached.bytes.size());
}

TEST_CASE("Recovery cache admission preserves output edits and revision changes",
          "[rpa][wire][redesign]") {
    detail::SharedSequence<std::uint64_t> values{1, 2, 3};
    detail::wire::Writer cached, fresh;
    fresh(values.materialize());
    cached(values);
    cached.bytes.clear();
    cached(values);
    CHECK(cached.bytes == fresh.bytes);
    std::fill(cached.bytes.begin(), cached.bytes.end(), std::byte{0xff});
    cached(values);
    CHECK(std::equal(fresh.bytes.begin(), fresh.bytes.end(),
                     cached.bytes.begin() + static_cast<std::ptrdiff_t>(fresh.bytes.size())));
    Fixture fixture;
    auto assembled = assembleGameplay(fixture.request());
    REQUIRE(assembled);
    detail::SharedSequence<KernelProjection> projections{
        KernelProjection{.judgementIdentity = assembled->prepared}};
    detail::wire::Writer revisions, reference, measuring{true};
    for (const auto revision : {2U, 2U, 3U, 3U, 3U, 2U}) {
        revisions.revision = reference.revision = measuring.revision = revision;
        revisions(projections);
        reference(projections.materialize());
        measuring(projections);
    }
    CHECK(revisions.bytes == reference.bytes);
    CHECK(measuring.size() == reference.size());
}

TEST_CASE("Vendored FNV-1a preserves exact serial recurrence and continuation",
          "[rpa][wire][fnv]") {
    std::vector<std::byte> bytes(1025);
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::byte>(i & 255U);
    for (std::size_t n = 0; n <= bytes.size(); ++n) {
        const auto input = std::span<const std::byte>{bytes}.first(n);
        std::uint64_t expected = 14695981039346656037ULL;
        for (const auto value : input)
            expected = (expected ^ std::to_integer<unsigned>(value)) * 1099511628211ULL;
        CHECK(detail::wire::digest(input) == expected);
        const auto split = n / 2;
        const auto partial = cuexis::third_party::fnv1a64(input.first(split));
        CHECK(cuexis::third_party::fnv1a64(input.subspan(split), partial) == expected);
    }
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R3 exact preparation ledger distinguishes recovery candidates from replay work",
          "[rpa][r3][observation]") {
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    Fixture3 f;
    auto live = f.session();
    REQUIRE(live.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(live.advance(Tick{903}));
    const auto saved = live.snapshot();
    REQUIRE(saved);
    const RecoveryInputs inputs{f.configuration(), f.prepared()};
    rpa::SeekLedger ledger{};
    {
        rpa::SeekSinkScope sink{&ledger};
        auto restored = JudgementSession::recover(*saved, inputs);
        REQUIRE(restored);
        CHECK(sameSnapshot(saved->state(), restored->snapshot()->state()));
    }
    const auto& preparation =
        ledger.work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)];
    const auto& execution =
        ledger.work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)];
    CHECK(preparation.kernelPrepareCalls == 1);
    CHECK(preparation.kernelPrepareSuccesses == preparation.kernelPrepareCalls);
    CHECK(preparation.derivedIndexBuilds == preparation.kernelPrepareCalls);
    CHECK(preparation.initialReserveCopies == preparation.kernelPrepareCalls);
    CHECK(preparation.recoveryConfigCopies == preparation.kernelPrepareCalls);
    CHECK(execution.draftCopies >= 1);
    CHECK(execution.foldCopies >= 1);
    CHECK(execution.finalizationCopies == 1);
    CHECK(execution.faultReserveCopies >= 3);
    CHECK(execution.tailRecordCopies == 1);
    CHECK(execution.tailBatchElements == 1);
    CHECK(execution.tailAdmissionElements == 1);
    std::cout << "R3_PREPARE_LEDGER prepare_calls=" << preparation.kernelPrepareCalls
              << " prepare_us=" << preparation.kernelPrepareUs
              << " index_us=" << preparation.derivedIndexUs << " restore_validation_us="
              << ledger.work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)]
                     .rpaRestoreValidationUs
              << " draft=" << execution.draftCopies << " reserve=" << execution.faultReserveCopies
              << " fold=" << execution.foldCopies << " final=" << execution.finalizationCopies
              << '\n';
}
#endif
TEST_CASE("R3 early RPA cuts recover only the applied history and continue independently",
          "[rpa][r3][recovery][seek]") {
    const auto variant = GENERATE(0, 1, 2, 3);
    const auto fold = GENERATE(false, true);
    Fixture3 f;
    if (fold)
        f.rules = controlRules(true);
    const RecoveryInputs inputs{f.configuration(), f.prepared()};
    auto source = f.session();
    auto reference = f.session();
    const auto admitted = std::vector{occurrence(900, 0, InputAction::press),
                                      occurrence(1000, 0, InputAction::release)};
    REQUIRE(source.submitCanonical(admitted));
    REQUIRE(reference.submitCanonical(admitted));
    REQUIRE(source.advance(Tick{903}));
    const RpaControlRecord pause{RpaControlKind::pause, Tick{900}, 0, 1, 0, {}};
    const RpaControlRecord resume{RpaControlKind::resume,
                                  Tick{900},
                                  0,
                                  1,
                                  1,
                                  {{0, occurrence(901, 0, InputAction::release).key}}};
    REQUIRE(source.controlRpa(pause));
    REQUIRE(source.controlRpa(resume));
    REQUIRE(source.advance(Tick{1103}));
    auto archive = source.archive();
    REQUIRE(archive);
    const auto originalBytes = encodeReplay(*archive);
    REQUIRE(originalBytes);
    const auto horizon = variant == 0 ? Tick{902} : Tick{903};
    const ReplayCut cut = variant == 0 ? ReplayCut{1, horizon}
                                       : ReplayCut{static_cast<std::uint64_t>(variant + 1), {}};
    REQUIRE(reference.advance(horizon));
    if (variant >= 2)
        REQUIRE(reference.controlRpa(pause));
    if (variant >= 3)
        REQUIRE(reference.controlRpa(resume));
    auto sought = f.session();
    REQUIRE(sought.seek(*archive, horizon, cut));
    CHECK(sameKernelResult(view(reference), view(sought)));
    // A Seek preserves the complete source, while a Snapshot describes only applied work.
    CHECK(sameReplayHistory(*archive, *sought.archive()));
    const auto saved = sought.snapshot();
    REQUIRE(saved);
    CHECK(sameSnapshot(saved->state(), reference.snapshot()->state()));
    auto wrongInputs = inputs;
    wrongInputs.configuration.identityDeclarations.session.judgementConfigToken += ".different";
    CHECK_FALSE(JudgementSession::recover(*saved, wrongInputs));
    auto malformedData = std::make_shared<ReplayData>(archive->data());
    malformedData->records.back().batch.push_back(occurrence(1040, 0, InputAction::press));
    const ReplayArchive malformed{malformedData, std::make_shared<const RecoveryInputs>(inputs)};
    CHECK_FALSE(sought.seek(malformed, horizon, cut));
    CHECK(sameSnapshot(saved->state(), sought.snapshot()->state()));
    REQUIRE(saved->state().kernel.rpa);
    CHECK(saved->state().kernel.rpa->accepted.size() == 2);
    CHECK(saved->state().pending.size() == (variant == 0 ? 2 : 1));
    if (variant != 0)
        CHECK(phase(saved->state().kernel, PhaseKind::head) == KernelPhaseState::Hit);
    if (fold) {
        REQUIRE(saved->state().kernel.fold);
        CHECK(saved->state().kernel.fold->score == (variant == 0 || variant == 3 ? 0 : 2));
    }
    auto direct = JudgementSession::recover(*saved, inputs);
    REQUIRE(direct);
    CHECK(sameSnapshot(saved->state(), direct->snapshot()->state()));
    const auto sourceBefore = source.snapshot();
    REQUIRE(sourceBefore);
    auto forked = source.forkForSeek(*archive, horizon, cut);
    REQUIRE(forked);
    CHECK(sameSnapshot(saved->state(), forked->snapshot()->state()));
    CHECK(sameSnapshot(sourceBefore->state(), source.snapshot()->state()));
    CHECK_FALSE(source.forkForSeek(malformed, horizon, cut));
    CHECK(sameSnapshot(sourceBefore->state(), source.snapshot()->state()));
    const auto bytes = encodeSnapshot(*saved);
    REQUIRE(bytes);
    auto decoded = decodeSnapshot(*bytes, inputs);
    REQUIRE(decoded);
    auto recovered = JudgementSession::recover(*decoded, inputs);
    INFO((recovered ? "" : std::string{recovered.error().message()}));
    REQUIRE(recovered);
    CHECK(sameSnapshot(saved->state(), recovered->snapshot()->state()));
    CHECK(view(*recovered).lastAdvanceHorizon == horizon);
    CHECK(view(*recovered).processedFrontier == Tick{horizon.value() - 3});
    CHECK(view(*recovered).rpa->paused == (variant == 2));
    CHECK(view(sought).runScope != view(*recovered).runScope);
    CHECK(view(*direct).runScope != view(*recovered).runScope);
    // Continue independent states, including same-Tick control and a future input.
    for (auto* session : {&reference, &sought, &*recovered, &*direct, &*forked}) {
        if (variant == 2)
            REQUIRE(session->controlRpa(resume));
        if (variant == 0) {
            REQUIRE(session->advance(Tick{903}));
            REQUIRE(session->controlRpa(pause));
            REQUIRE(session->controlRpa(resume));
        }
        REQUIRE(session->submitCanonical({occurrence(1040, 0, InputAction::press)}));
        REQUIRE(session->advance(Tick{1103}));
    }
    CHECK(sameSnapshot(reference.snapshot()->state(), sought.snapshot()->state()));
    CHECK(sameSnapshot(reference.snapshot()->state(), recovered->snapshot()->state()));
    CHECK(sameSnapshot(reference.snapshot()->state(), direct->snapshot()->state()));
    CHECK(sameSnapshot(reference.snapshot()->state(), forked->snapshot()->state()));
    CHECK(phase(view(*recovered), PhaseKind::head) == KernelPhaseState::Hit);
    CHECK(*encodeReplay(*archive) == *originalBytes);
    detail::KernelTestControls finalFault{};
    finalFault.failAdvanceFinalization = true;
    REQUIRE(detail::KernelTestAccess::inject(source, finalFault));
    REQUIRE_FALSE(source.advance(Tick{1200}));
    const auto faulted = view(source);
    auto fromFault = source.forkForSeek(*archive, horizon, cut);
    REQUIRE_FALSE(fromFault);
    auto directFaultSeek = source.seek(*archive, horizon, cut);
    REQUIRE_FALSE(directFaultSeek);
    CHECK(sameRecoveryError(fromFault.error(), directFaultSeek.error()));
    CHECK(sameKernelResult(faulted, view(source)));
    REQUIRE(source.reset());
    REQUIRE(sought.reset());
    CHECK(*encodeSnapshot(*saved) == *bytes);
    CHECK(*encodeReplay(*archive) == *originalBytes);
    auto replay = recovered->archive();
    REQUIRE(replay);
    auto evaluated = JudgementSession::evaluateReplay(*replay);
    REQUIRE(evaluated);
    CHECK(sameKernelResult(view(*recovered), *evaluated->result));
}
TEST_CASE("R3 recovered early RPA prefix preserves future semantic allocation failures",
          "[rpa][r3][recovery][fault]") {
    const auto failure = GENERATE(0, 1, 2, 3, 4);
    const auto certified = GENERATE(false, true);
    Fixture3 f;
    f.rules = controlRules(true);
    const RecoveryInputs inputs{f.configuration(), f.prepared()};
    auto source = f.session();
    REQUIRE(source.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(source.advance(Tick{903}));
    auto reference = f.session();
    REQUIRE(reference.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(reference.advance(Tick{903}));
    REQUIRE(source.advance(Tick{1103}));
    auto archive = source.archive();
    REQUIRE(archive);
    auto sought = f.session();
    auto checkpoint = JudgementSession::checkpoint(*archive, Tick{903});
    REQUIRE(checkpoint);
    REQUIRE(sought.seek(*archive, Tick{903}, replayCutAt(*archive, Tick{903}),
                        certified ? std::span<const ReplayCheckpoint>{&*checkpoint, 1}
                                  : std::span<const ReplayCheckpoint>{}));
    CHECK(detail::KernelTestAccess::certifiedPrefixRecords(sought) == (certified ? 2 : 0));
    auto saved = sought.snapshot();
    REQUIRE(saved);
    const auto savedBytes = encodeSnapshot(*saved);
    REQUIRE(savedBytes);
    auto recovered = JudgementSession::recover(*saved, inputs);
    REQUIRE(recovered);
    detail::KernelTestControls controls{};
    controls.failurePoint = failure == 1 ? detail::KernelFailurePoint::beforeFold
                                         : detail::KernelFailurePoint::beforeSeal;
    if (failure < 2)
        controls.failedTick = Tick{1000};
    controls.failAdvanceFinalization = failure == 2;
    if (failure == 3)
        controls.failRecordReservationHorizon = Tick{1103};
    controls.failSubmitRecordReservation = failure == 4;
    std::optional<cuexis::core::Error> expected;
    std::optional<SnapshotDTO> expectedState;
    for (auto* session : {&reference, &sought, &*recovered}) {
        REQUIRE(detail::KernelTestAccess::inject(*session, controls));
        const auto before = detail::KernelTestAccess::captureDTO(*session);
        REQUIRE(before);
        const auto oldArchive = session->archive();
        REQUIRE(oldArchive);
        std::optional<cuexis::core::Error> actual;
        if (failure == 4) {
            auto result = session->submitCanonical({occurrence(1040, 0, InputAction::press)});
            REQUIRE_FALSE(result);
            actual = result.error();
        } else {
            auto result = session->advance(Tick{1103});
            REQUIRE_FALSE(result);
            actual = result.error();
        }
        auto dto = detail::KernelTestAccess::captureDTO(*session);
        REQUIRE(dto);
        if (!expected) {
            expected = actual;
            expectedState = *dto;
        }
        CHECK(sameRecoveryError(expected, actual));
        CHECK(sameSnapshot(*expectedState, *dto));
        if (failure >= 3) {
            CHECK(sameSnapshot(*before, *dto));
            CHECK(sameReplayHistory(*oldArchive, *session->archive()));
            REQUIRE(detail::KernelTestAccess::inject(*session, {}));
            auto unaffected = session->snapshot();
            REQUIRE(unaffected);
            auto valid = JudgementSession::recover(*unaffected, inputs);
            REQUIRE(valid);
            auto encoded = encodeSnapshot(*unaffected);
            REQUIRE(encoded);
            REQUIRE(decodeSnapshot(*encoded, inputs));
            REQUIRE(session->advance(Tick{1103}));
        } else {
            CHECK(dto->kernel.state == KernelSessionState::Faulted);
            CHECK_FALSE(session->advance(Tick{1104}));
            auto value = detail::KernelTestAccess::snapshotValue(*dto, inputs);
            auto faultRecovery = JudgementSession::recover(value, inputs);
            INFO((faultRecovery ? "" : std::string{faultRecovery.error().message()}));
            REQUIRE(faultRecovery);
            CHECK(sameKernelResult(dto->kernel, view(*faultRecovery)));
        }
    }
    CHECK(*encodeSnapshot(*saved) == *savedBytes);
}

TEST_CASE("R3b one sealed checkpoint restores full state and preserves canonical branches",
          "[rpa][r3b][recovery][checkpoint]") {
    const auto controlCut = GENERATE(2U, 3U, 4U);
    Fixture3 f;
    f.rules = controlRules(true);
    auto source = f.session();
    REQUIRE(source.submitCanonical(
        {occurrence(900, 0, InputAction::press), occurrence(1000, 0, InputAction::release)}));
    REQUIRE(source.advance(Tick{903}));
    const RpaControlRecord pause{RpaControlKind::pause, Tick{900}, 0, 1, 0, {}};
    const RpaControlRecord resume{RpaControlKind::resume,
                                  Tick{900},
                                  0,
                                  1,
                                  1,
                                  {{0, occurrence(901, 0, InputAction::release).key}}};
    REQUIRE(source.controlRpa(pause));
    REQUIRE(source.controlRpa(resume));
    REQUIRE(source.submitCanonical({occurrence(1040, 0, InputAction::press)}));
    REQUIRE(source.advance(Tick{1103}));
    const auto archive = source.archive();
    REQUIRE(archive);
    const auto checkpoint = JudgementSession::checkpoint(*archive, Tick{903});
    REQUIRE(checkpoint);
    const ReplayCut cut{controlCut, {}};
    auto origin = f.session();
    auto restored = f.session();
    REQUIRE(origin.seek(*archive, Tick{903}, cut));
    REQUIRE(restored.seek(*archive, Tick{903}, cut, std::span{&*checkpoint, 1}));
    CHECK(detail::KernelTestAccess::certifiedPrefixRecords(restored) == 2);
    CHECK(sameSnapshot(origin.snapshot()->state(), restored.snapshot()->state()));
    CHECK(*encodeSnapshot(*origin.snapshot()) == *encodeSnapshot(*restored.snapshot()));
    CHECK(view(origin).runScope != view(restored).runScope);
    CHECK(sameReplayHistory(*archive, *restored.archive()));
    for (auto* session : {&origin, &restored}) {
        if (controlCut == 2)
            REQUIRE(session->controlRpa(pause));
        if (controlCut < 4)
            REQUIRE(session->controlRpa(resume));
        REQUIRE(session->submitCanonical({occurrence(1040, 0, InputAction::press)}));
        REQUIRE(session->advance(Tick{1103}));
    }
    CHECK(sameSnapshot(origin.snapshot()->state(), restored.snapshot()->state()));
    const auto branch = restored.archive();
    REQUIRE(branch);
    CHECK(branch->data().eventCount == 3);
    CHECK(sameReplayHistory(*archive, *branch));
    CHECK(*encodeReplay(*origin.archive()) == *encodeReplay(*branch));
    auto replayed = JudgementSession::evaluateReplay(*branch);
    REQUIRE(replayed);
    CHECK(sameKernelResult(view(restored), *replayed->result));
    auto saved = restored.snapshot();
    REQUIRE(saved);
    REQUIRE(source.reset());
    REQUIRE(restored.reset());
    auto recovered = JudgementSession::recover(*saved, {f.configuration(), f.prepared()});
    REQUIRE(recovered);
    CHECK(sameSnapshot(saved->state(), recovered->snapshot()->state()));
}
