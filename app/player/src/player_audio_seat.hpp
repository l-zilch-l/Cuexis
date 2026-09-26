#pragma once

// Audio operations that stay off the IAudioTransport vtable, plus the device opener the
// assembly layer injects into the control layer.
// The concrete seat lives in the assembly file.

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/core/result.hpp>

#include <functional>
#include <memory>
#include <optional>

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

// Opens the audio device for a target and returns the seat to use afterwards. An empty clip means
// the target has no audio track, so no device is opened and the returned seat is null. The start
// position is the source-domain millisecond the new stream must begin at, which is zero for a
// restart and the position the transaction preserved otherwise.
//
// The control layer calls this while activating a load or reload transaction. A physical device
// failure here is the last failure a transaction tolerates before the content swap: the
// application then enters Failed and does not claim the previous audio is still usable.
using PlayerAudioOpener = std::function<core::Result<std::unique_ptr<PlayerAudioSeat>>(
    const std::optional<audio::AudioClipHandle>& clip, double gain, double startPositionMs)>;

} // namespace cuexis::player
