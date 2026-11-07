#include "rpa_b4_fixture.hpp"

// Directed teardown, renderer steps and joint owner cases.

TEST_CASE("B4 Close completes only after every directed owner teardown", "[rpa][b4][close]") {
    auto initial = context();
    initial.controlSequence = 0;
    auto host = HostCoordinator::create(initial, 2);
    REQUIRE(host);
    auto request = control(ControlKind::close);
    REQUIRE(host->accept(request, 0));
    REQUIRE(host->observe(applied(request, Participant::capture, StepPhase::cut)));
    REQUIRE(host->observe(applied(request, Participant::audio, StepPhase::stop)));
    auto isolation = applied(request, Participant::audio, StepPhase::isolate);
    isolation.status = StepStatus::unknown;
    REQUIRE(host->observe(isolation));
    CHECK(host->state() == HostState::closing);
    REQUIRE_FALSE(host->finish());
    isolation.status = StepStatus::applied;
    isolation.effects.callbackIsolated = true;
    REQUIRE(host->observe(isolation));
    CHECK(host->closeReady(StepPhase::retire));
    for (auto p : {Participant::software, Participant::renderer}) {
        auto retired = applied(request, p, StepPhase::retire);
        retired.effects.referencesReleased = true;
        REQUIRE(host->observe(retired));
    }
    CHECK(host->closeReady(StepPhase::close));
    for (auto p :
         {Participant::capture, Participant::software, Participant::audio, Participant::renderer})
        REQUIRE(host->observe(applied(request, p, StepPhase::close)));
    REQUIRE(host->finish());
    CHECK(host->state() == HostState::closed);
    CHECK(controlFullyApplied(*host->query(1)));
}
TEST_CASE("B4 owning frames call actual renderer without advancing Playback",
          "[rpa][b4][present]") {
    PlaybackSession session;
    presentation_renderer::TestPresentationRenderer renderer;
    auto prepared = preparedProject(session);
    auto gpu = renderer.prepare(prepared);
    REQUIRE(gpu);
    REQUIRE(session.commit(std::move(prepared)));
    renderer.activate(std::move(*gpu));
    REQUIRE(session.update({0, 0, 0}));
    auto snapshot = session.extractFrame({640, 480});
    REQUIRE(snapshot);
    auto generation = HostCoordinationAccess::generation(session);
    REQUIRE(generation);
    FrameEnvelope frame;
    frame.context = context();
    frame.context.rendererGeneration = renderer.identity().generation;
    frame.frameId = 1;
    frame.projectionScope = 1;
    frame.snapshot = *snapshot;
    auto presented = presentOwnedFrame(frame, frame.context, 1, renderer.identity().instance,
                                       renderer, [] { return 42; });
    REQUIRE(presented);
    CHECK(presented->status == PresentStatus::succeeded);
    CHECK(presented->callMonotonicUs == 42);
    CHECK(*HostCoordinationAccess::generation(session) == *generation);
    renderer.failNextPresent(presentation_renderer::PresentationFailureClass::Recoverable);
    auto failed = presentOwnedFrame(frame, frame.context, 1, renderer.identity().instance, renderer,
                                    [] { return 43; });
    REQUIRE(failed);
    CHECK(failed->status == PresentStatus::failed);
    REQUIRE(failed->cause);
    CHECK(*HostCoordinationAccess::generation(session) == *generation);
    auto next = frame.context;
    ++next.controlSequence;
    REQUIRE_FALSE(presentOwnedFrame(frame, next, 1, renderer.identity().instance, renderer,
                                    [] { return 44; }));
    REQUIRE(renderer.close());
    REQUIRE(session.unload());
    CHECK_FALSE(frame.snapshot.objects.empty());
}
TEST_CASE("B4 presentation scope ABA cannot resurrect canceled feedback", "[rpa][b4][present]") {
    auto ledger = FrameObservationLedger::create(context(), 1, 2, 2);
    REQUIRE(ledger);
    FeedbackId id{1, 1, 0, 0, "target", "binding"};
    REQUIRE(ledger->feedback(id, 0, 10));
    auto next = context();
    ++next.controlSequence;
    REQUIRE(ledger->activate(next, 2));
    ++next.controlSequence;
    REQUIRE_FALSE(ledger->activate(next, 1));
    CHECK(ledger->query(id)->status == FeedbackStatus::canceledByControl);
}

