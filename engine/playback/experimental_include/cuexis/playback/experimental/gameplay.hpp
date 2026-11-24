#pragma once

// Experimental Playback Gameplay facade. It is installed only by the explicit candidate SDK flavor
// and is not part of the stable SDK surface. Every free function forwards to the same-version
// candidate entry it names; no validation, copy of a DTO or alternate semantic path is added here.
#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/execution_control.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if !defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#error The experimental Playback Gameplay surface requires the candidate SDK flavor.
#endif

namespace cuexis::playback::experimental {

CUEXIS_ABI_WARNING_PUSH

// PlaybackSource candidate factories. The old static members keep their declarations, symbols and
// behavior while a supported consumer moves to these entries.
[[nodiscard]] CUEXIS_PLAYBACK_API auto
fromCxcFileGameplayEntry(const std::filesystem::path& locator, std::string entryPath,
                         const GameplayConfiguration& configuration,
                         GameplayConfigurationDecodeBudget configurationBudget,
                         GameplayGraphDecodeBudget graphBudget, GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
fromCxcMemoryGameplayEntry(std::vector<std::byte> packageBytes, std::string entryPath,
                           const GameplayConfiguration& configuration,
                           GameplayConfigurationDecodeBudget configurationBudget,
                           GameplayGraphDecodeBudget graphBudget, GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
fromFilesystemGameplayEntry(const std::filesystem::path& locator, std::string entryPath,
                            const GameplayConfiguration& configuration,
                            GameplayConfigurationDecodeBudget configurationBudget,
                            GameplayGraphDecodeBudget graphBudget, GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto fromGameplayEntry(
    std::string sourceId, std::string metadataJson, std::vector<std::byte> entryBytes,
    const GameplayConfiguration& configuration, GameplayConfigurationDecodeBudget metadataBudget,
    GameplayGraphDecodeBudget graphBudget, std::vector<PlaybackAssetDescriptor> assets,
    std::shared_ptr<content::IContentProvider> provider, GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
fromGameplayPacked(std::string sourceId, std::string entryPath, std::vector<std::byte> bytes,
                   const GameplayConfiguration& configuration,
                   std::vector<PlaybackAssetDescriptor> assets,
                   std::shared_ptr<content::IContentProvider> provider,
                   GameplayPrepareIntent intent = GameplayPrepareIntent::Presentation)
    -> core::Result<PlaybackSource>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
fromGameplayGraph(std::string sourceId, std::string entryPath, std::vector<std::byte> bytes,
                  const GameplayConfiguration& configuration, GameplayGraphDecodeBudget budget,
                  std::vector<PlaybackAssetDescriptor> assets,
                  std::shared_ptr<content::IContentProvider> provider,
                  GameplayPrepareIntent intent = GameplayPrepareIntent::Presentation)
    -> core::Result<PlaybackSource>;

// PlaybackSession members. The first parameter replaces the original implicit object; every other
// parameter, default and return type keeps the member declaration.
[[nodiscard]] CUEXIS_PLAYBACK_API auto
prepareGameplayLoad(PlaybackSession& session, PlaybackSource&& source, PlaybackMode mode,
                    const GameplayContent& content) -> core::Result<PreparedPlayback>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
prepareGameplayReload(PlaybackSession& session, PlaybackSource&& source,
                      const GameplayContent& content, ReloadPolicy policy)
    -> core::Result<PreparedPlayback>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto submitGameplay(PlaybackSession& session,
                                                      std::span<const GameplayInput> inputs)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
advanceGameplay(PlaybackSession& session, GameplayTick advancementTick,
                GameplayPresentationTick presentationTick, const RuntimeFrame& frame)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto gameplayState(const PlaybackSession& session)
    -> core::Result<GameplayState>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
queryGameplayCapability(const PlaybackSession& session, std::string_view id,
                        std::string_view revision, const GameplayConfiguration* pending = nullptr)
    -> core::Result<GameplayCapabilityQuery>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto queryGameplay(const PlaybackSession& session)
    -> core::Result<GameplayResult>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto queryGameplayPresentation(const PlaybackSession& session)
    -> core::Result<GameplayPresentationMap>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto gameplayFeedback(const PlaybackSession& session,
                                                        std::uint64_t scope)
    -> core::Result<std::vector<FeedbackObservation>>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto sampleGameplayPresentation(PlaybackSession& session,
                                                                  GameplayPresentationTick tick,
                                                                  const RuntimeFrame& frame)
    -> core::Result<void>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto controlGameplay(PlaybackSession& session,
                                                       GameplayControl control)
    -> core::Result<void>;

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::playback::experimental
