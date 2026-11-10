#include "rpa_b5_host_fixture.hpp"

// Directed close, tail reply, capture queue and close observation cases.

TEST_CASE("B5 main host drives actual owners and pumps while awaiting directed Close",
          "[b5][host]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto host = player::ExecutionHost::create(renderer, options(), ports(pumps));
    REQUIRE(host);
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    auto loaded = (*host)->load(std::move(*source));
    INFO((loaded
              ? ""
              : std::string{loaded.error().code()} + ": " + std::string{loaded.error().message()}));
    REQUIRE(loaded);
    REQUIRE((*host)->resize({320, 200}));
    REQUIRE_FALSE((*host)->resize({0, 200}));
    REQUIRE((*host)->state() == HostState::running);
    auto frame = (*host)->sample(1000, {1, 1, 1}, false);
    INFO((frame ? ""
                : std::string{frame.error().code()} + ": " + std::string{frame.error().message()}));
    REQUIRE(frame);
    auto receipt = (*host)->present(*frame);
    REQUIRE(receipt);
    REQUIRE(receipt->status == PresentStatus::succeeded);
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    REQUIRE((*host)->state() == HostState::closed);
    CHECK(pumps.load() > 0);
    CHECK((*host)->pumpCount() == pumps.load());
    // External owning snapshots do not depend on either joined worker.
    CHECK_FALSE(frame->snapshotView().objects.empty());
    REQUIRE((*host)->close());
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R0 close observation keeps first cause and bounded slots", "[rpa][r0][observation]") {
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    rpa::Close close{};
    rpa::firstCause(close, rpa::Reason::deliveryFailed, rpa::CloseStage::closeFlush,
                    "host.rpa.delivery");
    rpa::firstCause(close, rpa::Reason::operationFailed, rpa::CloseStage::finish,
                    "host.rpa.capacity");
    CHECK(close.errors == 2);
    CHECK(close.firstCause == rpa::Reason::deliveryFailed);
    CHECK(close.firstCauseStage == rpa::CloseStage::closeFlush);
    CHECK(std::string{close.firstErrorCode} == "host.rpa.delivery");
    for (int index = 0; index < 40; ++index)
        rpa::mark(close, rpa::CloseStage::closeEnter);
    CHECK(close.markCount == rpa::closeMarkCapacity);
    CHECK(close.marksLost == 8);
    CHECK(close.truncated);
    for (int index = 0; index < 10; ++index)
        rpa::addWait(close, rpa::CloseStage::exitWait, index, index == 9);
    CHECK(close.waitCount == rpa::closeWaitCapacity);
    CHECK(close.waitsLost == 2);
    CHECK(close.firstOverBudget == rpa::CloseStage::exitWait);

    rpa::SampleClaim claim{};
    CHECK(rpa::classifySample(claim) == rpa::SampleReject::identityMismatch);
    claim.identityMatched = true;
    CHECK(rpa::classifySample(claim) == rpa::SampleReject::noFormalWindow);
    claim.formalWindow = true;
    CHECK(rpa::classifySample(claim) == rpa::SampleReject::none);
    claim.instrumentedBuild = true;
    CHECK(rpa::classifySample(claim) == rpa::SampleReject::instrumentedBuild);
}

