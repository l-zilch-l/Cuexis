#pragma once

#include <cuexis/judgement/judgement_session.hpp>
#include <map>
#include <variant>

namespace cuexis::judgement {
struct RpaContext final {
    std::uint64_t sessionScope, clockSegment, runEpoch;
    friend auto operator==(const RpaContext&, const RpaContext&) -> bool = default;
};
struct ClosedCaptureBatch final {
    Tick originalTick, observedTick;
    std::uint64_t batchId, fragmentIndex, fragmentCount, totalMembers;
    std::vector<CanonicalInput> members;
    bool epochTerminated{};
    std::uint64_t pauseEpoch{};
    friend auto operator==(const ClosedCaptureBatch&, const ClosedCaptureBatch&) -> bool = default;
};
struct CapturePrefix final {
    Tick exclusiveTick;
    friend auto operator==(CapturePrefix, CapturePrefix) -> bool = default;
};
struct EndRunEpoch final {
    Tick inclusiveTick;
    std::uint64_t pauseEpoch;
    friend auto operator==(EndRunEpoch, EndRunEpoch) -> bool = default;
};
struct CaptureMessage final {
    RpaContext context;
    std::uint64_t deliverySequence;
    std::variant<ClosedCaptureBatch, CapturePrefix, EndRunEpoch> payload;
    friend auto operator==(const CaptureMessage&, const CaptureMessage&) -> bool = default;
};
enum class AdvanceDisposition : std::uint8_t {
    waiting,
    busyNotAccepted,
    canceledNotExecuted,
    clockInvalidNotExecuted,
    retired,
    executed,
    failed
};
struct AdvanceRequest final {
    RpaContext context;
    std::uint64_t requestId, commandOrdinal;
    Tick horizon;
    friend auto operator==(AdvanceRequest, AdvanceRequest) -> bool = default;
};
struct AdvanceDecision final {
    AdvanceRequest request;
    AdvanceDisposition disposition;
    std::uint64_t decisionOrdinal;
    std::optional<Tick> verifiedExclusive;
    std::optional<KernelProjection> result;
    std::optional<core::Error> error;
};
struct AdmissionDecision final {
    Tick originalTick;
    std::vector<CanonicalInput> members;
    std::vector<InputReceiptPending> receipts;
    std::optional<core::Error> rejection;
};
// Same owner as the kernel. Waiting never occupies the owner's input/control loop.
class ExecutionScheduler final {
  public:
    [[nodiscard]] static auto create(JudgementSession&, RpaContext, TickSpan closeThreshold,
                                     std::size_t capacity, std::uint64_t firstCommand = 0,
                                     std::uint64_t firstDecision = 0)
        -> core::Result<ExecutionScheduler>;
    ExecutionScheduler(ExecutionScheduler&&) noexcept = default;
    ExecutionScheduler(const ExecutionScheduler&) = delete;
    [[nodiscard]] auto deliver(const CaptureMessage&) -> core::Result<void>;
    [[nodiscard]] auto request(AdvanceRequest) -> core::Result<AdvanceDecision>;
    [[nodiscard]] auto poll(std::uint64_t requestId) const -> core::Result<AdvanceDecision>;
    [[nodiscard]] auto retire(std::uint64_t requestId) -> core::Result<void>;
    [[nodiscard]] auto acceptControl(RpaContext, std::uint64_t commandOrdinal)
        -> core::Result<void>;
    [[nodiscard]] auto pause() -> core::Result<void>;
    [[nodiscard]] auto resume(const RpaControlRecord&, RpaContext newContext,
                              bool finalRouteApplied, bool deterministicPreparationFailure = false)
        -> core::Result<void>;
    [[nodiscard]] auto resumeStatus(const RpaControlRecord&, RpaContext) -> core::Result<bool>;
    void abortResume() noexcept;
    [[nodiscard]] auto clockValid(bool valid) -> core::Result<void>;
    [[nodiscard]] auto transitionClock(RpaContext) -> core::Result<void>;
    [[nodiscard]] auto canTransitionClock(RpaContext) const noexcept -> bool;
    [[nodiscard]] auto verifiedExclusive() const noexcept -> std::optional<Tick>;
    [[nodiscard]] auto confirmedSequence() const noexcept -> std::optional<std::uint64_t>;
    [[nodiscard]] auto admissions() const noexcept -> const std::vector<AdmissionDecision>&;
    [[nodiscard]] auto suspended() const noexcept -> bool;
    [[nodiscard]] auto pendingEpochEnd() const noexcept -> const std::optional<EndRunEpoch>& {
        return end_;
    }
    [[nodiscard]] auto lastDecisionOrdinal() const noexcept -> std::optional<std::uint64_t> {
        return nextDecision_ ? std::optional<std::uint64_t>{nextDecision_ - 1} : std::nullopt;
    }

  private:
    ExecutionScheduler(JudgementSession&, RpaContext, TickSpan closeThreshold,
                       std::size_t capacity) noexcept;
    [[nodiscard]] auto tryStart() -> core::Result<void>;
    [[nodiscard]] auto decision() -> core::Result<std::uint64_t>;
    JudgementSession& session_;
    RpaContext context_;
    TickSpan closeThreshold_;
    std::size_t capacity_;
    std::map<std::uint64_t, CaptureMessage> delivery_;
    std::map<std::uint64_t, std::vector<ClosedCaptureBatch>> fragments_;
    std::map<std::uint64_t, AdvanceDecision> requests_;
    std::optional<std::uint64_t> waiting_, confirmed_;
    std::optional<Tick> exclusive_, maxCaptured_;
    std::optional<EndRunEpoch> end_;
    std::vector<AdmissionDecision> admissions_;
    std::optional<RpaControlRecord> lastResume_;
    std::optional<RpaContext> lastResumeContext_;
    std::optional<core::Error> lastResumeFailure_;
    std::optional<EndRunEpoch> batchTermination_;
    std::uint64_t nextSequence_{}, nextDecision_{}, nextCommand_{};
    bool validClock_{true}, suspended_{}, directResume_{true}, paused_{}, controlPreparing_{};
};
} // namespace cuexis::judgement
