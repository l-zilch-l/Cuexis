#pragma once

//  BehaviorComponent — 行为组件，保存运行时行为索引
//  阶段 1A 只作为 opaque 数据保留；Behavior Track 求值属于阶段 1C
//  RuntimeBehaviorIndex: 紧凑句柄引用编译后的行为数据

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
