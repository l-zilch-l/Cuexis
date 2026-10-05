#pragma once

#include <cuexis/judgement/judgement_session.hpp>

namespace cuexis::judgement::detail {
enum class KernelFailurePoint : std::uint8_t { afterTimers, afterInput, beforeLedger, beforeSeal };
struct KernelTestControls final {
    std::optional<Tick> failedTick;
    KernelFailurePoint failurePoint;
    std::optional<std::uint64_t> nextCommitId;
    std::vector<Tick> signalTicks;
    std::optional<Tick> duplicatePhaseTick;
};
// Internal checking fixture, unavailable in installed headers or Playback.
class KernelTestAccess final {
  public:
    static auto inject(JudgementSession&, KernelTestControls) -> core::Result<void>;
    static auto visibleSignalCount(const JudgementSession&, Tick) -> std::size_t;
};
} // namespace cuexis::judgement::detail
