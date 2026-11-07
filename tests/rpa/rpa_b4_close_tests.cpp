#include "rpa_b4_fixture.hpp"

// Close retirement, foreign owner and partial audio load cases.

TEST_CASE("R1 C7b records failed Close retirement at both direct owner entrances",
          "[rpa][r1][c7b][b4]") {
    const bool useLedger = GENERATE(false, true);
    PlaybackSession session;
    auto prepared = preparedProject(session);
    REQUIRE(session.commit(std::move(prepared)));
    auto before = session.executionOwnerState();
    REQUIRE(before);
    auto request = control(ControlKind::close);
    request.context.sessionScope = before->sessionScope;
    request.operationId = 1;
    request.commandOrdinal = 0;
    request.participants = {false, true, false, false};
    request.target = {};
    request.target.context = request.context;
    request.target.participantGenerations[1] = before->generation;
    auto decision = session.acceptExecutionControl(request);
    REQUIRE(decision);
    auto proof = std::make_unique<ControlReceipt>();
    proof->request = request;
    proof->decisionOrdinal = *decision;
    proof->status = ControlStatus::failed;
    StepRequest retirement{{before->sessionScope, 1, Participant::software, StepPhase::retire, 0},
                           request.target};
    auto ledger = OwnerStepLedger::create(before->sessionScope, Participant::software, 16, 4);
    REQUIRE(ledger);
    bool accepted{};
    std::string code;
    if (useLedger) {
        auto result = HostCoordinationAccess::retireSoftware(session, *ledger, retirement, *proof);
        accepted = result && result->status == StepStatus::applied;
        code = result ? (result->cause ? std::string{result->cause->code()} : "none")
                      : std::string{result.error().code()};
    } else {
        auto result = session.retireExecutionStep(retirement, *proof);
        accepted = bool(result);
        code = result ? "none" : std::string{result.error().code()};
    }
    const auto after = session.executionOwnerState();
    REQUIRE(after);
    std::cout << "R1.C7b.layer=" << (useLedger ? "retireSoftware" : "retireExecutionStep")
              << ",accepted=" << accepted << ",error=" << code
              << ",generation=" << before->generation << "->" << after->generation << '\n';
    CHECK_FALSE(accepted);
    CHECK(code == "host.rpa.control_pending");
    CHECK(after->generation == before->generation);
    CHECK(session.state() == SessionState::Ready);
    CHECK_FALSE(ledger->query(retirement.key));
}

TEST_CASE("R1 C10 a new Close with stale software generation cannot unload", "[rpa][r1][c10][b4]") {
    const bool viaLedger = GENERATE(false, true);
    PlaybackSession session;
    auto prepared = preparedProject(session);
    REQUIRE(session.commit(std::move(prepared)));
    const auto before = session.executionOwnerState();
    REQUIRE(before);
    auto request = control(ControlKind::close);
    request.context.sessionScope = before->sessionScope;
    request.participants = {false, true, false, false};
    request.target = {};
    request.target.context = request.context;
    request.target.participantGenerations[1] = before->generation - 1;
    const auto decision = session.acceptExecutionControl(request);
    if (!decision) {
        CHECK(decision.error().code() == "host.rpa.context_stale");
        CHECK(session.executionOwnerState()->generation == before->generation);
        CHECK(session.state() == SessionState::Ready);
        return;
    }
    auto proof = std::make_unique<ControlReceipt>();
    proof->request = request;
    proof->decisionOrdinal = *decision;
    proof->status = ControlStatus::preparing;
    StepRequest retirement{
        {before->sessionScope, request.operationId, Participant::software, StepPhase::retire, 0},
        request.target};
    auto ledger = OwnerStepLedger::create(before->sessionScope, Participant::software, 16, 4);
    REQUIRE(ledger);
    if (viaLedger) {
        const auto result =
            HostCoordinationAccess::retireSoftware(session, *ledger, retirement, *proof);
        CHECK_FALSE((result && result->status == StepStatus::applied));
        CHECK_FALSE((result && result->effects.referencesReleased));
    } else {
        CHECK_FALSE(session.retireExecutionStep(retirement, *proof));
    }
    const auto after = session.executionOwnerState();
    REQUIRE(after);
    CHECK(after->generation == before->generation);
    CHECK(session.state() == SessionState::Ready);
}

