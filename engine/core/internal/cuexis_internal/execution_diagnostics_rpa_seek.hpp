#pragma once

// Owner-local seek phase, wait and envelope attribution records.

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace cuexis::internal::execution_diagnostics::rpa {

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
