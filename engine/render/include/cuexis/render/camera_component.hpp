#pragma once

//  CameraComponent - camera data component (owned by cuexis_render; exposes no graphics
//  backend types)
//  Holds the projection parameters and the cached projection matrix
//  projectionMatrix is computed on creation from fovY/nearPlane/farPlane/aspect

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