#pragma once

#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/presentation.hpp>

#include <compare>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::playback::detail {

struct PresentationResourceKey final {
    std::string assetId;
    PresentationResourceType type{PresentationResourceType::Mesh};

    friend auto operator<=>(const PresentationResourceKey&,
                            const PresentationResourceKey&) = default;
};

struct PreparedPresentation final {
    PresentationResourceManifest manifest;
    std::map<PresentationResourceKey, PortableResourcePtr> resources;
    std::vector<PortableResourcePtr> orderedResources;
};

[[nodiscard]] auto preparePresentation(const chart::ChartRuntime& chartRuntime,
                                       assets::ResourceManager* resourceManager)
    -> core::Result<std::optional<PreparedPresentation>>;

[[nodiscard]] auto findPresentationResource(const PreparedPresentation& presentation,
                                            const PresentationResourceRef& reference) noexcept
    -> const PortableResourcePtr*;

[[nodiscard]] auto findPresentationResource(const PreparedPresentation& presentation,
                                            std::string_view assetId,
                                            PresentationResourceType type) noexcept
    -> const PortableResourcePtr*;

} // namespace cuexis::playback::detail
