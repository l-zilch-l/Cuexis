#include "capture_route.hpp"
#include <algorithm>
#include <cuexis_internal/sha256.hpp>
#include <span>
#include <tuple>

namespace cuexis::playback::detail {
auto CaptureRouteOwner::canSwitchClock(GameplayExecutionContext next) const noexcept -> bool {
    return owner_.isCurrent() && !closed_ && !paused_ && ordinaryAllowed_ && !terminated_ &&
           fault_.reason == FaultSlot::Reason::none && pending_.empty() &&
           next.sessionScope == context_.sessionScope && next.runEpoch == context_.runEpoch &&
           next.clockSegment > context_.clockSegment;
}
auto CaptureRouteOwner::switchClock(GameplayExecutionContext next) -> core::Result<void> {
    if (!canSwitchClock(next))
        return core::unexpected(
            core::Error{"host.rpa.control_pending", "Capture clock transition not ready"});
    // The open Tick's owning occurrences survive the continuous clock transition.
    context_ = next;
    return {};
}
namespace {
auto error(std::string_view code) -> core::Error {
    return {"host.rpa." + std::string{code}, "Capture route protocol rejected"};
}
} // namespace
auto reconciliationIdentity(const GameplayReconciliation& r)
    -> core::Result<PreparedSemanticIdentity> {
    return computeGameplayReconciliationIdentity(r);
}
CaptureRouteOwner::CaptureRouteOwner(GameplayExecutionContext c, std::size_t capacity,
                                     OwnerStepLedger ledger, std::size_t dataReserve)
    : context_(c), capacity_(capacity), dataControlReserve_(dataReserve),
      steps_(std::move(ledger)) {
    batch_.reserve(capacity);
    tickMembers_.reserve(capacity);
    trace_.reserve(capacity);
    pending_.reserve(capacity);
}
auto CaptureRouteOwner::create(GameplayExecutionContext c, std::size_t capacity,
                               std::size_t reserve, std::size_t dataReserve)
    -> core::Result<CaptureRouteOwner> try {
    if (!c.clockSegment || !c.runEpoch || dataReserve >= capacity)
        return core::unexpected(error("context_stale"));
    auto ledger = OwnerStepLedger::create(c.sessionScope, Participant::capture, capacity, reserve);
    if (!ledger)
        return core::unexpected(ledger.error());
    return CaptureRouteOwner{c, capacity, std::move(*ledger), dataReserve};
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::enqueue(GameplayCaptureDelivery value, bool control) -> core::Result<void> {
    const auto limit = control ? capacity_ : capacity_ - dataControlReserve_;
    if (pending_.size() >= limit || sequence_ == UINT64_MAX) {
        fault_ = {FaultSlot::Reason::captureGap, {}};
        ordinaryAllowed_ = false;
        return core::unexpected(error("capacity_exceeded"));
    }
    value.deliverySequence = sequence_;
    pending_.push_back(std::move(value));
    ++sequence_;
    return {};
}
auto CaptureRouteOwner::prepare(const StepRequest& request) -> core::Result<StepReceipt> {
    return readiness(request, StepPhase::prepare);
}
auto CaptureRouteOwner::fence(const StepRequest& request) -> core::Result<StepReceipt> {
    return readiness(request, StepPhase::fence);
}
auto CaptureRouteOwner::readiness(const StepRequest& request, StepPhase phase)
    -> core::Result<StepReceipt> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    auto registered = steps_.apply(request);
    if (!registered)
        return core::unexpected(registered.error());
    if (registered->status != StepStatus::pending)
        return *registered;
    bool valid = !closed_ && fault_.reason == FaultSlot::Reason::none &&
                 request.key.phase == phase &&
                 request.target.context.sessionScope == context_.sessionScope;
    if (phase == StepPhase::prepare) {
        valid = valid && request.target.context.clockSegment == context_.clockSegment &&
                request.target.context.runEpoch == context_.runEpoch &&
                generationFor(request.target, Participant::capture) == context_.runEpoch &&
                request.target.tick.has_value() == request.target.pauseEpoch.has_value();
        if (request.target.tick)
            valid = valid && (paused_ ? (cutTick_ && *request.target.tick == cutTick_->value &&
                                         *request.target.pauseEpoch == pauseEpoch_)
                                      : (!lastTick_ || *request.target.tick >= lastTick_->value));
    } else {
        auto boundary = request;
        boundary.key.phase = StepPhase::cut;
        auto cutProof = steps_.query(boundary.key);
        boundary.key.phase = StepPhase::resumeRoute;
        auto routeProof = steps_.query(boundary.key);
        valid = valid && ((cutProof && cutProof->request.target == request.target &&
                           cutProof->status == StepStatus::applied) ||
                          (routeProof && routeProof->request.target == request.target &&
                           routeProof->status == StepStatus::applied));
    }
    auto receipt = *registered;
    if (!valid) {
        receipt.status = StepStatus::rejectedNotApplied;
        receipt.cause = error("context_stale");
    } else {
        auto started = steps_.start(request);
        if (!started)
            return core::unexpected(started.error());
        if (!*started)
            return steps_.query(request.key);
        receipt.status = StepStatus::applied;
        receipt.effects.generation = context_.runEpoch;
    }
    auto done = steps_.complete(receipt);
    if (!done)
        return core::unexpected(done.error());
    return receipt;
} catch (...) {
    steps_.reportAllocationFailure(request.key);
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::exportCloseEvidence() const -> core::Result<CaptureCloseEvidence> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    CaptureCloseEvidence result;
    result.owner_ = this;
    result.context_ = context_;
    result.captured_ = captured_;
    result.sequence_ = sequence_;
    result.pending_ = pending_;
    result.tentative_ = tickMembers_;
    result.fault_ = fault_;
    result.trace_ = trace_;
    result.retained_ = batch_;
    return result;
} catch (...) {
    fault_ = {FaultSlot::Reason::allocation, {}};
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::close(const StepRequest& request, const ControlReceipt& proof,
                              const CaptureCloseEvidence& evidence)
    -> core::Result<StepReceipt> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    if (auto previous = steps_.query(request.key); previous)
        return steps_.apply(request);
    const auto& cutProof = proof.steps[0][static_cast<std::size_t>(StepPhase::cut)];
    auto cutKey = request.key;
    cutKey.phase = StepPhase::cut;
    auto actualCut = steps_.query(cutKey);
    const auto& softwareRetire = proof.steps[static_cast<std::size_t>(Participant::software)]
                                            [static_cast<std::size_t>(StepPhase::retire)];
    const bool softwareReleased =
        !proof.request.participants[static_cast<std::size_t>(Participant::software)] ||
        (softwareRetire && softwareRetire->status == StepStatus::applied &&
         softwareRetire->effects.referencesReleased &&
         softwareRetire->request.key == StepKey{request.key.sessionScope, request.key.operationId,
                                                Participant::software, StepPhase::retire, 0} &&
         softwareRetire->request.target == request.target);
    if (request.key.phase != StepPhase::close || proof.request.kind != ControlKind::close ||
        (proof.status != ControlStatus::preparing && proof.status != ControlStatus::suspended) ||
        proof.request.context != request.target.context ||
        proof.request.operationId != request.key.operationId ||
        proof.request.target != request.target || !cutProof ||
        cutProof->status != StepStatus::applied || !actualCut ||
        actualCut->status != StepStatus::applied || actualCut->request != cutProof->request ||
        actualCut->effects != cutProof->effects ||
        cutProof->request.key.operationId != request.key.operationId ||
        cutProof->request.target != request.target || !cutTick_ || !paused_ ||
        request.target.context.sessionScope != context_.sessionScope ||
        request.target.context.clockSegment != context_.clockSegment ||
        request.target.context.runEpoch != context_.runEpoch || evidence.owner_ != this ||
        evidence.context_ != context_ || evidence.captured_ != captured_ ||
        evidence.sequence_ != sequence_ || evidence.pending_.size() != pending_.size() ||
        evidence.trace_.size() != trace_.size() || evidence.retained_.size() != batch_.size() ||
        evidence.tentative_.size() != tickMembers_.size() ||
        (!pending_.empty() &&
         !(softwareReleased &&
           (proof.request.participants[static_cast<std::size_t>(Participant::software)] ||
            fault_.reason != FaultSlot::Reason::none))))
        return core::unexpected(error("control_pending"));
    auto started = steps_.start(request);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return steps_.query(request.key);
    ordinaryAllowed_ = false;
    terminated_ = closed_ = true;
    std::vector<GameplayOccurrence>{}.swap(batch_);
    std::vector<GameplayOccurrence>{}.swap(tickMembers_);
    std::vector<GameplayPauseTransition>{}.swap(trace_);
    std::vector<GameplayCaptureDelivery>{}.swap(pending_);
    StepReceipt receipt{request, StepStatus::applied, {}, {}};
    receipt.effects.generation = context_.runEpoch;
    receipt.effects.callbackIsolated = true;
    receipt.effects.referencesReleased = true;
    receipt.effects.cut = cutCaptured_;
    receipt.effects.pauseEpoch = pauseEpoch_;
    auto done = steps_.complete(receipt);
    if (!done)
        return core::unexpected(done.error());
    return receipt;
} catch (...) {
    steps_.reportAllocationFailure(request.key);
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::closeBatch(GameplayTick observed, bool terminated) -> core::Result<void> {
    if (batch_.empty())
        return {};
    if (observed.value < batch_.front().input.observationTick.value ||
        (!terminated && observed.value == batch_.front().input.observationTick.value) ||
        batchId_ == UINT64_MAX)
        return core::unexpected(error("message_conflict"));
    GameplayClosedBatch value{GameplayTick{batch_.front().input.observationTick.value},
                              observed,
                              batchId_,
                              0,
                              1,
                              static_cast<std::uint64_t>(batch_.size()),
                              batch_,
                              terminated,
                              terminated ? pauseEpoch_ : 0};
    auto result = enqueue({context_, 0, std::move(value)});
    if (!result)
        return result;
    batch_.clear();
    ++batchId_;
    return {};
}
auto CaptureRouteOwner::capture(GameplayInput input, bool valid) -> core::Result<void> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    if (!valid || input.crossedSamplingGap || input.reconnected || input.droppedSamples ||
        terminated_ || fault_.reason != FaultSlot::Reason::none) {
        fault_ = {FaultSlot::Reason::captureGap, {}};
        ordinaryAllowed_ = false;
        return core::unexpected(error("capture_gap"));
    }
    if (captured_ == UINT64_MAX || (lastTick_ && input.observationTick.value < lastTick_->value))
        return core::unexpected(error("message_conflict"));
    if (!tickMembers_.empty() &&
        tickMembers_.front().input.observationTick.value != input.observationTick.value)
        tickMembers_.clear();
    if (tickMembers_.size() == capacity_) {
        fault_ = {FaultSlot::Reason::captureGap, {}};
        ordinaryAllowed_ = false;
        return core::unexpected(error("capture_gap"));
    }
    std::uint64_t position = 0;
    for (const auto& m : tickMembers_)
        if (std::tie(m.input.domain, m.input.sourceClass, m.input.channel) ==
            std::tie(input.domain, input.sourceClass, input.channel))
            ++position;
    tickMembers_.push_back({input, position});
    if (paused_) {
        if (trace_.size() == capacity_) {
            fault_ = {FaultSlot::Reason::captureGap, {}};
            return core::unexpected(error("capture_gap"));
        }
        trace_.push_back({static_cast<std::uint64_t>(trace_.size()), {std::move(input), position}});
    } else {
        if (!batch_.empty() &&
            input.observationTick.value > batch_.front().input.observationTick.value) {
            auto closed = closeBatch(GameplayTick{input.observationTick.value}, false);
            if (!closed)
                return closed;
        }
        if (batch_.size() == capacity_) {
            fault_ = {FaultSlot::Reason::captureGap, {}};
            return core::unexpected(error("capture_gap"));
        }
        batch_.push_back({std::move(input), position});
    }
    ++captured_;
    lastTick_ = GameplayTick{paused_ ? trace_.back().occurrence.input.observationTick.value
                                     : batch_.back().input.observationTick.value};
    return {};
} catch (...) {
    fault_ = {FaultSlot::Reason::allocation, {}};
    ordinaryAllowed_ = false;
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::prefix(GameplayTick observed) -> core::Result<void> try {
    if (!owner_.isCurrent() || paused_ || !ordinaryAllowed_ ||
        fault_.reason != FaultSlot::Reason::none ||
        (lastTick_ && observed.value < lastTick_->value))
        return core::unexpected(error("control_pending"));
    auto closed = closeBatch(observed, false);
    if (!closed)
        return closed;
    auto result = enqueue({context_, 0, GameplayCapturePrefix{observed}});
    if (result)
        lastTick_ = observed;
    return result;
} catch (...) {
    fault_ = {FaultSlot::Reason::allocation, {}};
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::cut(const StepRequest& r) -> core::Result<StepReceipt> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    if (auto previous = steps_.query(r.key); previous)
        return steps_.apply(r);
    if (closed_ || r.target.context.sessionScope != context_.sessionScope ||
        r.target.context.clockSegment != context_.clockSegment ||
        r.target.context.runEpoch != context_.runEpoch || !r.target.tick || !r.target.pauseEpoch ||
        r.key.phase != StepPhase::cut ||
        (paused_ ? (!cutTick_ || *r.target.tick != cutTick_->value ||
                    *r.target.pauseEpoch != pauseEpoch_)
                 : ((lastTick_ && *r.target.tick < lastTick_->value) ||
                    *r.target.pauseEpoch <= pauseEpoch_)))
        return core::unexpected(error("context_stale"));
    std::optional<GameplayCaptureDelivery> closingBatch;
    if (!paused_) {
        const std::size_t needed = batch_.empty() ? 1 : 2;
        if (needed > capacity_ - pending_.size() || sequence_ > UINT64_MAX - needed ||
            (!batch_.empty() && batchId_ == UINT64_MAX)) {
            fault_ = {FaultSlot::Reason::captureGap, r.key};
            ordinaryAllowed_ = false;
            auto pending = steps_.apply(r);
            if (!pending)
                return core::unexpected(pending.error());
            auto rejected = *pending;
            rejected.status = StepStatus::rejectedNotApplied;
            rejected.cause = error("capacity_exceeded");
            auto done = steps_.complete(rejected);
            if (!done)
                return core::unexpected(done.error());
            return rejected;
        }
        // Reserve the owning batch copy before the first irreversible owner step.
        if (!batch_.empty())
            closingBatch = GameplayCaptureDelivery{
                context_, 0,
                GameplayClosedBatch{GameplayTick{batch_.front().input.observationTick.value},
                                    GameplayTick{*r.target.tick}, batchId_, 0, 1,
                                    static_cast<std::uint64_t>(batch_.size()), batch_, true,
                                    *r.target.pauseEpoch}};
    }
    auto started = steps_.start(r);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return steps_.query(r.key);
    if (!paused_) {
        pauseEpoch_ = *r.target.pauseEpoch;
        if (closingBatch) {
            auto closed = enqueue(std::move(*closingBatch), true);
            if (!closed)
                return core::unexpected(closed.error());
            batch_.clear();
            ++batchId_;
        }
        auto end = enqueue({context_, 0, GameplayEpochEnd{{*r.target.tick}, pauseEpoch_}}, true);
        if (!end)
            return core::unexpected(end.error());
        paused_ = true;
        ordinaryAllowed_ = false;
        cutTick_ = GameplayTick{*r.target.tick};
        lastTick_ = cutTick_;
        cutCaptured_ = captured_;
        trace_.clear();
    }
    StepReceipt receipt{r, StepStatus::applied, {}, {}};
    receipt.effects.position = cutTick_->value;
    receipt.effects.cut = cutCaptured_;
    receipt.effects.pauseEpoch = pauseEpoch_;
    auto completed = steps_.complete(receipt);
    if (!completed)
        return core::unexpected(completed.error());
    return receipt;
} catch (...) {
    fault_ = {FaultSlot::Reason::allocation, r.key};
    ordinaryAllowed_ = false;
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::resumeRoute(const StepRequest& r, GameplayExecutionContext next)
    -> core::Result<StepReceipt> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    if (!r.target.nextContext || r.target.nextContext->sessionScope != next.sessionScope ||
        r.target.nextContext->clockSegment != next.clockSegment ||
        r.target.nextContext->runEpoch != next.runEpoch) {
        fault_ = {FaultSlot::Reason::conflict, r.key};
        ordinaryAllowed_ = false;
        return core::unexpected(error("message_conflict"));
    }
    if (auto previous = steps_.query(r.key); previous)
        return steps_.apply(r);
    if (r.target.context.sessionScope != context_.sessionScope ||
        r.target.context.clockSegment != context_.clockSegment ||
        r.target.context.runEpoch != context_.runEpoch || !paused_ || terminated_ ||
        next.sessionScope != context_.sessionScope || next.runEpoch <= context_.runEpoch ||
        next.clockSegment <= context_.clockSegment || r.key.phase != StepPhase::resumeRoute ||
        fault_.reason != FaultSlot::Reason::none || !pending_.empty())
        return core::unexpected(error("capture_gap"));
    std::optional<PreparedSemanticIdentity> identity;
    if (r.target.reconciliationOrdinal) {
        auto finalTrace = trace(*r.target.reconciliationOrdinal);
        if (!finalTrace)
            return core::unexpected(finalTrace.error());
        auto digest = reconciliationIdentity(*finalTrace);
        if (!digest)
            return core::unexpected(digest.error());
        identity = *digest;
    }
    if ((r.target.cut && *r.target.cut != trace_.size()) ||
        (r.target.payloadIdentity && identity && *r.target.payloadIdentity != *identity)) {
        auto registered = steps_.apply(r);
        if (!registered)
            return core::unexpected(registered.error());
        auto rejected = *registered;
        rejected.status = StepStatus::rejectedNotApplied;
        rejected.cause = error("context_stale");
        auto recorded = steps_.complete(rejected);
        if (!recorded)
            return core::unexpected(recorded.error());
        return rejected;
    }
    auto started = steps_.start(r);
    if (!started)
        return core::unexpected(started.error());
    if (!*started)
        return steps_.query(r.key);
    context_ = next;
    paused_ = false;
    ordinaryAllowed_ = false;
    lastTick_.reset();
    sequence_ = 0;
    batchId_ = 0;
    StepReceipt receipt{r, StepStatus::applied, {}, {}};
    receipt.effects.cut = trace_.size();
    receipt.effects.payloadIdentity = identity;
    receipt.effects.pauseEpoch = pauseEpoch_;
    auto completed = steps_.complete(receipt);
    if (!completed)
        return core::unexpected(completed.error());
    return receipt;
} catch (...) {
    fault_ = {FaultSlot::Reason::allocation, r.key};
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::trace(std::uint64_t ordinal) const
    -> core::Result<GameplayReconciliation> try {
    if (!owner_.isCurrent() || !cutTick_ || fault_.reason != FaultSlot::Reason::none)
        return core::unexpected(error("capture_gap"));
    return GameplayReconciliation{*cutTick_, ordinal, pauseEpoch_,
                                  static_cast<std::uint64_t>(trace_.size()), trace_};
} catch (...) {
    fault_ = {FaultSlot::Reason::allocation, {}};
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::pending() const -> core::Result<std::vector<GameplayCaptureDelivery>> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    return pending_;
} catch (...) {
    fault_ = {FaultSlot::Reason::allocation, {}};
    return core::unexpected(error("capacity_exceeded"));
}
auto CaptureRouteOwner::acknowledge(GameplayExecutionContext context, std::uint64_t sequence)
    -> core::Result<void> {
    if (!owner_.isCurrent() || context != context_ || sequence >= sequence_)
        return core::unexpected(error("context_stale"));
    pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                  [&](const auto& m) {
                                      return m.context == context_ &&
                                             m.deliverySequence <= sequence;
                                  }),
                   pending_.end());
    return {};
}
auto CaptureRouteOwner::enableOrdinary(const ControlReceipt& completion) -> core::Result<void> {
    if (!owner_.isCurrent() || !controlFullyApplied(completion) ||
        !completion.request.target.nextContext ||
        completion.request.target.nextContext->sessionScope != context_.sessionScope ||
        completion.request.target.nextContext->clockSegment != context_.clockSegment ||
        completion.request.target.nextContext->runEpoch != context_.runEpoch || terminated_ ||
        paused_ || fault_.reason != FaultSlot::Reason::none)
        return core::unexpected(error("control_pending"));
    ordinaryAllowed_ = true;
    return {};
}
auto CaptureRouteOwner::retained() const -> core::Result<std::vector<GameplayOccurrence>> try {
    if (!owner_.isCurrent())
        return core::unexpected(error("owner_mismatch"));
    return batch_;
} catch (...) {
    fault_ = {FaultSlot::Reason::allocation, {}};
    return core::unexpected(error("capacity_exceeded"));
}
void CaptureRouteOwner::abortResume() noexcept {
    terminated_ = true;
    ordinaryAllowed_ = false;
}
} // namespace cuexis::playback::detail
