#include "host_protocol.hpp"
#include <algorithm>
#include <cmath>

namespace cuexis::playback::detail {
auto HostCoordinator::switchClock(const HostContext& next) -> core::Result<void> {
    auto expected = context_;
    expected.clockSegment = next.clockSegment;
    const bool terminalControl =
        !active_ || !records_[*active_] || records_[*active_]->status == ControlStatus::complete;
    if (!owner_.isCurrent() || (state_ != HostState::running && state_ != HostState::suspended) ||
        !terminalControl || next != expected || next.clockSegment <= context_.clockSegment)
        return core::unexpected(
            core::Error{"host.rpa.context_stale", "Host clock transition not ready"});
    context_ = next;
    return {};
}
namespace {
auto failure(std::string_view code, std::string_view message) -> core::Error {
    return {"host.rpa." + std::string{code}, std::string{message}};
}
auto terminal(StepStatus s) -> bool {
    return s == StepStatus::applied || s == StepStatus::rejectedNotApplied ||
           s == StepStatus::canceledNotApplied || s == StepStatus::failed ||
           s == StepStatus::retired;
}
} // namespace
auto sameHostError(const std::optional<core::Error>& a, const std::optional<core::Error>& b)
    -> bool {
    const auto errorsEqual = [](const auto& self, const core::Error& x,
                                const core::Error& y) -> bool {
        if (x.code() != y.code() || x.message() != y.message() ||
            x.context().size() != y.context().size())
            return false;
        for (std::size_t i = 0; i < x.context().size(); ++i)
            if (x.context()[i].key != y.context()[i].key ||
                x.context()[i].value != y.context()[i].value)
                return false;
        if (bool(x.cause()) != bool(y.cause()))
            return false;
        return !x.cause() || self(self, *x.cause(), *y.cause());
    };
    return (!a && !b) || (a && b && errorsEqual(errorsEqual, *a, *b));
}
namespace {
auto same(const StepReceipt& a, const StepReceipt& b) -> bool {
    return a.request == b.request && a.status == b.status && a.effects == b.effects &&
           sameHostError(a.cause, b.cause);
}
auto index(Participant p) -> std::size_t {
    return static_cast<std::size_t>(p);
}
auto index(StepPhase p) -> std::size_t {
    return static_cast<std::size_t>(p);
}
auto validNextContext(const ControlRequest& request) -> bool {
    if (static_cast<std::uint8_t>(request.kind) > static_cast<std::uint8_t>(ControlKind::close))
        return false;
    if (request.target.audioPositionMs && !std::isfinite(*request.target.audioPositionMs))
        return false;
    if (!request.target.nextContext)
        return true;
    const auto& next = *request.target.nextContext;
    const auto& current = request.context;
    if (next.sessionScope != current.sessionScope ||
        next.controlSequence != current.controlSequence ||
        next.resourceGeneration < current.resourceGeneration ||
        next.rendererGeneration < current.rendererGeneration)
        return false;
    if (request.kind == ControlKind::resume || request.kind == ControlKind::seek)
        return next.clockSegment > current.clockSegment && next.runEpoch > current.runEpoch;
    return next.clockSegment == current.clockSegment && next.runEpoch == current.runEpoch;
}
auto cleanupPhase(StepPhase phase) -> bool {
    return phase == StepPhase::cut || phase == StepPhase::stop || phase == StepPhase::isolate ||
           phase == StepPhase::retire || phase == StepPhase::close;
}
auto confirmed(const ControlReceipt& control, Participant participant, StepPhase phase)
    -> const StepReceipt* {
    const auto& step = control.steps[index(participant)][index(phase)];
    if (!step || step->status != StepStatus::applied ||
        step->request.key.sessionScope != control.request.context.sessionScope ||
        step->request.key.operationId != control.request.operationId ||
        step->request.key.participant != participant || step->request.key.phase != phase ||
        step->request.target != control.request.target)
        return nullptr;
    return &*step;
}
auto ownersApplied(const ControlReceipt& control) -> bool {
    for (std::size_t p = 0; p < control.request.participants.size(); ++p) {
        if (!control.request.participants[p])
            continue;
        const auto participant = static_cast<Participant>(p);
        if (!confirmed(control, participant,
                       controlRequiredPhase(control.request.kind, participant)))
            return false;
        if (participant == Participant::audio && (control.request.kind == ControlKind::stop ||
                                                  control.request.kind == ControlKind::reset)) {
            const auto* isolated = confirmed(control, participant, StepPhase::isolate);
            if (!isolated || !isolated->effects.callbackIsolated)
                return false;
        }
    }
    return true;
}
} // namespace
auto controlRequiredPhase(ControlKind kind, Participant participant) -> StepPhase {
    if (kind == ControlKind::close)
        return StepPhase::close;
    if (participant == Participant::software)
        return StepPhase::softwareCommit;
    const bool stopped = kind == ControlKind::stop || kind == ControlKind::reset;
    if (participant == Participant::capture)
        return kind == ControlKind::pause || stopped ? StepPhase::cut : StepPhase::resumeRoute;
    if (participant == Participant::audio)
        return stopped                      ? StepPhase::stop
               : kind == ControlKind::pause ? StepPhase::fence
                                            : StepPhase::activate;
    return kind == ControlKind::pause || stopped ? StepPhase::fence : StepPhase::activate;
}
auto controlReadyForCommit(const ControlReceipt& control) -> bool {
    if (control.status != ControlStatus::preparing)
        return false;
    for (std::size_t p = 0; p < control.request.participants.size(); ++p)
        if (control.request.participants[p] &&
            (!confirmed(control, static_cast<Participant>(p), StepPhase::prepare) ||
             !confirmed(control, static_cast<Participant>(p), StepPhase::fence)))
            return false;
    return true;
}
auto controlFullyApplied(const ControlReceipt& control) -> bool {
    if (control.status != ControlStatus::complete || !control.cleanupComplete)
        return false;
    if (control.request.kind == ControlKind::close) {
        for (std::size_t p = 0; p < control.request.participants.size(); ++p)
            if (control.request.participants[p] &&
                !confirmed(control, static_cast<Participant>(p), StepPhase::close))
                return false;
        const auto active = [&](Participant p) { return control.request.participants[index(p)]; };
        if (active(Participant::capture) &&
            !confirmed(control, Participant::capture, StepPhase::cut))
            return false;
        if (active(Participant::audio)) {
            const auto* isolated = confirmed(control, Participant::audio, StepPhase::isolate);
            if (!confirmed(control, Participant::audio, StepPhase::stop) || !isolated ||
                !isolated->effects.callbackIsolated)
                return false;
        }
        for (auto p : {Participant::software, Participant::renderer}) {
            const auto* retired = confirmed(control, p, StepPhase::retire);
            if (active(p) && (!retired || !retired->effects.referencesReleased))
                return false;
        }
        return true;
    }
    if (!control.softwareApplied ||
        (control.request.participants[index(Participant::software)] &&
         !confirmed(control, Participant::software, StepPhase::softwareCommit)))
        return false;
    return ownersApplied(control);
}
OwnerStepLedger::OwnerStepLedger(std::uint64_t scope, Participant p, std::size_t reserve,
                                 std::vector<std::optional<Record>> records)
    : scope_(scope), participant_(p), reserve_(reserve), records_(std::move(records)) {}
auto OwnerStepLedger::create(std::uint64_t scope, Participant p, std::size_t capacity,
                             std::size_t reserve) -> core::Result<OwnerStepLedger> try {
    if (!scope || index(p) >= 4 || !reserve || reserve >= capacity)
        return core::unexpected(
            failure("profile_incomplete", "Explicit owner record and control capacity required"));
    return OwnerStepLedger{scope, p, reserve, std::vector<std::optional<Record>>(capacity)};
} catch (...) {
    return core::unexpected(failure("capacity_exceeded", "Owner record reservation failed"));
}
auto OwnerStepLedger::owner() const -> core::Result<void> try {
    if (!owner_.isCurrent())
        return core::unexpected(failure("owner_mismatch", "Owner step called from another thread"));
    return {};
} catch (...) {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::allocation, {}};
    return core::unexpected(allocationFailure_);
}
auto OwnerStepLedger::bind(const StepRequest& r, bool cancellation) -> core::Result<Record*> try {
    if (auto check = owner(); !check)
        return core::unexpected(check.error());
    if (r.key.sessionScope != scope_ || r.target.context.sessionScope != scope_ ||
        r.key.participant != participant_ || !r.key.operationId ||
        !r.target.context.controlSequence || index(r.key.phase) >= 10 ||
        (r.target.audioPositionMs && !std::isfinite(*r.target.audioPositionMs)))
        return core::unexpected(failure("context_stale", "Owner step identity invalid"));
    for (auto& entry : records_) {
        if (!entry || entry->receipt.request.key != r.key)
            continue;
        if (entry->receipt.request != r) {
            fault_ = {FaultSlot::Reason::conflict, r.key};
            return core::unexpected(
                failure("message_conflict", "Owner step immutable target differs"));
        }
        return &*entry;
    }
    // Ordinary Apply cannot consume capacity reserved for cancellation and cleanup.
    if (r.retryGeneration && !r.key.attempt)
        return core::unexpected(failure("context_stale", "Retry generation requires a retry"));
    if (r.key.attempt) {
        for (const auto& record : records_)
            if (record && record->receipt.request.key.sessionScope == r.key.sessionScope &&
                record->receipt.request.key.operationId == r.key.operationId &&
                record->receipt.request.key.attempt == r.key.attempt &&
                (r.key.phase == StepPhase::prepare || r.key.phase == StepPhase::fence ||
                 r.key.phase == StepPhase::activate) &&
                (record->receipt.request.key.phase == StepPhase::prepare ||
                 record->receipt.request.key.phase == StepPhase::fence ||
                 record->receipt.request.key.phase == StepPhase::activate) &&
                (record->receipt.request.target != r.target ||
                 record->receipt.request.retryGeneration != r.retryGeneration)) {
                fault_ = {FaultSlot::Reason::conflict, r.key};
                return core::unexpected(failure("message_conflict", "Retry binding differs"));
            }
        const auto find = [&](StepPhase phase, std::uint64_t attempt) -> const Record* {
            auto key = r.key;
            key.phase = phase;
            key.attempt = attempt;
            for (const auto& record : records_)
                if (record && record->receipt.request.key == key &&
                    record->receipt.request.target == r.target)
                    return &*record;
            return nullptr;
        };
        const auto status = [](const Record* record) {
            return !record ? StepStatus::pending
                           : record->retiredFrom.value_or(record->receipt.status);
        };
        const auto prior = r.key.attempt - 1;
        const auto* failed = find(StepPhase::activate, prior);
        const auto* retired = find(StepPhase::retire, prior);
        const auto* isolated = find(StepPhase::isolate, prior);
        const bool separateCleanup =
            status(retired) == StepStatus::applied && retired->receipt.effects.referencesReleased &&
            retired->receipt.effects.callbackIsolated &&
            (participant_ != Participant::audio || (status(isolated) == StepStatus::applied &&
                                                    isolated->receipt.effects.callbackIsolated));
        const bool retryReady =
            status(failed) == StepStatus::failed && ((failed->receipt.effects.callbackIsolated &&
                                                      failed->receipt.effects.referencesReleased) ||
                                                     separateCleanup);
        const auto* current = find(StepPhase::activate, r.key.attempt);
        const bool cleanup = cleanupPhase(r.key.phase) && (status(current) == StepStatus::failed ||
                                                           status(current) == StepStatus::unknown);
        const bool retryPhase = r.key.phase == StepPhase::prepare ||
                                r.key.phase == StepPhase::fence ||
                                r.key.phase == StepPhase::activate || cleanupPhase(r.key.phase);
        if (r.key.attempt == UINT64_MAX || !retryPhase ||
            (participant_ != Participant::audio && participant_ != Participant::renderer) ||
            (!retryReady && !cleanup))
            return core::unexpected(
                failure("control_pending", "Previous device attempt not terminal and isolated"));
    }
    const auto limit =
        cancellation || cleanupPhase(r.key.phase) ? records_.size() : records_.size() - reserve_;
    for (std::size_t i = 0; i < limit; ++i) {
        if (records_[i])
            continue;
        records_[i].emplace(Record{StepReceipt{r, StepStatus::pending, {}, {}}, false, {}});
        return &*records_[i];
    }
    fault_ = {FaultSlot::Reason::capacity, r.key};
    return core::unexpected(failure("capacity_exceeded", "Owner record capacity exhausted"));
} catch (...) {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::allocation, r.key};
    return core::unexpected(allocationFailure_);
}
auto OwnerStepLedger::apply(const StepRequest& r) -> core::Result<StepReceipt> try {
    auto record = bind(r, false);
    if (!record)
        return core::unexpected(record.error());
    return (*record)->receipt;
} catch (...) {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::allocation, r.key};
    return core::unexpected(allocationFailure_);
}
auto OwnerStepLedger::cancel(const StepRequest& r) -> core::Result<StepReceipt> try {
    auto record = bind(r, true);
    if (!record)
        return core::unexpected(record.error());
    if ((*record)->receipt.status == StepStatus::pending)
        (*record)->receipt.status = StepStatus::canceledNotApplied;
    return (*record)->receipt;
} catch (...) {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::allocation, r.key};
    return core::unexpected(allocationFailure_);
}
auto OwnerStepLedger::start(const StepRequest& r) -> core::Result<bool> try {
    auto record = bind(r, false);
    if (!record)
        return core::unexpected(record.error());
    if ((*record)->receipt.status != StepStatus::pending)
        return false;
    if (suspended() && !cleanupPhase(r.key.phase))
        return core::unexpected(failure("control_pending", "Owner execution permission revoked"));
    (*record)->receipt.status = StepStatus::inProgress;
    return true;
} catch (...) {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::allocation, r.key};
    return core::unexpected(allocationFailure_);
}
auto OwnerStepLedger::complete(const StepReceipt& receipt) -> core::Result<void> try {
    auto record = bind(receipt.request, true);
    if (!record)
        return core::unexpected(record.error());
    auto& previous = (*record)->receipt;
    if ((*record)->retiredFrom) {
        if (receipt.status == *(*record)->retiredFrom && previous.request == receipt.request &&
            previous.effects == receipt.effects && sameHostError(previous.cause, receipt.cause))
            return {};
        if (same(previous, receipt))
            return {};
        fault_ = {FaultSlot::Reason::conflict, receipt.request.key};
        return core::unexpected(failure("message_conflict", "Retired owner receipt differs"));
    }
    if (failNextCompletion_) {
        failNextCompletion_ = false;
        throw std::bad_alloc{};
    }
    if (same(previous, receipt))
        return {};
    if (terminal(previous.status) ||
        (previous.status != StepStatus::inProgress && previous.status != StepStatus::unknown &&
         !(previous.status == StepStatus::pending &&
           receipt.status == StepStatus::rejectedNotApplied)) ||
        (!terminal(receipt.status) && receipt.status != StepStatus::unknown) ||
        receipt.status == StepStatus::retired ||
        ((receipt.status == StepStatus::canceledNotApplied ||
          receipt.status == StepStatus::rejectedNotApplied) &&
         receipt.effects != StepEffects{})) {
        fault_ = {FaultSlot::Reason::conflict, receipt.request.key};
        return core::unexpected(failure("message_conflict", "Invalid owner completion transition"));
    }
    // A started step cannot claim that cancellation prevented it from starting.
    if (receipt.status == StepStatus::canceledNotApplied)
        return core::unexpected(
            failure("message_conflict", "Started step cannot be canceled as not applied"));
    StepReceipt retained{receipt};
    previous = std::move(retained);
    return {};
} catch (...) {
    reportAllocationFailure(receipt.request.key);
    for (auto& entry : records_)
        if (entry && entry->receipt.request == receipt.request &&
            (entry->receipt.status == StepStatus::inProgress ||
             entry->receipt.status == StepStatus::unknown)) {
            // Fixed numeric effects survive failed terminal publication. Unknown cannot rerun.
            entry->receipt.status = StepStatus::unknown;
            entry->receipt.effects = receipt.effects;
        }
    // The fixed fault slot survives even if constructing an Error hits process-level OOM.
    // Keep the diagnostic template intact for every later failed completion.
    return core::unexpected(allocationFailure_);
}
auto OwnerStepLedger::query(StepKey key) const -> core::Result<StepReceipt> try {
    if (auto check = owner(); !check)
        return core::unexpected(check.error());
    for (const auto& entry : records_)
        if (entry && entry->receipt.request.key == key)
            return entry->receipt;
    return core::unexpected(
        failure("control_pending", "Step outcome unknown; query owner before cleanup"));
} catch (...) {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::allocation, key};
    return core::unexpected(allocationFailure_);
}
auto OwnerStepLedger::acknowledge(StepKey key) -> core::Result<void> try {
    if (auto check = owner(); !check)
        return check;
    for (auto& entry : records_)
        if (entry && entry->receipt.request.key == key) {
            if (!terminal(entry->receipt.status))
                return core::unexpected(failure("control_pending", "Step not terminal"));
            entry->acknowledged = true;
            if (entry->receipt.status != StepStatus::retired) {
                entry->retiredFrom = entry->receipt.status;
                entry->receipt.status = StepStatus::retired;
            }
            return {};
        }
    return core::unexpected(failure("context_stale", "Step not retained"));
} catch (...) {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::allocation, key};
    return core::unexpected(allocationFailure_);
}
auto OwnerStepLedger::mayReclaim(StepKey key) const -> core::Result<bool> try {
    auto receipt = query(key);
    if (!receipt)
        return core::unexpected(receipt.error());
    auto status = receipt->status;
    for (const auto& record : records_)
        if (record && record->receipt.request.key == key && record->retiredFrom)
            status = *record->retiredFrom;
    return status == StepStatus::rejectedNotApplied || status == StepStatus::canceledNotApplied ||
           (receipt->effects.referencesReleased && receipt->effects.callbackIsolated &&
            terminal(receipt->status));
} catch (...) {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::allocation, key};
    return core::unexpected(allocationFailure_);
}
void OwnerStepLedger::reportConflict(std::optional<StepKey> key) noexcept {
    if (owner_.isCurrent())
        fault_ = {FaultSlot::Reason::conflict, key};
}
void OwnerStepLedger::reportAllocationFailure(std::optional<StepKey> key) noexcept {
    fault_ = {FaultSlot::Reason::allocation, key};
}

