#pragma once

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::playback {

struct PlaybackSource::State final {
    [[nodiscard]] auto entryChart() const noexcept -> const PlaybackProjectDocument*;

    std::string sourceId;
    std::string entryChartPath;
    std::vector<PlaybackProjectDocument> projectDocuments;
    std::optional<assets::AssetDatabase> database;
    std::shared_ptr<content::IContentProvider> provider;
    std::optional<std::array<std::uint8_t, 32>> cxcPackageIdentity;
};

} // namespace cuexis::playback
