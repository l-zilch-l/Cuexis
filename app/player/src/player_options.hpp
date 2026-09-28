#pragma once

// Command-line options for the reference Player. This file has no SDL or OpenGL types.

#include <cuexis/core/result.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>

namespace cuexis::player {

// Names the clock for the startup Load, mirroring playback::PlaybackMode without depending on
// the Playback headers.
enum class PlayerClockOption : std::uint8_t {
    Chart = 1,
    Host = 2,
    Audio = 3,
};

struct PlayerOptions final {
    bool smokeTest{};
    bool audioSmokeTest{};
    std::optional<std::filesystem::path> chartPath;
    std::optional<std::filesystem::path> projectPath;
    // The clock the startup Load names. Switching between content with and without an audio track
    // is an explicit choice, so it is named here instead of probed from a failed load. Kept as a
    // plain enum so this header stays free of Playback types.
    std::optional<PlayerClockOption> clock;
    std::optional<std::filesystem::path> frameStatsPrefix;
    std::optional<std::filesystem::path> shaderCacheDirectory;
};

[[nodiscard]] auto parsePlayerOptions(int argumentCount, char** arguments)
    -> core::Result<PlayerOptions>;

} // namespace cuexis::player
