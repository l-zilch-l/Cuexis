#include "rpa_b5_host_fixture.hpp"

// Audio endpoints, worker delivery failure and cut sequencing.

TEST_CASE("Finite audio endpoints bound extrapolation before explicit EOF tail", "[rpa][b5]") {
    auto clock = RealtimeClock::create({500000, 20000, 64, "fixture", true},
                                       {{0, 1}, {0, 1}, {0, 1}, {1, 1}}, "fixture", 1);
    REQUIRE(clock);
    ClockObservation sample{"fixture",
                            1,
                            1,
                            100,
                            500100,
                            0,
                            ClockFramePosition{9, 1000000},
                            ClockFramePosition{9, 1000000},
                            {},
                            ClockPlaybackState::Playing,
                            ClockQuality::Estimated,
                            false,
                            false,
                            false};
    sample.maximumPresented = ClockFramePosition{10, 1000000};
    auto ticks = clock->sample(sample, 200);
    REQUIRE(ticks);
    CHECK(ticks->horizon == 10);
    sample.publicationSequence = 2;
    sample.presented = ClockFramePosition{10, 1000000};
    sample.source = sample.presented;
    sample.playback = ClockPlaybackState::EndOfStream;
    auto tail = clock->startTail(sample, 201, 2, {{1, 1}, 15, 0, "fixture", true}, 10, 1);
    REQUIRE(tail);
    CHECK(tail->terminalHorizon == 16);
    auto invalid = RealtimeClock::create({500000, 20000, 64, "fixture", true},
                                         {{0, 1}, {0, 1}, {0, 1}, {1, 1}}, "fixture", 1);
    REQUIRE(invalid);
    sample.playback = ClockPlaybackState::Playing;
    sample.maximumPresented = ClockFramePosition{9, 1000000};
    REQUIRE_FALSE(invalid->sample(sample, 201));
}

TEST_CASE("R1 real worker delivery failure preserves the first Close error",
          "[rpa][r1][host][first-error]") {
    enum class FailureOrder { deliveryOnly, deliveryThenExport, cutThenDelivery };
    const auto order = GENERATE(FailureOrder::deliveryOnly, FailureOrder::deliveryThenExport,
                                FailureOrder::cutThenDelivery);
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    std::atomic<bool> armed{}, injected{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1000000}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "r1.first-error"};
    settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
        if (armed && std::holds_alternative<SessionCaptureDelivery>(command) &&
            !injected.exchange(true))
            throw std::bad_alloc{};
    };
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    test_support::rpa(*fixture, mapping);
    bool interrupted{};
    unsigned accepted{}, finished{};
    auto wired = ports(pumps);
    wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
        using Phase = player::ExecutionClosePhase;
        if (phase == Phase::postAcceptance)
            ++accepted;
        if (phase == Phase::finish)
            ++finished;
        if (phase == Phase::flush)
            armed = true;
        if (!interrupted && order == FailureOrder::cutThenDelivery && phase == Phase::cut) {
            interrupted = true;
            return core::unexpected(core::Error{"fixture.cut_failure", "Earlier cut failure"});
        }
        if (!interrupted && order == FailureOrder::deliveryThenExport &&
            phase == Phase::exportEvidence) {
            interrupted = true;
            return core::unexpected(core::Error{"fixture.export_failure", "Later export failure"});
        }
        return {};
    };
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture->content()));
    const auto original = (*host)->context();
    ClockObservation clock{};
    clock.monotonicDomain = "r1.first-error";
    clock.segment = original.clockSegment;
    clock.publicationSequence = 1;
    clock.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count();
    clock.validUntilMonotonicUs = clock.sampleMonotonicUs + 1000000;
    clock.playback = ClockPlaybackState::Playing;
    clock.quality = ClockQuality::Exact;
    clock.source = ClockFramePosition{1, 1};
    clock.presented = clock.source;
    REQUIRE((*host)->advance(clock));
    const auto first = (*host)->close();
    if (order == FailureOrder::deliveryOnly) {
        REQUIRE(first);
    } else {
        REQUIRE_FALSE(first);
        CHECK(first.error().code() == (order == FailureOrder::cutThenDelivery
                                           ? "fixture.cut_failure"
                                           : "fixture.export_failure"));
        CHECK((*host)->state() == HostState::closing);
        REQUIRE((*host)->closeFirstError());
        CHECK((*host)->closeFirstError()->code() == (order == FailureOrder::cutThenDelivery
                                                         ? "fixture.cut_failure"
                                                         : "host.rpa.capacity_exceeded"));
        REQUIRE((*host)->close());
    }
    REQUIRE(injected);
    CHECK((*host)->state() == HostState::closed);
    CHECK((*host)->closeOwnersExited());
    CHECK((*host)->context().controlSequence == original.controlSequence + 1);
    CHECK(accepted == 1);
    CHECK(finished == 1);
    REQUIRE((*host)->closeDeliveryError());
    CHECK((*host)->closeDeliveryError()->code() == "host.rpa.capacity_exceeded");
    REQUIRE((*host)->closeFirstError());
    CHECK((*host)->closeFirstError()->code() == (order == FailureOrder::cutThenDelivery
                                                     ? "fixture.cut_failure"
                                                     : (*host)->closeDeliveryError()->code()));
}

