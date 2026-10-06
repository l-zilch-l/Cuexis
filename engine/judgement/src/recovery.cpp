#include <cuexis/judgement/recovery.hpp>

namespace cuexis::judgement {
namespace {
auto sameError(const std::optional<core::Error>& a, const std::optional<core::Error>& b) -> bool {
    const auto* left = a ? &*a : nullptr;
    const auto* right = b ? &*b : nullptr;
    while (left && right) {
        if (left->code() != right->code() || left->message() != right->message() ||
            left->context().size() != right->context().size()) {
            return false;
        }
        for (std::size_t i = 0; i < left->context().size(); ++i) {
            if (left->context()[i].key != right->context()[i].key ||
                left->context()[i].value != right->context()[i].value) {
                return false;
            }
        }
        left = left->cause();
        right = right->cause();
    }
    return left == nullptr && right == nullptr;
}
} // namespace
auto sameKernelResult(const KernelProjection& a, const KernelProjection& b) -> bool {
    return a.phases == b.phases && a.resources == b.resources && a.contacts == b.contacts &&
           a.ownership == b.ownership && a.observers == b.observers && a.receipts == b.receipts &&
           a.facts == b.facts && a.processedFrontier == b.processedFrontier &&
           a.lastAdvanceHorizon == b.lastAdvanceHorizon && a.state == b.state &&
           sameError(a.faultDiagnostic, b.faultDiagnostic) &&
           a.judgementIdentity == b.judgementIdentity && a.failedTick == b.failedTick &&
           a.requestedHorizon == b.requestedHorizon && a.kernelWorkTick == b.kernelWorkTick &&
           a.sealedFactCursor == b.sealedFactCursor && a.fold == b.fold &&
           a.effectiveFactSemanticRevision == b.effectiveFactSemanticRevision &&
           a.faultStage == b.faultStage;
}
auto sameSnapshot(const SnapshotDTO& a, const SnapshotDTO& b) -> bool {
    if (!sameKernelResult(a.kernel, b.kernel) || a.matchers != b.matchers ||
        a.coverageHistory != b.coverageHistory || a.activeRequirements != b.activeRequirements ||
        a.timerCursor != b.timerCursor || a.nextCommitId != b.nextCommitId ||
        a.commitIdsExhausted != b.commitIdsExhausted || a.ingress != b.ingress ||
        a.pending.size() != b.pending.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.pending.size(); ++i) {
        const auto& x = a.pending[i];
        const auto& y = b.pending[i];
        if (x.observationKey != y.observationKey || x.dispatchTick != y.dispatchTick ||
            x.wasForwarded != y.wasForwarded || x.admissionFrontier != y.admissionFrontier ||
            x.admissionHorizon != y.admissionHorizon) {
            return false;
        }
    }
    return true;
}
auto replayCutAt(const ReplayArchive& archive, Tick horizon) -> ReplayCut {
    const auto& rows = archive.data().records;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].horizon && *rows[i].horizon >= horizon) {
            return *rows[i].horizon == horizon ? ReplayCut{static_cast<std::uint64_t>(i + 1), {}}
                                               : ReplayCut{static_cast<std::uint64_t>(i), horizon};
        }
    }
    return {static_cast<std::uint64_t>(rows.size()), horizon};
}
auto sameReplayHistory(const ReplayArchive& a, const ReplayArchive& b) -> bool {
    const auto& x = a.data();
    const auto& y = b.data();
    if (x.identity != y.identity || x.eventCount != y.eventCount ||
        x.records.size() != y.records.size()) {
        return false;
    }
    for (std::size_t i = 0; i < x.records.size(); ++i) {
        const auto& l = x.records[i];
        const auto& r = y.records[i];
        if (l.batch != r.batch || l.admission != r.admission || l.horizon != r.horizon ||
            static_cast<bool>(l.result) != static_cast<bool>(r.result) ||
            (l.result && !sameKernelResult(*l.result, *r.result))) {
            return false;
        }
    }
    return true;
}
} // namespace cuexis::judgement
