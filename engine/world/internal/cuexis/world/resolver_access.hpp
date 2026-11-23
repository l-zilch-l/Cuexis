#pragma once

#include <cuexis/world/property.hpp>
#include <cuexis/world/transform_update.hpp>

namespace cuexis::world::detail {
struct ResolverAccess final {
    // Borrowed values are valid only until the resolver is mutated.
    static auto resolvedAppearance(const PropertyResolver&, entt::entity) noexcept
        -> std::array<const PropertyValue*, 4>;
    // Applies the existing captured-baseline type and value-domain rules to a write without
    // storing it. Callers use this to reject a batch before it becomes observable.
    static auto validatePropertyValue(const PropertyResolver&, entt::entity, PropertyId,
                                      const PropertyValue&) -> core::Result<void>;
    static auto prepareTransforms(PropertyResolver&, std::vector<TransformWrite>&)
        -> core::Result<void>;
    static void installed(PropertyResolver&) noexcept;
};
} // namespace cuexis::world::detail
