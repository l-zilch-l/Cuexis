#pragma once

#include <cuexis/core/math.hpp>
#include <cuexis/core/result.hpp>

namespace cuexis::animation {

[[nodiscard]] auto shortestPathSlerp(const core::Quat& left, const core::Quat& right, double t)
    -> core::Result<core::Quat>;

} // namespace cuexis::animation
