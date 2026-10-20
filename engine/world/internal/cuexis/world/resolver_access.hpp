#pragma once

#include <cuexis/world/property.hpp>
#include <cuexis/world/transform_update.hpp>

namespace cuexis::world::detail {
struct ResolverAccess final {
    static auto prepareTransforms(PropertyResolver&, std::vector<TransformWrite>&)
        -> core::Result<void>;
    static void installed(PropertyResolver&) noexcept;
};
} // namespace cuexis::world::detail
