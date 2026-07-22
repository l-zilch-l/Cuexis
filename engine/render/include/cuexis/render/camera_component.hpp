#pragma once

//  CameraComponent — 相机数据组件（所属 cuexis_render，不暴露图形后端类型）
//  保存投影参数和缓存的 projection 矩阵
//  projectionMatrix 在创建时根据 fovY/nearPlane/farPlane/aspect 计算

#include <cuexis/core/math.hpp>

#include <cstdint>
#include <string>

namespace cuexis::render {

struct CameraComponent final {
    std::string type{"perspective"};
    double fovY{60.0};
    double nearPlane{0.1};
    double farPlane{1000.0};
    double pitch{0.0};
    double yaw{0.0};
    double roll{0.0};
    core::Mat4 projectionMatrix{};
};

} // namespace cuexis::render