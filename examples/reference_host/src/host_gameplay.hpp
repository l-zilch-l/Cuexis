#pragma once
#include <cuexis/playback/playback_source.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace cuexis_reference_host {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
struct GameplayHost final {
    cuexis::playback::GameplayConfiguration configuration;
    cuexis::playback::GameplayConfigurationDecodeBudget budget;
    std::int64_t horizonStep, presentationStep;
    std::vector<cuexis::playback::GameplayInput> observations;
};
[[nodiscard]] auto readGameplayHost(const std::filesystem::path& configuration,
                                    std::string_view budget, std::string_view horizonStep,
                                    std::string_view presentationStep,
                                    const std::optional<std::filesystem::path>& observations)
    -> cuexis::core::Result<GameplayHost>;
#endif
} // namespace cuexis_reference_host
