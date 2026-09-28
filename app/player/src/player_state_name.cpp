#include "player_state_name.hpp"

namespace cuexis::player {

std::string_view playbackStateName(audio::PlaybackState state) noexcept {
    switch (state) {
    case audio::PlaybackState::Empty:
        return "empty";
    case audio::PlaybackState::Stopped:
        return "stopped";
    case audio::PlaybackState::Playing:
        return "playing";
    case audio::PlaybackState::Paused:
        return "paused";
    case audio::PlaybackState::Ended:
        return "ended";
    case audio::PlaybackState::Error:
        return "error";
    }
    return "unknown";
}

} // namespace cuexis::player
