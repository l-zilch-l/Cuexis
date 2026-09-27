#pragma once

// Command-line options for the reference Player. This file has no SDL or OpenGL types.

#include <cuexis/core/result.hpp>

#include <filesystem>
#include <optional>

namespace cuexis::player {

struct PlayerOptions final {
    bool smokeTest{};
    bool audioSmokeTest{};
    std::optional<std::filesystem::path> chartPath;
    std::optional<std::filesystem::path> projectPath;
    std::optional<std::filesystem::path> frameStatsPrefix;
    std::optional<std::filesystem::path> shaderCacheDirectory;
};

[[nodiscard]] auto parsePlayerOptions(int argumentCount, char** arguments)
    -> core::Result<PlayerOptions>;

} // namespace cuexis::player
