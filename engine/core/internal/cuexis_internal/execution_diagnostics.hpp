#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cuexis::internal::execution_diagnostics {
enum class Part { copy, timers, validate, ledger, reserve, foldCopy, fold, idleDeadline, count };
} // namespace cuexis::internal::execution_diagnostics

#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace cuexis::internal::execution_diagnostics {
inline auto now() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
inline auto enabled() noexcept -> bool {
#ifdef _MSC_VER
    char value[8]{};
    std::size_t length{};
    return getenv_s(&length, value, sizeof(value), "CUEXIS_EXECUTION_DIAGNOSTIC_RUN") == 0 &&
           length == 2 && value[0] == '1';
#else
    const auto* value = std::getenv("CUEXIS_EXECUTION_DIAGNOSTIC_RUN");
    return value && value[0] == '1' && value[1] == 0;
#endif
}
struct Trace;
inline thread_local Trace* current{};
inline std::mutex outputMutex;
inline std::atomic<std::uint64_t> nextId{};
struct Trace final {
    const char* name;
    Trace* previous;
    bool recording;
    std::uint64_t id{}, timers{}, cursor{}, ticks{}, job{};
    std::int64_t horizon{}, tick{}, started{};
    int exceptions{};
    std::array<std::uint64_t, static_cast<unsigned>(Part::count)> calls{}, nanoseconds{};
    explicit Trace(const char* stage, std::int64_t target = 0, std::uint64_t count = 0,
                   std::uint64_t request = 0) noexcept
        : name(stage), previous(current), recording(enabled()), timers(count), job(request),
          horizon(target), exceptions(std::uncaught_exceptions()) {
        if (recording) {
            id = ++nextId;
            started = now();
            current = this;
            emit("begin");
        }
    }
    void emit(const char* state) const noexcept {
        if (!recording)
            return;
        try {
            std::lock_guard lock(outputMutex);
            std::fprintf(stderr,
                         "EXECUTION_DIAGNOSTIC {\"stage\":\"%s\",\"state\":\"%s\","
                         "\"id\":%llu,\"job_id\":%llu,\"parent_id\":%llu,\"thread\":%llu,\"time_"
                         "ns\":%lld,\"started_ns\":%lld,"
                         "\"elapsed_ns\":%lld,\"horizon\":%lld,\"timers\":%llu,"
                         "\"timer_cursor\":%llu,\"ticks\":%llu,\"tick\":%lld,\"calls\":[",
                         name, state, static_cast<unsigned long long>(id),
                         static_cast<unsigned long long>(job),
                         static_cast<unsigned long long>(previous ? previous->id : 0),
                         static_cast<unsigned long long>(
                             std::hash<std::thread::id>{}(std::this_thread::get_id())),
                         static_cast<long long>(now()), static_cast<long long>(started),
                         static_cast<long long>(now() - started), static_cast<long long>(horizon),
                         static_cast<unsigned long long>(timers),
                         static_cast<unsigned long long>(cursor),
                         static_cast<unsigned long long>(ticks), static_cast<long long>(tick));
            for (unsigned i = 0; i < calls.size(); ++i)
                std::fprintf(stderr, "%s%llu", i ? "," : "",
                             static_cast<unsigned long long>(calls[i]));
            std::fputs("],\"part_ns\":[", stderr);
            for (unsigned i = 0; i < nanoseconds.size(); ++i)
                std::fprintf(stderr, "%s%llu", i ? "," : "",
                             static_cast<unsigned long long>(nanoseconds[i]));
            std::fputs("]}\n", stderr);
            std::fflush(stderr);
        } catch (...) {
            // Diagnostic output cannot change a transaction or a worker outcome.
        }
    }
    void progress(std::int64_t at, std::uint64_t position) noexcept {
        tick = at;
        cursor = position;
        ++ticks;
        if ((ticks & (ticks - 1)) == 0)
            emit("progress");
    }
    void sealed(std::int64_t at, std::uint64_t position) noexcept {
        tick = at;
        cursor = position;
    }
    ~Trace() noexcept {
        if (recording) {
            emit(std::uncaught_exceptions() > exceptions ? "unwinding" : "end");
            current = previous;
        }
    }
};
struct PartScope final {
    Trace* trace{current};
    unsigned part;
    std::int64_t started{};
    explicit PartScope(Part value) noexcept : part(static_cast<unsigned>(value)) {
        if (trace)
            started = now();
    }
    ~PartScope() noexcept {
        if (trace) {
            ++trace->calls[part];
            trace->nanoseconds[part] += static_cast<std::uint64_t>(now() - started);
        }
    }
};
} // namespace cuexis::internal::execution_diagnostics
#else
namespace cuexis::internal::execution_diagnostics {
struct Trace final {
    explicit Trace(const char*, std::int64_t = 0, std::uint64_t = 0, std::uint64_t = 0) noexcept {}
    void emit(const char*) const noexcept {}
    void progress(std::int64_t, std::uint64_t) noexcept {}
    void sealed(std::int64_t, std::uint64_t) noexcept {}
};
struct PartScope final {
    explicit PartScope(Part) noexcept {}
};
} // namespace cuexis::internal::execution_diagnostics
#endif

