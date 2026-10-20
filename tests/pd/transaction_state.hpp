#pragma once
#include <bit>
#include <cstdint>
#include <cuexis/world/world.hpp>
namespace cuexis::world::detail {
// Test-only implementation of the opaque friend; no production observer or SDK operation.
struct WorldTestAccess final {
    static auto cache(const World& w) -> std::vector<std::uint64_t> {
        std::vector<std::uint64_t> out{w.transformCacheValid_, w.transformCache_.size()};
        for (const auto& e : w.transformCache_) {
            out.insert(out.end(), {entt::to_integral(e.entity), entt::to_integral(e.parent),
                                   e.parentIndex.value_or(UINT64_MAX), e.children.size()});
            for (auto child : e.children)
                out.push_back(child);
            for (float v : {e.local.position.x, e.local.position.y, e.local.position.z,
                            e.local.rotation.x, e.local.rotation.y, e.local.rotation.z,
                            e.local.rotation.w, e.local.scale.x, e.local.scale.y, e.local.scale.z})
                out.push_back(std::bit_cast<std::uint32_t>(v));
            for (auto v : e.localMatrix.values)
                out.push_back(std::bit_cast<std::uint32_t>(v));
            for (auto v : e.world.values)
                out.push_back(std::bit_cast<std::uint32_t>(v));
        }
        out.push_back(w.transformOrder_.size());
        for (auto i : w.transformOrder_)
            out.push_back(i);
        return out;
    }
};
} // namespace cuexis::world::detail
