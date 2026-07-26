#pragma once

//  RenderableComponent - renderable component holding typed Mesh/Material handles
//  Stores no Lease, raw resource pointer, shared_ptr or graphics backend ID
//  A Component stores only weak handles; resource lifetime is managed by the RuntimeSession
//  ResourceScope

#include <cuexis/assets/resource_handle.hpp>

namespace cuexis::render {

struct RenderableComponent final {
    assets::MeshHandle mesh{};
    assets::MaterialHandle material{};
};

} // namespace cuexis::render
