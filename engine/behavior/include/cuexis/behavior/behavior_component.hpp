#pragma once

//  BehaviorComponent - behavior component holding a runtime behavior index
//  Phase 1A keeps this as opaque data only; Behavior Track evaluation belongs to phase 1C
//  RuntimeBehaviorIndex: compact handle referencing compiled behavior data

#include <compare>
#include <cstdint>
#include <limits>

namespace cuexis::behavior {

struct RuntimeBehaviorIndex final {
    static constexpr std::uint32_t invalidValue = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{invalidValue};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return value != invalidValue;
    }

    auto operator<=>(const RuntimeBehaviorIndex&) const = default;
};

struct BehaviorComponent final {
    RuntimeBehaviorIndex behavior;
};

} // namespace cuexis::behavior
