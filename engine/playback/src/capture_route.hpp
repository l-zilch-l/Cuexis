#pragma once
#include "host_protocol.hpp"
#include <cuexis/playback/execution_coordination.hpp>

namespace cuexis::playback::detail {
class CaptureRouteOwner;
class CUEXIS_PLAYBACK_API CaptureCloseEvidence final {
  public:
    [[nodiscard]] auto fault() const noexcept -> const FaultSlot& {
        return fault_;
    }
    [[nodiscard]] auto pending() const noexcept -> const std::vector<GameplayCaptureDelivery>& {
        return pending_;
    }
    [[nodiscard]] auto trace() const noexcept -> const std::vector<GameplayPauseTransition>& {
        return trace_;
    }
    [[nodiscard]] auto retained() const noexcept -> const std::vector<GameplayOccurrence>& {
        return retained_;
    }
    [[nodiscard]] auto tentativeTickMembers() const noexcept
        -> const std::vector<GameplayOccurrence>& {
        return tentative_;
    }

  private:
    friend class CaptureRouteOwner;
    const CaptureRouteOwner* owner_{};
    GameplayExecutionContext context_{};
    std::uint64_t captured_{}, sequence_{};
    std::vector<GameplayCaptureDelivery> pending_;
    std::vector<GameplayPauseTransition> trace_;
    std::vector<GameplayOccurrence> retained_;
    std::vector<GameplayOccurrence> tentative_;
    FaultSlot fault_;
};
[[nodiscard]] CUEXIS_PLAYBACK_API auto reconciliationIdentity(const GameplayReconciliation&)
    -> core::Result<PreparedSemanticIdentity>;
class CUEXIS_PLAYBACK_API CaptureRouteOwner final {
  public:
    [[nodiscard]] static auto create(GameplayExecutionContext, std::size_t capacity,
                                     std::size_t controlReserve, std::size_t dataControlReserve = 0)
        -> core::Result<CaptureRouteOwner>;
    CaptureRouteOwner(CaptureRouteOwner&&) noexcept = default;
    CaptureRouteOwner(const CaptureRouteOwner&) = delete;
    [[nodiscard]] auto capture(GameplayInput, bool clockValid = true) -> core::Result<void>;
    [[nodiscard]] auto prefix(GameplayTick observed) -> core::Result<void>;
    [[nodiscard]] auto cut(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto prepare(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto fence(const StepRequest&) -> core::Result<StepReceipt>;
    [[nodiscard]] auto exportCloseEvidence() const -> core::Result<CaptureCloseEvidence>;
    [[nodiscard]] auto close(const StepRequest&, const ControlReceipt&, const CaptureCloseEvidence&)
        -> core::Result<StepReceipt>;
    [[nodiscard]] auto resumeRoute(const StepRequest&, GameplayExecutionContext)
        -> core::Result<StepReceipt>;
    [[nodiscard]] auto trace(std::uint64_t controlOrdinal) const
        -> core::Result<GameplayReconciliation>;
    [[nodiscard]] auto pending() const -> core::Result<std::vector<GameplayCaptureDelivery>>;
    [[nodiscard]] auto acknowledge(GameplayExecutionContext, std::uint64_t) -> core::Result<void>;
    [[nodiscard]] auto enableOrdinary(const ControlReceipt&) -> core::Result<void>;
    [[nodiscard]] auto retained() const -> core::Result<std::vector<GameplayOccurrence>>;
    void abortResume() noexcept;
    [[nodiscard]] auto canSwitchClock(GameplayExecutionContext) const noexcept -> bool;
    [[nodiscard]] auto switchClock(GameplayExecutionContext) -> core::Result<void>;
    [[nodiscard]] auto fault() const noexcept -> const FaultSlot& {
        return fault_;
    }
    [[nodiscard]] auto steps() noexcept -> OwnerStepLedger& {
        return steps_;
    }

  private:
    CaptureRouteOwner(GameplayExecutionContext, std::size_t, OwnerStepLedger, std::size_t);
    [[nodiscard]] auto closeBatch(GameplayTick observed, bool terminated) -> core::Result<void>;
    [[nodiscard]] auto enqueue(GameplayCaptureDelivery, bool control = false) -> core::Result<void>;
    [[nodiscard]] auto readiness(const StepRequest&, StepPhase) -> core::Result<StepReceipt>;
    core::ThreadChecker owner_;
    GameplayExecutionContext context_;
    std::size_t capacity_;
    std::size_t dataControlReserve_{};
    OwnerStepLedger steps_;
    std::vector<GameplayOccurrence> batch_;
    std::vector<GameplayOccurrence> tickMembers_;
    std::vector<GameplayPauseTransition> trace_;
    std::vector<GameplayCaptureDelivery> pending_;
    std::optional<GameplayTick> lastTick_, cutTick_;
    std::uint64_t sequence_{}, captured_{}, cutCaptured_{}, pauseEpoch_{}, batchId_{};
    bool paused_{}, ordinaryAllowed_{true}, terminated_{}, closed_{};
    mutable FaultSlot fault_;
};
} // namespace cuexis::playback::detail