TEST_CASE("B4 renderer steps cancel before GPU work and retain active presentation through Pause",
          "[rpa][b4][renderer]") {
    PlaybackSession session;
    presentation_renderer::TestPresentationRenderer renderer;
    auto prepared = preparedProject(session);
    auto input = HostCoordinationAccess::materialize(prepared, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    auto owner = RendererOwner<presentation_renderer::TestPresentationRenderer>::create(
        input->context.sessionScope, renderer, 24, 4);
    REQUIRE(owner);
    StepRequest request{
        {input->context.sessionScope, 1, Participant::renderer, StepPhase::prepare, 0}, {}};
    request.target.context = input->context;
    request.target.candidateId = input->candidateId;
    request.target.expectedGeneration = renderer.identity().generation;
    auto canceled = request;
    canceled.key.operationId = 2;
    REQUIRE(owner->cancel(canceled));
    CHECK(owner->prepare(canceled, *input)->status == StepStatus::canceledNotApplied);
    REQUIRE(owner->prepare(request, *input));
    REQUIRE(owner->prepare(request, *input));
    request.key.phase = StepPhase::fence;
    REQUIRE(owner->fence(request));
    request.key.phase = StepPhase::activate;
    REQUIRE(owner->activate(request));
    REQUIRE(renderer.hasActivePresentation());
    REQUIRE(owner->activate(request));
    auto pause = request;
    pause.key.operationId = 3;
    pause.target.context.controlSequence = 2;
    pause.key.phase = StepPhase::fence;
    auto fenced = owner->fence(pause);
    REQUIRE(fenced);
    CHECK(fenced->status == StepStatus::applied);
    CHECK(renderer.hasActivePresentation());
    pause.key.phase = StepPhase::activate;
    REQUIRE(owner->activate(pause));
    pause.key.phase = StepPhase::retire;
    REQUIRE(owner->retire(pause));
    CHECK_FALSE(renderer.hasActivePresentation());
    pause.key.phase = StepPhase::close;
    REQUIRE(owner->close(pause));
    REQUIRE(owner->steps().acknowledge(request.key));
    REQUIRE(owner->activate(request));
}
TEST_CASE("B4 capture reserves whole cut publication and can close an already paused route",
          "[rpa][b4][capture]") {
    SECTION("Full queue preserves unexecuted batch") {
        auto capture = CaptureRouteOwner::create({1, 1, 1}, 4, 1);
        REQUIRE(capture);
        REQUIRE(capture->prefix({1}));
        REQUIRE(capture->prefix({2}));
        REQUIRE(capture->prefix({3}));
        REQUIRE(capture->capture(event(3)));
        auto cut = step(Participant::capture, StepPhase::cut);
        cut.target.tick = 3;
        cut.target.pauseEpoch = 1;
        auto rejected = capture->cut(cut);
        REQUIRE(rejected);
        CHECK(rejected->status == StepStatus::rejectedNotApplied);
        CHECK(capture->pending()->size() == 3);
        CHECK(capture->retained()->size() == 1);
        CHECK(capture->fault().reason == FaultSlot::Reason::captureGap);
    }
    SECTION("Paused cleanup preserves original cut and trace") {
        auto capture = CaptureRouteOwner::create({1, 1, 1}, 8, 2);
        REQUIRE(capture);
        REQUIRE(capture->capture(event(5)));
        auto cut = step(Participant::capture, StepPhase::cut);
        cut.target.tick = 5;
        cut.target.pauseEpoch = 1;
        REQUIRE(capture->cut(cut));
        REQUIRE(capture->capture(event(5, GameplayInputAction::Release)));
        auto cleanup = cut;
        cleanup.key.operationId = 2;
        cleanup.target.context.controlSequence = 2;
        auto receipt = capture->cut(cleanup);
        REQUIRE(receipt);
        CHECK(receipt->effects.cut == 1);
        auto trace = capture->trace(0);
        REQUIRE(trace);
        REQUIRE(trace->trace.size() == 1);
        CHECK(trace->trace.front().occurrence.sourceLocalPosition == 1);
    }
}
TEST_CASE("B4 audio Reload prepares owning candidate while old playback stays active",
          "[rpa][b4][audio]") {
    audio::AudioClipStore store;
    auto one = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
    REQUIRE(one);
    auto two = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.3F));
    REQUIRE(two);
    auto h1 = store.registerClip(std::move(*one));
    REQUIRE(h1);
    auto h2 = store.registerClip(std::move(*two));
    REQUIRE(h2);
    test_support::FakeAudioTransport transport{store};
    auto owner =
        AudioOwner::create(1, transport, []() -> core::Result<bool> { return true; }, 32, 4);
    REQUIRE(owner);
    auto request = step(Participant::audio, StepPhase::prepare);
    request.target.expectedGeneration = 0;
    request.target.audioPositionMs = 10;
    request.target.audioClip = *h1;
    auto lease = store.lease(*h1);
    REQUIRE(lease);
    REQUIRE(owner->prepare(request, std::move(*lease)));
    request.key.phase = StepPhase::activate;
    REQUIRE(owner->activate(request));
    auto replacement = request;
    replacement.key.operationId = 2;
    replacement.key.phase = StepPhase::prepare;
    replacement.target.expectedGeneration = 1;
    replacement.target.candidateId = 2;
    replacement.target.context.controlSequence = 2;
    replacement.target.audioClip = *h2;
    lease = store.lease(*h2);
    REQUIRE(lease);
    REQUIRE(owner->prepare(replacement, std::move(*lease)));
    CHECK(transport.snapshot().source.state == audio::PlaybackState::Playing);
    CHECK(transport.snapshot().source.positionMs == 10);
    SECTION("Canceled preparation retires only candidate") {
        CHECK(owner->cancel(replacement)->status == StepStatus::applied);
        replacement.key.phase = StepPhase::retire;
        auto retired = owner->retire(replacement);
        REQUIRE(retired);
        CHECK(retired->effects.referencesReleased);
        CHECK(transport.snapshot().source.state == audio::PlaybackState::Playing);
    }
    SECTION("Confirmed isolation permits actual device replacement") {
        replacement.key.phase = StepPhase::fence;
        REQUIRE(owner->pause(replacement));
        replacement.key.phase = StepPhase::isolate;
        REQUIRE(owner->isolate(replacement));
        replacement.key.phase = StepPhase::activate;
        auto replaced = owner->activate(replacement);
        REQUIRE(replaced);
        CHECK(replaced->status == StepStatus::applied);
        CHECK(replaced->effects.generation == 2);
        CHECK(transport.snapshot().source.state == audio::PlaybackState::Playing);
    }
}

