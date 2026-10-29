#pragma once

#include <cuexis/world/property.hpp>
#include <cuexis/world/transform_update.hpp>

namespace cuexis::world::detail {
struct ResolverAccess final {
    // Borrowed values are valid only until the resolver is mutated.
    static auto resolvedAppearance(const PropertyResolver&, entt::entity,
                                   std::size_t* cachedIndex = nullptr) noexcept
        -> std::array<const PropertyValue*, 4>;
    static auto prepareTransforms(PropertyResolver&, std::vector<TransformWrite>&)
        -> core::Result<void>;
    static void installed(PropertyResolver&) noexcept;
};
} // namespace cuexis::world::detail
