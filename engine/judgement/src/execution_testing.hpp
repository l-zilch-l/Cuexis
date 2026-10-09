#pragma once

#include <cuexis/judgement/judgement_session.hpp>

namespace cuexis::judgement::detail {
enum class KernelFailurePoint : std::uint8_t {
    afterTimers,
    afterInput,
    beforeLedger,
    beforeSeal,
    beforeFold
};
enum class RegisterFault : std::uint8_t {
    none,
    secondExclusive,
    duplicateContribution,
    directDerived
};
struct KernelTestControls final {
    std::optional<Tick> failedTick;
    KernelFailurePoint failurePoint;
    std::optional<std::uint64_t> nextCommitId;
    std::vector<Tick> signalTicks;
    std::optional<Tick> duplicatePhaseTick;
    std::optional<Tick> invalidRegisterTick;
    RegisterFault registerFault{RegisterFault::none};
    std::optional<Tick> failRecordReservationHorizon;
    bool failSubmitRecordReservation{false};
};
// Internal checking fixture, unavailable in installed headers or Playback.
class KernelTestAccess final {
  public:
    static auto inject(JudgementSession&, KernelTestControls) -> core::Result<void>;
    static auto captureDTO(const JudgementSession&) -> core::Result<SnapshotDTO>;
    static auto visibleSignalCount(const JudgementSession&, Tick) -> std::size_t;
};
} // namespace cuexis::judgement::detail