TEST_CASE("R0 close observation records a normal complete Close path",
          "[rpa][r0][observation][b5]") {
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto host = player::ExecutionHost::create(renderer, options(), ports(pumps));
    REQUIRE(host);
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    REQUIRE((*host)->load(std::move(*source)));
    REQUIRE((*host)->resize({320, 200}));
    auto frame = (*host)->sample(1000, {1, 1, 1}, false);
    REQUIRE(frame);
    REQUIRE((*host)->present(*frame));
    const auto context = (*host)->context();
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    const auto& observed = (*host)->rpaObservation();
    CHECK(observed.requestedUs > 0);
    CHECK(observed.deadlineUs >= observed.requestedUs);
    CHECK(observed.identity.valid);
    CHECK(observed.identity.operationId != 0);
    CHECK(observed.identity.decisionOrdinal != 0);
    CHECK(observed.identity.scope == context.sessionScope);
    CHECK(observed.identity.clockSegment == context.clockSegment);
    CHECK(observed.identity.runEpoch == context.runEpoch);
    CHECK(observed.identity.resourceGeneration == context.resourceGeneration);
    CHECK(observed.identity.rendererGeneration == context.rendererGeneration);
    CHECK(observed.identity.contentIdentity == context.contentIdentity.sha256);
    CHECK(observed.identity.status == static_cast<std::uint8_t>(ControlStatus::complete));
    // This fixture loads a presentation-only project, so no gameplay capture route exists and the
    // capture terminal stages are not applicable rather than missing evidence.
    CHECK(observed.closeTick == 0);
    CHECK(observed.captureTerminal == 0);
    CHECK_FALSE(observed.captureClosed);
    CHECK(observed.softwareTerminal == 2);
    CHECK(observed.softwareRetired);
    CHECK(observed.softwareClosed);
    CHECK(observed.evidenceDeliveries == 0);
    CHECK(observed.unackedDeliveries == observed.evidenceDeliveries);
    CHECK(observed.pumpSamples > 0);
    CHECK(observed.pumpClosed > 0);
    CHECK(observed.errors == 0);
    CHECK(observed.receiptStatus == static_cast<std::uint8_t>(ControlStatus::complete));
    CHECK(observed.finishCalled);
    CHECK(observed.confirmCalled);
    CHECK(observed.shutdownRequested);
    CHECK(observed.workerExited);
    CHECK(observed.workerJoined);
    CHECK(observed.closedFlag);
    CHECK(observed.facadeAfter == static_cast<std::uint8_t>(HostState::closed));
    CHECK(observed.firstCause == rpa::Reason::none);
    CHECK(observed.firstOverBudget == rpa::CloseStage::none);
    CHECK(observed.markCount > 10);
    CHECK(observed.markCount <= rpa::closeMarkCapacity);
    CHECK(observed.marks[0].stage == rpa::CloseStage::closeEnter);
    CHECK(observed.marks[observed.markCount - 1].stage == rpa::CloseStage::closed);
    CHECK(observed.closeRetireFence.permitAttempted);
    CHECK(observed.closeRetireFence.permitDirected);
    // A complete Close still takes the normal software fence; the F1 permit bypass is reserved for
    // suspended/failed cleanup. No existing fence was present, so only the existing-fence branch is
    // not applicable; the normal fence itself is applicable and succeeded.
    CHECK(observed.closeRetireFence.path == rpa::FencePath::normalControl);
    CHECK(observed.closeRetireFence.applicability == rpa::FenceApplicability::applicable);
    CHECK_FALSE(observed.closeRetireFence.existingFence);
    CHECK(observed.closeRetireFence.attempted);
    CHECK(observed.closeRetireFence.succeeded);
    CHECK(observed.closeRetireFence.retireCalled);
    CHECK(observed.closeRetireFence.retireApplied);
    CHECK(observed.closeRetireFence.referencesReleased);
    CHECK(observed.fence.closeAttempted);
    CHECK(observed.fence.closeApplied);
    CHECK(observed.fence.closeState == rpa::CloseStepState::applied);
    CHECK(observed.fence.identity.operationId == observed.identity.operationId);
    CHECK_FALSE(observed.fence.retireCalled);
    CHECK_FALSE(observed.cleanupFence.identity.valid);
}

