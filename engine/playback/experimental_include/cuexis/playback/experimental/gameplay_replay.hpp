#pragma once

// Experimental Playback Gameplay replay facade. It is installed only by the explicit candidate SDK
// flavor and is not part of the stable SDK surface. Each free function forwards to the same-version
// candidate entry it names.
#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/execution_coordination.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_types.hpp>

#include <cstdint>
#include <span>

#if !defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#error The experimental Playback Gameplay replay surface requires the candidate SDK flavor.
#endif

namespace cuexis::playback::experimental {

CUEXIS_ABI_WARNING_PUSH

[[nodiscard]] CUEXIS_PLAYBACK_API auto archiveGameplay(const PlaybackSession& session)
    -> core::Result<GameplayReplay>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto snapshotGameplay(const PlaybackSession& session)
    -> core::Result<GameplaySnapshot>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto evaluateGameplayReplay(const PlaybackSession& session,
                                                              const GameplayReplay& replay)
    -> core::Result<GameplayReplayEvaluation>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
restoreGameplay(PlaybackSession& session, const GameplaySnapshot& snapshot,
                GameplayPresentationTick presentationTick, const RuntimeFrame& frame)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
seekGameplay(PlaybackSession& session, const GameplayReplay& replay, GameplayTick tick,
             GameplayPresentationTick presentationTick, const RuntimeFrame& frame)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
seekGameplay(PlaybackSession& session, const GameplayReplay& replay, GameplayTick tick,
             const GameplayReplayCut& cut, std::span<const GameplayCheckpoint> checkpoints,
             GameplayPresentationTick presentationTick, const RuntimeFrame& frame)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto checkpointGameplay(const PlaybackSession& session,
                                                          const GameplayReplay& replay,
                                                          GameplayTick horizon)
    -> core::Result<GameplayCheckpoint>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto gameplayReplayCut(const PlaybackSession& session,
                                                         const GameplayReplay& replay,
                                                         GameplayTick horizon)
    -> core::Result<GameplayReplayCut>;

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::playback::experimental
