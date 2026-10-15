#include "../../app/execution_host/audio_owner.hpp"
#include "../../app/execution_host/capture_route.hpp"
#include "../../app/execution_host/frame_observation.hpp"
#include "../../app/execution_host/host_candidate.hpp"
#include "../../app/execution_host/renderer_owner.hpp"
#include "../audio/fake_audio_transport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <filesystem>
#include <thread>

using namespace cuexis;
using namespace playback;
using namespace playback::detail;
namespace {
auto context() -> HostContext {
    HostContext c;
    c.sessionScope = 1;
    c.clockSegment = 1;
    c.runEpoch = 1;
    c.controlSequence = 1;
    c.resourceGeneration = 1;
    c.rendererGeneration = 1;
    return c;
}
auto step(Participant p, StepPhase phase, std::uint64_t id = 1) -> StepRequest {
    return {{1, id, p, phase, 0}, {context(), 1, 1, 0, {}, {}, {}, {}}};
}
auto control(ControlKind kind = ControlKind::resume) -> ControlRequest {
    auto c = context();
    return {c, 1, 0, kind, {c, 1, 1, 0, {}, {}, {}, {}}, {true, true, true, true}};
}
auto applied(const ControlRequest& r, Participant p, StepPhase phase) -> StepReceipt {
    return {{{r.context.sessionScope, r.operationId, p, phase, 0}, r.target},
            StepStatus::applied,
            {},
            {}};
}
auto preparedProject(PlaybackSession& session) -> PreparedPlayback {
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    auto p = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock);
    REQUIRE(p);
    return std::move(*p);
}
auto event(std::int64_t t, GameplayInputAction a = GameplayInputAction::Press) -> GameplayInput {
    return {{t}, 1, a, "lane.one", "domain.binding.one", "keyboard", {}};
}
class PartialLoadTransport final : public audio::IAudioTransport {
  public:
    explicit PartialLoadTransport(audio::AudioClipStore& store) : inner(store) {}
    test_support::FakeAudioTransport inner;
    unsigned stopCalls{}, pauseCalls{};
    bool rejectStop{};
    auto load(audio::AudioClipHandle handle) -> core::Result<void> override {
        auto loaded = inner.load(handle);
        if (!loaded)
            return loaded;
        auto playing = inner.play();
        if (!playing)
            return playing;
        return core::unexpected(
            core::Error{"audio.test.partial_load", "Load acted before failing"});
    }
    auto play() -> core::Result<void> override {
        return inner.play();
    }
    auto pause() -> core::Result<void> override {
        ++pauseCalls;
        return inner.pause();
    }
    auto stop() -> core::Result<void> override {
        ++stopCalls;
        if (rejectStop)
            return core::unexpected(core::Error{"audio.test.stop_failed", "Stop did not act"});
        return inner.stop();
    }
    auto seekMs(double value) -> core::Result<void> override {
        return inner.seekMs(value);
    }
    auto unload() -> core::Result<void> override {
        return inner.unload();
    }
    auto service() -> core::Result<void> override {
        return inner.service();
    }
    auto snapshot() const noexcept -> audio::AudioClockSnapshot override {
        return inner.snapshot();
    }
    auto metrics() const noexcept -> audio::AudioMetricsSnapshot override {
        return inner.metrics();
    }
    auto effectiveSettings() const noexcept -> audio::EffectiveAudioSettings override {
        return inner.effectiveSettings();
    }
};
} // namespace
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
