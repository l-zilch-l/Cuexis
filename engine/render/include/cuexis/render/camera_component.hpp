#pragma once

#include <cstddef>

//  CameraComponent - camera data component (owned by cuexis_render; exposes no graphics
//  backend types)
//  Holds viewport-independent projection parameters. Viewport-aware matrices belong to the
//  extracted render frame or Playback snapshot.

#include <string>

namespace cuexis::render {

struct CameraComponent final {
    static constexpr std::size_t page_size = 128;

    std::string type{"perspective"};
    double fovY{60.0};
    double nearPlane{0.1};
    double farPlane{1000.0};
    double pitch{0.0};
    double yaw{0.0};
    double roll{0.0};
};

} // namespace cuexis::render