TEST_CASE("R0 seek ledger keeps envelope, nesting and merge accounting",
          "[rpa][r0][observation][seek]") {
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    const auto index = [](rpa::SeekPhase phase) { return static_cast<std::size_t>(phase); };
    rpa::SeekLedger ledger{};
    ledger.timerCostNs = rpa::calibrateTimer();
    CHECK(ledger.timerCostNs < 1000000);
    // An envelope with no root phase cannot be attributed and must be visible as such.
    {
        rpa::SeekEnvelope envelope{&ledger};
        (void)envelope;
    }
    CHECK(ledger.seeks == 1);
    CHECK(ledger.seeksWithoutPhases == 1);
    CHECK(ledger.seekRootCount == 0);
    CHECK(ledger.unaccountedTotalUs == ledger.envelopeTotalUs);
    // A nested phase is attributed to its own slot but never counted as a root sample.
    {
        rpa::SeekEnvelope envelope{&ledger};
        rpa::SeekScope outer{&ledger, rpa::SeekPhase::s3Validation};
        {
            rpa::SeekScope inner{&ledger, rpa::SeekPhase::s1HostProtocol};
        }
    }
    CHECK(ledger.depth == 0);
    CHECK(ledger.seeks == 2);
    CHECK(ledger.seeksWithoutPhases == 1);
    CHECK(ledger.phases[index(rpa::SeekPhase::s3Validation)].count == 1);
    CHECK(ledger.phases[index(rpa::SeekPhase::s3Validation)].rootCount == 1);
    CHECK(ledger.phases[index(rpa::SeekPhase::s1HostProtocol)].count == 1);
    CHECK(ledger.phases[index(rpa::SeekPhase::s1HostProtocol)].rootCount == 0);
    CHECK(ledger.seekRootCount == 1);
    CHECK(ledger.seekRootUs <= ledger.envelopeTotalUs);
    CHECK(ledger.unaccountedTotalUs >= 0);
    // Worker samples merge into the host ledger without inventing envelopes.
    rpa::SeekLedger worker{};
    worker.phases[index(rpa::SeekPhase::s4InternalExecution)].count = 2;
    worker.phases[index(rpa::SeekPhase::s4InternalExecution)].totalUs = 30;
    worker.phases[index(rpa::SeekPhase::s4InternalExecution)].maxUs = 20;
    worker.seeksWithoutPhases = 1;
    worker.timerCostNs = 7;
    const auto beforeCount = ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].count;
    const auto beforeTotal = ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].totalUs;
    const auto beforeFetch = ledger.fetchCount;
    rpa::mergeSeek(ledger, worker);
    CHECK(ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].count == beforeCount + 2);
    CHECK(ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].totalUs >= beforeTotal + 30);
    CHECK(ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].maxUs >= 20);
    CHECK(ledger.seeksWithoutPhases == 2);
    CHECK(ledger.timerCostNs == 7);
    CHECK(ledger.seeks == 2);
    rpa::seekFetch(ledger, 5);
    CHECK(ledger.fetchCount == beforeFetch + 1);
    CHECK(ledger.fetchTotalUs >= 5);

    // Playback-side phases reach the owner ledger through the thread-scoped sink only.
    rpa::SeekLedger bound{};
    {
        rpa::SeekEnvelope envelope{&bound};
        rpa::SeekSinkScope bind{&bound};
        rpa::SeekScope outer{rpa::SeekPhase::s2CandidatePrep};
        rpa::SeekScope inner{rpa::SeekPhase::s3Validation};
        inner.finish();
        inner.finish(); // Idempotent: a finished scope never records twice.
    }
    CHECK(bound.seeks == 1);
    CHECK(bound.seeksWithoutPhases == 0);
    CHECK(bound.phases[index(rpa::SeekPhase::s2CandidatePrep)].count == 1);
    CHECK(bound.phases[index(rpa::SeekPhase::s2CandidatePrep)].rootCount == 1);
    CHECK(bound.phases[index(rpa::SeekPhase::s3Validation)].count == 1);
    CHECK(bound.phases[index(rpa::SeekPhase::s3Validation)].rootCount == 0);
    CHECK(bound.depth == 0);
    CHECK(rpa::seekSink == nullptr);

    // A recorded existing fence stays distinguishable from "not checked" on the permit path.
    rpa::Fence permit{};
    permit.path = rpa::FencePath::cleanupPermit;
    permit.applicability = rpa::FenceApplicability::notApplicable;
    rpa::Identity stale{};
    stale.scope = 7;
    stale.generation = 3;
    {
        rpa::FenceScope scope{&permit};
        rpa::noteExisting(stale);
    }
    CHECK(permit.attempted == false);
    CHECK(permit.applicability == rpa::FenceApplicability::notApplicable);
    CHECK(permit.existingFence);
    CHECK(permit.existingIdentity.scope == 7);
    CHECK(permit.existingIdentity.generation == 3);

    // The default record never claims a skipped fence was "not applicable"; only the owner sets
    // either verdict after deciding the path. An existing-fence reuse keeps the old identity and
    // still marks the normal fence as applicable even though it was not attempted.
    rpa::Fence unrecorded{};
    CHECK(unrecorded.applicability == rpa::FenceApplicability::missingEvidence);
    rpa::Fence reused{};
    reused.path = rpa::FencePath::existingFence;
    reused.applicability = rpa::FenceApplicability::applicable;
    {
        rpa::FenceScope scope{&reused};
        rpa::noteExisting(stale);
    }
    CHECK_FALSE(reused.attempted);
    CHECK(reused.applicability == rpa::FenceApplicability::applicable);
    CHECK(reused.existingFence);
    CHECK(reused.existingIdentity.scope == 7);
    CHECK(reused.existingIdentity.generation == 3);
}
#endif

