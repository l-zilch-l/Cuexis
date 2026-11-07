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
        // Startup failure seals admission and drains replies before publishing exit.
        // Do not bind a participant that can no longer accept its directed Close.
        // Only the actual exit permits omission; its wait keeps the original Close budget.
        if (audio_ && audio_->admissionClosed()) {
            while (!audio_->exited()) {
                if (closeNow() >= closeDeadline_) {
                    pump(closeDeadline_);
                    throw Failure{error("control_pending")};
                }
                pump(closeDeadline_);
            }
        }
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

} // namespace cuexis::player
