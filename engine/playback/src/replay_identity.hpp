#pragma once

#include <cuexis/playback/execution_control.hpp>

namespace cuexis::playback::detail {
// Non-installed bridge. Runtime source identity and canonical content digest are distinct.
struct ReplayIdentityAccess final {
    [[nodiscard]] static auto identity(const GameplayReplay& replay)
        -> core::Result<std::array<std::uint8_t, 32>> {
        return replay.canonicalIdentity();
    }
    [[nodiscard]] static auto source(const GameplayReplay& replay)
        -> core::Result<ReplaySourceIdentity> {
        return replay.sourceIdentity();
    }
};
[[nodiscard]] inline auto archiveIdentity(const GameplayReplay& replay)
    -> core::Result<std::array<std::uint8_t, 32>> {
    return ReplayIdentityAccess::identity(replay);
}
[[nodiscard]] inline auto
replayBindingMatches(const GameplayReplay& replay,
                     const std::optional<PreparedSemanticIdentity>& content,
                     const std::optional<ReplaySourceIdentity>& source) -> core::Result<bool> {
    if (content.has_value() == source.has_value())
        return false;
    if (source) {
        const auto actual = ReplayIdentityAccess::source(replay);
        if (!actual)
            return core::unexpected(actual.error());
        return source->valid() && *source == *actual;
    }
    const auto actual = archiveIdentity(replay);
    if (!actual)
        return core::unexpected(actual.error());
    return *content == PreparedSemanticIdentity{*actual};
}
} // namespace cuexis::playback::detail
