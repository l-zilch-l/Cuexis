#include <cuexis/player_support/resolved_config.hpp>

#include <cuexis_internal/sha256.hpp>

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

} // namespace cuexis::player_support
