#pragma once

// Locates the per-user Cuexis config directory without SDL or a JSON DOM.
// Callers pass an explicit environment so tests do not depend on the machine.

#include <cuexis/core/result.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace cuexis::player_support {

struct ConfigDirectoryEnvironment final {
    bool windows{false};
    std::string appData{};
    std::string xdgConfigHome{};
    std::string home{};
};

[[nodiscard]] auto captureConfigDirectoryEnvironment() -> ConfigDirectoryEnvironment;

[[nodiscard]] auto userConfigDirectory(const ConfigDirectoryEnvironment& environment)
    -> core::Result<std::filesystem::path>;

[[nodiscard]] auto preferencesFilePath(const std::filesystem::path& configDirectory)
    -> std::filesystem::path;

[[nodiscard]] auto audioProfileFilePath(const std::filesystem::path& configDirectory,
                                        std::string_view profileId)
    -> core::Result<std::filesystem::path>;

} // namespace cuexis::player_support
