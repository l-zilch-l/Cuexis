#pragma once

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <memory>
#include <optional>
#include <string>

namespace cuexis::playback {

struct PlaybackSource::State final {
    std::string chartJson;
    std::optional<assets::AssetDatabase> database;
    std::shared_ptr<content::IContentProvider> provider;
    std::string sourceId;
};

} // namespace cuexis::playback