TEST_CASE("B5 tail reply interruption is queried before main closes the new clock scope",
          "[b5][host][fault]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    std::atomic<bool> blockOwner{};
    std::promise<void> release;
    auto released = release.get_future().share();
    auto settings = options();
    settings.session.monotonicNow = [&] {
        if (blockOwner.load())
            released.wait();
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.reply"};
    auto wired = ports(pumps);
    bool interrupt{};
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (interrupt) {
            interrupt = false;
            release.set_value();
            return core::unexpected(
                core::Error{"probe.pump_interrupted", "Injected main wait interruption"});
        }
        return {};
    };
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto content = fixture.content();
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), content));
    const auto original = (*host)->context();
    ClockObservation eof{};
    eof.monotonicDomain = "b5.reply";
    eof.segment = original.clockSegment;
    eof.publicationSequence = 1;
    eof.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
    eof.validUntilMonotonicUs = eof.sampleMonotonicUs + 1000000;
    eof.source = ClockFramePosition{0, 1000000};
    eof.presented = eof.source;
    eof.quality = ClockQuality::Exact;
    eof.playback = ClockPlaybackState::EndOfStream;
    blockOwner.store(true);
    interrupt = true;
    auto tail = (*host)->beginTail(eof, {{1, 1}, 100000, 0, "explicit reply fixture", true});
    REQUIRE_FALSE(tail);
    CHECK(tail.error().code() == "probe.pump_interrupted");
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK((*host)->context().clockSegment == original.clockSegment + 1);
    CHECK((*host)->state() == HostState::closed);
}
TEST_CASE("B5 deadline retains original tail job until pumped Close completes",
          "[b5][host][fault][deadline]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, blockedCalls{};
    std::atomic<bool> blockOwner{};
    std::promise<void> release;
    auto released = release.get_future().share();
    auto settings = options();
    settings.replyDeadline = std::chrono::milliseconds{100};
    settings.session.monotonicNow = [&] {
        if (blockOwner.load()) {
            ++blockedCalls;
            // A failed assertion must not leave the owner join blocked indefinitely.
            released.wait();
        }
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.deadline"};
    bool didRelease{}, releaseOnPump{};
    auto wired = ports(pumps);
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (releaseOnPump && !didRelease) {
            didRelease = true;
            release.set_value();
        }
        return {};
    };
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    struct ReleaseOnExit {
        std::promise<void>& promise;
        bool& done;
        ~ReleaseOnExit() {
            if (!done) {
                done = true;
                promise.set_value();
            }
        }
    } releaseOnExit{release, didRelease};
    REQUIRE((*host)->load(test_support::presentation(), fixture.content()));
    const auto original = (*host)->context();
    ClockObservation eof{};
    eof.monotonicDomain = "b5.deadline";
    eof.segment = original.clockSegment;
    eof.publicationSequence = 1;
    eof.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
    eof.validUntilMonotonicUs = eof.sampleMonotonicUs + 1000000;
    eof.source = eof.presented = ClockFramePosition{0, 1000000};
    eof.quality = ClockQuality::Exact;
    eof.playback = ClockPlaybackState::EndOfStream;
    blockOwner.store(true);
    const auto beforeWait = std::chrono::steady_clock::now();
    auto tail = (*host)->beginTail(eof, {{1, 1}, 100000, 0, "explicit deadline fixture", true});
    REQUIRE_FALSE(tail);
    CHECK(tail.error().code() == "host.rpa.control_pending");
    CHECK(std::chrono::steady_clock::now() - beforeWait >= settings.replyDeadline);
    REQUIRE(blockedCalls.load() > 0);
    const auto beforeClose = pumps.load();
    auto pending = (*host)->close();
    REQUIRE_FALSE(pending);
    CHECK(pending.error().code() == "host.rpa.control_pending");
    CHECK((*host)->state() != HostState::closed);
    CHECK(pumps.load() > beforeClose);
    releaseOnPump = true;
    auto closed = (*host)->close();
    // An expired original transport deadline is not restarted by a new owner call. Continue
    // nonblocking cleanup under the same Close; this guard is a test deadlock limit, not a budget.
    const auto drainLimit = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!closed && (*host)->state() == HostState::closing &&
           std::chrono::steady_clock::now() < drainLimit) {
        (*host)->waitFor(std::chrono::milliseconds{1});
        closed = (*host)->close();
    }
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK(didRelease);
    // No second Tail request: cleanup collected its original reply exactly once.
    CHECK((*host)->context().clockSegment == original.clockSegment + 1);
    CHECK((*host)->state() == HostState::closed);
    REQUIRE((*host)->close());
}
TEST_CASE("B5 host rejects unaccepted budgets and closes CPU preparation failure", "[b5][host]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto unaccepted = options();
    unaccepted.session.testOnly = false;
    REQUIRE_FALSE(player::ExecutionHost::create(renderer, unaccepted, ports(pumps)));
    auto host = player::ExecutionHost::create(renderer, options(), ports(pumps));
    REQUIRE(host);
    auto loaded = (*host)->load(PlaybackSource{});
    REQUIRE_FALSE(loaded);
    REQUIRE((*host)->state() == HostState::suspended);
    REQUIRE((*host)->close());
    CHECK((*host)->state() == HostState::closed);
}

