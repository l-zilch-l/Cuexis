#include "execution_host.hpp"

#include "../../../engine/playback/src/replay_identity.hpp"
#include "execution_host_units_internal.hpp"

#include <algorithm>
#include <cuexis_internal/execution_diagnostics.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <limits>
#include <thread>

namespace cuexis::player {

using namespace playback;
using namespace playback::detail;

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
namespace rpa = cuexis::internal::execution_diagnostics::rpa;
#endif

auto ExecutionHost::load(PlaybackSource source, std::optional<GameplayContent> gameplay)
    -> core::Result<void> try {
    checkOwner();
    Busy operation{busy_};
    if (host_)
        throw Failure{error("control_pending")};
    SessionPrepareSource request;
    // This application supplies RuntimeFrame time, including its independent audio owner.
    request.mode = options_.sourceMode;
    request.source = std::move(source);
    request.gameplay = std::move(gameplay);
    request.presentation.enableDebugPass = options_.showFps;
    request.capabilities = renderer_.capabilities();
    request.rendererInstance = renderer_.identity().instance;
    request.rendererGeneration = renderer_.identity().generation;
    request.controlSequence = 1;
    candidate_ = value<PresentationCandidateInput>(session(std::move(request)));
    context_ = candidate_->context;
    context_.controlSequence = 0;
    softwareGeneration_ = candidate_->mutationGeneration;
    host_.emplace(take(HostCoordinator::create(context_, options_.hostCapacity)));
    rendererOwner_.emplace(take(RendererOwner<presentation_renderer::IPresentationRenderer>::create(
        context_.sessionScope, renderer_, options_.session.stepCapacity,
        options_.session.stepReserve)));
    if (candidate_->audio) {
        auto audioOptions = options_.audio;
        audioOptions.sessionScope = context_.sessionScope;
        audio_ = take(AudioWorker::start(audioOptions, ports_.audioDevice, ports_.decode));
        AudioWorkerCommand decode;
        decode.kind = AudioWorkerCommandKind::decode;
        decode.candidateId = candidate_->candidateId;
        decode.encoded = candidate_->audio->encoded;
        clip_ = audio(std::move(decode)).clip;
        if (!clip_)
            throw Failure{error("device_failed")};
    }
    StepTarget target;
    target.candidateId = candidate_->candidateId;
    target.audioClip = clip_;
    target.audioPositionMs = 0;
    begin(ControlKind::load, std::move(target), {false, true, bool(audio_), true});
    softwarePrepared();
    gpuTouched_ = true;
    observed(take(
        rendererOwner_->prepare(step(Participant::renderer, StepPhase::prepare), *candidate_)));
    audioStep(AudioWorkerCommandKind::prepare, StepPhase::prepare);
    observed(value<StepReceipt>(
        session(SessionSoftwareFence{step(Participant::software, StepPhase::fence), proof()})));
    observed(take(rendererOwner_->fence(step(Participant::renderer, StepPhase::fence))));
    audioStep(AudioWorkerCommandKind::fence, StepPhase::fence);
    observed(value<StepReceipt>(session(
        SessionSoftwareCommit{step(Participant::software, StepPhase::softwareCommit), proof()})));
    take(host_->markSoftwareApplied());
    observed(take(rendererOwner_->activate(step(Participant::renderer, StepPhase::activate))));
    audioStep(AudioWorkerCommandKind::activate, StepPhase::activate);
    finish();
    if (options_.realtime) {
        auto start = *options_.realtime;
        start.context = {context_.sessionScope, context_.clockSegment, context_.runEpoch};
        static_cast<void>(session(start));
        clock_.emplace(take(RealtimeClock::create(start.profile, start.mapping,
                                                  start.monotonicDomain, context_.clockSegment)));
        capture_.emplace(take(CaptureRouteOwner::create(start.context, options_.captureCapacity,
                                                        options_.captureReserve, 2)));
    }
    registerRenderer();
    return {};
} catch (const Failure& failure) {
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::capture(GameplayInput input, bool clockValid) -> core::Result<void> try {
    checkOwner();
    if (!capture_ || suspended_ || state() == HostState::closing)
        return core::unexpected(error("control_pending"));
    if (tail_ && input.observationTick.value > tail_->inputDeadline)
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Tail input deadline passed"});
    if (tailPreparing_) {
        if (clockTransitionInputs_.size() >= options_.captureCapacity)
            throw Failure{error("capacity_exceeded")};
        clockTransitionInputs_.emplace_back(std::move(input), clockValid);
        return {};
    }
    if (!lastObservedTick_ || input.observationTick.value > *lastObservedTick_)
        lastObservedTick_ = input.observationTick.value;
    take(capture_->capture(std::move(input), clockValid));
    return {};
} catch (const Failure& failure) {
    if (owner_.isCurrent())
        failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
void ExecutionHost::flush(const ClockObservation& clock, bool normalPrefix) {
    if (!capture_)
        return;
    if (normalPrefix) {
        const auto ticks = take(inputTicks(clock));
        take(capture_->prefix({ticks.observationTick}));
        // Close must cite the prefix actually published, including elapsed time since Advance's
        // earlier sample. A fresh clock sample can move D1 beyond that earlier cached Tick.
        lastObservedTick_ = ticks.observationTick;
    }
    auto deliveries = take(capture_->pending());
    for (const auto& delivery : deliveries) {
        if (closePhase_ != ExecutionClosePhase::idle)
            static_cast<void>(closeSession(SessionCaptureDelivery{delivery, clock}));
        else
            static_cast<void>(session(SessionCaptureDelivery{delivery, clock}, false));
        take(capture_->acknowledge(delivery.context, delivery.deliverySequence));
    }
}
auto ExecutionHost::inputTicks(const ClockObservation& observation) -> core::Result<ClockTicks> {
    if (!owner_.isCurrent() || !clock_)
        return core::unexpected(error("owner_mismatch"));
    if (tail_)
        return tailFrozen_ ? clock_->pausedTailTicks() : clock_->sampleTail(nowUs());
    if (tailPreparing_ && observation.playback == ClockPlaybackState::EndOfStream)
        return clock_->observeEof(observation, nowUs());
    return observation.playback == ClockPlaybackState::Paused
               ? clock_->observePaused(observation, nowUs())
               : clock_->sample(observation, nowUs());
}
auto ExecutionHost::beginTail(const ClockObservation& eof, TailProfile profile)
    -> core::Result<TailClockTransition> try {
    checkOwner();
    Busy operation{busy_};
    if (tail_ || suspended_ || state() != HostState::running || !clock_ || !capture_ ||
        context_.clockSegment == UINT64_MAX)
        throw Failure{error("control_pending")};
    auto next = context_;
    ++next.clockSegment;
    const GameplayExecutionContext execution{next.sessionScope, next.clockSegment, next.runEpoch};
    if (!capture_->canSwitchClock(execution))
        throw Failure{error("control_pending")};
    clockTransitionInputs_.reserve(options_.captureCapacity);
    pendingTailProfile_ = profile;
    tailPreparing_ = true;
    auto transition =
        value<TailClockTransition>(session(SessionTailStart{eof, next.clockSegment, profile}));
    finishTail(transition, std::move(profile));
    return transition;
} catch (const Failure& failure) {
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}

void ExecutionHost::finishTail(const TailClockTransition& transition, TailProfile profile) {
    auto next = context_;
    next.clockSegment = transition.tailSegment;
    take(capture_->switchClock({next.sessionScope, next.clockSegment, next.runEpoch}));
    take(host_->switchClock(next));
    context_ = std::move(next);
    pendingFrame_.reset();
    tail_ = transition;
    lastObservation_ = transition.eof;
    tailPreparing_ = false;
    for (const auto& queued : clockTransitionInputs_)
        take(capture_->capture(queued.first, queued.second));
    clockTransitionInputs_.clear();
    take(clock_->startTail(transition.eof, transition.transitionMonotonicUs, transition.tailSegment,
                           std::move(profile), transition.lastTimer, transition.closeThreshold));
    pendingTailProfile_.reset();
}

auto ExecutionHost::advance(const ClockObservation& observation)
    -> core::Result<GameplayAdvanceDecision> try {
    checkOwner();
    Busy operation{busy_};
    if (suspended_ || state() != HostState::running || !clock_ || !capture_)
        throw Failure{error("control_pending")};
    const auto ticks = take(inputTicks(observation));
    lastObservation_ = observation;
    lastObservedTick_ = ticks.observationTick;
    flush(observation, true);
    if (waitingAdvance_) {
        auto old =
            value<GameplayAdvanceDecision>(session(SessionQueryAdvance{*waitingAdvance_}, false));
        if (old.disposition != GameplayAdvanceDisposition::Waiting) {
            static_cast<void>(session(SessionRetireAdvance{*waitingAdvance_}, false));
            waitingAdvance_.reset();
        }
        return old;
    }
    if (advanceId_ == UINT64_MAX || command_ == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    auto result = value<GameplayAdvanceDecision>(session(
        SessionCoherentAdvance{{context_.sessionScope, context_.clockSegment, context_.runEpoch},
                               advanceId_,
                               command_,
                               observation},
        false));
    if (result.disposition != GameplayAdvanceDisposition::BusyNotAccepted) {
        if (result.disposition == GameplayAdvanceDisposition::Waiting)
            waitingAdvance_ = advanceId_;
        else
            static_cast<void>(session(SessionRetireAdvance{advanceId_}, false));
        ++advanceId_;
        ++command_;
    }
    return result;
} catch (const Failure& failure) {
    if (failure.error.code() != "host.rpa.control_pending" || sessionJob_ || audioJob_)
        failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::pause(const ClockObservation& observation) -> core::Result<void> {
    return pause(ClockSource{
        [&](const HostContext&) -> core::Result<ClockObservation> { return observation; }});
}
auto ExecutionHost::pause(const ClockSource& source) -> core::Result<void> try {
    checkOwner();
    Busy operation{busy_};
    if (suspended_ || state() != HostState::running || !capture_ || !clock_ || !source)
        throw Failure{error("control_pending")};
    // Inspect may pump and capture additional input. Take q only after that await completes.
    const auto inspection = value<SessionInspection>(session(SessionInspect{}));
    const auto observation = tail_ ? tail_->eof : take(source(context_));
    const auto ticks = take(inputTicks(observation));
    lastObservation_ = observation;
    lastObservedTick_ = ticks.observationTick;
    if (pauseEpoch_ == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    StepTarget target;
    target.candidateId = candidate_->candidateId;
    target.tick = ticks.observationTick;
    target.pauseEpoch = ++pauseEpoch_;
    if (tail_)
        target.tailPauseAt = nowUs();
    target.audioClip = clip_;
    if (audio_)
        target.audioPositionMs = take(audio_->publication()).clock.source.positionMs;
    prepareControl(ControlKind::pause, std::move(target),
                   {true, true, bool(audio_) && !tail_, true}, inspection.generation);
    postControl();
    if (tail_) {
        take(clock_->pauseTail(*control_.target.tailPauseAt));
        tailFrozen_ = true;
    }
    // No main pump between publishing K1 and the actual D1 boundary. Replies can arrive later.
    earlyCut_ = take(capture_->cut(step(Participant::capture, StepPhase::cut)));
    if (earlyCut_->status == StepStatus::applied) {
        routePaused_ = true;
        cut_ = ticks.observationTick;
    }
    confirmControl(value<std::uint64_t>(awaitSession()));
    flush(observation, false);
    audioStep(AudioWorkerCommandKind::fence, StepPhase::fence);
    observed(take(rendererOwner_->fence(step(Participant::renderer, StepPhase::fence))));
    observed(value<StepReceipt>(
        session(SessionPause{step(Participant::software, StepPhase::softwareCommit), proof()})));
    finish();
    return {};
} catch (const Failure& failure) {
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::resume(const ClockSource& source) -> core::Result<void> try {
    checkOwner();
    Busy operation{busy_};
    if (suspended_ || state() != HostState::paused || !cut_ || !capture_ || !source)
        throw Failure{error("control_pending")};
    if (context_.clockSegment == UINT64_MAX || context_.runEpoch == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    StepTarget target;
    target.candidateId = candidate_->candidateId;
    target.tick = *cut_;
    target.pauseEpoch = pauseEpoch_;
    target.reconciliationOrdinal = reconciliationOrdinal_;
    const auto inspection = value<SessionInspection>(session(SessionInspect{}));
    if (!inspection.reconciliationOrdinal)
        throw Failure{error("context_stale")};
    reconciliationOrdinal_ = *inspection.reconciliationOrdinal;
    target.reconciliationOrdinal = reconciliationOrdinal_;
    target.audioClip = clip_;
    if (audio_)
        target.audioPositionMs = take(audio_->publication()).clock.source.positionMs;
    target.nextContext = context_;
    ++target.nextContext->clockSegment;
    ++target.nextContext->runEpoch;
    ++target.nextContext->controlSequence;
    begin(ControlKind::resume, std::move(target), {true, true, bool(audio_) && !tail_, true});
    observed(take(rendererOwner_->fence(step(Participant::renderer, StepPhase::fence))));
    // ResumeStarting is a real device stage. A subsequent R2 preparation failure must stop it.
    audioStep(AudioWorkerCommandKind::activate, StepPhase::activate);
    std::optional<std::int64_t> tailResumeAt;
    if (tail_) {
        if (*cut_ > INT64_MAX - tail_->closeThreshold)
            throw Failure{error("capacity_exceeded")};
        tailResumeAt = nowUs();
        const auto minimum = *cut_ + tail_->closeThreshold;
        tail_->terminalHorizon = std::max(tail_->terminalHorizon, minimum);
        take(clock_->resumeTail(*tailResumeAt, control_.target.nextContext->clockSegment, minimum));
        tailFrozen_ = false;
    }
    auto observation = tail_ ? tail_->eof : take(source(*control_.target.nextContext));
    const auto deadline = std::chrono::steady_clock::now() + options_.replyDeadline;
    for (;;) {
        auto mapped = RealtimeClock::create(options_.realtime->profile, options_.realtime->mapping,
                                            options_.realtime->monotonicDomain,
                                            control_.target.nextContext->clockSegment);
        auto ticks = tail_    ? clock_->sampleTail(nowUs())
                     : mapped ? mapped->sample(observation, nowUs())
                              : core::Result<ClockTicks>{core::unexpected(mapped.error())};
        if (ticks && ticks->observationTick > *cut_)
            break;
        if (std::chrono::steady_clock::now() >= deadline)
            throw Failure{error("clock_invalid")};
        pump();
        observation = tail_ ? tail_->eof : take(source(*control_.target.nextContext));
    }
    const GameplayExecutionContext next{context_.sessionScope,
                                        control_.target.nextContext->clockSegment,
                                        control_.target.nextContext->runEpoch};
    auto route =
        take(capture_->resumeRoute(step(Participant::capture, StepPhase::resumeRoute), next));
    observed(route);
    routePaused_ = false;
    auto reconciliation = take(capture_->trace(reconciliationOrdinal_));
    auto request = step(Participant::software, StepPhase::softwareCommit);
    if (!route.effects.cut || !route.effects.payloadIdentity)
        throw Failure{error("message_conflict")};
    request.resumeTrace = ResumeTraceBinding{*route.effects.cut, *route.effects.payloadIdentity};
    request.tailResumeAt = tailResumeAt;
    observed(value<StepReceipt>(
        session(SessionResume{request, proof(), reconciliation, next, observation, route})));
    observed(take(rendererOwner_->activate(step(Participant::renderer, StepPhase::activate))));
    finish();
    take(capture_->enableOrdinary(proof()));
    if (!tail_)
        take(clock_->startSegment(context_.clockSegment, observation, nowUs()));
    lastObservation_ = observation;
    ++reconciliationOrdinal_;
    return {};
} catch (const Failure& failure) {
    if (capture_ && host_ && control_.kind == ControlKind::resume) {
        auto result = host_->abortResume();
        if (result)
            capture_->abortResume();
    }
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::sample(std::int64_t tick, RuntimeFrame runtime, bool frozen)
    -> core::Result<FrameEnvelope> {
    auto result = trySample(tick, runtime, frozen);
    if (!result)
        return core::unexpected(result.error());
    if (!*result)
        return core::unexpected(error("context_stale"));
    return std::move(**result);
}
auto ExecutionHost::trySample(std::int64_t tick, RuntimeFrame runtime, bool frozen)
    -> core::Result<std::optional<FrameEnvelope>> try {
    checkOwner();
    Busy operation{busy_};
    if (suspended_ || !host_ || frameId_ == UINT64_MAX || !consumer_ ||
        (state() != HostState::running && state() != HostState::paused) ||
        (state() == HostState::paused && !frozen))
        throw Failure{error("control_pending")};
    const auto rendererIdentity = renderer_.identity();
    if (!candidate_ || rendererIdentity.instance != candidate_->rendererInstance ||
        rendererIdentity.generation != context_.rendererGeneration ||
        !renderer_.hasActivePresentation()) {
        invalidateRetained();
        throw Failure{error("context_stale")};
    }
    pendingFrame_.reset();
    // Update is reliable work, independent of delivery admission or available CPU leases.
    if (!frozen)
        static_cast<void>(session(SessionUpdatePresentation{context_, projectionScope_, tick,
                                                            runtime, bool(options_.realtime)},
                                  false));
    FrameRequest request;
    request.context = context_;
    request.projectionScope = projectionScope_;
    request.consumerId = consumer_->consumerId;
    request.requestId = frameId_++;
    request.viewport = options_.viewport;
    request.frozen = frozen;
    request.retained = retainedPresentation_;
    const auto now = options_.session.monotonicNow ? options_.session.monotonicNow() : nowUs();
    const auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(options_.replyDeadline).count();
    if (now > INT64_MAX - duration)
        throw Failure{error("capacity_exceeded")};
    request.deadlineMonotonicUs = now + duration;
    auto info = value<DeliveryInfo>(session(SessionRequestFrame{request}, false));
    deliveryStatus_ = info.status;
    auto frame = take(session_->takeDelivery(request.consumerId, request.requestId));
    if (!frame) {
        deliveryStatus_ =
            take(session_->pollDelivery(request.consumerId, request.requestId)).status;
        return std::optional<FrameEnvelope>{};
    }
    deliveryStatus_ = DeliveryStatus::transferred;
    pendingFrame_ = std::make_shared<const FrameEnvelope>(*frame);
    return frame;
} catch (const Failure& failure) {
    if (failure.error.code() != "host.rpa.control_pending" || sessionJob_ || audioJob_)
        failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}

auto ExecutionHost::observationInfo() -> core::Result<ObservationInfo> try {
    checkOwner();
    Busy operation{busy_};
    return value<ObservationInfo>(session(SessionObservationQuery{}, false));
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::queryGameplay() -> core::Result<GameplayResult> {
    return value<GameplayResult>(session(SessionGameplayResult{}, false));
}

auto ExecutionHost::present(const FrameEnvelope& frame) -> core::Result<PresentReceipt> try {
    checkOwner();
    if (busy_ || suspended_ || (state() != HostState::running && state() != HostState::paused) ||
        !candidate_ || !pendingFrame_ || frame.frameId != pendingFrame_->frameId ||
        frame.context != pendingFrame_->context ||
        frame.projectionScope != pendingFrame_->projectionScope)
        return core::unexpected(error("context_stale"));
    Busy operation{busy_};
    // Consume before invoking the device. Reentry/retry cannot execute this attempt twice.
    auto attempt = std::move(pendingFrame_);
    take(rendererGate_.begin(*attempt, context_, projectionScope_, candidate_->rendererInstance));
    struct OverlayRenderer {
        presentation_renderer::IPresentationRenderer& renderer;
        PlayerFpsMeter& fps;
        bool enabled;
        auto identity() const noexcept {
            return renderer.identity();
        }
        auto submit(const FrameSnapshot& snapshot)
            -> core::Result<presentation_renderer::DrawSummary> {
            internal::pd::Timer measurement{internal::pd::Stage::renderer};
            if (!enabled)
                return renderer.submit(snapshot);
            render::RenderScene scene;
            auto appended = appendFpsOverlay(snapshot, fps.reading(), scene);
            if (!appended)
                return core::unexpected(appended.error());
            return renderer.submit(snapshot, &scene);
        }
        auto present() -> core::Result<void> {
            internal::pd::Timer measurement{internal::pd::Stage::renderer};
            return renderer.present();
        }
    } overlay{renderer_, fps_, options_.showFps};
    auto receipt = take(presentOwnedFrame(*attempt, context_, projectionScope_,
                                          candidate_->rendererInstance, overlay, nowUs));
    if (receipt.status == PresentStatus::succeeded) {
        retainedPresentation_.serial_ = take(rendererGate_.succeeded(*attempt));
        retainedPresentation_.frame_ = attempt;
        if (options_.showFps)
            fps_.notePresented();
    }
    session_->postReceipt(receipt);
    if (receipt.status != PresentStatus::succeeded) {
        invalidateRetained();
        failed();
    }
    return receipt;
} catch (const Failure& failure) {
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}

} // namespace cuexis::player
