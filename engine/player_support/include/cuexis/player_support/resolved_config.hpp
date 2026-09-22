#pragma once

// Resolved session configuration identity. Window, gain, and device name are not inputs.

#include <cuexis/player_support/user_preferences.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace cuexis::player_support {

enum class PlaybackClockMode : std::uint8_t {
    ChartClock = 1,
    HostClock = 2,
    CuexisAudio = 3,
};

enum class ConfigValueSource : std::uint8_t {
    CodeDefault,
    PreferencesFile,
    LaunchOption,
};

struct ResolvedAppConfig final {
    UserPreferences requested{};
    ConfigValueSource preferencesSource{ConfigValueSource::CodeDefault};
    bool preservePreferencesFile{false};
};

struct ResolvedSessionConfig final {
    PlaybackClockMode clockMode{PlaybackClockMode::ChartClock};
    std::int64_t outputCorrectionUs{0};
};

struct EffectiveSettings final {
    UserPreferences requested{};
    int appliedWindowWidth{0};
    int appliedWindowHeight{0};
    bool appliedFullscreen{false};
    bool appliedVsync{false};
    double appliedGain{0.0};
    std::string appliedProfileId{};
    bool audioDeviceOpen{false};
};

[[nodiscard]] auto consumedOutputCorrectionUs(PlaybackClockMode clockMode,
                                              std::int64_t profileCorrectionUs) noexcept
    -> std::int64_t;

[[nodiscard]] auto resolvedSessionConfigIdentity(const ResolvedSessionConfig& config) noexcept
    -> std::array<std::uint8_t, 32>;

[[nodiscard]] auto resolveAppConfig(const UserPreferencesLoad& loaded) noexcept
    -> ResolvedAppConfig;

} // namespace cuexis::player_support
