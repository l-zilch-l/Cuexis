#include "execution_host.hpp"
#include "../../../engine/playback/src/replay_identity.hpp"
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
      session_(std::move(session)) {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaSeek_.timerCostNs = rpa::calibrateTimer();
#endif
}
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
ExecutionHostLifetime::~ExecutionHostLifetime() {
    while (host_ && host_->state() != HostState::closed) {
        const auto result = host_->close();
        if (!result && host_->state() != HostState::closed)
            host_->waitFor(std::chrono::milliseconds{1});
    }
    host_.reset();
}
void ExecutionHost::pump(std::chrono::steady_clock::time_point deadline) {
    if (pumpCount_ == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    ++pumpCount_;
    if (ports_.pumpMain)
        take(ports_.pumpMain());
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (closePhase_ != ExecutionClosePhase::idle && !rpaClose_.closedFlag) {
        ++rpaClose_.pumpSamples;
        const auto observedState =
            host_ ? static_cast<std::uint8_t>(host_->state()) : std::uint8_t{};
        if (observedState == static_cast<std::uint8_t>(HostState::closing))
            ++rpaClose_.pumpClosing;
        else if (observedState == static_cast<std::uint8_t>(HostState::closed))
            ++rpaClose_.pumpClosed;
    }
#endif
    auto duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::milliseconds{1});
    if (deadline != std::chrono::steady_clock::time_point::max()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            return;
        duration = std::min(deadline - now, duration);
    }
    if (sessionJob_)
        session_->waitForReply(*sessionJob_, duration);
    else if (audioJob_)
        audio_->waitForReply(*audioJob_, duration);
}
auto ExecutionHost::sessionCommand(SessionCommand command, bool control) -> SessionReply {
    if (sessionJob_)
        throw Failure{error("control_pending")};
    sessionJob_ = take(session_->post(std::move(command), control));
    sessionDeadline_ = closeNow() + options_.replyDeadline;
    return awaitSession();
}
auto ExecutionHost::closeNow() const -> std::chrono::steady_clock::time_point {
    if (closePhase_ != ExecutionClosePhase::idle && ports_.closeNow)
        return ports_.closeNow();
    return std::chrono::steady_clock::now();
}
void ExecutionHost::closeBoundary(ExecutionClosePhase phase) {
    if (ports_.beforeClosePhase)
        take(ports_.beforeClosePhase(phase));
}
auto ExecutionHost::awaitSession() -> SessionReply {
    internal::execution_diagnostics::Trace diagnostic{"awaitSession", 0, 0,
                                                      sessionJob_.value_or(0)};
    if (!sessionJob_)
        throw Failure{error("control_pending")};
    const auto deadline = closePhase_ == ExecutionClosePhase::idle
                              ? sessionDeadline_
                              : std::min(sessionDeadline_, closeDeadline_);
    for (;;) {
        auto result = take(session_->poll(*sessionJob_));
        if (result) {
            if (closePhase_ == ExecutionClosePhase::awaitAcceptance)
                closeBoundary(ExecutionClosePhase::acceptReplyReady);
            take(session_->acknowledge(*sessionJob_));
            sessionJob_.reset();
            if (!*result)
                pendingControlAccept_ = false;
            return take(std::move(*result));
        }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        const auto waitStarted = rpa::nowUs();
#endif
        if (closeNow() >= deadline) {
            pump(deadline);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            rpa::seekWait(rpa::seekPhaseSink,
                          rpa::seekWaitReply | rpa::seekWaitDeadline |
                              (session_->exited() ? rpa::seekWaitExited : 0),
                          rpa::nowUs() - waitStarted);
#endif
            diagnostic.emit("deadline");
            diagnostic.emit(session_->exited() ? "worker-exited" : "worker-running");
            throw Failure{error("control_pending")};
        }
        pump(deadline);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpa::seekWait(rpa::seekPhaseSink, rpa::seekWaitReply | rpa::seekWaitPump,
                      rpa::nowUs() - waitStarted);
#endif
    }
}
auto ExecutionHost::audio(AudioWorkerCommand command) -> AudioWorkerReply {
    if (!audio_ || audioJob_)
        throw Failure{error("control_pending")};
    audioJob_ = take(audio_->post(std::move(command), true));
    audioDeadline_ = closeNow() + options_.replyDeadline;
    return awaitAudio();
}
auto ExecutionHost::awaitAudio() -> AudioWorkerReply {
    const auto deadline = closePhase_ == ExecutionClosePhase::idle
                              ? audioDeadline_
                              : std::min(audioDeadline_, closeDeadline_);
    for (;;) {
        auto result = take(audio_->poll(*audioJob_));
        if (result) {
            take(audio_->acknowledge(*audioJob_));
            audioJob_.reset();
            return take(std::move(*result));
        }
        if (closeNow() >= deadline) {
            pump(deadline);
            throw Failure{error("control_pending")};
        }
        pump(deadline);
    }
}
void ExecutionHost::waitFor(std::chrono::milliseconds duration) {
    checkOwner();
    if (sessionJob_)
        session_->waitForReply(*sessionJob_, duration);
    else if (audioJob_)
        audio_->waitForReply(*audioJob_, duration);
    else
        session_->waitFor(duration);
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
    const bool rebuild =
        kind != ControlKind::pause && kind != ControlKind::resume && kind != ControlKind::close;
    if (rebuild && projectionScope_ == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    pendingFrame_.reset();
    if (rebuild || kind == ControlKind::close)
        retainedPresentation_ = {};
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
    sessionDeadline_ = closeNow() + options_.replyDeadline;
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
auto ExecutionHost::audioStep(AudioWorkerCommandKind kind, StepPhase phase) -> StepReceipt {
    if (!control_.participants[2])
        return {};
    AudioWorkerCommand command;
    command.kind = kind;
    command.step = step(Participant::audio, phase);
    auto result = closePhase_ != ExecutionClosePhase::idle && audioJob_ ? awaitAudio()
                                                                        : audio(std::move(command));
    if (!result.step)
        throw Failure{error("message_conflict")};
    observed(*result.step);
    return *result.step;
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
    const bool rebuild =
        control_.kind != ControlKind::pause && control_.kind != ControlKind::resume;
    static_cast<void>(session(SessionConfirmControl{completed, projectionScope_ + rebuild}));
    if (rebuild) {
        ++projectionScope_;
        invalidateRetained();
    }
}
void ExecutionHost::registerRenderer() {
    if (consumer_)
        return;
    auto layout = value<FrameDeliveryLayout>(session(SessionDeliveryLayout{}));
    if (layout.bytes > SIZE_MAX / 2 || registrationOperation_ == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    ConsumerProfile profile{ConsumerMode::realtimeLatest,
                            true,
                            2,
                            layout.bytes * 2,
                            layout.bytes,
                            renderer_.identity().instance,
                            context_.rendererGeneration};
    consumer_ = value<ConsumerRegistration>(
        session(SessionRegisterConsumer{registrationOperation_++, context_, profile}));
    auto receipt =
        take(rendererGate_.activate(*consumer_, context_, renderer_.identity().instance));
    static_cast<void>(session(SessionConfirmConsumer{receipt, false}));
}
void ExecutionHost::retireRenderer() {
    if (!consumer_)
        return;
    auto retiring =
        value<ConsumerRegistration>(session(SessionRevokeConsumer{consumer_->consumerId}));
    auto receipt = take(rendererGate_.revoke(retiring));
    pendingFrame_.reset();
    retainedPresentation_ = {};
    static_cast<void>(session(SessionConfirmConsumer{receipt, true}));
    consumer_.reset();
}
void ExecutionHost::invalidateRetained() {
    if (!consumer_ || !retainedPresentation_.frame())
        return;
    const auto serial = retainedPresentation_.serial();
    if (!rendererGate_.revokeRetained(consumer_->consumerId, serial))
        throw Failure{error("context_stale")};
    if (pendingFrame_ && pendingFrame_->retainedSerial == serial)
        pendingFrame_.reset();
    retainedPresentation_ = {};
    static_cast<void>(session(SessionRevokeRetained{consumer_->consumerId, serial}));
    deliveryStatus_ = DeliveryStatus::noRetainedFrame;
}
auto ExecutionHost::revokeRetained() -> core::Result<void> try {
    checkOwner();
    Busy operation{busy_};
    invalidateRetained();
    return {};
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
void ExecutionHost::failed() noexcept {
    if (closePhase_ != ExecutionClosePhase::idle)
        return;
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
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    // The worker ledger belongs to the previous seek; harvesting it here keeps this seek's
    // envelope free of the observation round trip.
    rpaSeekFetch();
    rpa::SeekEnvelope rpaEnvelope{&rpaSeek_};
    rpa::SeekSinkScope rpaHostSink{&rpaSeek_};
#endif
    StepTarget target;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::SeekScope phase{&rpaSeek_, rpa::SeekPhase::s0HostIdentity};
#endif
    if (suspended_ || state() != HostState::paused || !capture_ || !cut_ || !source)
        throw Failure{error("control_pending")};
    if (context_.runEpoch == UINT64_MAX || context_.clockSegment == UINT64_MAX)
        throw Failure{error("capacity_exceeded")};
    if (options_.seekIdentity == SeekIdentityPolicy::owningSource)
        target.replaySourceIdentity = take(playback::detail::ReplayIdentityAccess::source(archive));
    else
        target.archiveIdentity =
            PreparedSemanticIdentity{take(playback::detail::archiveIdentity(archive))};
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s0HostIdentity, &rpa::SeekWork::sealedIdentityKnown, 1);
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s0HostIdentity, &rpa::SeekWork::sealedIdentityPresent,
                 target.archiveIdentity.has_value() ? 1 : 0);
#endif
    const auto mapped =
        take(RealtimeClock::seekPosition(options_.realtime->mapping, horizon.value));
    target.candidateId = candidate_->candidateId;
    target.tick = *cut_;
    target.pauseEpoch = pauseEpoch_;
    target.seekHorizon = horizon.value;
    target.seekCompleteRecords = cut.completeRecords;
    target.seekPartialHorizon =
        cut.partialHorizon ? std::optional{cut.partialHorizon->value} : std::nullopt;
    target.nextContext = context_;
    ++target.nextContext->clockSegment;
    ++target.nextContext->runEpoch;
    ++target.nextContext->controlSequence;
    target.audioClip = clip_;
    target.audioPositionMs =
        static_cast<double>(static_cast<long double>(mapped.sourceUs.numerator) /
                            mapped.sourceUs.denominator / 1000.0L);
    const GameplayExecutionContext next{context_.sessionScope, target.nextContext->clockSegment,
                                        target.nextContext->runEpoch};
    SessionSeek seekRequest;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::SeekScope seekS1{&rpaSeek_, rpa::SeekPhase::s1HostProtocol};
#endif
    begin(ControlKind::seek, std::move(target), {true, true, bool(audio_), true});
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s1HostProtocol, &rpa::SeekWork::operationId,
                 control_.operationId);
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s1HostProtocol, &rpa::SeekWork::generation,
                 context_.runEpoch);
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s1HostProtocol, &rpa::SeekWork::participants,
                 audio_ ? 4 : 3);
    // One prepare/fence per participating stage: capture, software, renderer, audio.
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s1HostProtocol, &rpa::SeekWork::prepareCalls,
                 audio_ ? 4 : 3);
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s1HostProtocol, &rpa::SeekWork::fenceCalls,
                 audio_ ? 4 : 3);
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s1HostProtocol, &rpa::SeekWork::activateCalls,
                 audio_ ? 3 : 2);
