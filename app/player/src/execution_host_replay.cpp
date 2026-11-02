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
                         GameplayTick horizon, const ClockSource& source,
                         std::span<const GameplayCheckpoint> checkpoints)
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
    seekRequest.checkpoints.assign(checkpoints.begin(), checkpoints.end());
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

} // namespace cuexis::player
