#include "audio_owner.hpp"
#include <cmath>
#include <limits>

namespace cuexis::playback::detail {
namespace {
auto failure(std::string_view reason) -> core::Error {
    return {"host.rpa." + std::string{reason}, "Audio owner operation rejected"};
}
} // namespace
AudioOwner::AudioOwner(audio::IAudioTransport& transport, IsolationProof proof,
                       OwnerStepLedger ledger)
    : transport_(&transport), isolationProof_(std::move(proof)), steps_(std::move(ledger)) {}
auto AudioOwner::create(std::uint64_t scope, audio::IAudioTransport& transport,
                        IsolationProof proof, std::size_t capacity, std::size_t reserve)
    -> core::Result<AudioOwner> try {
    if (!proof)
        return core::unexpected(failure("profile_incomplete"));
    auto ledger = OwnerStepLedger::create(scope, Participant::audio, capacity, reserve);
    if (!ledger)
        return core::unexpected(ledger.error());
    return AudioOwner{transport, std::move(proof), std::move(*ledger)};
} catch (...) {
    return core::unexpected(failure("capacity_exceeded"));
}
auto AudioOwner::cancel(const StepRequest& request) -> core::Result<StepReceipt> try {
    return steps_.cancel(request);
} catch (...) {
    steps_.reportAllocationFailure(request.key);
    return core::unexpected(failure("capacity_exceeded"));
}
auto AudioOwner::prepare(const StepRequest& request, audio::AudioClipLease lease)
    -> core::Result<StepReceipt> {
    return execute(request, StepPhase::prepare, std::move(lease));
}
auto AudioOwner::activate(const StepRequest& request) -> core::Result<StepReceipt> {
    return execute(request, StepPhase::activate);
}
auto AudioOwner::prepareActive(const StepRequest& request) -> core::Result<StepReceipt> {
    return execute(request, StepPhase::prepare, {}, true);
}
auto AudioOwner::pause(const StepRequest& request) -> core::Result<StepReceipt> {
    return execute(request, StepPhase::fence);
}
auto AudioOwner::stop(const StepRequest& request) -> core::Result<StepReceipt> {
    return execute(request, StepPhase::stop);
}
auto AudioOwner::isolate(const StepRequest& request) -> core::Result<StepReceipt> {
    return execute(request, StepPhase::isolate);
}
auto AudioOwner::retire(const StepRequest& request) -> core::Result<StepReceipt> {
    return execute(request, StepPhase::retire);
}
auto AudioOwner::close(const StepRequest& request) -> core::Result<StepReceipt> {
    return execute(request, StepPhase::close);
}
auto AudioOwner::execute(const StepRequest& request, StepPhase phase, audio::AudioClipLease lease,
                         bool activeReadiness) -> core::Result<StepReceipt> {
    if (!owner_.isCurrent())
        return core::unexpected(failure("owner_mismatch"));
    StepReceipt receipt;
    bool started = false;
    try {
        if (phase == StepPhase::prepare && lease &&
            (!request.target.audioClip || lease.handle() != *request.target.audioClip))
            return core::unexpected(failure("message_conflict"));
        auto recorded = steps_.apply(request);
        if (!recorded)
            return core::unexpected(recorded.error());
        const bool isolationProbe =
            phase == StepPhase::isolate && recorded->status == StepStatus::inProgress;
        if (recorded->status != StepStatus::pending && !isolationProbe)
            return *recorded;
        receipt = *recorded;
        const bool sameOperation = operation_ && *operation_ == request.key.operationId;
        const bool staleOperation = operation_ && request.key.operationId < *operation_;
        const bool invalidGeneration =
            (request.retryGeneration &&
             (phase == StepPhase::prepare || phase == StepPhase::fence ||
              phase == StepPhase::activate) &&
             *request.retryGeneration != generation_) ||
            (sameOperation ? (!operationTarget_ || request.target != *operationTarget_)
                           : generationFor(request.target, Participant::audio) != generation_);
        const long double positionUs =
            request.target.audioPositionMs
                ? static_cast<long double>(*request.target.audioPositionMs) * 1000.0L
                : 0.0L;
        const bool selectsPrepared = prepared_ && candidate_ == request.target.candidateId;
        const auto* activationClip = selectsPrepared ? prepared_.get() : lease_.get();
        const bool selectsActive = lease_ && activeCandidate_ == request.target.candidateId;
        const auto activeState = transport_->snapshot().source.state;
        const bool retirePrepared = phase == StepPhase::retire && selectsPrepared;
        const bool retireAll = phase == StepPhase::retire && request.target.candidateId == 0;
        const bool wrongPosition =
            phase == StepPhase::activate &&
            (!request.target.audioPositionMs || !std::isfinite(*request.target.audioPositionMs) ||
             *request.target.audioPositionMs < 0 || !activationClip ||
             *request.target.audioPositionMs > activationClip->durationMs() ||
             positionUs > static_cast<long double>(INT64_MAX));
        const bool invalid =
            closed_ || request.key.phase != phase || invalidGeneration || staleOperation ||
            (phase == StepPhase::prepare &&
             (uncertain_ || !request.target.audioClip || prepared_ || !request.target.candidateId ||
              (activeReadiness ? (!selectsActive || !loaded_ || !stopped_ ||
                                  lease_.handle() != *request.target.audioClip ||
                                  (activeState != audio::PlaybackState::Paused &&
                                   activeState != audio::PlaybackState::Stopped))
                               : (!lease || activeCandidate_ == request.target.candidateId)))) ||
            (phase == StepPhase::activate &&
             (wrongPosition || uncertain_ || (!selectsPrepared && !selectsActive) ||
              !request.target.audioClip ||
              (selectsPrepared ? prepared_.handle() : lease_.handle()) !=
                  *request.target.audioClip ||
              ((selectsPrepared || !loaded_) && generation_ == UINT64_MAX) ||
              (selectsPrepared && lease_ && (!stopped_ || !isolated_)))) ||
            (phase == StepPhase::fence && (!loaded_ && !prepared_)) ||
            (phase == StepPhase::retire &&
             ((!retirePrepared && !selectsActive && !retireAll) ||
              (!retirePrepared && (!stopped_ || !isolated_)) ||
              (retirePrepared &&
               (!request.target.audioClip || *request.target.audioClip != prepared_.handle())) ||
              (selectsActive &&
               (!request.target.audioClip || *request.target.audioClip != lease_.handle())))) ||
            (phase == StepPhase::close &&
             (!retired_ || prepared_ || lease_ || loaded_ || !isolated_));
        if (invalid) {
            receipt.status = StepStatus::rejectedNotApplied;
            receipt.cause = failure("context_stale");
        } else {
            if (retirePrepared || (retireAll && prepared_)) {
                auto activation = *preparedBinding_;
                activation.key.phase = StepPhase::activate;
                auto canceled = steps_.cancel(activation);
                if (!canceled)
                    return core::unexpected(canceled.error());
                if (canceled->status != StepStatus::canceledNotApplied &&
                    canceled->status != StepStatus::rejectedNotApplied &&
                    canceled->status != StepStatus::failed) {
                    receipt.status = StepStatus::rejectedNotApplied;
                    receipt.cause = failure("control_pending");
                    auto completed = steps_.complete(receipt);
                    if (!completed)
                        return core::unexpected(completed.error());
                    return receipt;
                }
            }
            if (!isolationProbe) {
                auto start = steps_.start(request);
                if (!start)
                    return core::unexpected(start.error());
                if (!*start)
                    return steps_.query(request.key);
            }
            started = true;
            if (!sameOperation) {
                operation_ = request.key.operationId;
                operationTarget_ = request.target;
            }
            receipt.status = StepStatus::applied;
            const auto invoke = [&](core::Result<void> result) {
                if (!result) {
                    uncertain_ = true;
                    receipt.status = StepStatus::failed;
                    receipt.cause = result.error();
                    return false;
                }
                return true;
            };
            switch (phase) {
            case StepPhase::prepare:
                if (activeReadiness)
                    break;
                preparedBinding_ = request;
                prepared_ = std::move(lease);
                candidate_ = request.target.candidateId;
                retired_ = false;
                break;
            case StepPhase::activate:
                // A failed backend call can have partially acted. Keep all leases and require
                // explicit isolation/cleanup; Unknown is never retried by this owner.
                if (selectsPrepared) {
                    // Precommit prepare retains the old active clip. Only an explicit fence
                    // and callback-isolation proof permit replacing the backend binding.
                    if (lease_ && !invoke(transport_->unload()))
                        break;
                    loaded_ = false;
                    lease_.reset();
                    activeCandidate_.reset();
                    lease_ = std::move(prepared_);
                    preparedBinding_.reset();
                    activeCandidate_ = candidate_;
                    candidate_.reset();
                }
                isolated_ = false;
                stopped_ = false;
                if (loaded_ || invoke(transport_->load(lease_.handle()))) {
                    if (!loaded_)
                        ++generation_;
                    loaded_ = true;
                    if (invoke(transport_->seekMs(*request.target.audioPositionMs))) {
                        receipt.effects.position =
                            static_cast<std::int64_t>(std::llround(positionUs));
                        static_cast<void>(invoke(transport_->play()));
                    }
                }
                break;
            case StepPhase::fence:
                if ((!uncertain_ && (!loaded_ || stopped_)) || invoke(transport_->pause()))
                    stopped_ = true;
                break;
            case StepPhase::stop:
                if ((!uncertain_ && !loaded_) || invoke(transport_->stop()))
                    stopped_ = true;
                break;
            case StepPhase::isolate: {
                auto proof = isolationProof_();
                if (!proof) {
                    receipt.status = StepStatus::failed;
                    receipt.cause = proof.error();
                } else if (!*proof) {
                    // This is a repeated proof query, never a repeated transport mutation.
                    return steps_.query(request.key);
                } else {
                    isolated_ = true;
                }
                break;
            }
            case StepPhase::retire:
                if (retirePrepared) {
                    prepared_.reset();
                    preparedBinding_.reset();
                    candidate_.reset();
                    receipt.effects.callbackIsolated = true;
                    receipt.effects.referencesReleased = true;
                    retired_ = !lease_;
                    break;
                }
                if (invoke(transport_->unload())) {
                    loaded_ = false;
                    lease_.reset();
                    activeCandidate_.reset();
                    if (retireAll) {
                        prepared_.reset();
                        preparedBinding_.reset();
                        candidate_.reset();
                    }
                    retired_ = !prepared_;
                    uncertain_ = false;
                }
                break;
            case StepPhase::close:
                closed_ = true;
                break;
            default:
                break;
            }
            receipt.effects.generation = generation_;
            if (!retirePrepared) {
                receipt.effects.callbackIsolated = isolated_;
                receipt.effects.referencesReleased = retired_ && !lease_ && !loaded_ && !prepared_;
            }
        }
        auto complete = steps_.complete(receipt);
        if (!complete)
            return core::unexpected(complete.error());
        return receipt;
    } catch (...) {
        if (started) {
            uncertain_ = true;
            receipt.status = StepStatus::unknown;
            receipt.effects.generation = generation_;
            receipt.effects.callbackIsolated = isolated_;
            receipt.effects.referencesReleased = retired_ && !lease_ && !loaded_ && !prepared_;
            receipt.cause = failure("control_pending");
            auto complete = steps_.complete(receipt);
            if (!complete)
                return core::unexpected(complete.error());
            return receipt;
        }
        steps_.reportAllocationFailure(request.key);
        return core::unexpected(failure("capacity_exceeded"));
    }
}
} // namespace cuexis::playback::detail