TEST_CASE("B5 realtime main host cuts after inspection and drives Pause Resume Seek Close",
          "[b5][host][realtime]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const auto canonical = GENERATE(false, true);
    settings.seekIdentity = canonical ? player::SeekIdentityPolicy::canonicalContent
                                      : player::SeekIdentityPolicy::owningSource;
    ClockMappingConfiguration mapping{{100000, 1}, {2, 1}, {9, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.host"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto wired = ports(pumps);
    player::ExecutionHost* live{};
    bool captureDuringInspect{};
    unsigned captures{};
    std::int64_t sourceH{1000};
    auto sourceAnchor = std::chrono::steady_clock::now();
    const auto sourceElapsed = [&] {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - sourceAnchor)
            .count();
    };
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (live && captureDuringInspect) {
            auto input = test_support::input();
            input.action = GameplayInputAction::Release;
            input.sequence = captures + 1;
            if (auto q = live->pausedCut())
                input.observationTick = {*q};
            else {
                sourceH += 1000;
                input.observationTick = {sourceH + sourceElapsed() - 2};
            }
            auto captured = live->capture(std::move(input));
            if (!captured)
                return captured;
            ++captures;
        }
        return {};
    };
    const auto capturePump = wired.pumpMain;
    unsigned capturesAtClockRead{};
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    live = host->get();
    auto loaded = live->load(test_support::presentation(), fixture.content());
    INFO((loaded ? "" : std::string{loaded.error().code()}));
    REQUIRE(loaded);
    std::uint64_t publication{};
    player::ExecutionHost::ClockSource source =
        [&](const HostContext& c) -> core::Result<ClockObservation> {
        // This callback is after inspection and before the control cut. A completed owner
        // reply need not enter the wait/pump loop, so inject here independently of scheduling.
        if (captureDuringInspect) {
            if (auto pumped = capturePump(); !pumped)
                return core::unexpected(std::move(pumped.error()));
            ++capturesAtClockRead;
        }
        const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
        ClockObservation o{};
        o.monotonicDomain = "b5.host";
        o.segment = c.clockSegment;
        o.publicationSequence = ++publication;
        o.sampleMonotonicUs = now;
        o.validUntilMonotonicUs = now + 1000000;
        o.presented = ClockFramePosition{sourceH + sourceElapsed() + 100000, 1000000};
        o.playback = ClockPlaybackState::Playing;
        o.quality = ClockQuality::Exact;
        return o;
    };
    captureDuringInspect = true;
    auto paused = live->pause(source);
    INFO((paused
              ? ""
              : std::string{paused.error().code()} + ": " + std::string{paused.error().message()}));
    REQUIRE(paused);
    REQUIRE(live->state() == HostState::paused);
    REQUIRE(live->pausedCut());
    CHECK(captures > 0);
    CHECK(capturesAtClockRead == 1);
    CHECK(*live->pausedCut() >= sourceH - 2);
    captureDuringInspect = false;
    sourceH = *live->pausedCut() + 10000;
    auto resumed = live->resume(source);
    INFO((resumed ? "" : std::string{resumed.error().code()}));
    REQUIRE(resumed);
    CHECK(live->state() == HostState::running);
    auto pausedAgain = live->pause(source);
    INFO((pausedAgain ? "" : std::string{pausedAgain.error().code()}));
    REQUIRE(pausedAgain);
    auto archive = live->archive();
    REQUIRE(archive);
    auto cut = live->replayCut(*archive, {4});
    REQUIRE(cut);
    sourceH = 4;
    sourceAnchor = std::chrono::steady_clock::now();
    auto sought = live->seek(*archive, *cut, {4}, source);
    INFO((sought
              ? ""
              : std::string{sought.error().code()} + ": " + std::string{sought.error().message()}));
    REQUIRE(sought);
    CHECK(live->state() == HostState::running);
    auto frame = live->sample(13, {0.013, 0, live->context().clockSegment}, false);
    INFO((frame ? "" : std::string{frame.error().code()}));
    REQUIRE(frame);
    REQUIRE(live->present(*frame));
    auto closed = live->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK(live->state() == HostState::closed);
    REQUIRE(live->closeEvidence());
    CHECK(live->closeEvidence()->pending().empty());
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    const auto& closedObservation = live->rpaObservation();
    CHECK(closedObservation.captureTerminal == 2);
    CHECK(closedObservation.captureClosed);
    CHECK(closedObservation.softwareTerminal == 2);
    CHECK(closedObservation.softwareClosed);
    CHECK(closedObservation.identity.status == static_cast<std::uint8_t>(ControlStatus::complete));
    CHECK(closedObservation.unackedDeliveries == closedObservation.evidenceDeliveries);
    // This Close is called once, with no pending job and no failure. The Seek completed and
    // released its own fence, so Close takes a fresh normal control fence rather than reusing an
    // existing one. The existing-fence branch is separately mapped on the cleanup-permit path.
    CHECK(closedObservation.attempts == 1);
    CHECK(closedObservation.reentries == 0);
    CHECK(closedObservation.firstDecisionOrdinal > 0);
    CHECK(closedObservation.pendingJobKind == 0);
    CHECK(closedObservation.pendingSessionJobAtEntry == 0);
    CHECK(closedObservation.pendingAudioJobAtEntry == 0);
    CHECK(closedObservation.firstCause == rpa::Reason::none);
    CHECK(closedObservation.closeRetireFence.path == rpa::FencePath::normalControl);
    CHECK(closedObservation.closeRetireFence.applicability == rpa::FenceApplicability::applicable);
    CHECK(closedObservation.closeRetireFence.attempted);
    CHECK_FALSE(closedObservation.closeRetireFence.existingFence);
    CHECK(closedObservation.closeRetireFence.succeeded);
    CHECK(closedObservation.closedFlag);
    const auto phaseCount = [&](rpa::SeekPhase phase) {
        return live->rpaSeekObservation().phases[static_cast<std::size_t>(phase)].count;
    };
    const auto& seekLedger = live->rpaSeekObservation();
    CHECK(seekLedger.seeks >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s0HostIdentity) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s1HostProtocol) >= 2);
    // S2 is measured inside Playback through the owner thread's seek sink; S3 wraps the
    // candidate validation and records the work the kernel actually performs.
    CHECK(phaseCount(rpa::SeekPhase::s2CandidatePrep) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s3Validation) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s4InternalExecution) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s5PublishSwap) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s6ClockPresent) >= 1);
    const auto& work = seekLedger.work;
    const auto& identityWork = work[static_cast<std::size_t>(rpa::SeekPhase::s0HostIdentity)];
    if (canonical)
        CHECK(identityWork.archiveBytes > 0);
    else
        CHECK(identityWork.archiveBytes == 0);
    CHECK(identityWork.encodeCalls == (canonical ? 1 : 0));
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s0HostIdentity)].sealedIdentityKnown == 1);
    CHECK(identityWork.sealedIdentityPresent == (canonical ? 1 : 0));
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s1HostProtocol)].operationId > 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s1HostProtocol)].participants >= 3);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].validCandidates == 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].entities >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].requirements >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].preparedHandles == 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].configCopies == 1);
    // Canonical encoding and the Seek candidate both reuse their immutable preparation.
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].kernelPrepareCalls == 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].derivedIndexBuilds == 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)].actualRecords >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)].batchInputs >= 1);
    // This fixture's cut contains admission batches but no recorded result rows, so the archive
    // has nothing for sameKernelResult to compare; zero is the measured value, not lost evidence.
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)].sameKernelCalls == 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)].nestedResultElements >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)].fullTicks >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)].matcherSteps >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)].runtimePeakElements >=
          1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)].recordPeakElements >=
          1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s5PublishSwap)].objects >= 2);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s5PublishSwap)].releases >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s5PublishSwap)].oldOwnersRetained == 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s6ClockPresent)].clockValid == 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s6ClockPresent)].targetGeneration > 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s6ClockPresent)].targetPresentationTick >
          0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s6ClockPresent)].endToEndUs > 0);
    CHECK(seekLedger.envelopeTotalUs > 0);
    CHECK(seekLedger.fetchCount >= 2);
    CHECK(seekLedger.timerCostNs > 0);
    CHECK(seekLedger.unaccountedMaxUs < seekLedger.envelopeMaxUs);
    CHECK(seekLedger.seeksWithoutPhases == 0);
