#pragma once
#include <cuexis/json/value.hpp>
#include <cuexis/playback/playback_source.hpp>
namespace cuexis::playback::detail {
// Private metadata assembler. No execution types or JSON values are installed.
[[nodiscard]] auto gameplayEntryMetadata(const GameplayContent&, const GameplayConfiguration&,
                                         std::string_view path, std::string_view kind,
                                         std::span<const std::byte>,
                                         std::span<const PlaybackAssetDescriptor>)
    -> core::Result<json::Value>;
[[nodiscard]] auto parseGameplayEntryMetadata(std::string_view, GameplayConfigurationDecodeBudget)
    -> core::Result<json::Value>;
} // namespace cuexis::playback::detail
