#pragma once

// Owner-local Close, cleanup and fence observation records.

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cuexis::internal::execution_diagnostics::rpa {

// Ordered Close/cleanup boundaries. The owner records only boundaries it actually crosses.
enum class CloseStage : std::uint8_t {
    none,
    closeEnter,
    cleanupEnter,
    cleanupDeadline,
    cleanupWait,
    cleanupSuspend,
    cleanupAudioStop,
    cleanupAudioIsolate,
    cleanupRendererRetire,
    cleanupCancelStep,
    cleanupSoftwareRetire,
    cleanupComplete,
    beginClose,
    captureCut,
    closeFlush,
    closeSuspend,
    closeAudioStop,
    closeAudioIsolate,
    closeRendererRetire,
    closeSoftwareRetire,
    closeAudioRetire,
    exportEvidence,
    softwareClose,
    finish,
    confirm,
    shutdown,
    exitWait,
    joins,
    closed,
    count
};

// Path actually taken by the software retirement. "cleanupPermit" is the F1 directed-cleanup
// path that deliberately skips the normal software fence.
enum class FencePath : std::uint8_t {
    none,
    normalControl,
    candidate,
    existingFence,
    cleanupPermit
};

// Whether the normal software fence applies to this path. The default is missing evidence: a
// zero-initialized snapshot never claims that a skipped check was not applicable. Applicability is
// set only when the owner records the path decision, so the F1 permit branch is distinguishable
// from a path that was never observed.
enum class FenceApplicability : std::uint8_t { missingEvidence, applicable, notApplicable };

// Bitmap of the predicates rejected by PlaybackSession::enterExecutionFence(ControlReceipt).
enum class FencePredicate : std::uint8_t {
    operationActive,
    existingFence,
    noAcceptedControl,
    requestMismatch,
    statusMismatch,
    typeMismatch,
    cutMissing,
    count
};

// Coarse reason recorded with the first owner-visible failure. Codes are attached separately.
enum class Reason : std::uint8_t {
    none,
    operationFailed,
    deliveryFailed,
    ownerMismatch,
    operationActive,
    existingFence,
    noAcceptedControl,
    requestMismatch,
    statusMismatch,
    typeMismatch,
    cutMissing,
    bindingMissing,
    bindingScope,
    bindingOperation,
    bindingTarget,
    bindingParticipant,
    bindingOwner,
    directedCleanup,
    retirePhase,
    retireTarget,
    retireKind,
    captureActive,
    audioActive,
    ledgerStart,
    ledgerPublish,
    ownerState
};

struct Identity final {
    bool valid{};
    std::uint64_t scope{};
    std::uint64_t generation{};
    std::uint64_t candidateId{};
    std::uint64_t controlSequence{};
    std::uint64_t operationId{};
    std::uint64_t decisionOrdinal{};
    std::uint8_t kind{};
    std::uint8_t status{};
    // Remaining HostContext fields, captured from the accepted request/proof by its owner. They are
    // zero when the source did not expose them (for example an existing fence identity).
    std::uint64_t clockSegment{};
    std::uint64_t runEpoch{};
    std::uint64_t resourceGeneration{};
    std::uint64_t rendererGeneration{};
    std::array<std::uint8_t, 32> contentIdentity{};
};

struct Mark final {
    CloseStage stage{CloseStage::none};
    std::int64_t atUs{};
};

struct Wait final {
    CloseStage stage{CloseStage::none};
    std::int64_t us{};
    bool overBudget{};
};

enum class CloseStepState : std::uint8_t {
    notExecuted,
    resultPending,
    rejected,
    applied,
    observationMissing
};
enum class CloseObservationPart : std::uint8_t { cleanup, closeRetire, closeStep };

struct Fence final {
    FencePath path{FencePath::none};
    FenceApplicability applicability{FenceApplicability::missingEvidence};
    Identity identity{};
    bool existingFence{};
    bool attempted{};
    bool succeeded{};
    std::uint32_t rejected{};
    Identity existingIdentity{};
    Reason reason{Reason::none};
    // Directed-cleanup permit checks (owner ledger first, then playback binding).
    bool permitAttempted{};
    bool permitAcceptedControl{};
    bool permitDirected{};
    bool permitRequestMatch{};
    bool permitTargetMatch{};
    bool permitOperationMatch{};
    bool permitParticipantMatch{};
    bool permitCaptureCut{};
    bool permitAudioIsolated{};
    bool permitLedgerStart{};
    bool retireCalled{};
    bool retireApplied{};
    bool referencesReleased{};
    // validateExecutionControlBinding() predicates.
    bool bindingChecked{};
    bool bindingOwner{};
    bool bindingParticipant{};
    bool bindingScope{};
    bool bindingOperation{};
    bool bindingTarget{};
    // SessionSoftwareClose path.
    bool closeAttempted{};
    bool closeApplied{};
    CloseStepState closeState{CloseStepState::notExecuted};
    char firstErrorCode[errorCodeCapacity]{};
};

struct OwnerCloseObservation final {
    Fence cleanup{};
    Fence closeRetire{};
    Fence closeStep{};
    Identity currentFence{};
};

struct Close final {
    std::int64_t requestedUs{};
    std::int64_t deadlineUs{};
    Mark marks[closeMarkCapacity]{};
    std::uint8_t markCount{};
    std::uint8_t marksLost{};
    bool truncated{};
    Reason firstCause{Reason::none};
    CloseStage firstCauseStage{CloseStage::none};
    char firstErrorCode[errorCodeCapacity]{};
    std::uint8_t errors{};
    Identity identity{};
    Fence cleanupFence{};
    Fence closeRetireFence{};
    // The Close step itself, never a cleanup or retirement record.
    Fence fence{};
    // Entry context of the original Close attempt. A second Close after a rejected first attempt
    // keeps the original values and only increments attempts/reentries.
    std::uint8_t attempts{};
    std::uint8_t reentries{};
    std::uint64_t firstDecisionOrdinal{};
    std::uint8_t pendingJobOwner{}; // 0 none, 1 session worker, 2 audio worker
    std::uint8_t pendingJobKind{};  // ControlKind of the unanswered control, 0 when none
    bool pendingSessionJobAtEntry{};
    bool pendingAudioJobAtEntry{};
    std::uint8_t cleanupQueryStatus{}; // HostState observed by cleanup's coordinator query
    bool cleanupSuspendCalled{};
    std::uint64_t cleanupWaitUs{};
    std::uint32_t cleanupWaitSamples{};
    std::uint64_t closeTick{};
    std::uint64_t unackedFirstTick{};
    std::uint64_t unackedLastTick{};
    std::uint32_t unackedDeliveries{};
    std::uint32_t evidenceDeliveries{};
    std::uint64_t evidenceMembers{};
    std::uint8_t captureTerminal{};
    std::uint8_t softwareTerminal{};
    std::uint8_t rendererTerminal{};
    std::uint8_t audioTerminal{};
    bool audioStopCalled{};
    bool audioIsolateCalled{};
    bool audioCallbackIsolated{};
    bool audioRetireCalled{};
    bool captureClosed{};
    bool softwareRetired{};
    bool softwareClosed{};
    bool rendererRetired{};
    bool rendererClosed{};
    std::uint8_t coordinatorBeforeSuspend{};
    std::uint8_t coordinatorAfterSuspend{};
    std::uint8_t coordinatorAtFinish{};
    std::uint32_t pumpSamples{};
    std::uint32_t pumpClosing{};
    std::uint32_t pumpClosed{};
    std::uint8_t facadeBefore{};
    std::uint8_t facadeAfter{};
    std::uint8_t receiptStatus{};
    bool finishCalled{};
    bool confirmCalled{};
    bool shutdownRequested{};
    bool workerExited{};
    bool workerJoined{};
    bool audioJoined{};
    bool closedFlag{};
    Wait waits[closeWaitCapacity]{};
    std::uint8_t waitCount{};
    std::uint8_t waitsLost{};
    CloseStage firstOverBudget{CloseStage::none};
};
inline void mark(Close& close, CloseStage stage) noexcept {
    if (close.markCount < closeMarkCapacity)
        close.marks[close.markCount++] = Mark{stage, nowUs()};
    else {
        ++close.marksLost;
        close.truncated = true;
    }
}

inline void copyCode(char (&target)[errorCodeCapacity], std::string_view code) noexcept {
    std::size_t index = 0;
    for (; index < code.size() && index + 1 < errorCodeCapacity; ++index)
        target[index] = code[index];
    target[index] = '\0';
}

// The first failure wins; later failures only increment the counter and never overwrite it.
inline void firstCause(Close& close, Reason reason, CloseStage stage,
                       std::string_view code) noexcept {
    if (close.errors++ != 0)
        return;
    close.firstCause = reason;
    close.firstCauseStage = stage;
    copyCode(close.firstErrorCode, code);
}

inline void addWait(Close& close, CloseStage stage, std::int64_t us, bool overBudget) noexcept {
    if (close.waitCount < closeWaitCapacity)
        close.waits[close.waitCount++] = Wait{stage, us, overBudget};
    else {
        ++close.waitsLost;
        close.truncated = true;
    }
    if (overBudget && close.firstOverBudget == CloseStage::none)
        close.firstOverBudget = stage;
}

// Thread-scoped sink used while a worker thread calls into playback. Only the owning thread
// sets it, only for the duration of one call, and it is never read from another thread.
inline thread_local Fence* fenceSink = nullptr;
struct FenceScope final {
    Fence* previous;
    explicit FenceScope(Fence* sink) noexcept : previous(fenceSink) {
        fenceSink = sink;
    }
    ~FenceScope() {
        fenceSink = previous;
    }
    FenceScope(const FenceScope&) = delete;
    FenceScope& operator=(const FenceScope&) = delete;
};
inline void reject(FencePredicate predicate) noexcept {
    if (fenceSink)
        fenceSink->rejected |= 1u << static_cast<unsigned>(predicate);
}
inline void noteReason(Reason reason) noexcept {
    if (fenceSink)
        fenceSink->reason = reason;
}
inline void noteExisting(const Identity& identity) noexcept {
    if (!fenceSink)
        return;
    fenceSink->existingFence = true;
    fenceSink->existingIdentity = identity;
}

} // namespace cuexis::internal::execution_diagnostics::rpa
#endif
