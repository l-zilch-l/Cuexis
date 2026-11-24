#pragma once

// Pure-value projection composition for the experimental frame bridge. The helper is private:
// both Playback flavors and private tests include this header from the source tree, and the
// installed SDK surface never sees it.

#include <cuexis/core/math.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/experimental/frame_layout.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace cuexis::playback::detail {

[[nodiscard]] inline auto frameProjectionError(std::string_view field) -> core::Error {
    return core::Error{"playback.snapshot.camera_invalid",
                       "Active camera parameters cannot produce a finite frame projection"}
        .withContext("field", std::string{field});
}

[[nodiscard]] inline auto composeFrameProjection(const core::Mat4& baseProjection,
                                                 const experimental::SelectedFrameLayout& selected)
    -> core::Result<core::Mat4> {
    using experimental::FrameRotation;

    if (!core::isFinite(baseProjection)) {
        return core::unexpected(frameProjectionError("base_projection"));
    }
    const auto rotation = selected.layout.target.rotation;
    core::Mat4 rotationMatrix;
    switch (rotation) {
    case FrameRotation::Deg0:
        break;
    case FrameRotation::Deg90:
        rotationMatrix.element(0, 0) = 0.0F;
        rotationMatrix.element(0, 1) = 1.0F;
        rotationMatrix.element(1, 0) = -1.0F;
        rotationMatrix.element(1, 1) = 0.0F;
        break;
    case FrameRotation::Deg180:
        rotationMatrix.element(0, 0) = -1.0F;
        rotationMatrix.element(1, 1) = -1.0F;
        break;
    case FrameRotation::Deg270:
        rotationMatrix.element(0, 0) = 0.0F;
        rotationMatrix.element(0, 1) = -1.0F;
        rotationMatrix.element(1, 0) = 1.0F;
        rotationMatrix.element(1, 1) = 0.0F;
        break;
    default:
        return core::unexpected(frameProjectionError("rotation"));
    }

    if (!selected.framing) {
        if (rotation == FrameRotation::Deg0) {
            return baseProjection;
        }
        auto rotated = core::multiply(rotationMatrix, baseProjection);
        if (!core::isFinite(rotated)) {
            return core::unexpected(frameProjectionError("legacy_projection"));
        }
        return rotated;
    }

    const auto width = selected.layout.content.width;
    const auto height = selected.layout.content.height;
    if (width == 0 || height == 0) {
        return core::unexpected(frameProjectionError("content_rect"));
    }
    const auto authorWidth = static_cast<double>(selected.framing->aspect.width);
    const auto authorHeight = static_cast<double>(selected.framing->aspect.height);
    if (!(authorWidth > 0.0) || !(authorHeight > 0.0)) {
        return core::unexpected(frameProjectionError("author_aspect"));
    }
    const bool quarterTurn = rotation == FrameRotation::Deg90 || rotation == FrameRotation::Deg270;
    const auto displayRatio = quarterTurn ? authorHeight / authorWidth : authorWidth / authorHeight;
    const auto contentWidth = static_cast<double>(width);
    const auto contentHeight = static_cast<double>(height);
    const auto scale = std::max(contentWidth / displayRatio, contentHeight);
    const auto horizontal = scale * displayRatio / contentWidth;
    const auto vertical = scale / contentHeight;
    constexpr auto maxFloat = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(displayRatio) || !(displayRatio > 0.0) || !std::isfinite(scale) ||
        !(scale > 0.0) || !std::isfinite(horizontal) || !std::isfinite(vertical) ||
        horizontal <= 0.0 || vertical <= 0.0 || horizontal > maxFloat || vertical > maxFloat ||
        static_cast<float>(horizontal) == 0.0F || static_cast<float>(vertical) == 0.0F) {
        return core::unexpected(frameProjectionError("raster_scale"));
    }

    core::Mat4 rasterScale;
    rasterScale.element(0, 0) = static_cast<float>(horizontal);
    rasterScale.element(1, 1) = static_cast<float>(vertical);
    auto projected = core::multiply(rotationMatrix, baseProjection);
    projected = core::multiply(rasterScale, projected);
    if (!core::isFinite(projected)) {
        return core::unexpected(frameProjectionError("projection_matrix"));
    }
    return projected;
}

} // namespace cuexis::playback::detail
