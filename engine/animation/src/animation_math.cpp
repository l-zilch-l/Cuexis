#include "animation_math.hpp"

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <cmath>

namespace cuexis::animation {
namespace {

[[nodiscard]] auto quaternionDot(const core::Quat& left, const core::Quat& right) noexcept
    -> double {
    return static_cast<double>(left.x) * right.x + static_cast<double>(left.y) * right.y +
           static_cast<double>(left.z) * right.z + static_cast<double>(left.w) * right.w;
}

[[nodiscard]] auto negateQuat(const core::Quat& value) noexcept -> core::Quat {
    return core::Quat{-value.x, -value.y, -value.z, -value.w};
}

[[nodiscard]] auto alignHemisphere(const core::Quat& reference, const core::Quat& value) noexcept
    -> core::Quat {
    return quaternionDot(reference, value) < 0.0 ? negateQuat(value) : value;
}

} // namespace

auto shortestPathSlerp(const core::Quat& left, const core::Quat& right, double t)
    -> core::Result<core::Quat> {
    core::Quat target = alignHemisphere(left, right);
    const double dot = std::clamp(quaternionDot(left, target), -1.0, 1.0);
    core::Quat result;
    if (dot > 0.9995) {
        const auto blend = static_cast<float>(t);
        result =
            core::Quat{left.x + (target.x - left.x) * blend, left.y + (target.y - left.y) * blend,
                       left.z + (target.z - left.z) * blend, left.w + (target.w - left.w) * blend};
    } else {
        const double theta = std::acos(dot);
        const double sinTheta = std::sin(theta);
        const double leftWeight = std::sin((1.0 - t) * theta) / sinTheta;
        const double rightWeight = std::sin(t * theta) / sinTheta;
        result = core::Quat{
            static_cast<float>(left.x * leftWeight + target.x * rightWeight),
            static_cast<float>(left.y * leftWeight + target.y * rightWeight),
            static_cast<float>(left.z * leftWeight + target.z * rightWeight),
            static_cast<float>(left.w * leftWeight + target.w * rightWeight),
        };
    }
    return core::normalize(result);
}

} // namespace cuexis::animation
