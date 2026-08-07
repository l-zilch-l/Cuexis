#pragma once

//  RenderableComponent - renderable component holding typed Mesh/Material handles
//  Stores no Lease, raw resource pointer, shared_ptr or graphics backend ID
//  A Component stores only weak handles; resource lifetime is managed by the RuntimeSession
//  ResourceScope

#include <cuexis/assets/resource_handle.hpp>
#include <cuexis/core/math.hpp>

#include <string>

namespace cuexis::render {

struct RenderableComponent final {
    assets::MeshHandle mesh{};
    assets::MaterialHandle material{};
};

struct AppearanceComponent final {
    bool visible{true};
    std::string materialAssetId;
    double opacity{1.0};
    core::Vec3 tint{1.0F, 1.0F, 1.0F};
};

} // namespace cuexis::render
