#pragma once

// Noninstalled read-only observation of World-owned buffers. EnTT private backing and
// TransformUpdate scratch are not enumerated here; this is not a complete owning graph.
#include <cuexis/world/world.hpp>
#include <cuexis_internal/semantic_memory.hpp>

namespace cuexis::world::detail {
struct WorldMemoryAccess final {
    static void append(const World& world, internal::pd::memory::SemanticPages& pages) noexcept {
        using Kind = internal::pd::memory::SemanticPage::Kind;
        pages.add(&world, sizeof(World), Kind::worldObject);
        pages.vector(world.transformCache_, Kind::transformCache);
        for (const auto& entry : world.transformCache_)
            pages.vector(entry.children, Kind::transformChildren);
        pages.vector(world.transformOrder_, Kind::transformOrder);
    }
};
} // namespace cuexis::world::detail
