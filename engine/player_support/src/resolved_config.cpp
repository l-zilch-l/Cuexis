#include <cuexis/player_support/resolved_config.hpp>

#include <cuexis/player_support/config_location.hpp>
#include <cuexis_internal/sha256.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

namespace cuexis::player_support {
namespace {

void appendU8(std::vector<std::byte>& output, std::uint8_t value) {
    output.push_back(static_cast<std::byte>(value));
}

void appendU32Le(std::vector<std::byte>& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        appendU8(output, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void appendI64Le(std::vector<std::byte>& output, std::int64_t value) {
    const auto bits = static_cast<std::uint64_t>(value);
    for (int shift = 0; shift < 64; shift += 8) {
        appendU8(output, static_cast<std::uint8_t>((bits >> shift) & 0xFFU));
    }
}

} // namespace

auto consumedOutputCorrectionUs(PlaybackClockMode clockMode,
                                std::int64_t profileCorrectionUs) noexcept -> std::int64_t {
    return clockMode == PlaybackClockMode::CuexisAudio ? profileCorrectionUs : 0;
}

auto resolvedSessionConfigIdentity(const ResolvedSessionConfig& config) noexcept
    -> std::array<std::uint8_t, 32> {
    std::vector<std::byte> preimage;
    static constexpr char domain[] = "cuexis.execution-config.v5.candidate.1";
    preimage.insert(preimage.end(), reinterpret_cast<const std::byte*>(domain),
                    reinterpret_cast<const std::byte*>(domain) + sizeof(domain));
    appendU32Le(preimage, 1U);
    appendU8(preimage, static_cast<std::uint8_t>(config.clockMode));
    appendI64Le(preimage, config.outputCorrectionUs);
    return core::detail::sha256(preimage);
}

auto resolveAppConfig(const UserPreferencesLoad& loaded) noexcept -> ResolvedAppConfig {
    ResolvedAppConfig resolved;
    resolved.requested = loaded.preferences;
    resolved.preservePreferencesFile = loaded.preserveExistingFile;
    resolved.preferencesSource =
        loaded.usedDefaults ? ConfigValueSource::CodeDefault : ConfigValueSource::PreferencesFile;
    return resolved;
}

auto loadAppConfig(const std::filesystem::path& configDirectory,
                   const std::filesystem::path& preferencesSchema,
                   const std::filesystem::path& profileSchema) -> core::Result<AppConfigLoad> {
    auto preferences = loadUserPreferences(preferencesFilePath(configDirectory), preferencesSchema);
    if (!preferences) {
        return core::unexpected(std::move(preferences.error()));
    }
    AppConfigLoad loaded;
    loaded.app = resolveAppConfig(*preferences);
    loaded.diagnostics = std::move(preferences->diagnostics);

    auto profilePath =
        audioProfileFilePath(configDirectory, loaded.app.requested.audioDeviceProfileId);
    if (!profilePath) {
        return core::unexpected(std::move(profilePath.error()));
    }
    std::error_code existsError;
    const bool profileExists = std::filesystem::exists(*profilePath, existsError) && !existsError;
    if (!profileExists) {
        if (loaded.app.requested.audioDeviceProfileId != "system-default") {
            return core::unexpected(
                core::Error{"player.audio_profile.missing",
                            "The selected audio device profile file is missing"}
                    .withContext("profile_id", loaded.app.requested.audioDeviceProfileId));
        }
        loaded.profile = {};
        loaded.diagnostics.add(core::Diagnostic{
            core::DiagnosticSeverity::Warning, std::string{"player.audio_profile.default"},
            std::string{"The system-default profile file is missing; the code profile is in use"},
            std::string{"$"}});
        return loaded;
    }
    auto profile = loadAudioDeviceProfile(*profilePath, profileSchema);
    if (!profile) {
        return core::unexpected(std::move(profile.error()));
    }
    if (profile->id != loaded.app.requested.audioDeviceProfileId) {
        return core::unexpected(core::Error{"player.audio_profile.identity_mismatch",
                                            "The profile file id does not match the requested id"}
                                    .withContext("profile_id", profile->id));
    }
    loaded.profile = std::move(*profile);
    return loaded;
}

auto correctConsumedAudioPositionMs(double rawPositionMs, std::int64_t consumedCorrectionUs)
    -> core::Result<double> {
    if (!std::isfinite(rawPositionMs)) {
        return core::unexpected(core::Error{"player.audio_profile.correction_invalid",
                                            "The raw audio position must be finite"});
    }
    const auto rawUs = std::llround(rawPositionMs * 1000.0);
    auto corrected = correctedAudioPositionUs(rawUs, consumedCorrectionUs);
    if (!corrected) {
        return core::unexpected(std::move(corrected.error()));
    }
    return static_cast<double>(*corrected) / 1000.0;
}

} // namespace cuexis::player_support
