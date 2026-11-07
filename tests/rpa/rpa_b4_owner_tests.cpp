#include "rpa_b4_fixture.hpp"

// Owner fencing, cancellation, capture cut and feedback cases.

TEST_CASE("B4 commit fence rechecks generation and blocks mutation through device completion",
          "[rpa][b4][fence]") {
    PlaybackSession s;
    presentation_renderer::TestPresentationRenderer renderer;
    auto p = preparedProject(s);
    auto input = HostCoordinationAccess::materialize(p, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    auto fence = HostCoordinationAccess::enterFence(s, *input);
    REQUIRE(fence);
    REQUIRE_FALSE(s.update({0, 0, 0}));
    REQUIRE_FALSE(s.unload());
    REQUIRE_FALSE(s.commit(std::move(p)));
    auto request = control(ControlKind::load);
    request.context = input->context;
    request.target.context = request.context;
    request.target.candidateId = input->candidateId;
    request.target.expectedGeneration = fence->generation();
    request.participants = {false, true, false, true};
    auto proof = ControlReceipt{};
    proof.request = request;
    proof.status = ControlStatus::preparing;
    for (auto participant : {Participant::software, Participant::renderer})
        for (auto phase : {StepPhase::prepare, StepPhase::fence})
            proof.steps[static_cast<unsigned>(participant)][static_cast<unsigned>(phase)] =
                applied(request, participant, phase);
    REQUIRE(HostCoordinationAccess::commit(s, std::move(p), *fence, proof));
    REQUIRE_FALSE(s.update({0, 0, 0}));
    REQUIRE_FALSE(HostCoordinationAccess::leaveFence(s, *fence, proof));
    proof.status = ControlStatus::complete;
    proof.softwareApplied = true;
    proof.cleanupComplete = true;
    proof.steps[1][static_cast<unsigned>(StepPhase::softwareCommit)] =
        applied(request, Participant::software, StepPhase::softwareCommit);
    proof.steps[3][static_cast<unsigned>(StepPhase::activate)] =
        applied(request, Participant::renderer, StepPhase::activate);
    auto wrong = ExecutionFence{};
    REQUIRE_FALSE(HostCoordinationAccess::leaveFence(s, wrong, proof));
    REQUIRE(HostCoordinationAccess::leaveFence(s, *fence, proof));
    REQUIRE(s.update({0, 0, 0}));
    REQUIRE(s.unload());
}
TEST_CASE("B4 prepared during active mutation becomes stale and can be rebuilt",
          "[rpa][b4][fence]") {
    PlaybackSession s;
    auto p = preparedProject(s);
    REQUIRE(s.commit(std::move(p)));
    presentation_renderer::TestPresentationRenderer renderer;
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    auto replacement = s.prepareReload(std::move(*source), {0, 0, 0}, ReloadPolicy::KeepChartTime);
    REQUIRE(replacement);
    auto input = HostCoordinationAccess::materialize(*replacement, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    REQUIRE(s.update({1, 1, 0}));
    REQUIRE_FALSE(HostCoordinationAccess::enterFence(s, *input));
}
TEST_CASE("B4 SoftwareApplied survives missing device receipts and retry", "[rpa][b4][control]") {
    auto c = context();
    c.controlSequence = 0;
    auto host = HostCoordinator::create(c, 8);
    REQUIRE(host);
    auto request = control();
    REQUIRE(host->accept(request, 1));
    REQUIRE(host->observe(applied(request, Participant::software, StepPhase::softwareCommit)));
    REQUIRE(host->markSoftwareApplied());
    auto lost = applied(request, Participant::audio, StepPhase::activate);
    lost.status = StepStatus::unknown;
    REQUIRE(host->observe(lost));
    CHECK(host->state() == HostState::suspended);
    CHECK(host->query(1)->softwareApplied);
    REQUIRE_FALSE(host->finish());
    auto busy = request;
    busy.operationId = 2;
    ++busy.context.controlSequence;
    busy.target.context = busy.context;
    busy.commandOrdinal = 1;
    busy.target.commandOrdinal = 1;
    CHECK(host->accept(busy, 2)->status == ControlStatus::busyNotAccepted);
    REQUIRE(host->observe(applied(request, Participant::audio, StepPhase::activate)));
    REQUIRE(host->observe(applied(request, Participant::capture, StepPhase::resumeRoute)));
    REQUIRE(host->observe(applied(request, Participant::renderer, StepPhase::activate)));
    REQUIRE(host->finish());
    REQUIRE(host->acknowledge(1));
    CHECK(host->accept(request, 9)->status == ControlStatus::retired);
}
TEST_CASE("B4 A1 cleanup never restores direct Resume permission", "[rpa][b4][control]") {
    auto c = context();
    c.controlSequence = 0;
    auto host = HostCoordinator::create(c, 8);
    REQUIRE(host);
    auto request = control();
    REQUIRE(host->accept(request, 1));
    REQUIRE_FALSE(host->abortResume());
    REQUIRE(host->observe(applied(request, Participant::capture, StepPhase::resumeRoute)));
    REQUIRE(host->abortResume());
    CHECK_FALSE(host->query(1)->directResumeAllowed);
    REQUIRE_FALSE(host->markSoftwareApplied());
    REQUIRE_FALSE(host->completeCleanup());
    REQUIRE(host->observe(applied(request, Participant::audio, StepPhase::stop)));
    auto isolation = applied(request, Participant::audio, StepPhase::isolate);
    isolation.effects.callbackIsolated = true;
    REQUIRE(host->observe(isolation));
    auto retired = applied(request, Participant::renderer, StepPhase::retire);
    retired.effects.referencesReleased = true;
    REQUIRE(host->observe(retired));
    REQUIRE(host->completeCleanup());
    CHECK(host->state() == HostState::suspended);
    auto another = request;
    another.operationId = 2;
    another.commandOrdinal = 1;
    ++another.context.controlSequence;
    another.target.context = another.context;
    another.target.commandOrdinal = 1;
    REQUIRE_FALSE(host->accept(another, 2));
    REQUIRE(host->acknowledge(1));
}
TEST_CASE("B4 Close stays directed until audio isolation and GPU retirement", "[rpa][b4][close]") {
    auto c = context();
    c.controlSequence = 0;
    auto host = HostCoordinator::create(c, 8);
    REQUIRE(host);
    auto request = control(ControlKind::close);
    REQUIRE(host->accept(request, 1));
    REQUIRE_FALSE(host->closeReady(StepPhase::stop));
    REQUIRE_FALSE(host->finish());
    REQUIRE(host->observe(applied(request, Participant::capture, StepPhase::cut)));
    CHECK(host->closeReady(StepPhase::stop));
    REQUIRE(host->observe(applied(request, Participant::audio, StepPhase::stop)));
    CHECK(host->closeReady(StepPhase::isolate));
    auto isolation = applied(request, Participant::audio, StepPhase::isolate);
    REQUIRE(host->observe(isolation));
    CHECK_FALSE(host->closeReady(StepPhase::retire));
    // A terminal non-isolation outcome cannot be rewritten into isolation evidence.
    auto blocked = host->finish();
    REQUIRE_FALSE(blocked);
    CHECK(host->state() == HostState::closing);
}
TEST_CASE("B4 capture cut is terminal and A1 keeps new epoch inputs unadmitted",
          "[rpa][b4][capture]") {
    auto capture = CaptureRouteOwner::create({1, 1, 1}, 16, 2);
    REQUIRE(capture);
    REQUIRE(capture->capture(event(5)));
    REQUIRE(capture->capture(event(5, GameplayInputAction::Release)));
    auto cut = step(Participant::capture, StepPhase::cut);
    cut.target.tick = 5;
    cut.target.pauseEpoch = 1;
    auto appliedCut = capture->cut(cut);
    REQUIRE(appliedCut);
    CHECK(appliedCut->effects.cut == 2);
    auto messages = capture->pending();
    REQUIRE(messages);
    REQUIRE(messages->size() == 2);
    CHECK(std::get<GameplayClosedBatch>((*messages)[0].payload).epochTerminated);
    CHECK(std::get<GameplayClosedBatch>((*messages)[0].payload).members[1].sourceLocalPosition ==
          1);
    REQUIRE(capture->cut(cut));
    REQUIRE(capture->acknowledge({1, 1, 1}, 1));
    REQUIRE(capture->capture(event(6)));
    auto route = step(Participant::capture, StepPhase::resumeRoute);
    route.key.operationId = 2;
    route.target.nextContext = context();
    route.target.nextContext->clockSegment = 2;
    route.target.nextContext->runEpoch = 2;
    REQUIRE(capture->resumeRoute(route, {1, 2, 2}));
    auto trace = capture->trace(0);
    REQUIRE(trace);
    CHECK(trace->finalTraceCut == 1);
    REQUIRE(capture->capture(event(7)));
    REQUIRE_FALSE(capture->prefix({8}));
    auto retained = capture->retained();
    REQUIRE(retained);
    REQUIRE(retained->size() == 1);
    CHECK(retained->front().input.observationTick.value == 7);
    REQUIRE_FALSE(capture->acknowledge({1, 1, 1}, 0));
    capture->abortResume();
    REQUIRE_FALSE(capture->enableOrdinary(ControlReceipt{}));
    REQUIRE_FALSE(capture->capture(event(8)));
    CHECK(capture->steps().query(route.key)->status == StepStatus::applied);
}
TEST_CASE("Frame feedback permits a forward EOF clock segment without a control command",
          "[rpa][b4][present]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 8, 8);
    REQUIRE(ledger);
    auto next = context();
    ++next.clockSegment;
    auto changedContent = next;
    ++changedContent.runEpoch;
    REQUIRE_FALSE(ledger->activate(changedContent, 2));
    REQUIRE(ledger->activate(next, 2));
    REQUIRE_FALSE(ledger->activate(context(), 3));
    REQUIRE_FALSE(ledger->activate(next, 3));
    FrameEnvelope frame;
    frame.context = next;
    frame.projectionScope = 2;
    frame.frameId = 1;
    REQUIRE(ledger->publish(frame));
}
TEST_CASE("B4 feedback separates expiry Unknown and late old scope evidence",
          "[rpa][b4][present]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 8, 8);
    REQUIRE(ledger);
    FeedbackId id{1, 1, 0, 0, "target", "binding"};
    REQUIRE(ledger->feedback(id, 0, 10));
    FrameEnvelope frame;
    frame.context = context();
    frame.frameId = 1;
    frame.projectionScope = 1;
    frame.presentationTick = 1;
    frame.membership = {id};
    REQUIRE(ledger->publish(frame));
    auto delivered = ledger->takeLatest();
    REQUIRE(delivered);
    REQUIRE(*delivered);
    REQUIRE(ledger->expire(10));
    CHECK(ledger->query(id)->status == FeedbackStatus::unknownEvidence);
    CHECK(ledger->unknownCount() == 1);
    auto next = context();
    ++next.controlSequence;
    REQUIRE(ledger->activate(next, 2));
    REQUIRE(ledger->observe({context(), 1, 1, PresentStatus::succeeded, 12, {}}));
    CHECK(ledger->query(id)->status == FeedbackStatus::unknownEvidence);
    CHECK(ledger->query(id)->laterConfirmed);
    REQUIRE(ledger->observe({context(), 1, 1, PresentStatus::succeeded, 12, {}}));
    FeedbackId other{2, 2, 0, 0, "target", "binding"};
    REQUIRE(ledger->feedback(other, 0, 10));
    REQUIRE(ledger->expire(10));
    CHECK(ledger->query(other)->status == FeedbackStatus::expiredUnconfirmed);
}

TEST_CASE("B4 actual audio calls retain leases until explicit isolation and retirement",
          "[rpa][b4][audio]") {
    audio::AudioClipStore store;
    auto clip = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
    REQUIRE(clip);
    auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle);
    test_support::FakeAudioTransport transport{store};
    bool isolated = false;
    auto owner =
        AudioOwner::create(1, transport, [&]() -> core::Result<bool> { return isolated; }, 24, 4);
    REQUIRE(owner);
    auto request = step(Participant::audio, StepPhase::prepare);
    request.target.expectedGeneration = 0;
    request.target.audioPositionMs = 10;
    request.target.audioClip = *handle;
    auto lease = store.lease(*handle);
    REQUIRE(lease);
    REQUIRE(owner->prepare(request, std::move(*lease)));
    request.key.phase = StepPhase::activate;
    auto activated = owner->activate(request);
    REQUIRE(activated);
    REQUIRE(activated->status == StepStatus::applied);
    CHECK(activated->effects.position == 10000);
    CHECK(activated->effects.generation == 1);
    CHECK(transport.snapshot().source.state == audio::PlaybackState::Playing);
    const auto services = transport.metrics().serviceCount;
    REQUIRE(owner->activate(request));
    CHECK(transport.metrics().serviceCount == services);
    request.key.phase = StepPhase::retire;
    CHECK(owner->retire(request)->status == StepStatus::rejectedNotApplied);
    request.key.phase = StepPhase::fence;
    REQUIRE(owner->pause(request));
    CHECK(transport.snapshot().source.state == audio::PlaybackState::Paused);
    request.key.phase = StepPhase::stop;
    REQUIRE(owner->stop(request));
    CHECK_FALSE(owner->steps().query(request.key)->effects.callbackIsolated);
    request.key.phase = StepPhase::isolate;
    CHECK(owner->isolate(request)->status == StepStatus::inProgress);
    CHECK(owner->cancel(request)->status == StepStatus::inProgress);
    isolated = true;
    CHECK(owner->isolate(request)->effects.callbackIsolated);
    auto cleanup = request;
    cleanup.key.operationId = 2;
    cleanup.target.expectedGeneration = 1;
    cleanup.target.context.controlSequence = 2;
    cleanup.key.phase = StepPhase::retire;
    auto retired = owner->retire(cleanup);
    REQUIRE(retired);
    REQUIRE(retired->status == StepStatus::applied);
    CHECK(retired->effects.referencesReleased);
    CHECK(transport.snapshot().source.state == audio::PlaybackState::Empty);
    cleanup.key.phase = StepPhase::close;
    REQUIRE(owner->close(cleanup));
}
TEST_CASE("B4 audio partial device failure is queryable without repeating playback",
          "[rpa][b4][audio]") {
    audio::AudioClipStore store;
    auto clip = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
    REQUIRE(clip);
    auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle);
    test_support::FakeAudioTransport transport{store};
    auto owner =
        AudioOwner::create(1, transport, []() -> core::Result<bool> { return true; }, 24, 4);
    REQUIRE(owner);
    auto request = step(Participant::audio, StepPhase::prepare);
    request.target.expectedGeneration = 0;
    request.target.audioPositionMs = 10;
    request.target.audioClip = *handle;
    auto lease = store.lease(*handle);
    REQUIRE(lease);
    REQUIRE(owner->prepare(request, std::move(*lease)));
    transport.failNextService();
    request.key.phase = StepPhase::activate;
    auto failed = owner->activate(request);
    REQUIRE(failed);
    REQUIRE(failed->status == StepStatus::failed);
    CHECK(failed->effects.generation == 1);
    CHECK(failed->effects.position == 10000);
    CHECK_FALSE(failed->effects.referencesReleased);
    auto count = transport.metrics().serviceCount;
    REQUIRE(owner->activate(request));
    CHECK(transport.metrics().serviceCount == count);
    CHECK(owner->cancel(request)->status == StepStatus::failed);
}
TEST_CASE("B4 completion faults and retired replays preserve terminal identity",
          "[rpa][b4][owner]") {
    auto ledger = OwnerStepLedger::create(1, Participant::audio, 8, 2);
    REQUIRE(ledger);
    auto request = step(Participant::audio, StepPhase::activate);
    REQUIRE(*ledger->start(request));
    auto receipt = StepReceipt{request, StepStatus::applied, {}, {}};
    for (int attempt = 0; attempt < 2; ++attempt) {
        ledger->injectReservationFailureForTest();
        auto failed = ledger->complete(receipt);
        REQUIRE_FALSE(failed);
        CHECK(failed.error().code() == "host.rpa.capacity_exceeded");
    }
    REQUIRE(ledger->complete(receipt));
    REQUIRE(ledger->acknowledge(request.key));
    REQUIRE(ledger->complete(receipt));
    CHECK(ledger->query(request.key)->status == StepStatus::retired);
}