TEST_CASE("R1 C2 foreign owner cannot retire an otherwise matching accepted Close",
          "[rpa][r1][c2][b4]") {
    const bool viaLedger = GENERATE(false, true);
    PlaybackSession session;
    auto prepared = preparedProject(session);
    REQUIRE(session.commit(std::move(prepared)));
    const auto before = session.executionOwnerState();
    REQUIRE(before);
    auto request = control(ControlKind::close);
    request.context.sessionScope = before->sessionScope;
    request.participants = {false, true, false, false};
    request.target = {};
    request.target.context = request.context;
    request.target.participantGenerations[1] = before->generation;
    const auto decision = session.acceptExecutionControl(request);
    REQUIRE(decision);
    auto proof = std::make_unique<ControlReceipt>();
    proof->request = request;
    proof->decisionOrdinal = *decision;
    proof->status = ControlStatus::preparing;
    StepRequest retirement{
        {before->sessionScope, request.operationId, Participant::software, StepPhase::retire, 0},
        request.target};
    auto ledger = OwnerStepLedger::create(before->sessionScope, Participant::software, 16, 4);
    REQUIRE(ledger);
    bool rejected{};
    std::thread foreign{[&] {
        if (viaLedger)
            rejected =
                !HostCoordinationAccess::retireSoftware(session, *ledger, retirement, *proof);
        else
            rejected = !session.retireExecutionStep(retirement, *proof);
    }};
    foreign.join();
    CHECK(rejected);
    CHECK(session.executionOwnerState()->generation == before->generation);
    CHECK(session.state() == SessionState::Ready);
    CHECK_FALSE(ledger->query(retirement.key));
}
TEST_CASE("B4 uncertain partial audio load requires real stop evidence", "[rpa][b4][audio]") {
    audio::AudioClipStore store;
    auto clip = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
    REQUIRE(clip);
    auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle);
    PartialLoadTransport transport{store};
    auto owner =
        AudioOwner::create(1, transport, []() -> core::Result<bool> { return true; }, 16, 5);
    REQUIRE(owner);
    auto request = step(Participant::audio, StepPhase::prepare);
    request.target.expectedGeneration = 0;
    request.target.audioPositionMs = 10;
    request.target.audioClip = *handle;
    auto lease = store.lease(*handle);
    REQUIRE(lease);
    REQUIRE(owner->prepare(request, std::move(*lease)));
    request.key.phase = StepPhase::activate;
    auto failed = owner->activate(request);
    REQUIRE(failed);
    REQUIRE(failed->status == StepStatus::failed);
    REQUIRE(transport.snapshot().source.state == audio::PlaybackState::Playing);
    SECTION("Stop confirms an actual transport mutation") {
        request.key.phase = StepPhase::stop;
        auto stopped = owner->stop(request);
        REQUIRE(stopped);
        CHECK(stopped->status == StepStatus::applied);
        CHECK(transport.stopCalls == 1);
        CHECK(transport.snapshot().source.state == audio::PlaybackState::Stopped);
        REQUIRE(owner->stop(request));
        CHECK(transport.stopCalls == 1);
    }
    SECTION("Failed stop cannot claim an applied stop") {
        transport.rejectStop = true;
        request.key.phase = StepPhase::stop;
        auto stopped = owner->stop(request);
        REQUIRE(stopped);
        CHECK(stopped->status == StepStatus::failed);
        CHECK(stopped->cause->code() == "audio.test.stop_failed");
        CHECK(transport.stopCalls == 1);
        CHECK(transport.snapshot().source.state == audio::PlaybackState::Playing);
    }
    SECTION("Fence also cannot infer paused from the missing load success") {
        request.key.phase = StepPhase::fence;
        auto paused = owner->pause(request);
        REQUIRE(paused);
        CHECK(paused->status == StepStatus::rejectedNotApplied);
        CHECK(transport.snapshot().source.state == audio::PlaybackState::Playing);
    }
}
TEST_CASE("B4 pending audio retirement seals original activation before releasing candidate",
          "[rpa][b4][audio][cancel]") {
    audio::AudioClipStore store;
    auto clip = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
    REQUIRE(clip);
    auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle);
    test_support::FakeAudioTransport transport{store};
    auto owner =
        AudioOwner::create(1, transport, []() -> core::Result<bool> { return true; }, 16, 5);
    REQUIRE(owner);
    auto request = step(Participant::audio, StepPhase::prepare);
    request.target.expectedGeneration = 0;
    request.target.audioPositionMs = 10;
    request.target.audioClip = *handle;
    auto lease = store.lease(*handle);
    REQUIRE(lease);
    REQUIRE(owner->prepare(request, std::move(*lease)));
    auto activation = request;
    activation.key.phase = StepPhase::activate;
    SECTION("Cancel before first activation remains terminal on late Apply") {
        request.key.phase = StepPhase::retire;
        auto retired = owner->retire(request);
        REQUIRE(retired);
        REQUIRE(retired->status == StepStatus::applied);
        CHECK(retired->effects.referencesReleased);
        CHECK(owner->steps().query(activation.key)->status == StepStatus::canceledNotApplied);
        CHECK(owner->activate(activation)->status == StepStatus::canceledNotApplied);
        CHECK(transport.snapshot().source.state == audio::PlaybackState::Empty);
    }
    SECTION("Started unknown activation cannot authorize candidate retirement") {
        REQUIRE(*owner->steps().start(activation));
        REQUIRE(owner->steps().complete({activation, StepStatus::unknown, {}, {}}));
        request.key.phase = StepPhase::retire;
        auto retired = owner->retire(request);
        REQUIRE(retired);
        CHECK(retired->status == StepStatus::rejectedNotApplied);
        CHECK_FALSE(retired->effects.referencesReleased);
        CHECK(owner->steps().query(activation.key)->status == StepStatus::unknown);
    }
}
TEST_CASE("B4 host control command exhaustion is rejected before accepting a wrapped ordinal",
          "[rpa][b4][owner]") {
    auto initial = context();
    initial.controlSequence = 0;
    auto host = HostCoordinator::create(initial, 4);
    REQUIRE(host);
    auto request = control(ControlKind::load);
    request.commandOrdinal = UINT64_MAX;
    auto preflight = host->preflight(request);
    REQUIRE_FALSE(preflight);
    CHECK(preflight.error().code() == "host.rpa.capacity_exceeded");
    auto accepted = host->accept(request, 0);
    REQUIRE_FALSE(accepted);
    CHECK(accepted.error().code() == "host.rpa.capacity_exceeded");
    CHECK(host->state() == HostState::running);
    request.commandOrdinal = 0;
    REQUIRE(host->preflight(request));
    CHECK(*host->preflight(request));
    REQUIRE(host->accept(request, 0));
}
TEST_CASE("B4 all audio retirement cancels the pending target while retiring an active clip",
          "[rpa][b4][audio][cancel]") {
    audio::AudioClipStore store;
    auto clip = audio::AudioClip::create(48000, 1, std::vector<float>(96000, 0.2F));
    REQUIRE(clip);
    auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle);
    test_support::FakeAudioTransport transport{store};
    auto owner =
        AudioOwner::create(1, transport, []() -> core::Result<bool> { return true; }, 32, 8);
    REQUIRE(owner);
    auto active = step(Participant::audio, StepPhase::prepare);
    active.target.expectedGeneration = 0;
    active.target.audioPositionMs = 10;
    active.target.audioClip = *handle;
    auto lease = store.lease(*handle);
    REQUIRE(lease);
    REQUIRE(owner->prepare(active, std::move(*lease)));
    active.key.phase = StepPhase::activate;
    REQUIRE(owner->activate(active));
    REQUIRE(transport.snapshot().source.state == audio::PlaybackState::Playing);

    auto pending = step(Participant::audio, StepPhase::prepare, 2);
    pending.target.context.controlSequence = 2;
    pending.target.candidateId = 2;
    pending.target.audioPositionMs = 15;
    pending.target.audioClip = *handle;
    lease = store.lease(*handle);
    REQUIRE(lease);
    REQUIRE(owner->prepare(pending, std::move(*lease)));
    auto activation = pending;
    activation.key.phase = StepPhase::activate;

    auto cleanup = pending;
    cleanup.key.operationId = 3;
    cleanup.target.context.controlSequence = 3;
    cleanup.target.candidateId = 0;
    cleanup.key.phase = StepPhase::stop;
    REQUIRE(owner->stop(cleanup));
    cleanup.key.phase = StepPhase::isolate;
    REQUIRE(owner->isolate(cleanup));
    cleanup.key.phase = StepPhase::retire;
    SECTION("All retirement seals the original pending activation") {
        auto retired = owner->retire(cleanup);
        REQUIRE(retired);
        REQUIRE(retired->status == StepStatus::applied);
        CHECK(retired->effects.referencesReleased);
        CHECK(owner->steps().query(activation.key)->status == StepStatus::canceledNotApplied);
        CHECK(owner->activate(activation)->status == StepStatus::canceledNotApplied);
        CHECK(transport.snapshot().source.state == audio::PlaybackState::Empty);
    }
    SECTION("Unknown pending activation prevents all retirement and unload") {
        REQUIRE(*owner->steps().start(activation));
        REQUIRE(owner->steps().complete({activation, StepStatus::unknown, {}, {}}));
        auto retired = owner->retire(cleanup);
        REQUIRE(retired);
        CHECK(retired->status == StepStatus::rejectedNotApplied);
        CHECK_FALSE(retired->effects.referencesReleased);
        CHECK(owner->steps().query(activation.key)->status == StepStatus::unknown);
        CHECK(transport.snapshot().source.state == audio::PlaybackState::Stopped);
    }
}
TEST_CASE("B4 Cancel before first Apply is terminal through retirement", "[rpa][b4][owner]") {
    auto l = OwnerStepLedger::create(1, Participant::renderer, 8, 2);
    REQUIRE(l);
    auto r = step(Participant::renderer, StepPhase::activate);
    CHECK_FALSE(l->query(r.key));
    REQUIRE(l->cancel(r));
    CHECK(l->query(r.key)->status == StepStatus::canceledNotApplied);
    REQUIRE(l->apply(r));
    CHECK_FALSE(*l->start(r));
    CHECK(*l->mayReclaim(r.key));
    REQUIRE(l->acknowledge(r.key));
    CHECK(l->query(r.key)->status == StepStatus::retired);
    CHECK_FALSE(*l->start(r));
    CHECK(*l->mayReclaim(r.key));
    auto changed = r;
    ++changed.target.candidateId;
    REQUIRE_FALSE(l->apply(changed));
    CHECK(l->suspended());
}
TEST_CASE("B4 started Apply cannot be canceled and device attempts retain history",
          "[rpa][b4][owner]") {
    auto l = OwnerStepLedger::create(1, Participant::audio, 8, 2);
    REQUIRE(l);
    auto r = step(Participant::audio, StepPhase::activate);
    REQUIRE(*l->start(r));
    CHECK(l->cancel(r)->status == StepStatus::inProgress);
    CHECK_FALSE(*l->mayReclaim(r.key));
    auto unknown = StepReceipt{r, StepStatus::unknown, {}, {}};
    REQUIRE(l->complete(unknown));
    CHECK(l->cancel(r)->status == StepStatus::unknown);
    CHECK_FALSE(*l->start(r));
    auto success = StepReceipt{r, StepStatus::applied, {2, 12, {}, {}, false, false}, {}};
    REQUIRE(l->complete(success));
    CHECK(l->cancel(r)->effects.position == 12);
    CHECK_FALSE(*l->start(r));
    CHECK_FALSE(*l->mayReclaim(r.key));
    auto cleanup = r;
    cleanup.key.phase = StepPhase::isolate;
    REQUIRE(*l->start(cleanup));
    REQUIRE(l->complete({cleanup, StepStatus::applied, {2, 12, {}, {}, true, true}, {}}));
    CHECK(*l->mayReclaim(cleanup.key));
    CHECK_FALSE(*l->mayReclaim(r.key));
    auto retry = r;
    retry.key.attempt = 1;
    REQUIRE_FALSE(l->start(retry));
    CHECK(l->query(r.key)->status == StepStatus::applied);
}
TEST_CASE("B4 control reserve and allocation fault slot remain available", "[rpa][b4][capacity]") {
    auto l = OwnerStepLedger::create(1, Participant::capture, 4, 1);
    REQUIRE(l);
    for (std::uint64_t id = 1; id <= 3; ++id)
        REQUIRE(l->apply(step(Participant::capture, StepPhase::prepare, id)));
    auto r = step(Participant::capture, StepPhase::prepare, 4);
    REQUIRE_FALSE(l->apply(r));
    REQUIRE(l->cancel(r));
    CHECK(l->query(r.key)->status == StepStatus::canceledNotApplied);
    REQUIRE_FALSE(l->cancel(step(Participant::capture, StepPhase::prepare, 5)));
    auto spare = OwnerStepLedger::create(1, Participant::audio, 4, 1);
    REQUIRE(spare);
    r = step(Participant::audio, StepPhase::activate);
    REQUIRE(*spare->start(r));
    spare->injectReservationFailureForTest();
    REQUIRE_FALSE(spare->complete({r, StepStatus::applied, {}, {}}));
    CHECK(spare->fault().reason == FaultSlot::Reason::allocation);
    CHECK(spare->fault().step == r.key);
    CHECK(spare->query(r.key)->status == StepStatus::unknown);
}
TEST_CASE("B4 owner rejects cross thread calls without changing outcome", "[rpa][b4][owner]") {
    auto l = OwnerStepLedger::create(1, Participant::renderer, 8, 2);
    REQUIRE(l);
    auto r = step(Participant::renderer, StepPhase::prepare);
    bool rejected = false;
    std::thread worker([&] { rejected = !l->apply(r); });
    worker.join();
    CHECK(rejected);
    REQUIRE(*l->start(r));
}
TEST_CASE("B4 owning renderer input survives Prepared and rejects stale generation",
          "[rpa][b4][resources]") {
    PlaybackSession s;
    presentation_renderer::TestPresentationRenderer renderer;
    auto p = preparedProject(s);
    auto input = HostCoordinationAccess::materialize(p, renderer.capabilities(), {},
                                                     renderer.identity().instance,
                                                     renderer.identity().generation, 1);
    REQUIRE(input);
    REQUIRE_FALSE(input->resources.empty());
    auto held = input->resources.front();
    p = PreparedPlayback{};
    auto gpu = renderer.prepareOwned(*input);
    REQUIRE(gpu);
    REQUIRE(renderer.accepts(*gpu));
    renderer.discard(std::move(*gpu));
    auto stale = *input;
    ++stale.context.rendererGeneration;
    REQUIRE_FALSE(renderer.prepareOwned(stale));
    REQUIRE(renderer.close());
    REQUIRE(held);
    CHECK(held->reference == input->manifest.entries.front().reference);
}
