#pragma once

// PlaybackSource owns chart text, the logical asset index, and its content provider.
// It is the only public input boundary for indexed Playback content.

#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_export.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace cuexis::playback {

CUEXIS_ABI_WARNING_PUSH

enum class PlaybackAssetType {
    Mesh,
    Material,
    Texture,
    Audio,
};

struct PlaybackAssetDescriptor final {
    std::string id;
    PlaybackAssetType type{PlaybackAssetType::Mesh};
    std::string rootId;
    std::string logicalSource;
    std::vector<std::string> dependencies;
};

struct TypedPlaybackProject final {
    std::string sourceId;
    std::string chartJson;
    std::vector<PlaybackAssetDescriptor> assets;
};

class CUEXIS_PLAYBACK_API PlaybackSource final {
  public:
    PlaybackSource() noexcept;
    ~PlaybackSource();

    PlaybackSource(const PlaybackSource&) = delete;
    auto operator=(const PlaybackSource&) -> PlaybackSource& = delete;
    PlaybackSource(PlaybackSource&& other) noexcept;
    auto operator=(PlaybackSource&& other) noexcept -> PlaybackSource&;

    [[nodiscard]] static auto fromChartText(std::string chartJson) -> core::Result<PlaybackSource>;
    [[nodiscard]] static auto fromTypedProject(TypedPlaybackProject project,
                                               std::shared_ptr<content::IContentProvider> provider)
        -> core::Result<PlaybackSource>;
    [[nodiscard]] static auto fromFilesystemProject(const std::filesystem::path& locator)
        -> core::Result<PlaybackSource>;

  private:
    friend class PlaybackSession;
    struct State;
    explicit PlaybackSource(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> state_;
};

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::playback
