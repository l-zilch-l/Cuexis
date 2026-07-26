#pragma once

//  ResourceHandle<Tag> - typed weak handle; does not own the resource
//  Identified by index + generation + managerToken:
//    index: slot index (generation is bumped on slot reuse to prevent ABA problems)
//    managerToken: per-process ResourceManager identity, prevents cross-manager aliasing
//  Handles are never serialized into documents or caches; a Component stores only the
//  Handle, never a Lease or a raw pointer

#include <compare>
#include <cstdint>
#include <limits>

namespace cuexis::assets {

template <typename Tag> struct ResourceHandle final {
    static constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalidIndex};
    std::uint32_t generation{};
    // Process-local identity of the ResourceManager that owns this handle.  It is
    // intentionally not serialized and is checked by ResourceManager::get().
    std::uint64_t managerToken{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalidIndex && generation != 0;
    }

    [[nodiscard]] constexpr bool belongsTo(std::uint64_t token) const noexcept {
        return valid() && managerToken != 0 && managerToken == token;
    }

    auto operator<=>(const ResourceHandle&) const = default;
};

struct MeshTag final {};
struct MaterialTag final {};
struct TextureTag final {};

using MeshHandle = ResourceHandle<MeshTag>;
using MaterialHandle = ResourceHandle<MaterialTag>;
using TextureHandle = ResourceHandle<TextureTag>;

} // namespace cuexis::assets
