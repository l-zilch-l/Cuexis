#pragma once

#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/presentation.hpp>

#include <compare>
#include <cstdint>
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

struct PresentationResourceKeyView final {
    std::string_view assetId;
    PresentationResourceType type{PresentationResourceType::Mesh};
};

struct PresentationResourceKeyLess final {
    using is_transparent = void;

    [[nodiscard]] static auto less(std::string_view leftAssetId, PresentationResourceType leftType,
                                   std::string_view rightAssetId,
                                   PresentationResourceType rightType) noexcept -> bool {
        if (leftAssetId != rightAssetId) {
            return leftAssetId < rightAssetId;
        }
        return static_cast<std::uint8_t>(leftType) < static_cast<std::uint8_t>(rightType);
    }

    [[nodiscard]] auto operator()(const PresentationResourceKey& left,
                                  const PresentationResourceKey& right) const noexcept -> bool {
        return less(left.assetId, left.type, right.assetId, right.type);
    }

    [[nodiscard]] auto operator()(const PresentationResourceKey& left,
                                  const PresentationResourceKeyView& right) const noexcept -> bool {
        return less(left.assetId, left.type, right.assetId, right.type);
    }

    [[nodiscard]] auto operator()(const PresentationResourceKeyView& left,
                                  const PresentationResourceKey& right) const noexcept -> bool {
        return less(left.assetId, left.type, right.assetId, right.type);
    }

    [[nodiscard]] auto operator()(const PresentationResourceKeyView& left,
                                  const PresentationResourceKeyView& right) const noexcept -> bool {
        return less(left.assetId, left.type, right.assetId, right.type);
    }
};

struct PreparedPresentation final {
    PresentationResourceManifest manifest;
    std::map<PresentationResourceKey, PortableResourcePtr, PresentationResourceKeyLess> resources;
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