#endif
    observed(take(capture_->prepare(step(Participant::capture, StepPhase::prepare))));
    softwarePrepared();
    observed(take(rendererOwner_->prepareActive(step(Participant::renderer, StepPhase::prepare))));
    audioStep(AudioWorkerCommandKind::prepareActive, StepPhase::prepare);
    observed(take(capture_->resumeRoute(step(Participant::capture, StepPhase::resumeRoute), next)));
    routePaused_ = false;
    observed(take(rendererOwner_->fence(step(Participant::renderer, StepPhase::fence))));
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
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    seekS1.finish();
#endif
    // Activation/finish are the host tail of the seek. The publish/swap work itself is recorded
    // at its real owner in playback_session.cpp; counting it here would double it.
    observed(take(rendererOwner_->activate(step(Participant::renderer, StepPhase::activate))));
    audioStep(AudioWorkerCommandKind::activate, StepPhase::activate);
    finish();
    take(capture_->enableOrdinary(proof()));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::SeekScope s6{&rpaSeek_, rpa::SeekPhase::s6ClockPresent};
#endif
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
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s6ClockPresent, &rpa::SeekWork::clockValid, 1);
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s6ClockPresent, &rpa::SeekWork::targetGeneration,
                 context_.clockSegment);
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s6ClockPresent, &rpa::SeekWork::targetPresentationTick,
                 static_cast<std::uint64_t>(mapped.presentationTick));
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s6ClockPresent, &rpa::SeekWork::endToEndUs,
                 static_cast<std::uint64_t>(rpa::nowUs() - rpaSeek_.seekStartedUs));
    s6.finish();
    rpa::SeekScope inspectPhase{&rpaSeek_, rpa::SeekPhase::s1HostProtocol};