TEST_CASE("B4 real joint owners preserve SoftwareApplied through failure and directed Close",
          "[rpa][b4][joint]") {
    PlaybackSession session;
    presentation_renderer::TestPresentationRenderer renderer;
    auto prepared = preparedProject(session);
    auto input = HostCoordinationAccess::materialize(prepared, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    auto initial = input->context;
    initial.controlSequence = 0;
    initial.resourceGeneration = 0;
    initial.contentIdentity = {};
    auto host = HostCoordinator::create(initial, 12);
    REQUIRE(host);
    auto software = OwnerStepLedger::create(initial.sessionScope, Participant::software, 24, 4);
    REQUIRE(software);
    auto graphics = RendererOwner<presentation_renderer::TestPresentationRenderer>::create(
        initial.sessionScope, renderer, 24, 4);
    REQUIRE(graphics);
    audio::AudioClipStore store;
    auto pcm = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
    REQUIRE(pcm);
    auto handle = store.registerClip(std::move(*pcm));
    REQUIRE(handle);
    test_support::FakeAudioTransport transport{store};
    auto sound = AudioOwner::create(
        initial.sessionScope, transport, []() -> core::Result<bool> { return true; }, 24, 4);
    REQUIRE(sound);
    ControlRequest load;
    load.context = initial;
    load.context.controlSequence = 1;
    load.operationId = 1;
    load.kind = ControlKind::load;
    load.participants = {false, true, true, true};
    load.target.context = load.context;
    load.target.nextContext = input->context;
    load.target.candidateId = input->candidateId;
    load.target.audioClip = *handle;
    load.target.audioPositionMs = 10;
    load.target.participantGenerations[1] = input->mutationGeneration;
    load.target.participantGenerations[2] = 0;
    load.target.participantGenerations[3] = renderer.identity().generation;
    REQUIRE(HostCoordinationAccess::acceptControl(session, *host, load));
    const auto requestFor = [&](Participant participant, StepPhase phase) {
        return StepRequest{{initial.sessionScope, 1, participant, phase, 0}, load.target};
    };
    for (auto phase : {StepPhase::prepare, StepPhase::fence}) {
        auto request = requestFor(Participant::software, phase);
        REQUIRE(*software->start(request));
        REQUIRE(software->complete({request, StepStatus::applied, {}, {}}));
        REQUIRE(host->observe(*software->query(request.key)));
    }
    auto lease = store.lease(*handle);
    REQUIRE(lease);
    auto audioPrepared =
        sound->prepare(requestFor(Participant::audio, StepPhase::prepare), std::move(*lease));
    REQUIRE(audioPrepared);
    REQUIRE(audioPrepared->status == StepStatus::applied);
    REQUIRE(host->observe(*audioPrepared));
    auto gpuPrepared =
        graphics->prepare(requestFor(Participant::renderer, StepPhase::prepare), *input);
    REQUIRE(gpuPrepared);
    REQUIRE(gpuPrepared->status == StepStatus::applied);
    REQUIRE(host->observe(*gpuPrepared));
    auto audioFence = sound->pause(requestFor(Participant::audio, StepPhase::fence));
    REQUIRE(audioFence);
    REQUIRE(audioFence->status == StepStatus::applied);
    REQUIRE(host->observe(*audioFence));
    auto gpuFence = graphics->fence(requestFor(Participant::renderer, StepPhase::fence));
    REQUIRE(gpuFence);
    REQUIRE(gpuFence->status == StepStatus::applied);
    REQUIRE(host->observe(*gpuFence));
    auto fence = HostCoordinationAccess::enterFence(session, *input);
    REQUIRE(fence);
    auto sw = requestFor(Participant::software, StepPhase::softwareCommit);
    auto committed = HostCoordinationAccess::commitOnce(session, *software, sw, std::move(prepared),
                                                        *fence, *host->query(1));
    REQUIRE(committed);
    REQUIRE(committed->status == StepStatus::applied);
    auto generation = *HostCoordinationAccess::generation(session);
    REQUIRE(HostCoordinationAccess::commitOnce(session, *software, sw, PreparedPlayback{}, *fence,
                                               *host->query(1)));
    CHECK(*HostCoordinationAccess::generation(session) == generation);
    REQUIRE(host->observe(*committed));
    REQUIRE(host->markSoftwareApplied());
    auto activated = graphics->activate(requestFor(Participant::renderer, StepPhase::activate));
    REQUIRE(activated);
    REQUIRE(host->observe(*activated));
    SECTION("All owners confirmed") {
        auto audioActivated = sound->activate(requestFor(Participant::audio, StepPhase::activate));
        REQUIRE(audioActivated);
        REQUIRE(audioActivated->status == StepStatus::applied);
        REQUIRE(host->observe(*audioActivated));
        REQUIRE(host->finish());
        REQUIRE(HostCoordinationAccess::leaveFence(session, *fence, *host->query(1)));
        REQUIRE(session.update({0, 0, 0}));
    }
    SECTION("Device retry never repeats successful software commit") {
        transport.failNextService();
        auto failed = sound->activate(requestFor(Participant::audio, StepPhase::activate));
        REQUIRE(failed);
        REQUIRE(failed->status == StepStatus::failed);
        REQUIRE(host->observe(*failed));
        REQUIRE(transport.submit(
            {10, audio::PlaybackState::Paused, transport.snapshot().source.discontinuityId + 1}));
        auto stopped = sound->stop(requestFor(Participant::audio, StepPhase::stop));
        REQUIRE(stopped);
        REQUIRE(host->observe(*stopped));
        auto isolated = sound->isolate(requestFor(Participant::audio, StepPhase::isolate));
        REQUIRE(isolated);
        REQUIRE(host->observe(*isolated));
        auto retired = sound->retire(requestFor(Participant::audio, StepPhase::retire));
        REQUIRE(retired);
        REQUIRE(host->observe(*retired));
        auto retry = requestFor(Participant::audio, StepPhase::prepare);
        retry.key.attempt = 1;
        retry.retryGeneration = retired->effects.generation;
        auto retryLease = store.lease(*handle);
        REQUIRE(retryLease);
        auto newPrepared = sound->prepare(retry, std::move(*retryLease));
        REQUIRE(newPrepared);
        REQUIRE(newPrepared->status == StepStatus::applied);
        REQUIRE(host->observe(*newPrepared));
        retry.key.phase = StepPhase::fence;
        auto newFence = sound->pause(retry);
        REQUIRE(newFence);
        REQUIRE(host->observe(*newFence));
        retry.key.phase = StepPhase::activate;
        auto newActivated = sound->activate(retry);
        REQUIRE(newActivated);
        REQUIRE(newActivated->status == StepStatus::applied);
        REQUIRE(host->observe(*newActivated));
        REQUIRE(host->finish());
        CHECK(*HostCoordinationAccess::generation(session) == generation);
        REQUIRE(HostCoordinationAccess::leaveFence(session, *fence, *host->query(1)));
        CHECK(sound->steps().query(failed->request.key)->status == StepStatus::failed);
    }
    SECTION("Protocol conflict can retire original software before Close") {
        auto conflict = *committed;
        ++conflict.request.target.candidateId;
        REQUIRE_FALSE(host->observe(conflict));
        CHECK(host->state() == HostState::suspended);
        auto stopped = sound->stop(requestFor(Participant::audio, StepPhase::stop));
        REQUIRE(stopped);
        REQUIRE(host->observe(*stopped));
        auto isolated = sound->isolate(requestFor(Participant::audio, StepPhase::isolate));
        REQUIRE(isolated);
        REQUIRE(host->observe(*isolated));
        auto retired = graphics->retire(requestFor(Participant::renderer, StepPhase::retire));
        REQUIRE(retired);
        REQUIRE(host->observe(*retired));
        auto swRetired = HostCoordinationAccess::retireSoftware(
            session, *software, requestFor(Participant::software, StepPhase::retire),
            *host->query(1));
        REQUIRE(swRetired);
        REQUIRE(swRetired->status == StepStatus::applied);
        REQUIRE(host->observe(*swRetired));
        REQUIRE(host->completeCleanup());
        CHECK(session.state() == SessionState::Empty);
        ControlRequest close = load;
        close.kind = ControlKind::close;
        close.operationId = 2;
        close.commandOrdinal = 1;
        close.context = host->query(1)->finalizedContext.value_or(*load.target.nextContext);
        close.context.controlSequence = 2;
        close.target.context = close.context;
        close.target.nextContext.reset();
        close.target.commandOrdinal = 1;
        // A new Close binds the current software owner, not the old Load generation.
        const auto closeGeneration = HostCoordinationAccess::generation(session);
        REQUIRE(closeGeneration);
        close.target.participantGenerations[1] = *closeGeneration;
        REQUIRE(HostCoordinationAccess::acceptControl(session, *host, close));
        CHECK(host->state() == HostState::closing);
    }
    SECTION("Device failure keeps prefix and Close can retire fence") {
        // Invalid physical seek yields Failed after software commit, before audio starts.
        // The fake service failure occurs after load/seek/play, preserving actual device effects.
        transport.failNextService();
        auto audioFailed = sound->activate(requestFor(Participant::audio, StepPhase::activate));
        REQUIRE(audioFailed);
        REQUIRE(audioFailed->status == StepStatus::failed);
        REQUIRE(host->observe(*audioFailed));
        CHECK(host->query(1)->softwareApplied);
        REQUIRE_FALSE(HostCoordinationAccess::leaveFence(session, *fence, *host->query(1)));
        REQUIRE_FALSE(session.unload());
        // Device failure makes stop fail; an explicit successful isolation proof still cannot
        // pretend the failed stop was Applied. Recover the fake device before real cleanup.
        REQUIRE(transport.submit(
            {10, audio::PlaybackState::Paused, transport.snapshot().source.discontinuityId + 1}));
        auto stopped = sound->stop(requestFor(Participant::audio, StepPhase::stop));
        REQUIRE(stopped);
        REQUIRE(stopped->status == StepStatus::applied);
        REQUIRE(host->observe(*stopped));
        auto isolated = sound->isolate(requestFor(Participant::audio, StepPhase::isolate));
        REQUIRE(isolated);
        REQUIRE(host->observe(*isolated));
        auto retired = graphics->retire(requestFor(Participant::renderer, StepPhase::retire));
        REQUIRE(retired);
        REQUIRE(host->observe(*retired));
        REQUIRE(host->completeCleanup());
        ControlRequest close = load;
        close.kind = ControlKind::close;
        close.operationId = 2;
        close.commandOrdinal = 1;
        close.context = host->query(1)->finalizedContext.value_or(*load.target.nextContext);
        close.context.controlSequence = 2;
        close.target.context = close.context;
        close.target.nextContext.reset();
        close.target.commandOrdinal = 1;
        close.target.participantGenerations[2] = 1;
        close.target.participantGenerations[3] = renderer.identity().generation;
        // A new Close binds the current software owner, not the old Load generation.
        const auto closeGeneration = HostCoordinationAccess::generation(session);
        REQUIRE(closeGeneration);
        close.target.participantGenerations[1] = *closeGeneration;
        REQUIRE(HostCoordinationAccess::acceptControl(session, *host, close));
        const auto closeStep = [&](Participant p, StepPhase phase) {
            return StepRequest{{initial.sessionScope, 2, p, phase, 0}, close.target};
        };
        stopped = sound->stop(closeStep(Participant::audio, StepPhase::stop));
        REQUIRE(stopped);
        REQUIRE(host->observe(*stopped));
        isolated = sound->isolate(closeStep(Participant::audio, StepPhase::isolate));
        REQUIRE(isolated);
        REQUIRE(host->observe(*isolated));
        auto softwareRetired = HostCoordinationAccess::retireSoftware(
            session, *software, closeStep(Participant::software, StepPhase::retire),
            *host->query(2));
        REQUIRE(softwareRetired);
        REQUIRE(host->observe(*softwareRetired));
        retired = graphics->retire(closeStep(Participant::renderer, StepPhase::retire));
        REQUIRE(retired);
        REQUIRE(retired->status == StepStatus::applied);
        REQUIRE(host->observe(*retired));
        auto audioRetired = sound->retire(closeStep(Participant::audio, StepPhase::retire));
        REQUIRE(audioRetired);
        REQUIRE(audioRetired->status == StepStatus::applied);
        auto audioClosed = sound->close(closeStep(Participant::audio, StepPhase::close));
        REQUIRE(audioClosed);
        REQUIRE(host->observe(*audioClosed));
        auto gpuClosed = graphics->close(closeStep(Participant::renderer, StepPhase::close));
        REQUIRE(gpuClosed);
        REQUIRE(gpuClosed->status == StepStatus::applied);
        REQUIRE(host->observe(*gpuClosed));
        auto softwareClosed = closeStep(Participant::software, StepPhase::close);
        REQUIRE(*software->start(softwareClosed));
        REQUIRE(software->complete(
            {softwareClosed, StepStatus::applied, {0, {}, {}, {}, true, true}, {}}));
        REQUIRE(host->observe(*software->query(softwareClosed.key)));
        REQUIRE(host->finish());
        REQUIRE(HostCoordinationAccess::completeClose(session, *fence, *host->query(2)));
        CHECK(session.state() == SessionState::Empty);
    }
}
