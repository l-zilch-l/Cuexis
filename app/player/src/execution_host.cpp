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

} // namespace cuexis::player