#endif
    reconciliationOrdinal_ =
        value<SessionInspection>(session(SessionInspect{})).reconciliationOrdinal.value_or(0);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::seekSet(rpaSeek_, rpa::SeekPhase::s1HostProtocol, &rpa::SeekWork::confirmCalls, 1);
    inspectPhase.finish();
#endif
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
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupEnter);
    const auto cleanupStarted = rpa::nowUs();
    const auto cleanupBudgetUs =
        std::chrono::duration_cast<std::chrono::microseconds>(options_.replyDeadline).count();
#endif
    auto deadline = closePhase_ == ExecutionClosePhase::idle ? closeNow() + options_.replyDeadline
                                                             : closeDeadline_;
    if (sessionJob_)
        deadline = std::min(deadline, closeOldSessionDeadline_.value_or(sessionDeadline_));
    if (audioJob_)
        deadline = std::min(deadline, closeOldAudioDeadline_.value_or(audioDeadline_));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupDeadline);
#endif
    const auto retainReplyError = [&](const core::Error& failure) {
        if (!closeFirstError_)
            closeFirstError_ = failure;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaCause(rpa::Reason::operationFailed, failure);
#endif
    };
    while (sessionJob_ || audioJob_) {
        if (sessionJob_) {
            auto result = take(session_->poll(*sessionJob_));
            if (result) {
                take(session_->acknowledge(*sessionJob_));
                sessionJob_.reset();
                closeOldSessionDeadline_.reset();
                if (*result) {
                    if (pendingControlAccept_) {
                        confirmControl(value<std::uint64_t>(std::move(**result)));
                    } else if (pendingTailProfile_ &&
                               std::holds_alternative<TailClockTransition>(**result)) {
                        finishTail(value<TailClockTransition>(std::move(**result)),
                                   *pendingTailProfile_);
                    } else if (const auto* receipt = std::get_if<StepReceipt>(&**result)) {
                        if (receipt->cause)
                            retainReplyError(*receipt->cause);
                        take(host_->observe(*receipt));
                        if (receipt->status == StepStatus::applied &&
                            receipt->request.key.participant == Participant::software)
                            softwareGeneration_ = receipt->effects.generation;
                        if (receipt->request.key.phase == StepPhase::softwareCommit &&
                            receipt->status == StepStatus::applied)
                            take(host_->markSoftwareApplied());
                    } else if (const auto* archive = std::get_if<GameplayReplay>(&**result))
                        retainedArchive_ = *archive;
                    else if (const auto* inspection = std::get_if<SessionInspection>(&**result))
                        softwareGeneration_ = inspection->generation;
                } else {
                    retainReplyError(result->error());
                    pendingControlAccept_ = false;
                }
            }
        }
        if (audioJob_) {
            auto result = take(audio_->poll(*audioJob_));
            if (result) {
                take(audio_->acknowledge(*audioJob_));
                audioJob_.reset();
                closeOldAudioDeadline_.reset();
                if (*result && (**result).step) {
                    if ((**result).step->cause)
                        retainReplyError(*(**result).step->cause);
                    take(host_->observe(*(**result).step));
                    if ((**result).step->status == StepStatus::applied)
                        audioGeneration_ = (**result).step->effects.generation;
                } else if (!*result)
                    retainReplyError(result->error());
            }
        }
        if (!sessionJob_ && !audioJob_)
            break;
        pump(deadline);
        if (closeNow() >= deadline)
            throw Failure{error("control_pending")};
    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupWait);
    const auto cleanupElapsed = rpa::nowUs() - cleanupStarted;
    rpaClose_.cleanupWaitUs += static_cast<std::uint64_t>(cleanupElapsed);
    ++rpaClose_.cleanupWaitSamples;
    rpa::addWait(rpaClose_, rpa::CloseStage::cleanupWait, cleanupElapsed,
                 cleanupElapsed > cleanupBudgetUs);
