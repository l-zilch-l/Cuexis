#include "execution_host.hpp"
#include "../../../engine/core/internal/cuexis_internal/sha256.hpp"
#include <limits>
#include <thread>

namespace cuexis::player {
using namespace playback;
using namespace playback::detail;
namespace {
struct Failure {
    core::Error error;
};
auto error(const char* code) -> core::Error {
    return {std::string{"host.rpa."} + code, "Execution host operation is incomplete"};
}
template <class T> auto take(core::Result<T> result) -> T {
    if (!result)
        throw Failure{std::move(result.error())};
    return std::move(*result);
}
void take(core::Result<void> result) {
    if (!result)
        throw Failure{std::move(result.error())};
}
template <class T> auto value(SessionReply reply) -> T {
    auto* result = std::get_if<T>(&reply);
    if (!result)
        throw Failure{error("message_conflict")};
    return std::move(*result);
}
auto nowUs() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
struct Busy final {
    bool& flag;
    explicit Busy(bool& value) : flag(value) {
        if (flag)
            throw Failure{error("control_pending")};
        flag = true;
    }
    ~Busy() {
        flag = false;
    }
};
} // namespace
ExecutionHost::ExecutionHost(presentation_renderer::IPresentationRenderer& renderer,
                             ExecutionHostOptions options, ExecutionHostPorts ports,
                             std::unique_ptr<SessionWorker> session)
    : renderer_(renderer), options_(std::move(options)), ports_(std::move(ports)),
      session_(std::move(session)) {}
auto ExecutionHost::create(presentation_renderer::IPresentationRenderer& renderer,
                           ExecutionHostOptions options, ExecutionHostPorts ports)
    -> core::Result<std::unique_ptr<ExecutionHost>> try {
    if (!options.session.testOnly || !options.audio.testOnly || options.replyDeadline.count() < 1 ||
        options.hostCapacity < 2 || options.captureCapacity <= options.captureReserve ||
        !options.captureReserve || options.captureCapacity < 3 || !options.viewport.width ||
        !options.viewport.height)
        return core::unexpected(error("profile_incomplete"));
    auto worker = SessionWorker::create(options.session);
    if (!worker)
        return core::unexpected(worker.error());
    return std::unique_ptr<ExecutionHost>{
        new ExecutionHost{renderer, std::move(options), std::move(ports), std::move(*worker)}};
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
ExecutionHost::~ExecutionHost() {
    if (!closed_ && owner_.isCurrent()) {
        auto result = close();
        if (!result)
            suspended_ = true;
    }
    // Workers never wait for main-thread calls. Applications explicitly close while pumping first.
    // Emergency destruction still retires GPU ownership on main before joining local-only owners.
    if (rendererOwner_) {
        auto retired = renderer_.rebuild();
        if (!retired)
            suspended_ = true;
        rendererOwner_.reset();
    }
    if (audio_)
        audio_->requestShutdown();
    if (session_)
        session_->requestShutdown();
}
void ExecutionHost::checkOwner() const {
    if (!owner_.isCurrent())
        throw Failure{error("owner_mismatch")};
    if (closed_)
        throw Failure{error("owner_closed")};
}
void ExecutionHost::pump() {
    if (pumpCount_ == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    ++pumpCount_;
    if (ports_.pumpMain)
        take(ports_.pumpMain());
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
}
auto ExecutionHost::session(SessionCommand command, bool control) -> SessionReply {
    if (sessionJob_)
        throw Failure{error("control_pending")};
    sessionJob_ = take(session_->post(std::move(command), control));
    return awaitSession();
}
auto ExecutionHost::awaitSession() -> SessionReply {
    if (!sessionJob_)
        throw Failure{error("control_pending")};
    const auto deadline = std::chrono::steady_clock::now() + options_.replyDeadline;
    for (;;) {
        auto result = take(session_->poll(*sessionJob_));
        if (result) {
            take(session_->acknowledge(*sessionJob_));
            sessionJob_.reset();
            if (!*result)
                pendingControlAccept_ = false;
            return take(std::move(*result));
        }
        if (std::chrono::steady_clock::now() >= deadline)
            throw Failure{error("control_pending")};
        pump();
    }
}
auto ExecutionHost::audio(AudioWorkerCommand command) -> AudioWorkerReply {
    if (!audio_ || audioJob_)
        throw Failure{error("control_pending")};
    audioJob_ = take(audio_->post(std::move(command), true));
    const auto deadline = std::chrono::steady_clock::now() + options_.replyDeadline;
    for (;;) {
        auto result = take(audio_->poll(*audioJob_));
        if (result) {
            take(audio_->acknowledge(*audioJob_));
            audioJob_.reset();
            return take(std::move(*result));
        }
        if (std::chrono::steady_clock::now() >= deadline)
            throw Failure{error("control_pending")};
        pump();
    }
}
auto ExecutionHost::proof() -> ControlReceipt {
    return take(host_->query(control_.operationId));
}
auto ExecutionHost::step(Participant participant, StepPhase phase) const -> StepRequest {
    return {{control_.context.sessionScope, control_.operationId, participant, phase, 0},
            control_.target,
            {},
            {}};
}
void ExecutionHost::begin(ControlKind kind, StepTarget target, std::array<bool, 4> participants) {
    const auto inspection = value<SessionInspection>(session(SessionInspect{}));
    prepareControl(kind, std::move(target), participants, inspection.generation);
    postControl();
    confirmControl(value<std::uint64_t>(awaitSession()));
}
void ExecutionHost::prepareControl(ControlKind kind, StepTarget target,
                                   std::array<bool, 4> participants, std::uint64_t generation) {
    softwareGeneration_ = generation;
    if (operation_ == UINT64_MAX || command_ == UINT64_MAX ||
        context_.controlSequence == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    auto next = context_;
    ++next.controlSequence;
    target.context = next;
    target.commandOrdinal = command_;
    target.participantGenerations[0] = context_.runEpoch;
    target.participantGenerations[1] = softwareGeneration_;
    target.participantGenerations[2] = audioGeneration_;
    target.participantGenerations[3] = renderer_.identity().generation;
    control_ = {next, operation_, command_, kind, std::move(target), participants};
    if (!take(host_->preflight(control_)))
        throw Failure{error("control_pending")};
}
void ExecutionHost::postControl() {
    if (sessionJob_)
        throw Failure{error("control_pending")};
    sessionJob_ = take(session_->post(SessionAcceptControl{control_}, true));
    pendingControlAccept_ = true;
    ++operation_;
}
void ExecutionHost::confirmControl(std::uint64_t decision) {
    auto accepted = take(host_->accept(control_, decision));
    if (accepted.status != ControlStatus::preparing)
        throw Failure{error("control_pending")};
    context_ = control_.context;
    ++command_;
    pendingControlAccept_ = false;
    if (earlyCut_) {
        auto cut = std::move(*earlyCut_);
        earlyCut_.reset();
        observed(cut);
    }
}
void ExecutionHost::observed(const StepReceipt& receipt) {
    take(host_->observe(receipt));
    if (receipt.request.key.participant == Participant::software)
        softwareGeneration_ = receipt.effects.generation;
    if (receipt.request.key.participant == Participant::audio)
        audioGeneration_ = receipt.effects.generation;
    if (receipt.status != StepStatus::applied)
        throw Failure{receipt.cause.value_or(error("device_pending"))};
}
void ExecutionHost::audioStep(AudioWorkerCommandKind kind, StepPhase phase) {
    if (!control_.participants[2])
        return;
    AudioWorkerCommand command;
    command.kind = kind;
    command.step = step(Participant::audio, phase);
    auto result = audio(std::move(command));
    if (!result.step)
        throw Failure{error("message_conflict")};
    observed(*result.step);
}
void ExecutionHost::softwarePrepared() {
    observed(value<StepReceipt>(
        session(SessionSoftwarePrepare{step(Participant::software, StepPhase::prepare)})));
}
void ExecutionHost::finish() {
    take(host_->markSoftwareApplied());
    take(host_->finish());
    auto completed = proof();
    if (!completed.finalizedContext)
        throw Failure{error("message_conflict")};
    context_ = *completed.finalizedContext;
    static_cast<void>(session(SessionConfirmControl{completed}));
    if (projectionScope_ == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    ++projectionScope_;
}
void ExecutionHost::failed() noexcept {
    suspended_ = true;
    if (host_) {
        auto result = host_->suspend(control_.operationId);
        // The coordinator's fixed fault/state is available even if this diagnostic cannot publish.
        if (!result)
            suspended_ = true;
    }
}
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
        context_.clockSegment == UINT64_MAX || projectionScope_ == UINT64_MAX)
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
    ++projectionScope_;
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
    -> core::Result<FrameEnvelope> try {
    checkOwner();
    Busy operation{busy_};
    if (suspended_ || !host_ || frameId_ == UINT64_MAX)
        throw Failure{error("control_pending")};
    SessionSample request;
    request.context = context_;
    request.projectionScope = projectionScope_;
    request.frameId = frameId_++;
    request.presentationTick = tick;
    request.runtimeFrame = runtime;
    request.viewport = options_.viewport;
    request.realtime = bool(options_.realtime);
    request.frozen = frozen;
    return value<FrameEnvelope>(session(request, false));
} catch (const Failure& failure) {
    if (failure.error.code() != "host.rpa.control_pending" || sessionJob_ || audioJob_)
        failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::queryGameplay() -> core::Result<GameplayResult> {
    return value<GameplayResult>(session(SessionGameplayResult{}, false));
}

auto ExecutionHost::present(const FrameEnvelope& frame) -> core::Result<PresentReceipt> try {
    checkOwner();
    struct OverlayRenderer {
        presentation_renderer::IPresentationRenderer& renderer;
        PlayerFpsMeter& fps;
        bool enabled;
        auto identity() const noexcept {
            return renderer.identity();
        }
        auto submit(const FrameSnapshot& snapshot)
            -> core::Result<presentation_renderer::DrawSummary> {
            if (!enabled)
                return renderer.submit(snapshot);
            render::RenderScene scene;
            auto appended = appendFpsOverlay(snapshot, fps.reading(), scene);
            if (!appended)
                return core::unexpected(appended.error());
            return renderer.submit(snapshot, &scene);
        }
        auto present() -> core::Result<void> {
            return renderer.present();
        }
    } overlay{renderer_, fps_, options_.showFps};
    auto receipt = take(presentOwnedFrame(frame, context_, projectionScope_,
                                          renderer_.identity().instance, overlay, nowUs));
    if (receipt.status == PresentStatus::succeeded && options_.showFps)
        fps_.notePresented();
    static_cast<void>(session(SessionPresentReceipt{receipt}, false));
    if (receipt.status != PresentStatus::succeeded)
        failed();
    return receipt;
} catch (const Failure& failure) {
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::lag(const ClockObservation& observation)
    -> core::Result<GameplayExecutionLag> try {
    checkOwner();
    auto ticks = take(inputTicks(observation));
    return value<GameplayExecutionLag>(session(SessionLag{{ticks.horizon}}, false));
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}

auto ExecutionHost::evaluateReplay(const GameplayReplay& replay)
    -> core::Result<GameplayReplayEvaluation> try {
    checkOwner();
    return value<GameplayReplayEvaluation>(session(SessionEvaluateReplay{replay}, false));
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}

auto ExecutionHost::archive() -> core::Result<GameplayReplay> try {
    if (closed_ && retainedArchive_)
        return *retainedArchive_;
    checkOwner();
    return value<GameplayReplay>(session(SessionArchive{}));
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::seek(const GameplayReplay& archive, const GameplayReplayCut& cut,
                         GameplayTick horizon, const ClockSource& source)
    -> core::Result<void> try {
    checkOwner();
    Busy operation{busy_};
    if (suspended_ || state() != HostState::paused || !capture_ || !cut_ || !source)
        throw Failure{error("control_pending")};
    if (context_.runEpoch == UINT64_MAX || context_.clockSegment == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    auto encoded = take(archive.toBytes());
    StepTarget target;
    target.candidateId = candidate_->candidateId;
    target.tick = *cut_;
    target.pauseEpoch = pauseEpoch_;
    target.seekHorizon = horizon.value;
    target.seekCompleteRecords = cut.completeRecords;
    target.seekPartialHorizon =
        cut.partialHorizon ? std::optional{cut.partialHorizon->value} : std::nullopt;
    target.archiveIdentity = PreparedSemanticIdentity{core::detail::sha256(encoded)};
    target.nextContext = context_;
    ++target.nextContext->clockSegment;
    ++target.nextContext->runEpoch;
    ++target.nextContext->controlSequence;
    target.audioClip = clip_;
    const auto mapped =
        take(RealtimeClock::seekPosition(options_.realtime->mapping, horizon.value));
    target.audioPositionMs =
        static_cast<double>(static_cast<long double>(mapped.sourceUs.numerator) /
                            mapped.sourceUs.denominator / 1000.0L);
    begin(ControlKind::seek, std::move(target), {true, true, bool(audio_), true});
    observed(take(capture_->prepare(step(Participant::capture, StepPhase::prepare))));
    softwarePrepared();
    observed(take(rendererOwner_->prepareActive(step(Participant::renderer, StepPhase::prepare))));
    audioStep(AudioWorkerCommandKind::prepareActive, StepPhase::prepare);
    const GameplayExecutionContext next{context_.sessionScope,
                                        control_.target.nextContext->clockSegment,
                                        control_.target.nextContext->runEpoch};
    observed(take(capture_->resumeRoute(step(Participant::capture, StepPhase::resumeRoute), next)));
    routePaused_ = false;
    observed(take(rendererOwner_->fence(step(Participant::renderer, StepPhase::fence))));
    SessionSeek seekRequest;
    observed(take(capture_->fence(step(Participant::capture, StepPhase::fence))));
    audioStep(AudioWorkerCommandKind::fence, StepPhase::fence);
    observed(value<StepReceipt>(
        session(SessionSoftwareFence{step(Participant::software, StepPhase::fence), proof()})));
    seekRequest.request = step(Participant::software, StepPhase::softwareCommit);
    seekRequest.proof = proof();
    seekRequest.archive = archive;
    seekRequest.horizon = horizon;
    seekRequest.cut = cut;
    seekRequest.presentationTick = {mapped.presentationTick};
    seekRequest.runtimeFrame = {static_cast<double>(mapped.presentationTick) / 1000.0, 0,
                                next.clockSegment};
    observed(value<StepReceipt>(session(std::move(seekRequest))));
    observed(take(rendererOwner_->activate(step(Participant::renderer, StepPhase::activate))));
    audioStep(AudioWorkerCommandKind::activate, StepPhase::activate);
    finish();
    take(capture_->enableOrdinary(proof()));
    // Explicit Seek can move the clock backwards; create a fresh mapper for its new epoch.
    tail_.reset();
    tailFrozen_ = false;
    clock_.emplace(
        take(RealtimeClock::create(options_.realtime->profile, options_.realtime->mapping,
                                   options_.realtime->monotonicDomain, context_.clockSegment)));
    lastObservation_ = take(source(context_));
    take(clock_->sample(*lastObservation_, nowUs()));
    waitingAdvance_.reset();
    cut_.reset();
    lastObservedTick_ = horizon.value;
    reconciliationOrdinal_ =
        value<SessionInspection>(session(SessionInspect{})).reconciliationOrdinal.value_or(0);
    return {};
} catch (const Failure& failure) {
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
void ExecutionHost::cleanup() {
    // A lost transport reply is queried under its original ID before any retirement. Timeout
    // never manufactures Cancel or authorizes an alternative operation.
    const auto deadline = std::chrono::steady_clock::now() + options_.replyDeadline;
    while (sessionJob_ || audioJob_) {
        if (sessionJob_) {
            auto result = take(session_->poll(*sessionJob_));
            if (result) {
                take(session_->acknowledge(*sessionJob_));
                sessionJob_.reset();
                if (*result) {
                    if (pendingControlAccept_) {
                        confirmControl(value<std::uint64_t>(std::move(**result)));
                    } else if (pendingTailProfile_ &&
                               std::holds_alternative<TailClockTransition>(**result)) {
                        finishTail(value<TailClockTransition>(std::move(**result)),
                                   *pendingTailProfile_);
                    } else if (const auto* receipt = std::get_if<StepReceipt>(&**result)) {
                        take(host_->observe(*receipt));
                        if (receipt->request.key.phase == StepPhase::softwareCommit &&
                            receipt->status == StepStatus::applied)
                            take(host_->markSoftwareApplied());
                    }
                } else
                    pendingControlAccept_ = false;
            }
        }
        if (audioJob_) {
            auto result = take(audio_->poll(*audioJob_));
            if (result) {
                take(audio_->acknowledge(*audioJob_));
                audioJob_.reset();
                if (*result && (**result).step)
                    take(host_->observe(*(**result).step));
            }
        }
        if (!sessionJob_ && !audioJob_)
            break;
        if (std::chrono::steady_clock::now() >= deadline)
            throw Failure{error("control_pending")};
        pump();
    }
    if (!host_ || !control_.operationId)
        return;
    auto originalResult = host_->query(control_.operationId);
    if (!originalResult) {
        if (earlyCut_)
            earlyCut_.reset(); // The real D1 ledger and paused cut remain available to new Close.
        return;
    }
    auto original = std::move(*originalResult);
    if (original.status == ControlStatus::complete || original.cleanupComplete)
        return;
    take(host_->suspend(control_.operationId));
    if (control_.kind == ControlKind::resume) {
        auto aborted = host_->abortResume();
        if (aborted && capture_)
            capture_->abortResume();
    }
    audioStep(AudioWorkerCommandKind::stop, StepPhase::stop);
    audioStep(AudioWorkerCommandKind::isolate, StepPhase::isolate);
    if (rendererOwner_)
        observed(take(rendererOwner_->retire(step(Participant::renderer, StepPhase::retire))));
    auto canceled = value<StepReceipt>(
        session(SessionCancelStep{step(Participant::software, StepPhase::softwareCommit)}));
    take(host_->observe(canceled));
    observed(value<StepReceipt>(
        session(SessionSoftwareRetire{step(Participant::software, StepPhase::retire), proof()})));
    take(host_->completeCleanup());
    const auto cleaned = proof();
    if (cleaned.finalizedContext)
        context_ = *cleaned.finalizedContext;
}
auto ExecutionHost::resize(FrameViewport viewport) -> core::Result<void> try {
    checkOwner();
    if (busy_ || suspended_ || !viewport.width || !viewport.height)
        return core::unexpected(error("control_pending"));
    take(renderer_.resize(viewport.width, viewport.height));
    options_.viewport = viewport;
    return {};
} catch (const Failure& failure) {
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}

auto ExecutionHost::close() -> core::Result<void> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    if (closed_)
        return {};
    Busy operation{busy_};
    cleanup();
    if (capture_ && !clockTransitionInputs_.empty()) {
        for (const auto& queued : clockTransitionInputs_) {
            auto kept = capture_->capture(queued.first, queued.second);
            if (!kept)
                throw Failure{kept.error()};
        }
        clockTransitionInputs_.clear();
        tailPreparing_ = false;
    }
    if (!host_) {
        // No device candidate was published. Only local CPU/thread cleanup exists.
        if (audio_)
            audio_->requestShutdown();
        session_->requestShutdown();
    } else {
        if (options_.realtime) {
            auto archived = archive();
            if (archived)
                retainedArchive_ = *archived;
        }
        StepTarget target;
        target.candidateId = 0; // Explicitly retire all retained active/pending owner resources.
        target.audioClip = clip_;
        target.tick = routePaused_ ? cut_.value_or(0) : lastObservedTick_.value_or(0);
        if (capture_) {
            if (!routePaused_) {
                if (pauseEpoch_ == UINT64_MAX)
                    throw Failure{error("capacity_exceeded")};
                ++pauseEpoch_;
            }
            target.pauseEpoch = pauseEpoch_;
        }
        const bool audioParticipates = audio_ && !audio_->exited();
        begin(ControlKind::close, std::move(target),
              {bool(capture_), true, audioParticipates, gpuTouched_});
        if (capture_)
            observed(take(capture_->cut(step(Participant::capture, StepPhase::cut))));
        if (capture_ && capture_->fault().reason == FaultSlot::Reason::none && lastObservation_) {
            try {
                flush(*lastObservation_, false);
            } catch (const Failure& failure) {
                if (sessionJob_)
                    throw;
                closeDeliveryError_ = failure.error;
                take(host_->suspend(control_.operationId));
                // Actual undelivered records remain in D1 and the owning Close export.
                // Software retirement below forbids future admission; no ACK is fabricated.
            }
        }
        audioStep(AudioWorkerCommandKind::stop, StepPhase::stop);
        audioStep(AudioWorkerCommandKind::isolate, StepPhase::isolate);
        observed(value<StepReceipt>(session(
            SessionSoftwareRetire{step(Participant::software, StepPhase::retire), proof()})));
        if (gpuTouched_)
            observed(take(rendererOwner_->retire(step(Participant::renderer, StepPhase::retire))));
        audioStep(AudioWorkerCommandKind::retire, StepPhase::retire);
        if (capture_) {
            closeEvidence_ = take(capture_->exportCloseEvidence());
            retained_ = closeEvidence_->pending();
            observed(take(capture_->close(step(Participant::capture, StepPhase::close), proof(),
                                          *closeEvidence_)));
        }
        observed(value<StepReceipt>(
            session(SessionSoftwareClose{step(Participant::software, StepPhase::close), proof()})));
        if (gpuTouched_)
            observed(take(rendererOwner_->close(step(Participant::renderer, StepPhase::close))));
        audioStep(AudioWorkerCommandKind::close, StepPhase::close);
        take(host_->finish());
        auto completed = proof();
        if (!completed.finalizedContext)
            throw Failure{error("message_conflict")};
        context_ = *completed.finalizedContext;
        static_cast<void>(session(SessionConfirmControl{completed}));
        if (audio_)
            audio_->requestShutdown();
        session_->requestShutdown();
    }
    const auto deadline = std::chrono::steady_clock::now() + options_.replyDeadline;
    while (!session_->exited() || (audio_ && !audio_->exited())) {
        if (std::chrono::steady_clock::now() >= deadline)
            throw Failure{error("control_pending")};
        pump();
    }
    if (audio_)
        take(audio_->joinExited());
    take(session_->joinExited());
    rendererOwner_.reset();
    closed_ = true;
    return {};
} catch (const Failure& failure) {
    failed();
    return core::unexpected(failure.error);
} catch (...) {
    failed();
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::audioPublication() const -> core::Result<AudioPublication> {
    if (!owner_.isCurrent() || !audio_)
        return core::unexpected(error("device_pending"));
    return audio_->publication();
}
auto ExecutionHost::replayCut(const GameplayReplay& archive, GameplayTick horizon)
    -> core::Result<GameplayReplayCut> try {
    checkOwner();
    return value<GameplayReplayCut>(session(SessionArchiveCut{archive, horizon}));
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::context() const noexcept -> const HostContext& {
    return context_;
}
auto ExecutionHost::state() const noexcept -> HostState {
    return closed_      ? HostState::closed
           : suspended_ ? HostState::suspended
           : host_      ? host_->state()
                        : HostState::controlPreparing;
}
} // namespace cuexis::player
