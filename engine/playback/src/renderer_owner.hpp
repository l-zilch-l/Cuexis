#pragma once

#include "host_candidate.hpp"
#include <utility>

namespace cuexis::playback::detail {
// The adapter and all device-owned candidate destruction stay on this owner's thread.
template <typename Renderer> class RendererOwner final {
  public:
    using PrepareResult = decltype(std::declval<Renderer&>().prepareOwned(
        std::declval<const PresentationCandidateInput&>()));
    using Prepared = typename PrepareResult::value_type;
    [[nodiscard]] static auto create(std::uint64_t scope, Renderer& renderer, std::size_t capacity,
                                     std::size_t reserve) -> core::Result<RendererOwner> try {
        auto ledger = OwnerStepLedger::create(scope, Participant::renderer, capacity, reserve);
        if (!ledger)
            return core::unexpected(ledger.error());
        return RendererOwner{renderer, std::move(*ledger)};
    } catch (...) {
        return core::unexpected(failure("capacity_exceeded"));
    }
    RendererOwner(RendererOwner&&) noexcept = default;
    RendererOwner(const RendererOwner&) = delete;
    [[nodiscard]] auto prepare(const StepRequest& request, const PresentationCandidateInput& input)
        -> core::Result<StepReceipt> {
        return execute(request, StepPhase::prepare, &input);
    }
    // Readiness for a control retaining the already active device resources.
    [[nodiscard]] auto prepareActive(const StepRequest& request) -> core::Result<StepReceipt> {
        return execute(request, StepPhase::prepare, nullptr, true);
    }
    [[nodiscard]] auto fence(const StepRequest& request) -> core::Result<StepReceipt> {
        return execute(request, StepPhase::fence);
    }
    [[nodiscard]] auto activate(const StepRequest& request) -> core::Result<StepReceipt> {
        return execute(request, StepPhase::activate);
    }
    [[nodiscard]] auto retire(const StepRequest& request) -> core::Result<StepReceipt> {
        return execute(request, StepPhase::retire);
    }
    [[nodiscard]] auto close(const StepRequest& request) -> core::Result<StepReceipt> {
        return execute(request, StepPhase::close);
    }
    [[nodiscard]] auto cancel(const StepRequest& request) -> core::Result<StepReceipt> try {
        // Cancel binds a tombstone only. A separate retire step releases prepared GPU data.
        return steps_.cancel(request);
    } catch (...) {
        steps_.reportAllocationFailure(request.key);
        return core::unexpected(allocationFailure_);
    }
    [[nodiscard]] auto steps() noexcept -> OwnerStepLedger& {
        return steps_;
    }

  private:
    RendererOwner(Renderer& renderer, OwnerStepLedger ledger)
        : renderer_(&renderer), steps_(std::move(ledger)) {}
    [[nodiscard]] static auto failure(std::string_view code) -> core::Error {
        return {"host.rpa." + std::string{code}, "Renderer owner operation rejected"};
    }
    [[nodiscard]] auto execute(const StepRequest& request, StepPhase phase,
                               const PresentationCandidateInput* input = nullptr,
                               bool prepareActive = false) -> core::Result<StepReceipt> {
        if (!owner_.isCurrent())
            return core::unexpected(failure("owner_mismatch"));
        StepReceipt receipt;
        bool started = false;
        try {
            const auto identity = renderer_->identity();
            auto context = request.target.nextContext.value_or(request.target.context);
            if (request.retryGeneration)
                context.rendererGeneration = *request.retryGeneration;
            const bool cleanup = phase == StepPhase::retire || phase == StepPhase::close;
            const bool allCandidates = cleanup && request.target.candidateId == 0;
            const bool selectsPending =
                cleanup && input_ &&
                (allCandidates || request.target.candidateId == input_->candidateId);
            const bool selectsActive =
                cleanup && activeInput_ &&
                (allCandidates || request.target.candidateId == activeInput_->candidateId);
            const auto* selected =
                selectsPending  ? &*pendingBinding_
                : selectsActive ? &*activeBinding_
                : cleanup && retired_ && retirementBinding_ &&
                        identity.generation == retirementGeneration_ &&
                        (allCandidates ||
                         request.target.candidateId == retirementBinding_->target.candidateId)
                    ? &*retirementBinding_
                    : nullptr;
            const auto previousContext =
                activeBinding_
                    ? activeBinding_->target.nextContext.value_or(activeBinding_->target.context)
                    : HostContext{};
            if (input &&
                (input->context != context || input->candidateId != request.target.candidateId ||
                 (request.target.payloadIdentity &&
                  *request.target.payloadIdentity != input->context.contentIdentity)))
                return core::unexpected(failure("message_conflict"));
            auto recorded = steps_.apply(request);
            if (!recorded)
                return core::unexpected(recorded.error());
            if (recorded->status != StepStatus::pending)
                return *recorded;
            receipt = *recorded;
            const bool sameOperation = operation_ && *operation_ == request.key.operationId;
            const bool staleOperation = operation_ && request.key.operationId < *operation_;
            const bool invalidTarget =
                sameOperation
                    ? (!target_ || *target_ != request.target)
                    : generationFor(request.target, Participant::renderer) != identity.generation;
            bool failedPendingCleanup = false;
            if (phase == StepPhase::retire && selectsPending && pendingBinding_ &&
                request.key.operationId == pendingBinding_->key.operationId) {
                auto activationKey = pendingBinding_->key;
                activationKey.phase = StepPhase::activate;
                auto activation = steps_.query(activationKey);
                failedPendingCleanup = activation && activation->status == StepStatus::failed;
            }
            const bool retiredClose =
                cleanup && retired_ && retirementBinding_ &&
                request.key.operationId == retirementBinding_->key.operationId &&
                request.target == retirementBinding_->target &&
                identity.generation == retirementGeneration_;
            const bool wrongCleanup =
                cleanup &&
                ((generationFor(request.target, Participant::renderer) != identity.generation &&
                  !failedPendingCleanup && !retiredClose) ||
                 (selected && !allCandidates &&
                  ((request.key.operationId == selected->key.operationId &&
                    request.target != selected->target) ||
                   context.sessionScope != selected->target.context.sessionScope ||
                   context.contentIdentity !=
                       selected->target.nextContext.value_or(selected->target.context)
                           .contentIdentity ||
                   context.resourceGeneration !=
                       selected->target.nextContext.value_or(selected->target.context)
                           .resourceGeneration ||
                   (request.key.operationId != selected->key.operationId &&
                    context.controlSequence <=
                        selected->target.nextContext.value_or(selected->target.context)
                            .controlSequence))) ||
                 (phase == StepPhase::retire && !allCandidates && !selected) ||
                 (phase == StepPhase::retire && selectsActive && input_ && !allCandidates) ||
                 (allCandidates &&
                  ((pendingBinding_ &&
                    request.key.operationId != pendingBinding_->key.operationId &&
                    context.controlSequence <= pendingBinding_->target.nextContext
                                                   .value_or(pendingBinding_->target.context)
                                                   .controlSequence) ||
                   (activeBinding_ && request.key.operationId != activeBinding_->key.operationId &&
                    context.controlSequence <=
                        activeBinding_->target.nextContext.value_or(activeBinding_->target.context)
                            .controlSequence))));
            const bool activeFence =
                phase == StepPhase::fence && !prepared_ && renderer_->hasActivePresentation();
            const bool wrongActiveFence =
                activeFence &&
                (!activeInput_ || context.rendererGeneration != identity.generation ||
                 request.target.candidateId != activeInput_->candidateId ||
                 context.contentIdentity != previousContext.contentIdentity ||
                 context.resourceGeneration != previousContext.resourceGeneration ||
                 (!sameOperation && context.controlSequence <= previousContext.controlSequence));
            const bool activeActivation = phase == StepPhase::activate && !prepared_ &&
                                          sameOperation && renderer_->hasActivePresentation();
            const bool wrongActiveActivation =
                activeActivation &&
                (!activeInput_ ||
                 request.retryGeneration.value_or(
                     generationFor(request.target, Participant::renderer)) != identity.generation ||
                 context.rendererGeneration != identity.generation ||
                 request.target.candidateId != activeInput_->candidateId ||
                 context.contentIdentity != activeInput_->context.contentIdentity ||
                 context.resourceGeneration != activeInput_->context.resourceGeneration);
            const bool activePreparation = phase == StepPhase::prepare && prepareActive;
            const bool wrongActivePreparation =
                activePreparation &&
                (input || prepared_ || input_ || uncertain_ || !activeInput_ || !activeBinding_ ||
                 !renderer_->hasActivePresentation() ||
                 activeInput_->rendererInstance != identity.instance ||
                 request.target.candidateId != activeInput_->candidateId ||
                 context.sessionScope != previousContext.sessionScope ||
                 context.contentIdentity != previousContext.contentIdentity ||
                 context.resourceGeneration != previousContext.resourceGeneration ||
                 context.rendererGeneration != identity.generation ||
                 (!sameOperation && context.controlSequence <= previousContext.controlSequence));
            const bool invalid =
                closed_ || (!cleanup && (staleOperation || invalidTarget)) || wrongCleanup ||
                wrongActiveFence ||
                (request.retryGeneration && *request.retryGeneration != identity.generation) ||
                wrongActiveActivation || wrongActivePreparation || request.key.phase != phase ||
                (!request.target.candidateId && !cleanup) ||
                (phase == StepPhase::prepare && !activePreparation &&
                 (!input || prepared_ || uncertain_ ||
                  input->rendererInstance != identity.instance ||
                  input->context.rendererGeneration != identity.generation)) ||
                (phase == StepPhase::fence &&
                 ((!activeFence && (!sameOperation || !prepared_)) || uncertain_)) ||
                (phase == StepPhase::activate &&
                 (!sameOperation || (!prepared_ && !activeActivation) || uncertain_)) ||
                (phase == StepPhase::activate && !fenced_) ||
                (phase == StepPhase::close &&
                 (!retired_ || prepared_ || input_ || activeInput_ || uncertain_));
            if (invalid) {
                receipt.status = StepStatus::rejectedNotApplied;
                receipt.cause = failure("context_stale");
            } else {
                // A pending candidate may be discarded only after its exact activation
                // target is canceled, or after an activation has definitively failed.
                if (phase == StepPhase::retire && selectsPending) {
                    auto activation = *pendingBinding_;
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
                // Reserve/copy operation bindings before the owner start point.
                auto target = request.target;
                std::optional<PresentationCandidateInput> retained;
                if (input)
                    retained = *input;
                auto start = steps_.start(request);
                if (!start)
                    return core::unexpected(start.error());
                if (!*start)
                    return steps_.query(request.key);
                started = true;
                if (!sameOperation) {
                    operation_ = request.key.operationId;
                    target_ = std::move(target);
                }
                receipt.status = StepStatus::applied;
                const auto failed = [&](const core::Error& cause) {
                    receipt.status = StepStatus::failed;
                    receipt.cause = cause;
                    uncertain_ = true;
                };
                switch (phase) {
                case StepPhase::prepare: {
                    if (activePreparation) {
                        fenced_ = false;
                        break;
                    }
                    input_ = std::move(retained);
                    pendingBinding_ = request;
                    retired_ = false;
                    fenced_ = false;
                    auto candidate = renderer_->prepareOwned(*input_);
                    if (!candidate)
                        failed(candidate.error());
                    else
                        prepared_ = std::move(*candidate);
                    break;
                }
                case StepPhase::fence: {
                    if (!prepared_)
                        fenced_ = true;
                    else {
                        auto accepted = renderer_->accepts(*prepared_);
                        if (!accepted)
                            failed(accepted.error());
                        else
                            fenced_ = true;
                    }
                    break;
                }
                case StepPhase::activate: {
                    // Same-content Resume acknowledges the already active presentation.
                    // The coordinator still owns the fence; no second GPU activation occurs.
                    if (activeActivation) {
                        activeBinding_ = request;
                        break;
                    }
                    auto accepted = renderer_->accepts(*prepared_);
                    if (!accepted)
                        failed(accepted.error());
                    else {
                        renderer_->activate(std::move(*prepared_));
                        prepared_.reset();
                        activeInput_ = std::move(input_);
                        input_.reset();
                        activeBinding_ = request;
                        pendingBinding_.reset();
                    }
                    break;
                }
                case StepPhase::retire: {
                    if (selectsPending) {
                        // Candidate destruction stays on the GPU owner; the old active
                        // presentation survives a canceled Reload preparation.
                        if (prepared_ && prepared_->valid())
                            renderer_->discard(std::move(*prepared_));
                        prepared_.reset();
                        input_.reset();
                        pendingBinding_.reset();
                    }
                    if (selectsActive || allCandidates || !activeInput_) {
                        auto rebuilt = renderer_->rebuild();
                        if (!rebuilt)
                            failed(rebuilt.error());
                        else {
                            activeInput_.reset();
                            activeBinding_.reset();
                            retired_ = !input_ && !prepared_;
                            retirementGeneration_ = renderer_->identity().generation;
                            retirementBinding_ = request;
                            uncertain_ = false;
                            fenced_ = false;
                        }
                    } else {
                        retired_ = false;
                        uncertain_ = false;
                        fenced_ = false;
                    }
                    if (receipt.status == StepStatus::applied) {
                        receipt.effects.referencesReleased =
                            (!selectsPending || !input_) && (!selectsActive || !activeInput_);
                        receipt.effects.callbackIsolated = true;
                    }
                    break;
                }
                case StepPhase::close: {
                    auto closed = renderer_->close();
                    if (!closed)
                        failed(closed.error());
                    else
                        closed_ = true;
                    break;
                }
                default:
                    break;
                }
                receipt.effects.generation = renderer_->identity().generation;
                if (phase != StepPhase::retire) {
                    receipt.effects.referencesReleased =
                        retired_ && !prepared_ && !input_ && !activeInput_;
                    receipt.effects.callbackIsolated = retired_;
                }
            }
            auto completed = steps_.complete(receipt);
            if (!completed)
                return core::unexpected(completed.error());
            return receipt;
        } catch (...) {
            uncertain_ = started || uncertain_;
            if (started) {
                receipt.status = StepStatus::unknown;
                auto recorded = steps_.complete(receipt);
                if (!recorded)
                    steps_.reportAllocationFailure(request.key);
            } else
                steps_.reportAllocationFailure(request.key);
            return core::unexpected(allocationFailure_);
        }
    }
    core::ThreadChecker owner_;
    Renderer* renderer_;
    OwnerStepLedger steps_;
    std::optional<Prepared> prepared_;
    std::optional<PresentationCandidateInput> input_, activeInput_;
    std::optional<StepRequest> pendingBinding_, activeBinding_, retirementBinding_;
    std::uint64_t retirementGeneration_{};
    std::optional<std::uint64_t> operation_;
    std::optional<StepTarget> target_;
    bool retired_{}, closed_{}, uncertain_{}, fenced_{};
    core::Error allocationFailure_{"host.rpa.capacity_exceeded",
                                   "Reserved renderer allocation fault"};
};
} // namespace cuexis::playback::detail
