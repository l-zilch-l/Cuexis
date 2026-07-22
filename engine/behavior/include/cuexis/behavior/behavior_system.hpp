#pragma once

#include <cuexis/behavior/behavior_program.hpp>
#include <cuexis/core/result.hpp>

namespace cuexis::behavior {

class BehaviorSystem final {
  public:
    [[nodiscard]] static auto evaluate(const BehaviorProgram& program, double chartTimeMs,
                                       world::PropertyWriteBuffer& writes) -> core::Result<void>;
};

} // namespace cuexis::behavior
