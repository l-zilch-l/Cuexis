#pragma once

// Internal owner protocol; no threads or backend dependencies are created here.
#include <array>
#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <optional>
#include <vector>

namespace cuexis::playback::detail {
using ::cuexis::playback::AudioCandidateInput;
using ::cuexis::playback::controlFullyApplied;
using ::cuexis::playback::ControlKind;
using ::cuexis::playback::controlReadyForCommit;
using ::cuexis::playback::ControlReceipt;
using ::cuexis::playback::ControlRequest;
using ::cuexis::playback::controlRequiredPhase;
using ::cuexis::playback::ControlStatus;
using ::cuexis::playback::FeedbackId;
using ::cuexis::playback::FeedbackObservation;
using ::cuexis::playback::FeedbackStatus;
using ::cuexis::playback::generationFor;
using ::cuexis::playback::HostContext;
using ::cuexis::playback::Participant;
using ::cuexis::playback::PresentationCandidateInput;
using ::cuexis::playback::ResumeTraceBinding;
using ::cuexis::playback::StepEffects;
using ::cuexis::playback::StepKey;
using ::cuexis::playback::StepPhase;
using ::cuexis::playback::StepReceipt;
using ::cuexis::playback::StepRequest;
using ::cuexis::playback::StepStatus;
using ::cuexis::playback::StepTarget;

[[nodiscard]] auto sameHostError(const std::optional<core::Error>&,
                                 const std::optional<core::Error>&) -> bool;
struct FaultSlot final {
    enum class Reason : std::uint8_t { none, capacity, allocation, conflict, captureGap } reason{};
    std::optional<StepKey> step;
};
// Fixed records are allocated at create; fault publication never allocates.
class OwnerStepLedger final {
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
// Explicit coordinator drive: acknowledgements are evidence, never inferred from timeout.
class HostCoordinator final {
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
