#pragma once

//  TransformSystem - world matrix updates
//  Computes top-down in parent-first order; dirty flags avoid recomputing unchanged
//  hierarchies
//  Atomic on validation failure: no partial update is applied to the World state

#include <cuexis/core/result.hpp>

namespace cuexis::world {

class World;

// Atomically recomputes every world matrix; nothing is modified if validation fails
[[nodiscard]] auto updateWorldTransforms(World& world) -> core::Result<void>;

} // namespace cuexis::world
