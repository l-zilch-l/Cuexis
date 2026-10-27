#pragma once

#include <cuexis/playback/gameplay_candidate.hpp>

namespace cuexis::playback::detail {
// Non-installed bridge for execution owners. The first request pays canonical encoding;
// only a kernel-sealed immutable archive can reuse the resulting exact SHA-256.
struct ReplayIdentityAccess final {
    [[nodiscard]] static auto identity(const GameplayReplay& replay)
        -> core::Result<std::array<std::uint8_t, 32>> {
        return replay.canonicalIdentity();
    }
};
[[nodiscard]] inline auto archiveIdentity(const GameplayReplay& replay)
    -> core::Result<std::array<std::uint8_t, 32>> {
    return ReplayIdentityAccess::identity(replay);
}
} // namespace cuexis::playback::detail
