#pragma once
#include <functional>
#include <memory>

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
struct KernelWorkObservation final {
    std::uint64_t fullTickTransactions{}, idleDeadlineTicks{}, timers{};
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
    bool failAdvanceFinalization{false};
    bool forceReferenceAdvance{false};
    bool failCertifiedRestore{false};
    std::shared_ptr<KernelWorkObservation> work;
};
// Internal checking fixture, unavailable in installed headers or Playback.
class KernelTestAccess final {
  public:
    static auto duringMutation(JudgementSession&, const std::function<void()>&)
        -> core::Result<void>;
    static auto inject(JudgementSession&, KernelTestControls) -> core::Result<void>;
    static auto snapshotValue(SnapshotDTO, RecoveryInputs) -> SnapshotPayload;
    static auto captureDTO(const JudgementSession&) -> core::Result<SnapshotDTO>;
    static auto visibleSignalCount(const JudgementSession&, Tick) -> std::size_t;
    static auto certifiedPrefixRecords(const JudgementSession&) -> std::uint64_t;
    static auto nativePrefixRecords(const JudgementSession&) -> std::uint64_t;
};
} // namespace cuexis::judgement::detail
