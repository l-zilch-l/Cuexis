#include <cuexis/judgement/advance_kernel.hpp>

#include <algorithm>
#include <utility>

namespace cuexis::judgement::runtime {
namespace {

auto kernelError(std::string code, std::string reason, std::string path, std::string message)
    -> core::Error {
    return core::Error{std::move(code), std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("reason", std::move(reason))
        .withContext("fieldPath", std::move(path));
}

auto lateError(std::string message) -> core::Error {
    return core::Error{"judgement.late.rejected", std::move(message)}
        .withContext("category", "late_policy_incomplete")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}

auto validateLate(const LatePolicyParameters& late) -> core::Result<void> {
    if (!late.policy || !late.windowOpenThreshold.isMeasured() ||
        !late.windowCloseThreshold.isMeasured() || !late.finalizationWatermark.isMeasured() ||
        !late.maxQueueHop.isMeasured()) {
        return core::unexpected(lateError("the late policy is incomplete"));
    }
    const auto open = late.windowOpenThreshold.measuredValue()->value();
    const auto close = late.windowCloseThreshold.measuredValue()->value();
    const auto finalization = late.finalizationWatermark.measuredValue()->value();
    const auto hop = late.maxQueueHop.measuredValue()->value();
    if (open < 0 || close < 0 || finalization <= close ||
        (*late.policy == LateEventPolicy::rejectLate && hop != 0) ||
        (*late.policy == LateEventPolicy::queueNextTick &&
         (hop < 1 || hop > finalization - close))) {
        return core::unexpected(lateError("the late policy has an invalid O/C/F/H relationship"));
    }
    return {};
}

} // namespace

AdvanceKernel::AdvanceKernel(PreparedGameplay prepared, RunKey run, LatePolicyParameters late)
    : prepared_(std::move(prepared)), run_(run), late_(std::move(late)) {}

auto AdvanceKernel::prepare(PreparedGameplay prepared, RunKey run) -> core::Result<AdvanceKernel> {
    auto valid = validateLate(prepared.records().latePolicy);
    if (!valid) {
        return core::unexpected(valid.error());
    }
    auto late = prepared.records().latePolicy;
    return AdvanceKernel{std::move(prepared), run, std::move(late)};
}

auto AdvanceKernel::route(Tick observation, bool& forwarded) const -> std::optional<Tick> {
    forwarded = false;
    if (!frontier_) {
        return observation;
    }
    const auto close =
        offsetTicks(*frontier_, TickSpan{-late_.windowCloseThreshold.measuredValue()->value()});
    if (!close) {
        return std::nullopt;
    }
    if (observation >= *close) {
        return observation;
    }
    const auto finalization =
        offsetTicks(observation, *late_.finalizationWatermark.measuredValue());
    if (!finalization || observation >= *finalization ||
        *late_.policy == LateEventPolicy::rejectLate) {
        return std::nullopt;
    }
    auto queued = offsetTicks(*close, TickSpan{1});
    if (!queued) {
        return std::nullopt;
    }
    const auto hop = differenceTicks(*queued, observation);
    if (!hop || hop->value() < 1 || hop->value() > late_.maxQueueHop.measuredValue()->value()) {
        return std::nullopt;
    }
    forwarded = true;
    return *queued;
}

auto AdvanceKernel::receiveCanonicalBatch(std::span<const TickRecord> records)
    -> core::Result<std::vector<IngressReceipt>> {
    if (receiveClosed_) {
        return core::unexpected(kernelError("judgement.stop_cut.closed", "receive_closed",
                                            "/receive", "the receive boundary is closed"));
    }
    for (const auto& record : records) {
        if (record.domainToken.empty() || record.sourceClass.empty() ||
            record.channelToken.empty()) {
            return core::unexpected(kernelError("judgement.execution.relation_invalid",
                                                "canonical_invalid", "/records",
                                                "a canonical record has an empty identity field"));
        }
    }
    if (!records.empty() &&
        records.size() > std::numeric_limits<std::uint64_t>::max() - nextSequence_.value()) {
        return core::unexpected(kernelError("host.rpa.capacity_exceeded", "capacity", "/records",
                                            "the sequence space is exhausted"));
    }
    //  Number the whole batch before any late/admission decision, then commit records and cursor
    //  together. No operation after this point can fail.
    std::vector<IngressReceipt> receipts;
    receipts.reserve(records.size());
    auto sequence = nextSequence_.value();
    for (const auto& record : records) {
        bool forwarded = false;
        const auto dispatch = route(record.observationTick, forwarded);
        IngressReceipt receipt;
        receipt.sequence = IngressSequence{sequence};
        receipt.dispatchTick = dispatch.value_or(record.observationTick);
        receipt.accepted = dispatch.has_value();
        receipt.forwarded = forwarded;
        if (!dispatch) {
            receipt.rejectionCode = "judgement.late.rejected";
        }
        ++sequence;
        receipts.push_back(std::move(receipt));
    }
    for (const auto& receipt : receipts) {
        archive_.push_back(receipt);
        if (receipt.accepted) {
            pending_.push_back(archive_.size() - 1);
        }
        acceptedThrough_ = receipt.sequence;
    }
    nextSequence_ = IngressSequence{sequence};
    return receipts;
}

auto AdvanceKernel::acceptedIngressThrough() const noexcept -> std::optional<IngressSequence> {
    return acceptedThrough_;
}

auto AdvanceKernel::processThrough(Tick horizon) -> std::size_t {
    std::vector<std::size_t> ordered;
    ordered.reserve(pending_.size());
    for (const auto index : pending_) {
        if (archive_[index].dispatchTick <= horizon) {
            ordered.push_back(index);
        }
    }
    std::sort(ordered.begin(), ordered.end(), [this](auto left, auto right) {
        if (archive_[left].dispatchTick != archive_[right].dispatchTick) {
            return archive_[left].dispatchTick < archive_[right].dispatchTick;
        }
        return archive_[left].sequence < archive_[right].sequence;
    });
    for (const auto index : ordered) {
        pending_.erase(std::find(pending_.begin(), pending_.end(), index));
    }
    return ordered.size();
}

auto AdvanceKernel::advance(Tick horizon) -> core::Result<AdvanceSummary> {
    if (receiveClosed_) {
        return core::unexpected(kernelError("judgement.stop_cut.closed", "receive_closed",
                                            "/advance", "the receive boundary is closed"));
    }
    if (frontier_ && horizon < *frontier_) {
        return core::unexpected(kernelError("judgement.execution.horizon_reversed",
                                            "horizon_reversed", "/horizon",
                                            "the advance horizon moved backwards"));
    }
    const auto processed = processThrough(horizon);
    frontier_ = horizon;
    return AdvanceSummary{horizon, processed, 0};
}

auto AdvanceKernel::advanceStopCut(const StopCut& cut, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceSummary> {
    if (receiveClosed_) {
        return core::unexpected(kernelError("judgement.stop_cut.closed", "receive_closed",
                                            "/stopCut", "a StopCut was already accepted"));
    }
    if (cut.run != run_) {
        return core::unexpected(kernelError("judgement.stop_cut.run_mismatch", "run_mismatch",
                                            "/run", "the StopCut names another run"));
    }
    if (cut.acceptedThrough != acceptedThrough_) {
        return core::unexpected(kernelError("judgement.stop_cut.bound_mismatch", "bound_mismatch",
                                            "/acceptedThrough",
                                            "the StopCut upper bound is not the kernel cursor"));
    }
    if (frontier_ && cut.cutoff < *frontier_) {
        return core::unexpected(kernelError("judgement.stop_cut.cutoff_reversed", "cutoff_reversed",
                                            "/cutoff",
                                            "the Stop cutoff precedes the sealed frontier"));
    }
    if (slots.size() < prepared_.settlementCapacity()) {
        return core::unexpected(kernelError("capability.budget_insufficient", "budget", "/slots",
                                            "the settlement slot count is too small"));
    }
    receiveClosed_ = true;
    const auto processed = processThrough(cut.cutoff);
    frontier_ = cut.cutoff;
    return AdvanceSummary{cut.cutoff, processed, 0};
}

} // namespace cuexis::judgement::runtime
