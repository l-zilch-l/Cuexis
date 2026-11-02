#include "host_protocol.hpp"
#include "host_protocol_helpers.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace cuexis::playback::detail {

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

} // namespace cuexis::playback::detail