#endif
    if (!host_ || !control_.operationId)
        return;
    auto originalResult = host_->query(control_.operationId);
    if (!originalResult) {
        if (earlyCut_)
            earlyCut_.reset(); // The real D1 ledger and paused cut remain available to new Close.
        return;
    }
    auto original = std::move(*originalResult);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaClose_.cleanupQueryStatus = static_cast<std::uint8_t>(original.status);
#endif
    if (original.status == ControlStatus::complete || original.cleanupComplete)
        return;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaClose_.coordinatorBeforeSuspend = host_ ? static_cast<std::uint8_t>(host_->state()) : 0;
    rpaClose_.cleanupSuspendCalled = true;
    rpaMark(rpa::CloseStage::cleanupSuspend);
#endif
    take(host_->suspend(control_.operationId));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaClose_.coordinatorAfterSuspend = host_ ? static_cast<std::uint8_t>(host_->state()) : 0;
#endif
    if (control_.kind == ControlKind::resume) {
        auto aborted = host_->abortResume();
        if (aborted && capture_)
            capture_->abortResume();
    }
    audioStep(AudioWorkerCommandKind::stop, StepPhase::stop);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupAudioStop);
#endif
    audioStep(AudioWorkerCommandKind::isolate, StepPhase::isolate);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupAudioIsolate);
#endif
    if (rendererOwner_)
        observed(take(rendererOwner_->retire(step(Participant::renderer, StepPhase::retire))));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupRendererRetire);
#endif
    auto canceled = value<StepReceipt>(
        session(SessionCancelStep{step(Participant::software, StepPhase::softwareCommit)}));
    take(host_->observe(canceled));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupCancelStep);
#endif
    observed(value<StepReceipt>(
        session(SessionSoftwareRetire{step(Participant::software, StepPhase::retire), proof()})));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupSoftwareRetire);
    rpaCaptureFence(rpa::CloseObservationPart::cleanup);