TEST_CASE("R1 C6 failed accepted Load cleans its own identity before independent Close",
          "[rpa][r1][c6][host]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto host = player::ExecutionHost::create(renderer, options(), ports(pumps));
    REQUIRE(host);
    // Fail renderer prepare after D2 Load acceptance and software prepare, before the software
    // fence. This is a deterministic test renderer fault, not evidence about a real device.
    renderer.markDeviceLost();
    auto loaded = (*host)->load(test_support::presentation());
    REQUIRE_FALSE(loaded);
    const std::string originalError{loaded.error().code()};
    CHECK(originalError == "host.rpa.context_stale");
    CHECK((*host)->state() == HostState::suspended);
    auto closed = (*host)->close();
    INFO((closed ? "" : std::string{closed.error().code()}));
    REQUIRE(closed);
    CHECK((*host)->state() == HostState::closed);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    const auto& seen = (*host)->rpaObservation();
    CHECK(seen.cleanupFence.path == obs::FencePath::cleanupPermit);
    CHECK(seen.cleanupFence.permitAcceptedControl);
    CHECK(seen.cleanupFence.retireApplied);
    CHECK(seen.cleanupFence.referencesReleased);
    CHECK_FALSE(seen.cleanupFence.existingFence);
    CHECK(seen.cleanupFence.identity.kind == static_cast<std::uint8_t>(ControlKind::load));
    CHECK(seen.cleanupFence.identity.operationId == 1);
    CHECK(seen.closeRetireFence.identity.operationId == 2);
    CHECK(seen.fence.identity.operationId == 2);
    CHECK(seen.cleanupFence.identity.decisionOrdinal < seen.fence.identity.decisionOrdinal);
    CHECK(seen.attempts == 1);
    CHECK(seen.reentries == 0);
    CHECK(seen.fence.closeState == obs::CloseStepState::applied);
    CHECK(seen.workerExited);
    CHECK(seen.workerJoined);
    CHECK(seen.closedFlag);
#endif
    CHECK(loaded.error().code() == originalError);
    REQUIRE((*host)->close());
}

