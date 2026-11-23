#pragma once

// Experimental Playback realtime facade. It is installed only by the explicit candidate SDK flavor
// and is not part of the stable SDK surface. Each free function forwards to the same-version
// candidate entry it names; the narrow type chain stays in execution_coordination.hpp and
// realtime_clock.hpp instead of a private owner control header.
#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/execution_coordination.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_types.hpp>
#include <cuexis/playback/realtime_clock.hpp>

#include <cstdint>
#include <optional>
#include <string>

#if !defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#error The experimental Playback realtime surface requires the candidate SDK flavor.
#endif

namespace cuexis::playback::experimental {

CUEXIS_ABI_WARNING_PUSH

// The old installed free function keeps its declaration and export during the compatibility
// window; this entry forwards the same reconciliation identity algorithm and takes no session.
[[nodiscard]] CUEXIS_PLAYBACK_API auto
computeGameplayReconciliationIdentity(const GameplayReconciliation& reconciliation)
    -> core::Result<PreparedSemanticIdentity>;

[[nodiscard]] CUEXIS_PLAYBACK_API auto invalidateGameplayClock(PlaybackSession& session)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
mapGameplayClock(PlaybackSession& session, const ClockObservation& clock, std::int64_t now)
    -> core::Result<ClockTicks>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
beginRealtimeGameplay(PlaybackSession& session, SchedulingProfile profile,
                      ClockMappingConfiguration mapping, GameplayExecutionContext context,
                      std::string monotonicDomain) -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
beginRealtimeTail(PlaybackSession& session, const ClockObservation& eof, std::uint64_t tailSegment,
                  TailProfile profile, std::int64_t nowMonotonicUs)
    -> core::Result<TailClockTransition>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
deliverGameplayCapture(PlaybackSession& session, const GameplayCaptureDelivery& delivery,
                       const ClockObservation& clock, std::int64_t monotonicNowUs)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
requestGameplayAdvance(PlaybackSession& session, GameplayAdvanceRequest request,
                       const ClockObservation& clock, std::int64_t nowMonotonicUs)
    -> core::Result<GameplayAdvanceDecision>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto queryGameplayAdvance(const PlaybackSession& session,
                                                            std::uint64_t requestId)
    -> core::Result<GameplayAdvanceDecision>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto retireGameplayAdvance(PlaybackSession& session,
                                                             std::uint64_t requestId)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto gameplayAdvanceReceipt(const PlaybackSession& session)
    -> core::Result<GameplayAdvanceReceipt>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto acceptGameplayControl(PlaybackSession& session,
                                                             GameplayExecutionContext context,
                                                             std::uint64_t commandOrdinal)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto pauseRealtimeGameplay(PlaybackSession& session,
                                                             std::int64_t nowMonotonicUs = 0)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
resumeRealtimeGameplay(PlaybackSession& session, const GameplayReconciliation& reconciliation,
                       GameplayExecutionContext context, const ClockObservation& clock,
                       std::int64_t nowMonotonicUs, bool finalRouteApplied,
                       std::optional<std::int64_t> tailResumeAt = {}) -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto gameplayExecutionLag(const PlaybackSession& session,
                                                            GameplayTick clockHorizon)
    -> core::Result<GameplayExecutionLag>;

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::playback::experimental