#endif
    take(host_->completeCleanup());
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaMark(rpa::CloseStage::cleanupComplete);
#endif
    const auto cleaned = proof();
    if (cleaned.finalizedContext)
        context_ = *cleaned.finalizedContext;
}
auto ExecutionHost::resize(FrameViewport viewport) -> core::Result<void> try {
    checkOwner();
    if (busy_ || suspended_ || !viewport.width || !viewport.height)
        return core::unexpected(error("control_pending"));
    Busy operation{busy_};
    take(renderer_.resize(viewport.width, viewport.height));
    pendingFrame_.reset();
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
    const bool first = closePhase_ == ExecutionClosePhase::idle;
    if (first) {
        const auto realStart = std::chrono::steady_clock::now();
        const auto start = ports_.closeNow ? ports_.closeNow() : realStart;
        // Translate an already-running transport deadline once; keep its remaining interval,
        // including an already-expired deadline, without restarting the original request.
        if (sessionJob_)
            closeOldSessionDeadline_ = start + (sessionDeadline_ - realStart);
        if (audioJob_)
            closeOldAudioDeadline_ = start + (audioDeadline_ - realStart);
        closeDeadline_ = start + std::chrono::milliseconds{1000};
        closePhase_ = ExecutionClosePhase::cleanup;
    }
    if (closeNow() > closeDeadline_)
        closeBudgetExceeded_ = true;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    constexpr auto budgetUs = std::int64_t{1000000};
    if (first) {
        rpaStage_ = rpa::CloseStage::none;
        rpaClose_ = {};
        rpaClose_.requestedUs =
            std::chrono::duration_cast<std::chrono::microseconds>(
                (closeDeadline_ - std::chrono::milliseconds{1000}).time_since_epoch())
                .count();
        rpaClose_.deadlineUs = rpaClose_.requestedUs + budgetUs;
        rpaClose_.facadeBefore = static_cast<std::uint8_t>(state());
    }
    rpaMark(rpa::CloseStage::closeEnter);
    ++rpaClose_.attempts;
    if (rpaClose_.attempts > 1)
        ++rpaClose_.reentries;
    if (sessionJob_) {
        rpaClose_.pendingSessionJobAtEntry = true;
        rpaClose_.pendingJobOwner = 1;
        rpaClose_.pendingJobKind = static_cast<std::uint8_t>(control_.kind);
    } else if (audioJob_) {
        rpaClose_.pendingAudioJobAtEntry = true;
        rpaClose_.pendingJobOwner = 2;
        rpaClose_.pendingJobKind = static_cast<std::uint8_t>(control_.kind);
    }
#endif
    if (closePhase_ == ExecutionClosePhase::cleanup) {
        closeBoundary(ExecutionClosePhase::cleanup);
        cleanup();
        closePhase_ = ExecutionClosePhase::revokeConsumer;
    }
    if (closePhase_ == ExecutionClosePhase::revokeConsumer) {
        closeBoundary(ExecutionClosePhase::revokeConsumer);
        if (consumer_) {
            auto retiring = value<ConsumerRegistration>(
                closeSession(SessionRevokeConsumer{consumer_->consumerId}));
            closeConsumerReceipt_ = take(rendererGate_.revoke(retiring));
        }
        pendingFrame_.reset();
        retainedPresentation_ = {};
        closePhase_ = ExecutionClosePhase::confirmConsumer;
    }
    if (closePhase_ == ExecutionClosePhase::confirmConsumer) {
        closeBoundary(ExecutionClosePhase::confirmConsumer);
        if (closeConsumerReceipt_)
            static_cast<void>(closeSession(SessionConfirmConsumer{*closeConsumerReceipt_, true}));
        consumer_.reset();
        closePhase_ = ExecutionClosePhase::retainedInput;
    }
    if (closePhase_ == ExecutionClosePhase::retainedInput) {
        closeBoundary(ExecutionClosePhase::retainedInput);
        if (capture_ && !clockTransitionInputs_.empty()) {
            for (const auto& queued : clockTransitionInputs_) {
                auto kept = capture_->capture(queued.first, queued.second);
                if (!kept)
                    throw Failure{kept.error()};
            }
            clockTransitionInputs_.clear();
            tailPreparing_ = false;
        }

        closePhase_ = ExecutionClosePhase::archive;
    }
    if (!host_ && closePhase_ == ExecutionClosePhase::archive)
        closePhase_ = ExecutionClosePhase::shutdown;
    if (closePhase_ == ExecutionClosePhase::archive) {
        closeBoundary(ExecutionClosePhase::archive);
        if (options_.realtime) {
            try {
                retainedArchive_ = value<GameplayReplay>(closeSession(SessionArchive{}));
            } catch (const Failure& failure) {
                if (sessionJob_)
                    throw;
                // No gameplay archive is available on an incompletely loaded session.
                if (!closeFirstError_)
                    closeFirstError_ = failure.error;
            }
        }
        closePhase_ = ExecutionClosePhase::inspect;
    }
    if (closePhase_ == ExecutionClosePhase::inspect) {
        closeBoundary(ExecutionClosePhase::inspect);
        closeInspectionGeneration_ =
            value<SessionInspection>(closeSession(SessionInspect{})).generation;
        closePhase_ = ExecutionClosePhase::prepareTarget;
    }
    if (closePhase_ == ExecutionClosePhase::prepareTarget) {
        closeBoundary(ExecutionClosePhase::prepareTarget);
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
        closeTarget_ = std::move(target);
        closePhase_ = ExecutionClosePhase::prepareControl;
    }
    if (closePhase_ == ExecutionClosePhase::prepareControl) {
        closeBoundary(ExecutionClosePhase::prepareControl);
        prepareControl(ControlKind::close, *closeTarget_,
                       {bool(capture_), true, bool(audio_ && !audio_->exited()), gpuTouched_},
                       closeInspectionGeneration_);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::beginClose);
#endif
        closePhase_ = ExecutionClosePhase::postAcceptance;
    }
    if (closePhase_ == ExecutionClosePhase::postAcceptance) {
        closeBoundary(ExecutionClosePhase::postAcceptance);
        postControl();
        closePhase_ = ExecutionClosePhase::awaitAcceptance;
    }
    if (closePhase_ == ExecutionClosePhase::awaitAcceptance) {
        closeBoundary(ExecutionClosePhase::awaitAcceptance);
        const auto decision = value<std::uint64_t>(awaitSession());
        // Retain the decision independently of the acknowledged transport reply.
        closeDecision_ = decision;
        closePhase_ = ExecutionClosePhase::confirmAcceptance;
    }
    if (closePhase_ == ExecutionClosePhase::confirmAcceptance) {
        closeBoundary(ExecutionClosePhase::confirmAcceptance);
        confirmControl(closeDecision_);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaSyncIdentity();
        rpaClose_.identity.decisionOrdinal = proof().decisionOrdinal;
        rpaClose_.identity.status = static_cast<std::uint8_t>(proof().status);
        rpaClose_.firstDecisionOrdinal = rpaClose_.identity.decisionOrdinal;
        rpaClose_.closeTick = static_cast<std::uint64_t>(control_.target.tick.value_or(0));
#endif
        closePhase_ = ExecutionClosePhase::cut;
    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    const bool audioParticipates = host_ && control_.participants[2];
#endif
    if (closePhase_ == ExecutionClosePhase::cut) {
        closeBoundary(ExecutionClosePhase::cut);
        if (capture_)
            observed(take(capture_->cut(step(Participant::capture, StepPhase::cut))));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        if (capture_) {
            rpaMark(rpa::CloseStage::captureCut);
            rpaClose_.captureTerminal = 1; // cut crossed
        }
#endif

        closePhase_ = ExecutionClosePhase::flush;
    }
    if (closePhase_ == ExecutionClosePhase::flush) {
        closeBoundary(ExecutionClosePhase::flush);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        const bool injectedFlushFailure = rpa::takeFlushFault();
#else
        constexpr bool injectedFlushFailure = false;
#endif
        if (capture_ && capture_->fault().reason == FaultSlot::Reason::none &&
            (lastObservation_ || injectedFlushFailure)) {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            const auto flushStarted = rpa::nowUs();
#endif
            try {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                if (injectedFlushFailure)
                    throw Failure{error("capacity_exceeded")};
#endif
                flush(*lastObservation_, false);
            } catch (const Failure& failure) {
                if (sessionJob_)
                    throw;
                closeDeliveryError_ = failure.error;
                if (!closeFirstError_)
                    closeFirstError_ = failure.error;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                // The first delivery failure happens inside flush(), before any later mark, so
                // attribute it to closeFlush instead of the preceding captureCut boundary.
                rpaStage_ = rpa::CloseStage::closeFlush;
                rpaCause(rpa::Reason::deliveryFailed, failure.error);
                rpaClose_.coordinatorBeforeSuspend =
                    host_ ? static_cast<std::uint8_t>(host_->state()) : 0;
                rpaMark(rpa::CloseStage::closeSuspend);
#endif
                take(host_->suspend(control_.operationId));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                rpaClose_.coordinatorAfterSuspend =
                    host_ ? static_cast<std::uint8_t>(host_->state()) : 0;
#endif
                // Actual undelivered records remain in D1 and the owning Close export.
                // Software retirement below forbids future admission; no ACK is fabricated.
            }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            const auto flushElapsed = rpa::nowUs() - flushStarted;
            rpa::addWait(rpaClose_, rpa::CloseStage::closeFlush, flushElapsed,
                         flushElapsed > budgetUs);
#endif
        }

        closePhase_ = ExecutionClosePhase::audioStop;
    }
    if (closePhase_ == ExecutionClosePhase::audioStop) {
        closeBoundary(ExecutionClosePhase::audioStop);
        audioStep(AudioWorkerCommandKind::stop, StepPhase::stop);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::closeAudioStop);
        rpaClose_.audioStopCalled = audioParticipates;
#endif

        closePhase_ = ExecutionClosePhase::audioIsolate;
    }
    if (closePhase_ == ExecutionClosePhase::audioIsolate) {
        closeBoundary(ExecutionClosePhase::audioIsolate);
        {
            const auto isolated = audioStep(AudioWorkerCommandKind::isolate, StepPhase::isolate);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            rpaMark(rpa::CloseStage::closeAudioIsolate);
            rpaClose_.audioIsolateCalled = audioParticipates;
            rpaClose_.audioCallbackIsolated =
                audioParticipates && isolated.effects.callbackIsolated;
#else
            static_cast<void>(isolated);
#endif
        }

        closePhase_ = ExecutionClosePhase::softwareRetire;
    }
    if (closePhase_ == ExecutionClosePhase::softwareRetire) {
        closeBoundary(ExecutionClosePhase::softwareRetire);
        observed(value<StepReceipt>(closeSession(
            SessionSoftwareRetire{step(Participant::software, StepPhase::retire), proof()})));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::closeSoftwareRetire);
        rpaClose_.softwareRetired = true;
        rpaClose_.softwareTerminal = 1;
        rpaCaptureFence(rpa::CloseObservationPart::closeRetire);
