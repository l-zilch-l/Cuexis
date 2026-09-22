#pragma once

// Internal Player preferences. Not an installed SDK type.
// Missing or malformed v1 files fall back to the single code default.
// A future version is left untouched on disk.

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>

#include <filesystem>
#include <string>

namespace cuexis::player_support {

struct UserPreferences final {
    int windowWidth{1280};
    int windowHeight{720};
    bool fullscreen{false};
    bool vsync{true};
    double gain{1.0};
    std::string audioDeviceProfileId{"system-default"};

    friend bool operator==(const UserPreferences&, const UserPreferences&) = default;
};

struct UserPreferencesLoad final {
    UserPreferences preferences{};
    core::Diagnostics diagnostics{};
    bool usedDefaults{false};
    bool preserveExistingFile{false};
};

[[nodiscard]] auto defaultUserPreferences() noexcept -> UserPreferences;

[[nodiscard]] auto loadUserPreferences(const std::filesystem::path& preferencesPath,
                                       const std::filesystem::path& schemaPath)
    -> core::Result<UserPreferencesLoad>;

// Refuses to replace a future-version file. A busy lock leaves the previous file unchanged.
[[nodiscard]] auto saveUserPreferences(const std::filesystem::path& preferencesPath,
                                       const UserPreferences& preferences,
                                       const std::filesystem::path& schemaPath)
    -> core::Result<void>;

} // namespace cuexis::player_support
