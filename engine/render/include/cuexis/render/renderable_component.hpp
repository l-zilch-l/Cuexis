#pragma once

//  RenderableComponent - renderable component holding typed Mesh/Material handles
//  Stores no Lease, raw resource pointer, shared_ptr or graphics backend ID
//  A Component stores only weak handles; resource lifetime is managed by the RuntimeSession
//  ResourceScope

#include <cuexis/assets/resource_handle.hpp>
#include <cuexis/core/math.hpp>

#include <cstddef>
#include <string>

namespace cuexis::render {

struct RenderableComponent final {
    static constexpr std::size_t page_size = 128;

    assets::MeshHandle mesh{};
    assets::MaterialHandle material{};
};

struct AppearanceComponent final {
    static constexpr std::size_t page_size = 128;

    bool visible{true};
    std::string materialAssetId;
    double opacity{1.0};
    core::Vec3 tint{1.0F, 1.0F, 1.0F};
};

} // namespace cuexis::render
