#pragma once

// Audio operations that stay off the IAudioTransport vtable.
// The concrete seat lives in the assembly file.

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/core/result.hpp>

namespace cuexis::player {

class PlayerAudioSeat {
  public:
    virtual ~PlayerAudioSeat() = default;

    PlayerAudioSeat(const PlayerAudioSeat&) = delete;
    auto operator=(const PlayerAudioSeat&) -> PlayerAudioSeat& = delete;
    PlayerAudioSeat(PlayerAudioSeat&&) = delete;
    auto operator=(PlayerAudioSeat&&) -> PlayerAudioSeat& = delete;

    [[nodiscard]] virtual auto transport() -> audio::IAudioTransport& = 0;
    [[nodiscard]] virtual auto recheckBoundDevice() -> core::Result<void> = 0;
    [[nodiscard]] virtual auto prepareReplacement(audio::AudioClipHandle handle, double positionMs)
        -> core::Result<void> = 0;
    [[nodiscard]] virtual auto activateReplacement() -> core::Result<void> = 0;
    [[nodiscard]] virtual auto applyGain(float gain) -> core::Result<void> = 0;
    [[nodiscard]] virtual auto unload() -> core::Result<void> = 0;

  protected:
    PlayerAudioSeat() = default;
};

} // namespace cuexis::player