#endif
}

TEST_CASE("B5 headless EOF tail preserves owners through Pause Resume and terminal execution",
          "[b5][host][tail]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.tail"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    auto content = GameplayContent::fromPacked(fixture.packed(), fixture.config);
    REQUIRE(content);
    REQUIRE((*host)->load(test_support::presentation(), *content));
    const auto before = (*host)->context();
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    ClockObservation eof{};
    eof.monotonicDomain = "b5.tail";
    eof.segment = before.clockSegment;
    eof.publicationSequence = 1;
    eof.sampleMonotonicUs = now;
    eof.validUntilMonotonicUs = now + 1000000;
    eof.source = ClockFramePosition{0, 1000000};
    eof.presented = eof.source;
    eof.playback = ClockPlaybackState::EndOfStream;
    eof.quality = ClockQuality::Exact;
    auto tail = (*host)->beginTail(eof, {{1, 1}, 100000, 0, "explicit CPU fixture", true});
    INFO((tail ? ""
               : std::string{tail.error().code()} + ": " + std::string{tail.error().message()}));
    REQUIRE(tail);
    CHECK((*host)->context().sessionScope == before.sessionScope);
    CHECK((*host)->context().runEpoch == before.runEpoch);
    CHECK((*host)->context().clockSegment == before.clockSegment + 1);
    auto advanced = (*host)->advance(eof);
    INFO((advanced ? "" : std::string{advanced.error().code()}));
    REQUIRE(advanced);
    player::ExecutionHost::ClockSource source =
        [&](const HostContext&) -> core::Result<ClockObservation> { return eof; };
    auto paused = (*host)->pause(source);
    INFO((paused
              ? ""
              : std::string{paused.error().code()} + ": " + std::string{paused.error().message()}));
    REQUIRE(paused);
    const auto frozen = (*host)->inputTicks(eof);
    REQUIRE(frozen);
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    REQUIRE((*host)->inputTicks(eof)->horizon == frozen->horizon);
    auto resumed = (*host)->resume(source);
    INFO((resumed ? ""
                  : std::string{resumed.error().code()} + ": " +
                        std::string{resumed.error().message()}));
    REQUIRE(resumed);
    bool complete{};
    GameplayResult terminalResult;
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (!complete && std::chrono::steady_clock::now() < limit) {
        const auto requestStart = std::chrono::steady_clock::now();
        auto result = (*host)->advance(eof);
        if (!result && result.error().code() == "host.rpa.control_pending")
            continue;
        INFO((result ? ""
                     : std::string{result.error().code()} + ": " +
                           std::string{result.error().message()}));
        REQUIRE(result);
        const auto requestUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now() - requestStart)
                                   .count();
        auto lag = (*host)->lag(eof);
        REQUIRE(lag);
        std::cout << "{\"event\":\"b5.cpu.owner_sample\",\"test_only\":true,\"fixture\":\"single_"
                     "requirement_tail\",\"request_us\":"
                  << requestUs << ",\"clock_h\":" << lag->clockHorizon.value
                  << ",\"h_lag\":" << (lag->horizonLag ? std::to_string(*lag->horizonLag) : "null")
                  << ",\"frontier_lag\":"
                  << (lag->frontierLag ? std::to_string(*lag->frontierLag) : "null")
                  << ",\"sealed_cursor\":" << lag->sealedFactCursor
                  << ",\"folded_cursor\":" << lag->foldedFactCursor << "}\n";
        complete = result->disposition == GameplayAdvanceDisposition::Executed &&
                   result->request.horizon.value == (*host)->tail()->terminalHorizon;
        if (complete)
            terminalResult = result->result;
    }
    REQUIRE(complete);
    auto replay = (*host)->archive();
    REQUIRE(replay);
    auto evaluated = (*host)->evaluateReplay(*replay);
    REQUIRE(evaluated);
    REQUIRE(evaluated->evidenceValid);
    auto equal = terminalResult.sameResult(evaluated->result);
    REQUIRE(equal);
    CHECK(*equal);
    auto frame = (*host)->sample(
        tail->terminalHorizon,
        {static_cast<double>(tail->terminalHorizon) / 1000.0, 0, (*host)->context().clockSegment},
        false);
    REQUIRE(frame);
    REQUIRE((*host)->present(*frame));
    const auto closeStart = std::chrono::steady_clock::now();
    REQUIRE((*host)->close());
    std::cout << "{\"event\":\"b5.cpu.close\",\"test_only\":true,\"close_join_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::steady_clock::now() - closeStart)
                     .count()
              << ",\"session_capacity\":" << settings.session.capacity
              << ",\"session_control_reserve\":" << settings.session.controlReserve
              << ",\"capture_capacity\":" << settings.captureCapacity << "}\n";
    REQUIRE(replay->toBytes());
    CHECK_FALSE(frame->snapshotView().objects.empty());
}