TEST_CASE("B5 full capture queue preserves control cut space and owning fault evidence",
          "[b5][host][fault]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    settings.captureCapacity = 4;
    settings.captureReserve = 1;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.full"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture.content()));
    for (std::int64_t tick = 0; tick < 3; ++tick) {
        auto event = test_support::input();
        event.observationTick = {tick};
        event.sequence = static_cast<std::uint64_t>(tick + 1);
        REQUIRE((*host)->capture(event));
    }
    auto event = test_support::input();
    event.observationTick = {3};
    event.sequence = 4;
    REQUIRE_FALSE((*host)->capture(event));
    CHECK((*host)->state() == HostState::suspended);
    REQUIRE((*host)->close());
    REQUIRE((*host)->closeEvidence());
    CHECK((*host)->closeEvidence()->pending().size() == 4);
    REQUIRE((*host)->closeEvidence()->tentativeTickMembers().size() == 1);
    CHECK((*host)->closeEvidence()->tentativeTickMembers()[0].input.sequence == 4);
    CHECK((*host)->state() == HostState::closed);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    // The capture queue fault suspends the host before Close. Cleanup queries the suspended
    // coordinator but must not suspend it a second time; the directed-cleanup permit checks run,
    // yet the completed Close still enters the normal software fence rather than the permit
    // bypass. Four unacked deliveries become evidence, so none may remain unaccounted.
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    const auto& fullClose = (*host)->rpaObservation();
    CHECK(fullClose.attempts == 1);
    CHECK(fullClose.reentries == 0);
    CHECK(fullClose.firstCause == rpa::Reason::none);
    CHECK(fullClose.closeRetireFence.path == rpa::FencePath::normalControl);
    CHECK(fullClose.closeRetireFence.permitAttempted);
    CHECK(fullClose.closeRetireFence.permitAcceptedControl);
    CHECK(fullClose.closeRetireFence.permitDirected);
    CHECK_FALSE(fullClose.closeRetireFence.existingFence);
    CHECK(fullClose.closeRetireFence.succeeded);
    // cleanup() queries the coordinator, not the host facade. Depending on whether the failed
    // capture reached the coordinator's suspend, the original control is either complete or
    // suspended; only a non-terminal status may be suspended.
    const auto queryStatus = fullClose.cleanupQueryStatus;
    CHECK((queryStatus == static_cast<std::uint8_t>(ControlStatus::complete) ||
           queryStatus == static_cast<std::uint8_t>(ControlStatus::suspended)));
    CHECK(fullClose.cleanupSuspendCalled ==
          (queryStatus != static_cast<std::uint8_t>(ControlStatus::complete)));
    CHECK(fullClose.cleanupWaitSamples >= 1);
    CHECK(fullClose.unackedDeliveries == fullClose.evidenceDeliveries);
    CHECK(fullClose.captureTerminal == 2);
    CHECK(fullClose.closedFlag);
#endif
}

