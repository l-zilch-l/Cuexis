#include <cuexis/player_support/config_location.hpp>

#include <cstdlib>
#include <string>

namespace {

[[nodiscard]] auto environmentValue(const char* name) -> std::string {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }
    std::string text{value};
    std::free(value);
    return text;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
#endif
}

} // namespace

namespace cuexis::player_support {
namespace {

[[nodiscard]] auto acceptableProfileId(std::string_view profileId) noexcept -> bool {
    if (profileId.empty() || profileId.size() > 64) {
        return false;
    }
    if (profileId.front() < 'a' || profileId.front() > 'z') {
        return false;
    }
    for (const char character : profileId) {
        const bool allowed = (character >= 'a' && character <= 'z') ||
                             (character >= '0' && character <= '9') || character == '.' ||
                             character == '_' || character == '-';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

} // namespace

auto captureConfigDirectoryEnvironment() -> ConfigDirectoryEnvironment {
    ConfigDirectoryEnvironment environment;
#if defined(_WIN32)
    environment.windows = true;
    environment.appData = environmentValue("APPDATA");
#else
    environment.windows = false;
    environment.xdgConfigHome = environmentValue("XDG_CONFIG_HOME");
    environment.home = environmentValue("HOME");
#endif
    return environment;
}

auto userConfigDirectory(const ConfigDirectoryEnvironment& environment)
    -> core::Result<std::filesystem::path> {
    if (environment.windows) {
        if (environment.appData.empty()) {
            return core::unexpected(
                core::Error{"player.preferences.directory_unavailable", "APPDATA is not set"});
        }
        return std::filesystem::path{environment.appData} / "Cuexis";
    }
    if (!environment.xdgConfigHome.empty()) {
        return std::filesystem::path{environment.xdgConfigHome} / "Cuexis";
    }
    if (environment.home.empty()) {
        return core::unexpected(core::Error{"player.preferences.directory_unavailable",
                                            "Neither XDG_CONFIG_HOME nor HOME is set"});
    }
    return std::filesystem::path{environment.home} / ".config" / "Cuexis";
}

auto preferencesFilePath(const std::filesystem::path& configDirectory) -> std::filesystem::path {
    return configDirectory / "preferences.json";
}

auto audioProfileFilePath(const std::filesystem::path& configDirectory, std::string_view profileId)
    -> core::Result<std::filesystem::path> {
    if (!acceptableProfileId(profileId)) {
        return core::unexpected(core::Error{"player.audio_profile.invalid_id",
                                            "The audio profile id is not a portable file name"}
                                    .withContext("profile_id", std::string{profileId}));
    }
    return configDirectory / "audio-profiles" / (std::string{profileId} + ".json");
}

} // namespace cuexis::player_support
