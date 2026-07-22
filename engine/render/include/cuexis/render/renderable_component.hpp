#pragma once

//  RenderableComponent — 可渲染组件，保存类型化 Mesh/Material Handle
//  不保存 Lease、裸资源指针、shared_ptr 或图形后端 ID
//  Component 只存弱句柄；资源生命周期由 RuntimeSession 的 ResourceScope 管理

#include <cuexis/assets/resource_handle.hpp>

namespace cuexis::render {

struct RenderableComponent final {
    assets::MeshHandle mesh{};
    assets::MaterialHandle material{};
};

} // namespace cuexis::render