#endif

        closePhase_ = ExecutionClosePhase::rendererRetire;
    }
    if (closePhase_ == ExecutionClosePhase::rendererRetire) {
        closeBoundary(ExecutionClosePhase::rendererRetire);
        if (gpuTouched_)
            observed(take(rendererOwner_->retire(step(Participant::renderer, StepPhase::retire))));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::closeRendererRetire);
        rpaClose_.rendererRetired = gpuTouched_;
        rpaClose_.rendererTerminal = gpuTouched_ ? 1 : 0;
#endif

        closePhase_ = ExecutionClosePhase::audioRetire;
    }
    if (closePhase_ == ExecutionClosePhase::audioRetire) {
        closeBoundary(ExecutionClosePhase::audioRetire);
        audioStep(AudioWorkerCommandKind::retire, StepPhase::retire);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::closeAudioRetire);
        rpaClose_.audioRetireCalled = audioParticipates;
        rpaClose_.audioTerminal = audioParticipates ? 1 : 0;
#endif

        closePhase_ = ExecutionClosePhase::exportEvidence;
    }
    if (closePhase_ == ExecutionClosePhase::exportEvidence) {
        closeBoundary(ExecutionClosePhase::exportEvidence);
        if (capture_) {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            try {
#endif
                if (!closeEvidence_)
                    closeEvidence_ = take(capture_->exportCloseEvidence());
                retained_ = closeEvidence_->pending();
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            } catch (const Failure& failure) {
                // Export allocates the D1 copy. A capacity failure is attributed here and then
                // rethrown unchanged, so the caller sees exactly the same error as before.
                rpaStage_ = rpa::CloseStage::exportEvidence;
                rpaCause(rpa::Reason::operationFailed, failure.error);
                throw;
            } catch (...) {
                rpaStage_ = rpa::CloseStage::exportEvidence;
                rpa::firstCause(rpaClose_, rpa::Reason::operationFailed, rpaStage_, "");
                throw;
            }
#endif
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            rpaMark(rpa::CloseStage::exportEvidence);
            rpaClose_.evidenceDeliveries =
                static_cast<std::uint32_t>(closeEvidence_->pending().size());
            for (const auto& delivery : closeEvidence_->pending()) {
                if (const auto* batch = std::get_if<GameplayClosedBatch>(&delivery.payload)) {
                    rpaClose_.evidenceMembers += batch->members.size();
                    if (!rpaClose_.unackedFirstTick)
                        rpaClose_.unackedFirstTick =
                            static_cast<std::uint64_t>(batch->originalTick.value);
                    rpaClose_.unackedLastTick =
                        static_cast<std::uint64_t>(batch->observedTick.value);
                }
            }
            rpaClose_.unackedDeliveries = rpaClose_.evidenceDeliveries;
#endif
        }
        closePhase_ = ExecutionClosePhase::captureClose;
    }
    if (closePhase_ == ExecutionClosePhase::captureClose) {
        closeBoundary(ExecutionClosePhase::captureClose);
        if (capture_) {
            observed(take(capture_->close(step(Participant::capture, StepPhase::close), proof(),
                                          *closeEvidence_)));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            rpaClose_.captureClosed = true;
            rpaClose_.captureTerminal = 2;
#endif
        }

        closePhase_ = ExecutionClosePhase::softwareClose;
    }
    if (closePhase_ == ExecutionClosePhase::softwareClose) {
        closeBoundary(ExecutionClosePhase::softwareClose);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaClose_.fence.closeState = rpa::CloseStepState::resultPending;
        rpaClose_.fence.identity = rpaClose_.identity;
#endif
        const auto receipt = value<StepReceipt>(closeSession(
            SessionSoftwareClose{step(Participant::software, StepPhase::close), proof()}));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaClose_.fence.closeAttempted = true;
        rpaClose_.fence.closeApplied = receipt.status == StepStatus::applied;
        rpaClose_.fence.closeState = rpaClose_.fence.closeApplied ? rpa::CloseStepState::applied
                                                                  : rpa::CloseStepState::rejected;
        if (receipt.cause)
            rpa::copyCode(rpaClose_.fence.firstErrorCode, receipt.cause->code());
#endif
        observed(receipt);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaCaptureFence(rpa::CloseObservationPart::closeStep);
        rpaMark(rpa::CloseStage::softwareClose);
        rpaClose_.softwareClosed = true;
        rpaClose_.softwareTerminal = 2;
#endif

        closePhase_ = ExecutionClosePhase::rendererClose;
    }
    if (closePhase_ == ExecutionClosePhase::rendererClose) {
        closeBoundary(ExecutionClosePhase::rendererClose);
        if (gpuTouched_)
            observed(take(rendererOwner_->close(step(Participant::renderer, StepPhase::close))));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        if (gpuTouched_) {
            rpaClose_.rendererClosed = true;
            rpaClose_.rendererTerminal = 2;
        }
#endif

        closePhase_ = ExecutionClosePhase::audioClose;
    }
    if (closePhase_ == ExecutionClosePhase::audioClose) {
        closeBoundary(ExecutionClosePhase::audioClose);
        {
            const auto audioClosed = audioStep(AudioWorkerCommandKind::close, StepPhase::close);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
            rpaClose_.audioTerminal =
                audioParticipates ? (audioClosed.status == StepStatus::applied ? 2 : 1) : 0;
#else
            static_cast<void>(audioClosed);
#endif
        }

        closePhase_ = ExecutionClosePhase::finish;
    }
    if (closePhase_ == ExecutionClosePhase::finish) {
        closeBoundary(ExecutionClosePhase::finish);
        take(host_->finish());
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::finish);
        rpaClose_.finishCalled = true;
        rpaClose_.coordinatorAtFinish = static_cast<std::uint8_t>(host_->state());