// F5/R0 owner-local read-only observation. Fixed-size aggregates only: no allocation, no
// formatting and no locks on the recording path. The channel is disabled at compile time
// unless CUEXIS_RPA_OBSERVATION_TEST_ONLY is defined, and a build with it defined is never
// acceptable as formal performance evidence.
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
#include <array>
#include <atomic>
#include <string_view>
namespace cuexis::internal::execution_diagnostics::rpa {
inline constexpr std::size_t closeMarkCapacity = 32;
inline constexpr std::size_t closeWaitCapacity = 8;
inline constexpr std::size_t errorCodeCapacity = 48;

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

// Seek attribution. Phases are fixed by the remediation plan §5.1; nesting is allowed, and
// only root scopes account toward the envelope deduction. No per-seek record is retained.
enum class SeekPhase : std::uint8_t {
    s0HostIdentity,
    s1HostProtocol,
    s2CandidatePrep,
    s3Validation,
    s4InternalExecution,
    s5PublishSwap,
    s6ClockPresent,
    count
};
inline constexpr std::size_t seekPhaseCount = static_cast<std::size_t>(SeekPhase::count);
struct SeekPhaseSlot final {
    std::uint64_t count{};
    std::uint64_t rootCount{};
    std::int64_t totalUs{};
    std::int64_t rootTotalUs{};
    std::int64_t maxUs{};
};
// Work volume per phase. Counts are collected from values the owner already holds; a zero means
// "not available at this boundary", never "no work". No field is estimated by traversing state.
#define CUEXIS_RPA_SEEK_WORK_FIELDS(X)                                                             \
    X(archiveBytes)                                                                                \
    X(encodeCalls)                                                                                 \
    X(encodeAllocations)                                                                           \
    X(encodeCopies)                                                                                \
    X(sealedIdentityKnown)                                                                         \
    X(sealedIdentityPresent)                                                                       \
    X(sealedIdentity)                                                                              \
    X(operationId)                                                                                 \
    X(generation)                                                                                  \
    X(participants)                                                                                \
    X(prepareCalls)                                                                                \
    X(fenceCalls)                                                                                  \
    X(activateCalls)                                                                               \
    X(confirmCalls)                                                                                \
    X(waitSamples)                                                                                 \
    X(waitUs)                                                                                      \
    X(waitReasons)                                                                                 \
    X(candidateCalls)                                                                              \
    X(validCandidates)                                                                             \
    X(discardedCandidates)                                                                         \
    X(entities)                                                                                    \
    X(requirements)                                                                                \
    X(phaseOffsets)                                                                                \
    X(matcherElements)                                                                             \
    X(timerElements)                                                                               \
    X(timerRequirements)                                                                           \
    X(preparedHandles)                                                                             \
    X(configCopies)                                                                                \
    X(configValues)                                                                                \
    X(requirementSets)                                                                             \
    X(actualRecords)                                                                               \
    X(batchInputs)                                                                                 \
    X(controlHorizons)                                                                             \
    X(sameKernelCalls)                                                                             \
    X(sameSnapshotCalls)                                                                           \
    X(comparedElements)                                                                            \
    X(nestedResultElements)                                                                        \
    X(nestedResultBytes)                                                                           \
    X(fullTicks)                                                                                   \
    X(idleTicks)                                                                                   \
    X(timers)                                                                                      \
    X(matcherSteps)                                                                                \
    X(folds)                                                                                       \
    X(facts)                                                                                       \
    X(runtimeCopies)                                                                               \
    X(runtimeElements)                                                                             \
    X(runtimePayloadBytes)                                                                         \
    X(runtimePeakElements)                                                                         \
    X(recordPeakElements)                                                                          \
    X(objects)                                                                                     \
    X(publishedFacts)                                                                              \
    X(copyAllocations)                                                                             \
    X(releases)                                                                                    \
    X(oldOwnersRetained)                                                                           \
    X(clockValid)                                                                                  \
    X(targetGeneration)                                                                            \
    X(targetPresentationTick)                                                                      \
    X(endToEndUs)                                                                                  \
    X(kernelPrepareCalls)                                                                          \
    X(kernelPrepareSuccesses)                                                                      \
    X(kernelPrepareUs)                                                                             \
    X(derivedIndexBuilds)                                                                          \
    X(derivedIndexUs)                                                                              \
    X(initialReserveCopies)                                                                        \
    X(recoveryConfigCopies)                                                                        \
    X(rpaRestoreValidationUs)                                                                      \
    X(draftCopies)                                                                                 \
    X(faultReserveCopies)                                                                          \
    X(foldCopies)                                                                                  \
    X(finalizationCopies)                                                                          \
    X(tailRecordCopies)                                                                            \
    X(tailBatchElements)                                                                           \
    X(tailAdmissionElements)                                                                       \
    X(tailControlElements)
struct SeekWork final {
#define CUEXIS_RPA_SEEK_WORK_DECLARE(name) std::uint64_t name{};
    CUEXIS_RPA_SEEK_WORK_FIELDS(CUEXIS_RPA_SEEK_WORK_DECLARE)
#undef CUEXIS_RPA_SEEK_WORK_DECLARE
};
struct SeekLedger final {
    std::uint64_t seeks{};
    std::int64_t envelopeTotalUs{};
    std::int64_t envelopeMaxUs{};
    SeekPhaseSlot phases[seekPhaseCount]{};
    SeekWork work[seekPhaseCount]{};
    std::int64_t unaccountedTotalUs{};
    std::int64_t unaccountedMaxUs{};
    std::uint64_t seeksWithoutPhases{};
    // The observation fetch is a perturbation, not a seek phase.
    std::uint64_t fetchCount{};
    std::int64_t fetchTotalUs{};
    // One-off cost of the instrumented clock; recorded so overhead is never silently assumed.
    std::uint32_t timerCostNs{};
    std::uint8_t depth{};
    std::uint8_t seekRootCount{};
    std::int64_t seekRootUs{};
    std::int64_t seekStartedUs{};
};

// Thread-scoped sink used while the owning worker thread calls into Playback. Phases recorded from
// Playback source (S2/S3) land in the same owner-local ledger as the worker phases.
inline thread_local SeekLedger* seekSink = nullptr;
inline auto seekWork(SeekLedger& ledger, SeekPhase phase) noexcept -> SeekWork& {
    return ledger.work[static_cast<std::size_t>(phase)];
}
inline auto seekWork(SeekPhase phase) noexcept -> SeekWork* {
    if (!seekSink)
        return nullptr;
    return &seekWork(*seekSink, phase);
}
inline void seekAdd(SeekLedger& ledger, SeekPhase phase, std::uint64_t SeekWork::* field,
                    std::uint64_t amount = 1) noexcept {
    seekWork(ledger, phase).*field += amount;
}
inline void seekAdd(SeekPhase phase, std::uint64_t SeekWork::* field,
                    std::uint64_t amount = 1) noexcept {
    if (auto* work = seekWork(phase))
        work->*field += amount;
}
inline void seekSet(SeekLedger& ledger, SeekPhase phase, std::uint64_t SeekWork::* field,
                    std::uint64_t value) noexcept {
    seekWork(ledger, phase).*field = value;
}
inline void seekSet(SeekPhase phase, std::uint64_t SeekWork::* field,
                    std::uint64_t value) noexcept {
    if (auto* work = seekWork(phase))
        work->*field = value;
}
// Current innermost phase on the owning thread, used only by wait accounting that cannot pass a
// phase explicitly (the host protocol wait inside begin()). Never a global.
inline thread_local SeekPhase seekPhaseSink{SeekPhase::count};
// Wait reason bits. Fixed slots, no per-wait record: 1 reply-not-ready, 2 deadline expired,
// 4 worker exited, 8 pump ran, 16 audio worker waiting.
inline constexpr std::uint64_t seekWaitReply = 1;
inline constexpr std::uint64_t seekWaitDeadline = 2;
inline constexpr std::uint64_t seekWaitExited = 4;
inline constexpr std::uint64_t seekWaitPump = 8;
inline constexpr std::uint64_t seekWaitAudio = 16;
inline void seekWait(SeekPhase phase, std::uint64_t reason, std::int64_t us) noexcept {
    if (!seekSink || phase == SeekPhase::count)
        return;
    auto& work = seekWork(*seekSink, phase);
    ++work.waitSamples;
    work.waitUs += static_cast<std::uint64_t>(us);
    work.waitReasons |= reason;
}
struct SeekSinkScope final {
    SeekLedger* previous;
    explicit SeekSinkScope(SeekLedger* sink) noexcept : previous(seekSink) {
        seekSink = sink;
    }
    ~SeekSinkScope() {
        seekSink = previous;
    }
    SeekSinkScope(const SeekSinkScope&) = delete;
    SeekSinkScope& operator=(const SeekSinkScope&) = delete;
};

inline auto nowUs() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// A nested cost belongs to its enclosing phase and is never added to the seek envelope.
// No clock read is needed when an observation build has no owner-local sink bound.
struct SeekWorkTimer final {
    SeekWork* work;
    std::uint64_t SeekWork::* field;
    std::int64_t started;
    SeekWorkTimer(SeekPhase phase, std::uint64_t SeekWork::* member) noexcept
        : work(seekWork(phase)), field(member), started(work ? nowUs() : 0) {}
    void finish() noexcept {
        if (work)
            work->*field += static_cast<std::uint64_t>(nowUs() - started);
        work = nullptr;
    }
    ~SeekWorkTimer() {
        finish();
    }
    SeekWorkTimer(const SeekWorkTimer&) = delete;
    SeekWorkTimer& operator=(const SeekWorkTimer&) = delete;
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

// A sample is admissible only when it comes from a normal build, matches the recorded
// identity and was taken inside a pre-registered window. Instrumented builds never qualify.
enum class SampleReject : std::uint8_t {
    none,
    instrumentedBuild,
    identityMismatch,
    noFormalWindow
};
struct SampleClaim final {
    bool instrumentedBuild{};
    bool formalWindow{};
    bool identityMatched{};
};
inline auto classifySample(const SampleClaim& claim) noexcept -> SampleReject {
    if (claim.instrumentedBuild)
        return SampleReject::instrumentedBuild;
    if (!claim.identityMatched)
        return SampleReject::identityMismatch;
    if (!claim.formalWindow)
        return SampleReject::noFormalWindow;
    return SampleReject::none;
}

// Average cost of one instrumented clock read. Called once outside any measured window.
inline auto calibrateTimer() noexcept -> std::uint32_t {
    constexpr int iterations = 1024;
    std::int64_t sink = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int index = 0; index < iterations; ++index)
        sink += nowUs();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();
    (void)sink;
    return static_cast<std::uint32_t>(elapsed / iterations);
}

inline void seekRecord(SeekLedger& ledger, SeekPhase phase, std::int64_t us,
                       std::uint8_t depth) noexcept {
    auto& slot = ledger.phases[static_cast<std::size_t>(phase)];
    ++slot.count;
    slot.totalUs += us;
    if (us > slot.maxUs)
        slot.maxUs = us;
    if (depth == 0) {
        ++slot.rootCount;
        slot.rootTotalUs += us;
        ++ledger.seekRootCount;
        ledger.seekRootUs += us;
    }
}

struct SeekScope final {
    SeekLedger* ledger;
    SeekPhase phase;
    std::int64_t startedUs;
    std::uint8_t depth;
    SeekPhase previousPhase;
    explicit SeekScope(SeekLedger* target, SeekPhase which) noexcept
        : ledger(target), phase(which), startedUs(nowUs()), depth(target ? target->depth : 0),
          previousPhase(seekPhaseSink) {
        if (ledger)
            ++ledger->depth;
        seekPhaseSink = which;
    }
    // Records into the thread-scoped ledger bound by the owning worker, if any.
    explicit SeekScope(SeekPhase which) noexcept : SeekScope(seekSink, which) {}
    // Records exactly once and detaches; the destructor then has nothing left to do.
    void finish() noexcept {
        seekPhaseSink = previousPhase;
        if (!ledger)
            return;
        --ledger->depth;
        seekRecord(*ledger, phase, nowUs() - startedUs, depth);
        ledger = nullptr;
    }
    ~SeekScope() {
        finish();
        seekPhaseSink = previousPhase;
    }
    SeekScope(const SeekScope&) = delete;
    SeekScope& operator=(const SeekScope&) = delete;
};

// The per-seek envelope is the deduction basis: unaccounted = envelope - root phase total.
struct SeekEnvelope final {
    SeekLedger* ledger;
    explicit SeekEnvelope(SeekLedger* target) noexcept : ledger(target) {
        if (!ledger)
            return;
        ledger->seekRootCount = 0;
        ledger->seekRootUs = 0;
        ledger->seekStartedUs = nowUs();
    }
    ~SeekEnvelope() {
        if (!ledger)
            return;
        const auto elapsed = nowUs() - ledger->seekStartedUs;
        ++ledger->seeks;
        ledger->envelopeTotalUs += elapsed;
        if (elapsed > ledger->envelopeMaxUs)
            ledger->envelopeMaxUs = elapsed;
        if (ledger->seekRootCount == 0)
            ++ledger->seeksWithoutPhases;
        const auto unaccounted = elapsed - ledger->seekRootUs;
        ledger->unaccountedTotalUs += unaccounted;
        if (unaccounted > ledger->unaccountedMaxUs)
            ledger->unaccountedMaxUs = unaccounted;
    }
    SeekEnvelope(const SeekEnvelope&) = delete;
    SeekEnvelope& operator=(const SeekEnvelope&) = delete;
};

inline void seekFetch(SeekLedger& ledger, std::int64_t us) noexcept {
    ++ledger.fetchCount;
    ledger.fetchTotalUs += us;
}

// R0 fault injection. These switches exist only in an observation build and are consumed by the
// exact production branch they fault: no alternate Close/export implementation is compiled in.
// A test arms one switch, calls the normal API once, and must observe the ordinary error path.
inline std::atomic<bool> testFlushFaultArmed{};
inline std::atomic<bool> testExportFaultArmed{};
inline auto takeFlushFault() noexcept -> bool {
    return testFlushFaultArmed.exchange(false, std::memory_order_relaxed);
}
inline auto takeExportFault() noexcept -> bool {
    return testExportFaultArmed.exchange(false, std::memory_order_relaxed);
}

// Worker-side samples are merged into the host ledger; per-seek bookkeeping stays host-local.
inline void mergeSeek(SeekLedger& into, const SeekLedger& from) noexcept {
    for (std::size_t index = 0; index < seekPhaseCount; ++index) {
        into.phases[index].count += from.phases[index].count;
        into.phases[index].totalUs += from.phases[index].totalUs;
        if (from.phases[index].maxUs > into.phases[index].maxUs)
            into.phases[index].maxUs = from.phases[index].maxUs;
#define CUEXIS_RPA_SEEK_WORK_MERGE(name) into.work[index].name += from.work[index].name;
        CUEXIS_RPA_SEEK_WORK_FIELDS(CUEXIS_RPA_SEEK_WORK_MERGE)
#undef CUEXIS_RPA_SEEK_WORK_MERGE
    }
    into.seeksWithoutPhases += from.seeksWithoutPhases;
    if (from.timerCostNs)
        into.timerCostNs = from.timerCostNs;
}
} // namespace cuexis::internal::execution_diagnostics::rpa
#endif