TEST_CASE("R0 close flush fault records delivery cause and still completes",
          "[rpa][r0][observation][b5]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.flush"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture.content()));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    rpa::testFlushFaultArmed.store(true);
#endif
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK((*host)->state() == HostState::closed);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    const auto& observed = (*host)->rpaObservation();
    CHECK(observed.firstCause == rpa::Reason::deliveryFailed);
    CHECK(observed.firstCauseStage == rpa::CloseStage::closeFlush);
    CHECK(std::string{observed.firstErrorCode} == "host.rpa.capacity_exceeded");
    CHECK(observed.coordinatorBeforeSuspend == static_cast<std::uint8_t>(HostState::closing));
    // HostCoordinator::suspend() marks a Close receipt suspended but keeps HostState::closing,
    // because only a non-Close control can return the coordinator to HostState::suspended. The
    // delivery failure is still recorded as the first cause and the Close still completes.
    CHECK(observed.coordinatorAfterSuspend == static_cast<std::uint8_t>(HostState::closing));
    CHECK(observed.attempts == 1);
    CHECK(observed.captureTerminal == 2);
    CHECK(observed.softwareTerminal == 2);
    CHECK(observed.errors == 1);
    CHECK(observed.unackedDeliveries == observed.evidenceDeliveries);
    CHECK(observed.closedFlag);
#else
    CHECK(true);
#endif
}

TEST_CASE("R1 close export failure preserves its cause while the original Close completes",
          "[rpa][r1][observation][b5]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.export"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture.content()));
#if !defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    // The fault hook is compiled out in the OFF build, so this case only checks that the same
    // public Close path still completes without observation code.
    CHECK((*host)->close());
#else
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    rpa::testExportFaultArmed.store(true);
    auto first = (*host)->close();
    REQUIRE_FALSE(first);
    CHECK(first.error().code() == "host.rpa.capacity_exceeded");
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK((*host)->rpaObservation().firstCause == rpa::Reason::operationFailed);
    CHECK((*host)->rpaObservation().firstCauseStage == rpa::CloseStage::exportEvidence);
    CHECK((*host)->rpaObservation().attempts == 1);
#endif
    auto second = (*host)->close();
    INFO((second
              ? ""
              : std::string{second.error().code()} + ": " + std::string{second.error().message()}));
    REQUIRE(second);
    CHECK((*host)->state() == HostState::closed);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    const auto& observed = (*host)->rpaObservation();
    CHECK(observed.attempts == 2);
    CHECK(observed.reentries == 1);
    CHECK(observed.firstCause == rpa::Reason::operationFailed);
    CHECK(observed.firstCauseStage == rpa::CloseStage::exportEvidence);
    CHECK(std::string{observed.firstErrorCode} == "host.rpa.capacity_exceeded");
    CHECK(observed.closedFlag);
#endif
#endif
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R1 Close resumes its original export phase after an allocation rejection",
          "[rpa][r1][continuation][host]") {
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "r1.export"};
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    test_support::rpa(*fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture->content()));
    obs::testExportFaultArmed.store(true);
    const auto first = (*host)->close();
    REQUIRE_FALSE(first);
    CHECK((*host)->state() == HostState::closing);
    const auto original = (*host)->rpaObservation().identity;
    const auto second = (*host)->close();
    INFO((second ? "" : std::string{second.error().code()}));
    REQUIRE(second);
    CHECK((*host)->state() == HostState::closed);
    CHECK((*host)->rpaObservation().identity.operationId == original.operationId);
    CHECK((*host)->rpaObservation().firstDecisionOrdinal == original.decisionOrdinal);
    CHECK(std::string{(*host)->rpaObservation().firstErrorCode} == first.error().code());
}
#endif