#endif

        closePhase_ = ExecutionClosePhase::completedProof;
    }
    if (closePhase_ == ExecutionClosePhase::completedProof) {
        closeBoundary(ExecutionClosePhase::completedProof);
        closeCompleted_ = proof();
        if (!closeCompleted_->finalizedContext)
            throw Failure{error("message_conflict")};
        context_ = *closeCompleted_->finalizedContext;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaClose_.receiptStatus = static_cast<std::uint8_t>(closeCompleted_->status);
        rpaClose_.identity.status = rpaClose_.receiptStatus;
#endif
        closePhase_ = ExecutionClosePhase::confirm;
    }
    if (closePhase_ == ExecutionClosePhase::confirm) {
        closeBoundary(ExecutionClosePhase::confirm);
        static_cast<void>(closeSession(SessionConfirmControl{*closeCompleted_}));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::confirm);
        rpaClose_.confirmCalled = true;
#endif

        closePhase_ = ExecutionClosePhase::shutdown;
    }
    if (closePhase_ == ExecutionClosePhase::shutdown) {
        closeBoundary(ExecutionClosePhase::shutdown);
        if (audio_)
            audio_->requestShutdown();
        session_->requestShutdown();
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::shutdown);
        rpaClose_.shutdownRequested = true;
#endif

        closePhase_ = ExecutionClosePhase::exitWait;
    }
    if (closePhase_ == ExecutionClosePhase::exitWait) {
        closeBoundary(ExecutionClosePhase::exitWait);
        while (!closeOwnersExited()) {
            if (closeNow() >= closeDeadline_) {
                pump(closeDeadline_);
                throw Failure{error("control_pending")};
            }
            pump(closeDeadline_);
        }
        if (audio_)
            take(audio_->joinExited());
        take(session_->joinExited());
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::joins);
        rpaClose_.workerExited = session_->exited();
        rpaClose_.workerJoined = true;
        rpaClose_.audioJoined = audio_ != nullptr;
#endif
        rendererOwner_.reset();
        closed_ = true;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
        rpaMark(rpa::CloseStage::closed);
        rpaClose_.closedFlag = true;
        rpaClose_.facadeAfter = static_cast<std::uint8_t>(state());
