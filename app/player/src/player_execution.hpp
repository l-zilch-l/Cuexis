#pragma once
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include "../../execution_host/audio_worker.hpp"
#include "player_log.hpp"
#include "player_options.hpp"
#include <cuexis/player_support/resolved_config.hpp>

namespace cuexis::player {
[[nodiscard]] auto makeExecutionAudioFactory(const player_support::AudioDeviceProfile&, float gain)
    -> playback::detail::AudioWorker::DeviceFactory;
[[nodiscard]] auto runExecutionPlayer(const PlayerOptions&, const player_support::AppConfigLoad&,
                                      PlayerLogger&) -> core::Result<void>;
} // namespace cuexis::player

#endif
