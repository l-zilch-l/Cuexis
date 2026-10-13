#include <algorithm>
#include <cuexis/judgement/execution_scheduler.hpp>

namespace cuexis::judgement {
namespace {
auto hostError(std::string_view suffix, std::string_view message) -> core::Error {
    return core::Error{"host.rpa." + std::string{suffix}, std::string{message}};
}
} // namespace
ExecutionScheduler::ExecutionScheduler(JudgementSession& s, RpaContext c, TickSpan threshold,
                                       std::size_t cap) noexcept
    : session_(s), context_(c), closeThreshold_(threshold), capacity_(cap) {}
auto ExecutionScheduler::create(JudgementSession& s, RpaContext c, TickSpan threshold,
                                std::size_t cap, std::uint64_t firstCommand,
                                std::uint64_t firstDecision)
    -> core::Result<ExecutionScheduler> try {
    if (threshold.value() < 0 || !cap || !c.sessionScope || !c.clockSegment || !c.runEpoch)
        return core::unexpected(hostError("profile_incomplete", "invalid execution context"));
    auto state = s.query();
    if (!state || !state->kernelView().rpa)
        return core::unexpected(
            hostError("profile_incomplete", "explicit execution profile required"));
    const auto& logical = *state->kernelView().rpa;
    ExecutionScheduler result{s, c, threshold, cap};
    if (firstCommand == UINT64_MAX || firstDecision == UINT64_MAX)
        return core::unexpected(hostError("capacity_exceeded", "startup ordinal exhausted"));
    result.nextCommand_ = firstCommand;
    result.nextDecision_ = firstDecision;
    if (logical.paused) {
        if (!logical.cutTick)
            return core::unexpected(hostError("context_stale", "paused cut is absent"));
        result.paused_ = true;
        result.end_ = EndRunEpoch{*logical.cutTick, logical.pauseEpoch};
    }
    return result;
} catch (...) {
    return core::unexpected(hostError("capacity_exceeded", "scheduler preparation failed"));
}
auto ExecutionScheduler::decision() -> core::Result<std::uint64_t> {
    if (nextDecision_ == UINT64_MAX)
        return core::unexpected(hostError("capacity_exceeded", "decision ordinal exhausted"));
    return nextDecision_++;
}
auto ExecutionScheduler::verifiedExclusive() const noexcept -> std::optional<Tick> {
    return exclusive_;
}
auto ExecutionScheduler::confirmedSequence() const noexcept -> std::optional<std::uint64_t> {
    return confirmed_;
}
auto ExecutionScheduler::admissions() const noexcept -> const std::vector<AdmissionDecision>& {
    return admissions_;
}
auto ExecutionScheduler::suspended() const noexcept -> bool {
    return suspended_;
}
auto ExecutionScheduler::deliver(const CaptureMessage& message) -> core::Result<void> try {
    if (message.context != context_)
        return core::unexpected(hostError("context_stale", "capture context mismatch"));
    if (confirmed_ && message.deliverySequence <= *confirmed_)
        return {};
    if (auto old = delivery_.find(message.deliverySequence); old != delivery_.end()) {
        if (old->second == message)
            return {};
        suspended_ = true;
        return core::unexpected(
            hostError("message_conflict", "delivery sequence content conflict"));
    }
    if (delivery_.size() >= capacity_ || capacity_ == 0) {
        suspended_ = true;
        return core::unexpected(hostError("capacity_exceeded", "capture retention exhausted"));
    }
    delivery_.emplace(message.deliverySequence, message);
    while (true) {
        auto next = delivery_.find(nextSequence_);
        if (next == delivery_.end())
            break;
        if (nextSequence_ == UINT64_MAX)
            return core::unexpected(hostError("capacity_exceeded", "delivery sequence exhausted"));
        const auto& payload = next->second.payload;
        if (const auto* batch = std::get_if<ClosedCaptureBatch>(&payload)) {
            if (end_ || (exclusive_ && batch->originalTick < *exclusive_) ||
                (batch->epochTerminated
                     ? batch->observedTick < batch->originalTick || !batch->pauseEpoch
                     : batch->observedTick <= batch->originalTick) ||
                !batch->fragmentCount || batch->fragmentIndex >= batch->fragmentCount ||
                batch->members.empty() || batch->totalMembers < batch->members.size() ||
                batch->fragmentCount > capacity_ || batch->totalMembers > capacity_) {
                suspended_ = true;
                return core::unexpected(
                    hostError("message_conflict", "invalid closed batch evidence"));
            }
            auto& parts = fragments_[batch->batchId];
            for (const auto& old : parts) {
                if (old.fragmentIndex == batch->fragmentIndex ||
                    old.originalTick != batch->originalTick ||
                    old.totalMembers != batch->totalMembers ||
                    old.fragmentCount != batch->fragmentCount ||
                    old.observedTick != batch->observedTick ||
                    old.epochTerminated != batch->epochTerminated ||
                    old.pauseEpoch != batch->pauseEpoch) {
                    suspended_ = true;
                    return core::unexpected(
                        hostError("message_conflict", "fragment metadata conflict"));
                }
            }
            std::size_t retained = batch->members.size();
            for (const auto& entry : fragments_)
                for (const auto& part : entry.second) {
                    if (part.members.size() > capacity_ - retained) {
                        suspended_ = true;
                        return core::unexpected(
                            hostError("capacity_exceeded", "fragment member retention exhausted"));
                    }
                    retained += part.members.size();
                }
            parts.push_back(*batch);
            if (parts.size() == batch->fragmentCount) {
                std::vector<CanonicalInput> members;
                for (const auto& part : parts)
                    members.insert(members.end(), part.members.begin(), part.members.end());
                std::sort(members.begin(), members.end(),
                          [](const auto& a, const auto& b) { return a.key < b.key; });
                if (members.size() != batch->totalMembers)
                    return core::unexpected(hostError("message_conflict", "batch count mismatch"));
                std::optional<CanonicalObservationKey> last;
                for (const auto& member : members) {
                    const auto& k = member.key;
                    const bool sameSource =
                        last &&
                        std::tie(last->domainToken, last->sourceClass, last->channelToken) ==
                            std::tie(k.domainToken, k.sourceClass, k.channelToken);
                    if (k.observationTick.tick() != batch->originalTick || !k.sourceLocalPosition ||
                        *k.sourceLocalPosition !=
                            (sameSource ? *last->sourceLocalPosition + 1 : 0)) {
                        suspended_ = true;
                        return core::unexpected(
                            hostError("message_conflict", "source positions are not complete"));
                    }
                    last = k;
                }
                if (batch->epochTerminated)
                    batchTermination_ = EndRunEpoch{batch->observedTick, batch->pauseEpoch};
                auto receipt = session_.submitCanonical(members);
                admissions_.push_back({batch->originalTick, members,
                                       receipt ? *receipt : std::vector<InputReceiptPending>{},
                                       receipt ? std::optional<core::Error>{} : receipt.error()});
                maxCaptured_ = maxCaptured_ ? std::max(*maxCaptured_, batch->originalTick)
                                            : batch->originalTick;
                fragments_.erase(batch->batchId);
            }
        } else if (const auto* prefix = std::get_if<CapturePrefix>(&payload)) {
            if (!fragments_.empty() || end_ || batchTermination_ ||
                (exclusive_ && prefix->exclusiveTick < *exclusive_) ||
                (maxCaptured_ && prefix->exclusiveTick <= *maxCaptured_)) {
                suspended_ = true;
                return core::unexpected(
                    hostError("message_conflict", "prefix cannot cover incomplete capture"));
            }
            exclusive_ = prefix->exclusiveTick;
        } else {
            const auto cut = std::get<EndRunEpoch>(payload);
            if (!fragments_.empty() || end_ || (batchTermination_ && *batchTermination_ != cut) ||
                (exclusive_ && cut.inclusiveTick < *exclusive_) ||
                (maxCaptured_ && cut.inclusiveTick < *maxCaptured_)) {
                suspended_ = true;
                return core::unexpected(hostError("message_conflict", "invalid epoch termination"));
            }
            end_ = cut;
        }
        auto ordinal = decision();
        if (!ordinal)
            return core::unexpected(ordinal.error());
        ++nextSequence_;
        if (fragments_.empty()) {
            confirmed_ = nextSequence_ - 1;
            delivery_.erase(delivery_.begin(), delivery_.upper_bound(*confirmed_));
        }
    }
    return tryStart();
} catch (...) {
    suspended_ = true;
    return core::unexpected(hostError("capacity_exceeded", "capture allocation failed"));
}
auto ExecutionScheduler::tryStart() -> core::Result<void> {
    if (!waiting_ || suspended_ || paused_ || controlPreparing_)
        return {};
    auto& request = requests_.at(*waiting_);
    auto frontier = offsetTicks(request.request.horizon, TickSpan{-closeThreshold_.value()});
    if (!frontier)
        return core::unexpected(frontier.error());
    if (!exclusive_ || *frontier >= *exclusive_)
        return {};
    auto ordinal = decision();
    if (!ordinal)
        return core::unexpected(ordinal.error());
    request.decisionOrdinal = *ordinal;
    request.verifiedExclusive = exclusive_;
    waiting_.reset();
    auto result = session_.advance(request.request.horizon);
    request.disposition = result ? AdvanceDisposition::executed : AdvanceDisposition::failed;
    if (result)
        request.result = result->kernelView();
    else {
        request.error = result.error();
        auto prefix = session_.query();
        if (prefix)
            request.result = prefix->kernelView();
    }
    return {};
}
auto ExecutionScheduler::request(AdvanceRequest r) -> core::Result<AdvanceDecision> try {
    if (r.context != context_)
        return core::unexpected(hostError("context_stale", "request context mismatch"));
    if (auto old = requests_.find(r.requestId); old != requests_.end()) {
        if (old->second.request == r)
            return old->second;
        suspended_ = true;
        return core::unexpected(hostError("message_conflict", "request ID content conflict"));
    }
    if (waiting_)
        return AdvanceDecision{
            r, AdvanceDisposition::busyNotAccepted, nextDecision_, exclusive_, {}, {}};
    if (suspended_ || paused_ || controlPreparing_ || !validClock_)
        return core::unexpected(hostError("clock_invalid", "execution permission revoked"));
    if (requests_.size() >= capacity_) {
        suspended_ = true;
        return core::unexpected(
            hostError("capacity_exceeded", "request identity retention exhausted"));
    }
    if (r.commandOrdinal != nextCommand_ || nextCommand_ == UINT64_MAX)
        return core::unexpected(
            hostError("message_conflict", "command ordinal or request budget invalid"));
    auto state = session_.query();
    if (!state)
        return core::unexpected(state.error());
    if (state->kernelView().lastAdvanceHorizon &&
        r.horizon < *state->kernelView().lastAdvanceHorizon)
        return core::unexpected(hostError(
            "control_pending", "clock horizon must catch the last committed control drain"));
    auto frontier = offsetTicks(r.horizon, TickSpan{-closeThreshold_.value()});
    if (!frontier)
        return core::unexpected(frontier.error());
    auto ordinal = decision();
    if (!ordinal)
        return core::unexpected(ordinal.error());
    ++nextCommand_;
    requests_.emplace(
        r.requestId, AdvanceDecision{r, AdvanceDisposition::waiting, *ordinal, exclusive_, {}, {}});
    waiting_ = r.requestId;
    auto started = tryStart();
    if (!started)
        return core::unexpected(started.error());
    return requests_.at(r.requestId);
} catch (...) {
    return core::unexpected(hostError("capacity_exceeded", "request reservation failed"));
}
auto ExecutionScheduler::poll(std::uint64_t id) const -> core::Result<AdvanceDecision> {
    auto found = requests_.find(id);
    if (found == requests_.end())
        return core::unexpected(hostError("context_stale", "request ID unknown"));
    return found->second;
}
auto ExecutionScheduler::retire(std::uint64_t id) -> core::Result<void> {
    auto found = requests_.find(id);
    if (found == requests_.end() || found->second.disposition == AdvanceDisposition::waiting)
        return core::unexpected(hostError("context_stale", "request not terminal"));
    found->second.disposition = AdvanceDisposition::retired;
    found->second.result.reset();
    found->second.error.reset();
    return {};
}
auto ExecutionScheduler::acceptControl(RpaContext c, std::uint64_t command) -> core::Result<void> {
    if (c != context_)
        return core::unexpected(hostError("context_stale", "control context mismatch"));
    if (command != nextCommand_ || nextCommand_ == UINT64_MAX)
        return core::unexpected(hostError("message_conflict", "control command ordinal invalid"));
    auto ordinal = decision();
    if (!ordinal)
        return core::unexpected(ordinal.error());
    ++nextCommand_;
    controlPreparing_ = true;
    if (waiting_) {
        auto& r = requests_.at(*waiting_);
        r.disposition = AdvanceDisposition::canceledNotExecuted;
        r.decisionOrdinal = *ordinal;
        waiting_.reset();
    }
    return {};
}
auto ExecutionScheduler::pause() -> core::Result<void> {
    if (!end_ || waiting_ || paused_ || suspended_ || !directResume_)
        return core::unexpected(hostError("control_pending", "cut not complete"));
    auto horizon = offsetTicks(end_->inclusiveTick, closeThreshold_);
    if (!horizon) {
        suspended_ = true;
        return core::unexpected(horizon.error());
    }
    auto drained = session_.advance(*horizon, true);
    if (!drained) {
        suspended_ = true;
        return core::unexpected(drained.error());
    }
    const auto& logical = *drained->kernelView().rpa;
    auto paused = session_.controlRpa({RpaControlKind::pause,
                                       end_->inclusiveTick,
                                       logical.nextControlOrdinal,
                                       end_->pauseEpoch,
                                       0,
                                       {}});
    if (!paused) {
        suspended_ = true;
        return paused;
    }
    paused_ = true;
    return {};
}
auto ExecutionScheduler::resume(const RpaControlRecord& r, RpaContext c, bool switched, bool failed)
    -> core::Result<void> {
    if (lastResume_ && lastResume_->controlOrdinal == r.controlOrdinal) {
        if (*lastResume_ != r || *lastResumeContext_ != c) {
            suspended_ = true;
            return core::unexpected(
                hostError("message_conflict", "Resume identity content conflict"));
        }
        if (lastResumeFailure_)
            return core::unexpected(*lastResumeFailure_);
        return {};
    }
    if (!directResume_)
        return core::unexpected(hostError("resume_aborted", "direct continuation terminated"));
    if (!paused_ || c.sessionScope != context_.sessionScope || c.runEpoch <= context_.runEpoch ||
        c.clockSegment <= context_.clockSegment)
        return core::unexpected(hostError("context_stale", "resume context invalid"));
    if (!switched)
        return core::unexpected(hostError("control_pending", "final route outcome not applied"));
    if (!failed && r.trace.size() < r.finalTraceCut)
        return core::unexpected(hostError("control_pending", "final trace prefix incomplete"));
    auto applied = failed ? core::Result<void>{core::unexpected(
                                hostError("resume_aborted", "final preparation failed"))}
                          : session_.controlRpa(r);
    lastResume_ = r;
    lastResumeContext_ = c;
    if (!applied) {
        lastResumeFailure_ = applied.error();
        auto state = session_.query();
        if (state && state->kernelView().state != KernelSessionState::Faulted)
            directResume_ = false;
        suspended_ = true;
        return applied;
    }
    lastResumeFailure_.reset();
    context_ = c;
    nextSequence_ = 0;
    confirmed_.reset();
    exclusive_.reset();
    maxCaptured_.reset();
    end_.reset();
    batchTermination_.reset();
    delivery_.clear();
    fragments_.clear();
    paused_ = false;
    controlPreparing_ = false;
    suspended_ = false;
    validClock_ = true;
    return {};
}
auto ExecutionScheduler::resumeStatus(const RpaControlRecord& r, RpaContext c)
    -> core::Result<bool> {
    if (!lastResume_ || lastResume_->controlOrdinal != r.controlOrdinal)
        return false;
    if (*lastResume_ != r || *lastResumeContext_ != c) {
        suspended_ = true;
        return core::unexpected(hostError("message_conflict", "Resume identity content conflict"));
    }
    if (lastResumeFailure_)
        return core::unexpected(*lastResumeFailure_);
    return true;
}
void ExecutionScheduler::abortResume() noexcept {
    directResume_ = false;
    suspended_ = true;
}

auto ExecutionScheduler::canTransitionClock(RpaContext next) const noexcept -> bool {
    return !(suspended_ || paused_ || controlPreparing_ || end_ || !delivery_.empty() ||
             !fragments_.empty() || next.sessionScope != context_.sessionScope ||
             next.runEpoch != context_.runEpoch || next.clockSegment <= context_.clockSegment);
}
auto ExecutionScheduler::transitionClock(RpaContext next) -> core::Result<void> {
    if (!canTransitionClock(next))
        return core::unexpected(hostError("control_pending", "Clock transition is not ready"));
    if (waiting_) {
        requests_.at(*waiting_).disposition = AdvanceDisposition::clockInvalidNotExecuted;
        waiting_.reset();
    }
    context_ = next;
    validClock_ = true;
    return {};
}

auto ExecutionScheduler::clockValid(bool valid) -> core::Result<void> {
    validClock_ = valid;
    if (!valid) {
        suspended_ = true;
        if (waiting_) {
            requests_.at(*waiting_).disposition = AdvanceDisposition::clockInvalidNotExecuted;
            waiting_.reset();
        }
    }
    return {};
}
} // namespace cuexis::judgement