#endif
        closePhase_ = ExecutionClosePhase::joined;
    }
    if (closeNow() > closeDeadline_) {
        closeBudgetExceeded_ = true;
        throw Failure{error("control_pending")};
    }
    return {};
} catch (const Failure& failure) {
    if (closePhase_ != ExecutionClosePhase::idle && closeNow() >= closeDeadline_)
        closeBudgetExceeded_ = true;
    if (closePhase_ != ExecutionClosePhase::idle && !closeFirstError_) {
        try {
            closeFirstError_ = failure.error;
        } catch (...) {
        }
    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaCause(rpa::Reason::operationFailed, failure.error);
    if (closeBudgetExceeded_ && rpaClose_.firstOverBudget == rpa::CloseStage::none)
        rpaClose_.firstOverBudget = rpaStage_;
    if (rpaClose_.fence.closeState == rpa::CloseStepState::resultPending)
        rpaCaptureFence(rpa::CloseObservationPart::closeStep);
#endif
    return core::unexpected(failure.error);
} catch (...) {
    const auto failure = error("capacity_exceeded");
    if (closePhase_ != ExecutionClosePhase::idle && !closeFirstError_) {
        try {
            closeFirstError_ = failure;
        } catch (...) {
        }
    }
    if (closePhase_ != ExecutionClosePhase::idle && closeNow() >= closeDeadline_)
        closeBudgetExceeded_ = true;
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::firstCause(rpaClose_, rpa::Reason::operationFailed, rpaStage_, failure.code());
    if (rpaClose_.fence.closeState == rpa::CloseStepState::resultPending)
        rpaCaptureFence(rpa::CloseObservationPart::closeStep);
#endif
    return core::unexpected(failure);
}

auto ExecutionHost::audioPublication() const -> core::Result<AudioPublication> {
    if (!owner_.isCurrent() || !audio_)
        return core::unexpected(error("device_pending"));
    return audio_->publication();
}
auto ExecutionHost::snapshotGameplay() -> core::Result<GameplaySnapshot> try {
    checkOwner();
    return value<GameplaySnapshot>(session(SessionSnapshot{}));
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
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
    return closed_                                    ? HostState::closed
           : closePhase_ != ExecutionClosePhase::idle ? HostState::closing
           : suspended_                               ? HostState::suspended
           : host_                                    ? host_->state()
                                                      : HostState::controlPreparing;
}
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
void ExecutionHost::rpaMark(rpa::CloseStage stage) noexcept {
    rpaStage_ = stage;
    rpa::mark(rpaClose_, stage);
}
void ExecutionHost::rpaCause(rpa::Reason reason, const core::Error& failure) noexcept {
    rpa::firstCause(rpaClose_, reason, rpaStage_, failure.code());
}
void ExecutionHost::rpaSyncIdentity() noexcept {
    rpaClose_.identity.valid = true;
    rpaClose_.identity.operationId = control_.operationId;
    rpaClose_.identity.generation = control_.target.expectedGeneration;
    rpaClose_.identity.candidateId = control_.target.candidateId;
    rpaClose_.identity.scope = context_.sessionScope;
    rpaClose_.identity.controlSequence = context_.controlSequence;
    rpaClose_.identity.kind = static_cast<std::uint8_t>(control_.kind);
    rpaClose_.identity.clockSegment = context_.clockSegment;
    rpaClose_.identity.runEpoch = context_.runEpoch;
    rpaClose_.identity.resourceGeneration = context_.resourceGeneration;
    rpaClose_.identity.rendererGeneration = context_.rendererGeneration;
    rpaClose_.identity.contentIdentity = context_.contentIdentity.sha256;
}
void ExecutionHost::rpaCaptureFence(rpa::CloseObservationPart part) noexcept {
    // Diagnostic replies have their own IDs and destinations. They must never occupy the
    // production in-flight slot or be consumed as a later Close step's typed reply.
    if (sessionJob_)
        return;
    const auto harvest = [&]() -> bool {
        auto result = take(session_->poll(*rpaObservationJob_));
        if (!result)
            return false;
        take(session_->acknowledge(*rpaObservationJob_));
        rpaObservationJob_.reset();
        const auto* observation =
            *result ? std::get_if<rpa::OwnerCloseObservation>(&**result) : nullptr;
        if (!observation) {
            if (rpaObservationPart_ == rpa::CloseObservationPart::closeStep)
                rpaClose_.fence.closeState = rpa::CloseStepState::observationMissing;
            return true;
        }
        switch (rpaObservationPart_) {
        case rpa::CloseObservationPart::cleanup:
            rpaClose_.cleanupFence = observation->cleanup;
            break;
        case rpa::CloseObservationPart::closeRetire:
            rpaClose_.closeRetireFence = observation->closeRetire;
            break;
        case rpa::CloseObservationPart::closeStep:
            rpaClose_.fence = observation->closeStep;
            break;
        }
        return true;
    };
    try {
        if (rpaObservationJob_ && !harvest())
            return;
        if (closeNow() >= closeDeadline_)
            return;
        rpaObservationPart_ = part;
        rpaObservationJob_ = take(session_->post(SessionRpaObservation{}, false));
        // A diagnostic-only wait cap; production budget decisions use closeNow/closeDeadline.
        const auto observationLimit =
            std::chrono::steady_clock::now() + std::chrono::milliseconds{5};
        while (!harvest()) {
            if (closeNow() >= closeDeadline_ ||
                std::chrono::steady_clock::now() >= observationLimit)
                return;
            session_->waitForReply(*rpaObservationJob_, std::chrono::milliseconds{1});
        }
    } catch (...) {
        if (part == rpa::CloseObservationPart::closeStep)
            rpaClose_.fence.closeState = rpa::CloseStepState::observationMissing;
    }
    rpaSeekFetch();
}
void ExecutionHost::rpaSeekFetch() noexcept {
    // One control round trip outside every seek phase; its cost is recorded as perturbation.
    const auto started = rpa::nowUs();
    try {
        if (closePhase_ != ExecutionClosePhase::idle) {
            if (sessionJob_ || closeNow() >= closeDeadline_)
                return;
            if (!rpaSeekJob_)
                rpaSeekJob_ = take(session_->post(SessionRpaSeekObservation{}, false));
            const auto limit = std::chrono::steady_clock::now() + std::chrono::milliseconds{5};
            for (;;) {
                auto result = take(session_->poll(*rpaSeekJob_));
                if (result) {
                    take(session_->acknowledge(*rpaSeekJob_));
                    rpaSeekJob_.reset();
                    if (*result) {
                        if (const auto* ledger = std::get_if<rpa::SeekLedger>(&**result))
                            rpa::mergeSeek(rpaSeek_, *ledger);
                    }
                    break;
                }
                if (closeNow() >= closeDeadline_ || std::chrono::steady_clock::now() >= limit)
                    break;
                session_->waitForReply(*rpaSeekJob_, std::chrono::milliseconds{1});
            }
            rpa::seekFetch(rpaSeek_, rpa::nowUs() - started);
            return;
        }
        auto reply = session(SessionRpaSeekObservation{}, false);
        if (const auto* ledger = std::get_if<rpa::SeekLedger>(&reply))
            rpa::mergeSeek(rpaSeek_, *ledger);
    } catch (...) {
    }
    rpa::seekFetch(rpaSeek_, rpa::nowUs() - started);
}
#endif
} // namespace cuexis::player
