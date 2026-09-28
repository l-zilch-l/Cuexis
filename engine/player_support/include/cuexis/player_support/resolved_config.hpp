#pragma once

// Resolved session configuration identity. Window, gain, and device name are not inputs.

#include <cuexis/player_support/audio_device_profile.hpp>
#include <cuexis/player_support/user_preferences.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
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
};

struct ResolvedAppConfig final {
    UserPreferences requested{};
    // Records the source of the whole requested snapshot, not a per-field provenance. The loader
    // either accepts the preferences file or falls back to the code defaults as one unit, so every
    // field shares this source. Field-level location is carried by core::Diagnostic codes and
    // context, per ADR 0024.
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
    // Read back from the negotiated window flags by SdlWindow::fullscreen, never a requested
    // mirror.
    bool appliedFullscreen{false};
    // The swap interval is set once through SDL_GL_SetSwapInterval and cannot be read back, so this
    // records the value the Player asked for rather than a negotiated result.
    bool requestedVsync{false};
    // The gain the controller opened the audio device with, read back from PlayerController.
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

struct AppConfigLoad final {
    ResolvedAppConfig app{};
    AudioDeviceProfile profile{};
    core::Diagnostics diagnostics{};
};

// Reads preferences, then the named profile. A missing system-default profile uses the code
// profile. Any other missing, malformed, or future profile fails and does not publish a request.
[[nodiscard]] auto loadAppConfig(const std::filesystem::path& configDirectory,
                                 const std::filesystem::path& preferencesSchema,
                                 const std::filesystem::path& profileSchema)
    -> core::Result<AppConfigLoad>;

// Applies consumed correction to a copy of the raw audio position. The transport snapshot stays
// raw.
[[nodiscard]] auto correctConsumedAudioPositionMs(double rawPositionMs,
                                                  std::int64_t consumedCorrectionUs)
    -> core::Result<double>;

} // namespace cuexis::player_support
