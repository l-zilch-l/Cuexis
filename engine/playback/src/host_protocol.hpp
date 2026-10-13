#pragma once

// Internal owner protocol; no threads or backend dependencies are created here.
#include <array>
#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <optional>
#include <vector>

namespace cuexis::playback::detail {
[[nodiscard]] CUEXIS_PLAYBACK_API auto sameHostError(const std::optional<core::Error>&,
                                                     const std::optional<core::Error>&) -> bool;
enum class Participant : std::uint8_t { capture, software, audio, renderer };
enum class StepPhase : std::uint8_t {
    prepare,
    fence,
    cut,
    resumeRoute,
    softwareCommit,
    activate,
    stop,
    isolate,
    retire,
    close
};
struct HostContext final {
    std::uint64_t sessionScope{}, clockSegment{}, runEpoch{}, controlSequence{};
    std::uint64_t resourceGeneration{}, rendererGeneration{};
    PreparedSemanticIdentity contentIdentity{};
    friend auto operator==(const HostContext&, const HostContext&) -> bool = default;
};
struct StepKey final {
    std::uint64_t sessionScope{}, operationId{};
    Participant participant{};
    StepPhase phase{};
    std::uint64_t attempt{};
    friend auto operator==(StepKey, StepKey) -> bool = default;
};
struct StepTarget final {
    HostContext context;
    std::uint64_t expectedGeneration{}, candidateId{}, commandOrdinal{};
    std::optional<std::int64_t> tick;
    std::optional<std::uint64_t> cut, pauseEpoch;
    std::optional<PreparedSemanticIdentity> archiveIdentity;
    std::optional<HostContext> nextContext;
    std::optional<PreparedSemanticIdentity> payloadIdentity;
    std::optional<double> audioPositionMs;
    std::optional<audio::AudioClipHandle> audioClip;
    std::array<std::optional<std::uint64_t>, 4> participantGenerations;
    std::optional<std::uint64_t> reconciliationOrdinal;
    std::optional<std::int64_t> seekHorizon;
    std::optional<std::uint64_t> seekCompleteRecords;
    std::optional<std::int64_t> seekPartialHorizon;
    std::optional<std::int64_t> tailPauseAt;
    friend auto operator==(const StepTarget&, const StepTarget&) -> bool = default;
};
[[nodiscard]] inline auto generationFor(const StepTarget& target, Participant participant)
    -> std::uint64_t {
    const auto index = static_cast<std::size_t>(participant);
    return index < target.participantGenerations.size()
               ? target.participantGenerations[index].value_or(target.expectedGeneration)
               : target.expectedGeneration;
}
struct ResumeTraceBinding final {
    std::uint64_t finalCut{};
    PreparedSemanticIdentity identity{};
    friend auto operator==(const ResumeTraceBinding&, const ResumeTraceBinding&) -> bool = default;
};
struct StepRequest final {
    StepKey key;
    StepTarget target;
    // Physical generation may change only in an isolated retry of this immutable target.
    std::optional<std::uint64_t> retryGeneration;
    std::optional<ResumeTraceBinding> resumeTrace;
    std::optional<std::int64_t> tailResumeAt;
    friend auto operator==(const StepRequest&, const StepRequest&) -> bool = default;
};
enum class StepStatus : std::uint8_t {
    pending,
    inProgress,
    rejectedNotApplied,
    canceledNotApplied,
    applied,
    failed,
    unknown,
    retired
};
struct StepEffects final {
    std::uint64_t generation{};
    std::optional<std::int64_t> position;
    std::optional<std::uint64_t> cut, pauseEpoch;
    bool callbackIsolated{}, referencesReleased{};
    std::optional<std::uint64_t> sealedFactCursor, foldedFactCursor;
    std::optional<PreparedSemanticIdentity> payloadIdentity;
    friend auto operator==(const StepEffects&, const StepEffects&) -> bool = default;
};
struct StepReceipt final {
    StepRequest request;
    StepStatus status{StepStatus::pending};
    StepEffects effects;
    std::optional<core::Error> cause;
};
struct FaultSlot final {
    enum class Reason : std::uint8_t { none, capacity, allocation, conflict, captureGap } reason{};
    std::optional<StepKey> step;
};
// Fixed records are allocated at create; fault publication never allocates.
class CUEXIS_PLAYBACK_API OwnerStepLedger final {
  public:
    [[nodiscard]] static auto create(std::uint64_t scope, Participant, std::size_t capacity,
                                     std::size_t controlReserve) -> core::Result<OwnerStepLedger>;
    OwnerStepLedger(OwnerStepLedger&&) noexcept = default;
    OwnerStepLedger(const OwnerStepLedger&) = delete;
    [[nodiscard]] auto apply(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto cancel(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto start(const StepRequest&) -> core::Result<bool>;
    [[nodiscard]] auto complete(const StepReceipt&) -> core::Result<void>;
    [[nodiscard]] auto query(StepKey) const -> core::Result<StepReceipt>;
    [[nodiscard]] auto acknowledge(StepKey) -> core::Result<void>;
    [[nodiscard]] auto mayReclaim(StepKey) const -> core::Result<bool>;
    void reportAllocationFailure(std::optional<StepKey> = {}) noexcept;
    void reportConflict(std::optional<StepKey> = {}) noexcept;
    void injectReservationFailureForTest() noexcept {
        failNextCompletion_ = true;
    }
    [[nodiscard]] auto fault() const noexcept -> const FaultSlot& {
        return fault_;
    }
    [[nodiscard]] auto suspended() const noexcept -> bool {
        return fault_.reason != FaultSlot::Reason::none;
    }

  private:
    struct Record final {
        StepReceipt receipt;
        bool acknowledged{};
        std::optional<StepStatus> retiredFrom;
    };
    OwnerStepLedger(std::uint64_t, Participant, std::size_t, std::vector<std::optional<Record>>);
    [[nodiscard]] auto bind(const StepRequest&, bool cancel) -> core::Result<Record*>;
    [[nodiscard]] auto owner() const -> core::Result<void>;
    core::ThreadChecker owner_;
    std::uint64_t scope_;
    Participant participant_;
    std::size_t reserve_;
    std::vector<std::optional<Record>> records_;
    mutable FaultSlot fault_;
    core::Error allocationFailure_{"host.rpa.capacity_exceeded", "Reserved owner allocation fault"};
    bool failNextCompletion_{};
};
enum class HostState : std::uint8_t {
    running,
    controlPreparing,
    paused,
    resumeStarting,
    suspended,
    closing,
    closed
};
enum class ControlKind : std::uint8_t { load, reload, pause, resume, seek, stop, reset, close };
struct ControlRequest final {
    HostContext context;
    std::uint64_t operationId{}, commandOrdinal{};
    ControlKind kind{};
    StepTarget target;
    std::array<bool, 4> participants{true, true, true, true};
    friend auto operator==(const ControlRequest&, const ControlRequest&) -> bool = default;
};
enum class ControlStatus : std::uint8_t {
    preparing,
    softwareApplied,
    complete,
    suspended,
    failed,
    busyNotAccepted,
    retired
};
struct ControlReceipt final {
    ControlRequest request;
    ControlStatus status{};
    std::uint64_t decisionOrdinal{};
    std::array<std::array<std::optional<StepReceipt>, 10>, 4> steps;
    bool directResumeAllowed{true}, cleanupComplete{}, softwareApplied{};
    std::optional<HostContext> finalizedContext;
};
[[nodiscard]] CUEXIS_PLAYBACK_API auto controlReadyForCommit(const ControlReceipt&) -> bool;
[[nodiscard]] CUEXIS_PLAYBACK_API auto controlFullyApplied(const ControlReceipt&) -> bool;
[[nodiscard]] CUEXIS_PLAYBACK_API auto controlRequiredPhase(ControlKind, Participant) -> StepPhase;
// Explicit coordinator drive: acknowledgements are evidence, never inferred from timeout.
class CUEXIS_PLAYBACK_API HostCoordinator final {
  public:
    [[nodiscard]] static auto create(HostContext, std::size_t capacity,
                                     std::uint64_t firstCommand = 0)
        -> core::Result<HostCoordinator>;
    HostCoordinator(HostCoordinator&&) noexcept = default;
    HostCoordinator(const HostCoordinator&) = delete;
    [[nodiscard]] auto preflight(const ControlRequest&) -> core::Result<bool>;
    [[nodiscard]] auto accept(const ControlRequest&, std::uint64_t decisionOrdinal)
        -> core::Result<ControlReceipt>;
    [[nodiscard]] auto observe(const StepReceipt&) -> core::Result<void>;
    [[nodiscard]] auto markSoftwareApplied() -> core::Result<void>;
    [[nodiscard]] auto finish() -> core::Result<void>;
    [[nodiscard]] auto abortResume() -> core::Result<void>;
    [[nodiscard]] auto suspend(std::uint64_t operationId) -> core::Result<void>;
    [[nodiscard]] auto switchClock(const HostContext&) -> core::Result<void>;
    [[nodiscard]] auto completeCleanup() -> core::Result<void>;
    [[nodiscard]] auto query(std::uint64_t) const -> core::Result<ControlReceipt>;
    [[nodiscard]] auto acknowledge(std::uint64_t) -> core::Result<void>;
    [[nodiscard]] auto closeReady(StepPhase) const -> bool;
    [[nodiscard]] auto state() const noexcept -> HostState {
        return state_;
    }
    [[nodiscard]] auto fault() const noexcept -> const FaultSlot& {
        return fault_;
    }

  private:
    HostCoordinator(HostContext, std::vector<std::optional<ControlReceipt>>, std::uint64_t);
    void reportAllocationFailure(std::optional<StepKey> = {}) const noexcept;
    core::ThreadChecker owner_;
    HostContext context_;
    mutable HostState state_{HostState::running};
    mutable std::vector<std::optional<ControlReceipt>> records_;
    std::optional<std::size_t> active_;
    std::uint64_t nextOperation_{1}, nextCommand_{};
    std::optional<std::uint64_t> lastDecision_;
    bool resumeTerminated_{};
    mutable FaultSlot fault_;
    core::Error allocationFailure_{"host.rpa.capacity_exceeded",
                                   "Reserved coordinator allocation fault"};
};
} // namespace cuexis::playback::detail
