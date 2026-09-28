#pragma once

// Shared spelling of audio::PlaybackState for Player diagnostics. The final
// state line and the audio trace column must stay identical, so both callers
// use this one cascade.

#include <cuexis/audio/audio_transport.hpp>

#include <string_view>

namespace cuexis::player {

[[nodiscard]] std::string_view playbackStateName(audio::PlaybackState state) noexcept;

} // namespace cuexis::player
