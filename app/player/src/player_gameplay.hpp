#pragma once
#include "player_gameplay_guide.hpp"
#include "player_options.hpp"
#include "player_surface.hpp"
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include <cuexis/playback/playback_session.hpp>
namespace cuexis::player {
struct PlayerGameplayProfile final {
    playback::GameplayConfiguration configuration;
    playback::GameplayConfigurationDecodeBudget budget;
    std::int64_t hStep{}, tStep{};
    struct Key final {
        std::uint32_t scanCode;
        std::string channel, domain;
    };
    std::vector<Key> keys;
    std::vector<PlayerGuideNote> guide;
};
[[nodiscard]] auto readPlayerGameplay(const PlayerOptions&) -> core::Result<PlayerGameplayProfile>;
class PlayerGameplay final {
  public:
    explicit PlayerGameplay(PlayerGameplayProfile profile) : profile_(std::move(profile)) {}
    [[nodiscard]] auto profile() const -> const PlayerGameplayProfile& {
        return profile_;
    }
    [[nodiscard]] auto mapped(std::uint32_t code) const -> bool;
    [[nodiscard]] auto step(playback::PlaybackSession&, const PlayerInput&,
                            const playback::RuntimeFrame&) -> core::Result<playback::GameplayScore>;
    [[nodiscard]] auto seek(playback::PlaybackSession&, playback::GameplayTick,
                            const playback::RuntimeFrame&) -> core::Result<void>;
    void reset(bool restart) {
        h_ = 0;
        if (restart)
            t_ = 0;
        sequence_ = 0;
    }
    [[nodiscard]] auto horizon() const -> playback::GameplayTick {
        return {h_};
    }

  private:
    PlayerGameplayProfile profile_;
    std::int64_t h_{}, t_{};
    std::uint64_t sequence_{};
};
} // namespace cuexis::player
#endif