void HostCoordinator::reportAllocationFailure(std::optional<StepKey> key) const noexcept {
    fault_ = {FaultSlot::Reason::allocation, key};
    if (active_)
        records_[*active_]->status = ControlStatus::suspended;
    if (state_ != HostState::closed)
        state_ = active_ && records_[*active_]->request.kind == ControlKind::close
                     ? HostState::closing
                     : HostState::suspended;
}

HostCoordinator::HostCoordinator(HostContext context,
                                 std::vector<std::optional<ControlReceipt>> records,
                                 std::uint64_t firstCommand)
    : context_(context), records_(std::move(records)), nextCommand_(firstCommand) {}
auto HostCoordinator::create(HostContext context, std::size_t capacity, std::uint64_t firstCommand)
    -> core::Result<HostCoordinator> try {
    if (!context.sessionScope || !context.clockSegment || !context.runEpoch || capacity < 2)
        return core::unexpected(
            failure("profile_incomplete", "Explicit coordinator context and capacity required"));
    return HostCoordinator{context, std::vector<std::optional<ControlReceipt>>(capacity),
                           firstCommand};
} catch (...) {
    return core::unexpected(failure("capacity_exceeded", "Control record reservation failed"));
}
auto HostCoordinator::accept(const ControlRequest& request, std::uint64_t decision)
    -> core::Result<ControlReceipt> try {
    if (!owner_.isCurrent())
        return core::unexpected(failure("owner_mismatch", "Coordinator owner mismatch"));
    for (const auto& entry : records_) {
        if (!entry || entry->request.operationId != request.operationId)
            continue;
        if (entry->request == request)
            return *entry;
        fault_ = {FaultSlot::Reason::conflict, {}};
        if (active_)
            records_[*active_]->status = ControlStatus::suspended;
        if (state_ != HostState::closed)
            state_ = active_ && records_[*active_]->request.kind == ControlKind::close
                         ? HostState::closing
                         : HostState::suspended;
        return core::unexpected(failure("message_conflict", "Control ID payload conflict"));
    }
    if (request.context.sessionScope != context_.sessionScope ||
        request.context.clockSegment != context_.clockSegment ||
        request.context.runEpoch != context_.runEpoch ||
        request.context.resourceGeneration != context_.resourceGeneration ||
        request.context.rendererGeneration != context_.rendererGeneration ||
        request.context.contentIdentity != context_.contentIdentity ||
        request.target.context != request.context || context_.controlSequence == UINT64_MAX ||
        request.context.controlSequence != context_.controlSequence + 1 || !request.operationId ||
        request.operationId < nextOperation_ || request.commandOrdinal < nextCommand_ ||
        !validNextContext(request))
        return core::unexpected(failure("context_stale", "Control identity or context invalid"));
    if (active_ && request.kind != ControlKind::close)
        return ControlReceipt{
            request, ControlStatus::busyNotAccepted, decision, {}, !resumeTerminated_, false, false,
            {}};
    if (active_)
        return core::unexpected(
            failure("control_pending", "Close waits for active owner isolation"));
    if (state_ == HostState::closed || (resumeTerminated_ && request.kind == ControlKind::resume))
        return core::unexpected(
            failure("resume_aborted", "Control cannot resume a terminated session"));
    if (fault_.reason != FaultSlot::Reason::none && request.kind != ControlKind::close)
        return core::unexpected(
            failure("control_pending", "Protocol fault requires cleanup and Close"));
    if (request.operationId == UINT64_MAX || request.commandOrdinal == UINT64_MAX ||
        nextCommand_ == UINT64_MAX || (lastDecision_ && decision <= *lastDecision_))
        return core::unexpected(
            failure("capacity_exceeded", "Control ordinal exhausted or out of order"));
    const auto limit = request.kind == ControlKind::close ? records_.end() : records_.end() - 1;
    auto slot = std::find_if(records_.begin(), limit, [](const auto& entry) { return !entry; });
    if (slot == limit) {
        fault_ = {FaultSlot::Reason::capacity, {}};
        state_ = HostState::suspended;
        return core::unexpected(
            failure("capacity_exceeded", "Control identity retention exhausted"));
    }
    slot->emplace(ControlReceipt{
        request, ControlStatus::preparing, decision, {}, !resumeTerminated_, false, false, {}});
    active_ = static_cast<std::size_t>(slot - records_.begin());
    context_ = request.context;
    nextOperation_ = request.operationId + 1;
    nextCommand_ = request.commandOrdinal + 1;
    lastDecision_ = decision;
    state_ = request.kind == ControlKind::close    ? HostState::closing
             : request.kind == ControlKind::resume ? HostState::resumeStarting
                                                   : HostState::controlPreparing;
    return **slot;
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::preflight(const ControlRequest& request) -> core::Result<bool> try {
    if (!owner_.isCurrent())
        return core::unexpected(failure("owner_mismatch", "Coordinator owner mismatch"));
    for (const auto& entry : records_)
        if (entry && entry->request.operationId == request.operationId) {
            if (entry->request == request)
                return false;
            if (state_ != HostState::closed)
                state_ = active_ && records_[*active_]->request.kind == ControlKind::close
                             ? HostState::closing
                             : HostState::suspended;
            fault_ = {FaultSlot::Reason::conflict, {}};
            if (active_)
                records_[*active_]->status = ControlStatus::suspended;
            return core::unexpected(failure("message_conflict", "Control ID payload conflict"));
        }
    if (request.context.sessionScope != context_.sessionScope ||
        request.context.clockSegment != context_.clockSegment ||
        request.context.runEpoch != context_.runEpoch ||
        request.context.resourceGeneration != context_.resourceGeneration ||
        request.context.rendererGeneration != context_.rendererGeneration ||
        request.context.contentIdentity != context_.contentIdentity ||
        request.target.context != request.context || context_.controlSequence == UINT64_MAX ||
        request.context.controlSequence != context_.controlSequence + 1 || !request.operationId ||
        request.operationId < nextOperation_ || request.commandOrdinal < nextCommand_ ||
        !validNextContext(request))
        return core::unexpected(failure("context_stale", "Control identity or context invalid"));
    if (active_)
        return false;
    if (state_ == HostState::closed || (resumeTerminated_ && request.kind == ControlKind::resume))
        return core::unexpected(failure("resume_aborted", "Direct continuation unavailable"));
    if (fault_.reason != FaultSlot::Reason::none && request.kind != ControlKind::close)
        return core::unexpected(
            failure("control_pending", "Protocol fault requires cleanup and Close"));
    if (request.operationId == UINT64_MAX || request.commandOrdinal == UINT64_MAX ||
        nextCommand_ == UINT64_MAX)
        return core::unexpected(failure("capacity_exceeded", "Control ordinal exhausted"));
    const auto limit = request.kind == ControlKind::close ? records_.end() : records_.end() - 1;
    if (std::none_of(records_.begin(), limit, [](const auto& r) { return !r; }))
        return core::unexpected(failure("capacity_exceeded", "Control reservation unavailable"));
    return true;
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::observe(const StepReceipt& receipt) -> core::Result<void> try {
    if (!owner_.isCurrent())
        return core::unexpected(failure("owner_mismatch", "Coordinator owner mismatch"));
    const auto p = index(receipt.request.key.participant), phase = index(receipt.request.key.phase);
    if (p >= 4 || phase >= 10)
        return core::unexpected(failure("context_stale", "Unknown participant or phase"));
    auto found = std::find_if(records_.begin(), records_.end(), [&](const auto& entry) {
        return entry && entry->request.operationId == receipt.request.key.operationId &&
               entry->request.context.sessionScope == receipt.request.key.sessionScope;
    });
    if (found == records_.end() || receipt.request.target.context != (*found)->request.context ||
        !(*found)->request.participants[p])
        return core::unexpected(failure("context_stale", "Receipt belongs to another operation"));
    if (receipt.request.target != (*found)->request.target) {
        fault_ = {FaultSlot::Reason::conflict, receipt.request.key};
        if (active_ && &records_[*active_] == &*found) {
            (*found)->status = ControlStatus::suspended;
            state_ = (*found)->request.kind == ControlKind::close ? HostState::closing
                                                                  : HostState::suspended;
        }
        return core::unexpected(
            failure("message_conflict", "Receipt target differs from accepted control"));
    }
    // Copy before changing retained history: an allocation fault cannot erase evidence.
    StepReceipt retained{receipt};
    if (receipt.request.resumeTrace) {
        const auto& route =
            (*found)->steps[index(Participant::capture)][index(StepPhase::resumeRoute)];
        if (p != index(Participant::software) || phase != index(StepPhase::softwareCommit) ||
            (*found)->request.kind != ControlKind::resume || !route ||
            route->status != StepStatus::applied ||
            route->effects.cut != receipt.request.resumeTrace->finalCut ||
            route->effects.payloadIdentity != receipt.request.resumeTrace->identity) {
            fault_ = {FaultSlot::Reason::conflict, receipt.request.key};
            if (active_ && &records_[*active_] == &*found) {
                (*found)->status = ControlStatus::suspended;
                state_ = HostState::suspended;
            }
            return core::unexpected(
                failure("message_conflict", "Software trace is not bound to final route evidence"));
        }
    }
    auto& previous = (*found)->steps[p][phase];
    if (previous) {
        if (same(*previous, receipt))
            return {};
        // The owner keeps prior attempts; their late acknowledgements cannot replace the retry.
        if (receipt.request.key.attempt < previous->request.key.attempt)
            return {};
        const auto attempt = receipt.request.key.attempt;
        const auto& activation = (*found)->steps[p][index(StepPhase::activate)];
        const auto& retirement = (*found)->steps[p][index(StepPhase::retire)];
        const auto& isolation = (*found)->steps[p][index(StepPhase::isolate)];
        const bool cleanup = retirement && retirement->status == StepStatus::applied &&
                             retirement->effects.referencesReleased &&
                             retirement->effects.callbackIsolated &&
                             retirement->request.key.attempt + 1 == attempt &&
                             (receipt.request.key.participant != Participant::audio ||
                              (isolation && isolation->status == StepStatus::applied &&
                               isolation->effects.callbackIsolated &&
                               isolation->request.key.attempt + 1 == attempt));
        const bool retryReady =
            activation && activation->status == StepStatus::failed &&
            activation->request.key.attempt + 1 == attempt &&
            ((activation->effects.referencesReleased && activation->effects.callbackIsolated) ||
             cleanup);
        const bool retryPhase = receipt.request.key.phase == StepPhase::prepare ||
                                receipt.request.key.phase == StepPhase::fence ||
                                receipt.request.key.phase == StepPhase::activate;
        const bool attemptCleanup =
            cleanupPhase(receipt.request.key.phase) && activation &&
            activation->request.key.attempt == attempt &&
            (activation->status == StepStatus::failed || activation->status == StepStatus::unknown);
        if (attempt > previous->request.key.attempt &&
            ((retryPhase && retryReady) || attemptCleanup))
            previous.reset();
    }
    if (previous) {
        if (terminal(previous->status) || previous->request != receipt.request ||
            (previous->status == StepStatus::inProgress && receipt.status == StepStatus::pending)) {
            fault_ = {FaultSlot::Reason::conflict, receipt.request.key};
            if (active_ && &records_[*active_] == &*found) {
                (*found)->status = ControlStatus::suspended;
                state_ = (*found)->request.kind == ControlKind::close ? HostState::closing
                                                                      : HostState::suspended;
            }
            return core::unexpected(failure("message_conflict", "Conflicting owner receipt"));
        }
    }
    previous = std::move(retained);
    if (receipt.status == StepStatus::unknown || receipt.status == StepStatus::failed) {
        (*found)->status = ControlStatus::suspended;
        if (active_ && &records_[*active_] == &*found)
            state_ = (*found)->request.kind == ControlKind::close ? HostState::closing
                                                                  : HostState::suspended;
    }
    return {};
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure(receipt.request.key);
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::markSoftwareApplied() -> core::Result<void> try {
    if (!owner_.isCurrent() || !active_)
        return core::unexpected(failure("control_pending", "No active software commit"));
    auto& r = *records_[*active_];
    if (fault_.reason != FaultSlot::Reason::none)
        return core::unexpected(failure("control_pending", "Protocol permission revoked"));
    if (resumeTerminated_)
        return core::unexpected(failure("resume_aborted", "Software continuation terminated"));
    const auto& software = r.steps[index(Participant::software)][index(StepPhase::softwareCommit)];
    if (!software || software->status != StepStatus::applied)
        return core::unexpected(
            failure("control_pending", "Software owner has not confirmed commit"));
    r.status = ControlStatus::softwareApplied;
    r.softwareApplied = true;
    return {};
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::closeReady(StepPhase phase) const -> bool {
    if (!owner_.isCurrent() || !active_ || state_ != HostState::closing)
        return false;
    const auto& r = *records_[*active_];
    const auto applied = [&](Participant p, StepPhase ph) {
        const auto& step = r.steps[index(p)][index(ph)];
        return !r.request.participants[index(p)] || (step && step->status == StepStatus::applied);
    };
    if (phase == StepPhase::stop)
        return applied(Participant::capture, StepPhase::cut);
    if (phase == StepPhase::isolate)
        return applied(Participant::audio, StepPhase::stop);
    if (phase == StepPhase::retire) {
        const auto& isolation = r.steps[index(Participant::audio)][index(StepPhase::isolate)];
        return applied(Participant::audio, StepPhase::isolate) &&
               (!r.request.participants[index(Participant::audio)] ||
                isolation->effects.callbackIsolated);
    }
    if (phase == StepPhase::close) {
        const auto released = [&](Participant p) {
            const auto& step = r.steps[index(p)][index(StepPhase::retire)];
            return !r.request.participants[index(p)] ||
                   (step && step->status == StepStatus::applied &&
                    step->effects.referencesReleased);
        };
        return closeReady(StepPhase::retire) && released(Participant::renderer) &&
               released(Participant::software);
    }
    return phase == StepPhase::cut;
}
auto HostCoordinator::finish() -> core::Result<void> try {
    if (!owner_.isCurrent() || !active_)
        return core::unexpected(failure("control_pending", "No active operation"));
    auto& r = *records_[*active_];
    if (fault_.reason != FaultSlot::Reason::none && r.request.kind != ControlKind::close)
        return core::unexpected(failure("control_pending", "Protocol permission revoked"));
    if (r.request.kind == ControlKind::close) {
        if (!closeReady(StepPhase::close))
            return core::unexpected(
                failure("device_pending", "Close still has owner dependencies"));
        for (std::size_t p = 0; p < 4; ++p)
            if (r.request.participants[p]) {
                const auto& closed = r.steps[p][index(StepPhase::close)];
                if (!closed || closed->status != StepStatus::applied)
                    return core::unexpected(
                        failure("device_pending", "Owner teardown not confirmed"));
            }
        state_ = HostState::closed;
    } else {
        if (!r.softwareApplied)
            return core::unexpected(failure("control_pending", "Software commit not confirmed"));
        if (!ownersApplied(r))
            return core::unexpected(failure("device_pending", "Participating owner not applied"));
        state_ = r.request.kind == ControlKind::pause || r.request.kind == ControlKind::stop ||
                         r.request.kind == ControlKind::reset
                     ? HostState::paused
                     : HostState::running;
    }
    r.status = ControlStatus::complete;
    r.cleanupComplete = true;
    if (r.request.target.nextContext)
        context_ = *r.request.target.nextContext;
    const auto rendererPhase = r.request.kind == ControlKind::close
                                   ? StepPhase::close
                                   : controlRequiredPhase(r.request.kind, Participant::renderer);
    const auto& renderer = r.steps[index(Participant::renderer)][index(rendererPhase)];
    if (r.request.participants[index(Participant::renderer)] && renderer &&
        renderer->effects.generation)
        context_.rendererGeneration = renderer->effects.generation;
    r.finalizedContext = context_;
    active_.reset();
    return {};
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::suspend(std::uint64_t operationId) -> core::Result<void> try {
    if (!owner_.isCurrent())
        return core::unexpected(failure("owner_mismatch", "Coordinator owner mismatch"));
    if (!active_ || records_[*active_]->request.operationId != operationId)
        return core::unexpected(failure("control_pending", "No matching active operation"));
    auto& receipt = *records_[*active_];
    receipt.status = ControlStatus::suspended;
    state_ = receipt.request.kind == ControlKind::close ? HostState::closing : HostState::suspended;
    return {};
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::abortResume() -> core::Result<void> try {
    if (!owner_.isCurrent() || !active_)
        return core::unexpected(failure("control_pending", "No active Resume"));
    auto& r = *records_[*active_];
    if (r.request.kind != ControlKind::resume || r.softwareApplied)
        return core::unexpected(
            failure("message_conflict", "A1 cannot roll back software application"));
    const auto& route = r.steps[index(Participant::capture)][index(StepPhase::resumeRoute)];
    if (!route || route->status != StepStatus::applied)
        return core::unexpected(failure("control_pending", "Final route application not known"));
    resumeTerminated_ = true;
    r.directResumeAllowed = false;
    r.status = ControlStatus::failed;
    state_ = HostState::suspended;
    return {};
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::completeCleanup() -> core::Result<void> try {
    if (!owner_.isCurrent() || !active_)
        return core::unexpected(failure("control_pending", "No active cleanup"));
    auto& r = *records_[*active_];
    const bool protocolCleanup = fault_.reason != FaultSlot::Reason::none;
    if (r.request.kind == ControlKind::close || r.status == ControlStatus::complete ||
        r.status == ControlStatus::retired)
        return core::unexpected(failure("control_pending", "No unfinished control cleanup"));
    for (const auto& participant : r.steps)
        for (const auto& step : participant)
            if (step && !terminal(step->status))
                return core::unexpected(failure("device_pending", "Owner step is not isolated"));
    if (protocolCleanup && r.request.participants[index(Participant::capture)]) {
        const auto& cut = r.steps[index(Participant::capture)][index(StepPhase::cut)];
        if (!cut || cut->status != StepStatus::applied)
            return core::unexpected(failure("device_pending", "Capture cleanup unconfirmed"));
    }
    if (r.request.participants[index(Participant::audio)]) {
        const auto& stop = r.steps[index(Participant::audio)][index(StepPhase::stop)];
        const auto& isolation = r.steps[index(Participant::audio)][index(StepPhase::isolate)];
        if (!stop || stop->status != StepStatus::applied || !isolation ||
            isolation->status != StepStatus::applied || !isolation->effects.callbackIsolated)
            return core::unexpected(failure("device_pending", "Audio cleanup unconfirmed"));
    }
    if (r.request.participants[index(Participant::renderer)]) {
        const auto& retired = r.steps[index(Participant::renderer)][index(StepPhase::retire)];
        if (!retired || retired->status != StepStatus::applied ||
            !retired->effects.referencesReleased)
            return core::unexpected(failure("device_pending", "Renderer cleanup unconfirmed"));
    }
    if (protocolCleanup && r.request.participants[index(Participant::software)]) {
        const auto& retired = r.steps[index(Participant::software)][index(StepPhase::retire)];
        if (!retired || retired->status != StepStatus::applied ||
            !retired->effects.referencesReleased)
            return core::unexpected(failure("device_pending", "Software cleanup unconfirmed"));
    }
    r.cleanupComplete = true;
    r.status = ControlStatus::failed;
    if (r.softwareApplied && r.request.target.nextContext)
        context_ = *r.request.target.nextContext;
    if (r.request.kind == ControlKind::resume && r.request.target.nextContext) {
        const auto& route = r.steps[index(Participant::capture)][index(StepPhase::resumeRoute)];
        if (route && route->status == StepStatus::applied)
            context_ = *r.request.target.nextContext;
    }
    const auto& retiredRenderer = r.steps[index(Participant::renderer)][index(StepPhase::retire)];
    if (retiredRenderer && retiredRenderer->status == StepStatus::applied &&
        retiredRenderer->effects.generation)
        context_.rendererGeneration = retiredRenderer->effects.generation;
    r.finalizedContext = context_;
    active_.reset();
    return {};
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::query(std::uint64_t id) const -> core::Result<ControlReceipt> try {
    if (!owner_.isCurrent())
        return core::unexpected(failure("owner_mismatch", "Coordinator owner mismatch"));
    for (const auto& r : records_)
        if (r && r->request.operationId == id)
            return *r;
    return core::unexpected(failure("control_pending", "Control result unavailable"));
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
auto HostCoordinator::acknowledge(std::uint64_t id) -> core::Result<void> try {
    auto r = query(id);
    if (!r)
        return core::unexpected(r.error());
    if (r->status != ControlStatus::complete && r->status != ControlStatus::retired &&
        !(r->status == ControlStatus::failed && r->cleanupComplete))
        return core::unexpected(failure("control_pending", "Control or cleanup not terminal"));
    // Keep the identity tombstone for the entire scope; bounded exhaustion is explicit.
    for (auto& entry : records_)
        if (entry && entry->request.operationId == id)
            entry->status = ControlStatus::retired;
    return {};
} catch (...) {
    if (owner_.isCurrent())
        reportAllocationFailure({});
    return core::unexpected(allocationFailure_);
}
} // namespace cuexis::playback::detail
