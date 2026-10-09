#pragma once
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include <array>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/render/render_scene.hpp>
#include <filesystem>
namespace cuexis::player {
struct PlayerGuideNote final {
    char key{};
    std::int64_t head{}, tail{};
    // Actual FactBinding marker objects: head Hit/Miss, body Hit/Miss, tail Hit/Miss.
    std::array<std::string, 6> cues;
};
[[nodiscard]] auto readPlayerGuide(const std::filesystem::path&,
                                   const playback::GameplayConfigurationDecodeBudget&)
    -> core::Result<std::vector<PlayerGuideNote>>;
[[nodiscard]] auto appendPlayerGuide(std::span<const PlayerGuideNote>, std::int64_t horizon,
                                     const playback::GameplayScore&, bool playing,
                                     const playback::FrameSnapshot&, render::RenderScene&)
    -> core::Result<void>;
} // namespace cuexis::player
#endif
