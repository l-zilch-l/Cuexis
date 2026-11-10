#include "rpa_b4_fixture.hpp"

// Capacity, retry and canceled Reload cases.

TEST_CASE("B4 normal capacity exhaustion still permits real audio cleanup", "[rpa][b4][capacity]") {
    audio::AudioClipStore store;
    auto pcm = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
    REQUIRE(pcm);
    auto handle = store.registerClip(std::move(*pcm));
    REQUIRE(handle);
    test_support::FakeAudioTransport transport{store};
    auto owner =
        AudioOwner::create(1, transport, []() -> core::Result<bool> { return true; }, 8, 4);
    REQUIRE(owner);
    auto request = step(Participant::audio, StepPhase::prepare);
    request.target.expectedGeneration = 0;
    request.target.audioPositionMs = 10;
    request.target.audioClip = *handle;
    auto lease = store.lease(*handle);
    REQUIRE(lease);
    REQUIRE(owner->prepare(request, std::move(*lease)));
    request.key.phase = StepPhase::activate;
    REQUIRE(owner->activate(request));
    REQUIRE(owner->steps().apply(step(Participant::audio, StepPhase::prepare, 10)));
    REQUIRE(owner->steps().apply(step(Participant::audio, StepPhase::prepare, 11)));
    REQUIRE_FALSE(owner->steps().apply(step(Participant::audio, StepPhase::prepare, 12)));
    for (auto phase : {StepPhase::stop, StepPhase::isolate, StepPhase::retire, StepPhase::close}) {
        request.key.phase = phase;
        auto receipt = phase == StepPhase::stop      ? owner->stop(request)
                       : phase == StepPhase::isolate ? owner->isolate(request)
                       : phase == StepPhase::retire  ? owner->retire(request)
                                                     : owner->close(request);
        REQUIRE(receipt);
        CHECK(receipt->status == StepStatus::applied);
    }
    CHECK(transport.snapshot().source.state == audio::PlaybackState::Empty);
}
TEST_CASE("B4 lost software publication preserves actual prefix without repeating commit",
          "[rpa][b4][publication]") {
    PlaybackSession session;
    presentation_renderer::TestPresentationRenderer renderer;
    auto prepared = preparedProject(session);
    auto input = HostCoordinationAccess::materialize(prepared, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    auto initial = input->context;
    initial.controlSequence = 0;
    auto host = HostCoordinator::create(initial, 4);
    REQUIRE(host);
    auto owner = OwnerStepLedger::create(initial.sessionScope, Participant::software, 8, 3);
    REQUIRE(owner);
    ControlRequest load;
    load.context = input->context;
    load.operationId = 1;
    load.kind = ControlKind::load;
    load.participants = {false, true, false, false};
    load.target.context = load.context;
    load.target.candidateId = input->candidateId;
    load.target.participantGenerations[1] = input->mutationGeneration;
    REQUIRE(HostCoordinationAccess::acceptControl(session, *host, load));
    StepRequest request{{initial.sessionScope, 1, Participant::software, StepPhase::prepare, 0},
                        load.target};
    for (auto phase : {StepPhase::prepare, StepPhase::fence}) {
        request.key.phase = phase;
        REQUIRE(*owner->start(request));
        REQUIRE(owner->complete({request, StepStatus::applied, {}, {}}));
        REQUIRE(host->observe(*owner->query(request.key)));
    }
    auto fence = HostCoordinationAccess::enterFence(session, *input);
    REQUIRE(fence);
    request.key.phase = StepPhase::softwareCommit;
    owner->injectReservationFailureForTest();
    REQUIRE_FALSE(HostCoordinationAccess::commitOnce(session, *owner, request, std::move(prepared),
                                                     *fence, *host->query(1)));
    CHECK(session.state() == SessionState::Ready);
    auto generation = *HostCoordinationAccess::generation(session);
    auto unknown = owner->query(request.key);
    REQUIRE(unknown);
    CHECK(unknown->status == StepStatus::unknown);
    CHECK(unknown->effects.generation == generation);
    auto retry = HostCoordinationAccess::commitOnce(session, *owner, request, PreparedPlayback{},
                                                    *fence, *host->query(1));
    REQUIRE(retry);
    CHECK(retry->status == StepStatus::unknown);
    CHECK(*HostCoordinationAccess::generation(session) == generation);
    auto confirmed = *unknown;
    confirmed.status = StepStatus::applied;
    REQUIRE(owner->complete(confirmed));
    REQUIRE(host->observe(confirmed));
    REQUIRE(host->markSoftwareApplied());
    REQUIRE(host->finish());
    REQUIRE(HostCoordinationAccess::leaveFence(session, *fence, *host->query(1)));
}

TEST_CASE("B4 audio candidate owns encoded bytes beyond Prepared lifetime",
          "[rpa][b4][resources]") {
    PlaybackSession session;
    presentation_renderer::TestPresentationRenderer renderer;
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage1d_project");
    REQUIRE(source);
    auto prepared = session.prepareLoad(std::move(*source), PlaybackMode::CuexisAudio);
    REQUIRE(prepared);
    auto input = HostCoordinationAccess::materialize(*prepared, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    REQUIRE(input->audio);
    CHECK(input->audio->assetId == "audio.main");
    CHECK(input->audio->encoded.size() == 192044);
    auto bytes = input->audio->encoded;
    prepared = PreparedPlayback{};
    CHECK(input->audio->encoded == bytes);
}

TEST_CASE("B4 real device attempts retry only after separate terminal cleanup",
          "[rpa][b4][retry]") {
    SECTION("Audio keeps original failed effects and reuses an owning lease") {
        audio::AudioClipStore store;
        auto pcm = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
        REQUIRE(pcm);
        auto handle = store.registerClip(std::move(*pcm));
        REQUIRE(handle);
        test_support::FakeAudioTransport transport{store};
        auto owner =
            AudioOwner::create(1, transport, []() -> core::Result<bool> { return true; }, 24, 5);
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
        auto retry = request;
        retry.key.attempt = 1;
        retry.retryGeneration = 1;
        REQUIRE_FALSE(owner->activate(retry));
        REQUIRE(transport.submit(
            {10, audio::PlaybackState::Paused, transport.snapshot().source.discontinuityId + 1}));
        request.key.phase = StepPhase::stop;
        REQUIRE(owner->stop(request));
        request.key.phase = StepPhase::isolate;
        REQUIRE(owner->isolate(request));
        request.key.phase = StepPhase::retire;
        REQUIRE(owner->retire(request));
        retry.key.phase = StepPhase::prepare;
        lease = store.lease(*handle);
        REQUIRE(lease);
        auto prepared = owner->prepare(retry, std::move(*lease));
        REQUIRE(prepared);
        REQUIRE(prepared->status == StepStatus::applied);
        retry.key.phase = StepPhase::fence;
        REQUIRE(owner->pause(retry));
        retry.key.phase = StepPhase::activate;
        auto activated = owner->activate(retry);
        REQUIRE(activated);
        REQUIRE(activated->status == StepStatus::applied);
        CHECK(activated->effects.generation == 2);
        auto old = owner->steps().query(failed->request.key);
        REQUIRE(old);
        CHECK(old->status == StepStatus::failed);
        CHECK_FALSE(old->effects.referencesReleased);
        CHECK(old->effects.generation == 1);
    }
    SECTION("Renderer rebinds only physical generation after failed stale activation") {
        PlaybackSession session;
        presentation_renderer::TestPresentationRenderer renderer;
        auto prepared = preparedProject(session);
        auto input = HostCoordinationAccess::materialize(prepared, renderer.capabilities(), {},
                                                         renderer.identity().instance,
                                                         renderer.identity().generation, 1);
        REQUIRE(input);
        auto owner = RendererOwner<presentation_renderer::TestPresentationRenderer>::create(
            input->context.sessionScope, renderer, 24, 5);
        REQUIRE(owner);
        StepRequest request{
            {input->context.sessionScope, 1, Participant::renderer, StepPhase::prepare, 0}, {}};
        request.target.context = input->context;
        request.target.candidateId = input->candidateId;
        request.target.expectedGeneration = renderer.identity().generation;
        REQUIRE(owner->prepare(request, *input));
        request.key.phase = StepPhase::fence;
        REQUIRE(owner->fence(request));
        REQUIRE(renderer.rebuild());
        request.key.phase = StepPhase::activate;
        auto failed = owner->activate(request);
        REQUIRE(failed);
        REQUIRE(failed->status == StepStatus::failed);
        request.key.phase = StepPhase::retire;
        auto retired = owner->retire(request);
        REQUIRE(retired);
        REQUIRE(retired->status == StepStatus::applied);
        auto retry = failed->request;
        retry.key.attempt = 1;
        retry.retryGeneration = renderer.identity().generation;
        auto replacement = *input;
        replacement.context.rendererGeneration = *retry.retryGeneration;
        retry.key.phase = StepPhase::prepare;
        auto gpu = owner->prepare(retry, replacement);
        REQUIRE(gpu);
        REQUIRE(gpu->status == StepStatus::applied);
        retry.key.phase = StepPhase::fence;
        REQUIRE(owner->fence(retry));
        retry.key.phase = StepPhase::activate;
        auto activated = owner->activate(retry);
        REQUIRE(activated);
        REQUIRE(activated->status == StepStatus::applied);
        CHECK(renderer.hasActivePresentation());
        CHECK(owner->steps().query(failed->request.key)->status == StepStatus::failed);
    }
}

TEST_CASE("B4 renderer retry publishes resolved context for actual first frame",
          "[rpa][b4][retry][joint]") {
    PlaybackSession session;
    presentation_renderer::TestPresentationRenderer renderer;
    auto prepared = preparedProject(session);
    auto input = HostCoordinationAccess::materialize(prepared, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    auto initial = input->context;
    initial.controlSequence = 0;
    auto host = HostCoordinator::create(initial, 8);
    REQUIRE(host);
    auto software = OwnerStepLedger::create(initial.sessionScope, Participant::software, 20, 4);
    REQUIRE(software);
    auto graphics = RendererOwner<presentation_renderer::TestPresentationRenderer>::create(
        initial.sessionScope, renderer, 24, 5);
    REQUIRE(graphics);
    ControlRequest load;
    load.context = input->context;
    load.operationId = 1;
    load.kind = ControlKind::load;
    load.participants = {false, true, false, true};
    load.target.context = load.context;
    load.target.nextContext = load.context;
    load.target.candidateId = input->candidateId;
    load.target.participantGenerations[1] = input->mutationGeneration;
    load.target.participantGenerations[3] = renderer.identity().generation;
    REQUIRE(HostCoordinationAccess::acceptControl(session, *host, load));
    StepRequest sw{{initial.sessionScope, 1, Participant::software, StepPhase::prepare, 0},
                   load.target};
    for (auto phase : {StepPhase::prepare, StepPhase::fence}) {
        sw.key.phase = phase;
        REQUIRE(*software->start(sw));
        REQUIRE(software->complete({sw, StepStatus::applied, {}, {}}));
        REQUIRE(host->observe(*software->query(sw.key)));
    }
    StepRequest gpu{{initial.sessionScope, 1, Participant::renderer, StepPhase::prepare, 0},
                    load.target};
    auto receipt = graphics->prepare(gpu, *input);
    REQUIRE(receipt);
    REQUIRE(host->observe(*receipt));
    gpu.key.phase = StepPhase::fence;
    receipt = graphics->fence(gpu);
    REQUIRE(receipt);
    REQUIRE(host->observe(*receipt));
    auto fence = HostCoordinationAccess::enterFence(session, *input);
    REQUIRE(fence);
    sw.key.phase = StepPhase::softwareCommit;
    auto committed = HostCoordinationAccess::commitOnce(session, *software, sw, std::move(prepared),
                                                        *fence, *host->query(1));
    REQUIRE(committed);
    REQUIRE(host->observe(*committed));
    REQUIRE(host->markSoftwareApplied());
    REQUIRE(renderer.rebuild());
    gpu.key.phase = StepPhase::activate;
    auto failed = graphics->activate(gpu);
    REQUIRE(failed);
    REQUIRE(failed->status == StepStatus::failed);
    REQUIRE(host->observe(*failed));
    gpu.key.phase = StepPhase::retire;
    receipt = graphics->retire(gpu);
    REQUIRE(receipt);
    REQUIRE(host->observe(*receipt));
    auto retry = failed->request;
    retry.key.attempt = 1;
    retry.retryGeneration = renderer.identity().generation;
    auto replacement = *input;
    replacement.context.rendererGeneration = *retry.retryGeneration;
    retry.key.phase = StepPhase::prepare;
    receipt = graphics->prepare(retry, replacement);
    REQUIRE(receipt);
    REQUIRE(host->observe(*receipt));
    retry.key.phase = StepPhase::fence;
    receipt = graphics->fence(retry);
    REQUIRE(receipt);
    REQUIRE(host->observe(*receipt));
    retry.key.phase = StepPhase::activate;
    receipt = graphics->activate(retry);
    REQUIRE(receipt);
    REQUIRE(host->observe(*receipt));
    REQUIRE(host->finish());
    auto completed = host->query(1);
    REQUIRE(completed);
    REQUIRE(completed->finalizedContext);
    CHECK(completed->finalizedContext->rendererGeneration == renderer.identity().generation);
    CHECK(completed->request.target.nextContext->rendererGeneration !=
          completed->finalizedContext->rendererGeneration);
    REQUIRE(HostCoordinationAccess::leaveFence(session, *fence, *completed));
    REQUIRE(session.update({0, 0, 0}));
    auto snapshot = session.extractFrame({640, 480});
    REQUIRE(snapshot);
    FrameEnvelope frame;
    frame.context = *completed->finalizedContext;
    frame.frameId = 1;
    frame.projectionScope = 1;
    frame.snapshot = *snapshot;
    auto presented = presentOwnedFrame(frame, frame.context, 1, renderer.identity().instance,
                                       renderer, [] { return 42; });
    REQUIRE(presented);
    CHECK(presented->status == PresentStatus::succeeded);
}

TEST_CASE("B4 canceled renderer Reload retains the old active combination",
          "[rpa][b4][renderer][cancel]") {
    PlaybackSession session;
    presentation_renderer::TestPresentationRenderer renderer;
    auto prepared = preparedProject(session);
    auto input = HostCoordinationAccess::materialize(prepared, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    auto owner = RendererOwner<presentation_renderer::TestPresentationRenderer>::create(
        input->context.sessionScope, renderer, 24, 5);
    REQUIRE(owner);
    StepRequest original{
        {input->context.sessionScope, 1, Participant::renderer, StepPhase::prepare, 0}, {}};
    original.target.context = input->context;
    original.target.candidateId = input->candidateId;
    original.target.expectedGeneration = renderer.identity().generation;
    REQUIRE(owner->prepare(original, *input));
    original.key.phase = StepPhase::fence;
    REQUIRE(owner->fence(original));
    REQUIRE(session.commit(std::move(prepared)));
    original.key.phase = StepPhase::activate;
    REQUIRE(owner->activate(original));
    REQUIRE(session.update({0, 0, 0}));
    auto oldFrame = session.extractFrame({640, 480});
    REQUIRE(oldFrame);
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    auto replacement =
        session.prepareReload(std::move(*source), {0, 0, 0}, ReloadPolicy::KeepChartTime);
    REQUIRE(replacement);
    auto next = HostCoordinationAccess::materialize(*replacement, renderer.capabilities(), {},
                                                    renderer.identity().instance,
                                                    renderer.identity().generation, 2);
    REQUIRE(next);
    auto request = original;
    request.key.operationId = 2;
    request.key.phase = StepPhase::prepare;
    request.target.context.controlSequence = 2;
    request.target.nextContext = next->context;
    request.target.candidateId = next->candidateId;
    auto staged = owner->prepare(request, *next);
    REQUIRE(staged);
    REQUIRE(staged->status == StepStatus::applied);
    auto generation = renderer.identity().generation;
    request.key.phase = StepPhase::retire;
    auto retired = owner->retire(request);
    REQUIRE(retired);
    REQUIRE(retired->status == StepStatus::applied);
    CHECK(retired->effects.referencesReleased);
    CHECK(renderer.identity().generation == generation);
    CHECK(renderer.hasActivePresentation());
    request.key.phase = StepPhase::activate;
    CHECK(owner->activate(request)->status == StepStatus::canceledNotApplied);
    REQUIRE(renderer.submit(*oldFrame));
    REQUIRE(renderer.present());
    CHECK(session.state() != SessionState::Empty);
}
