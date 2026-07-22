#pragma once

#include <cuexis/behavior/behavior_component.hpp>
#include <cuexis/world/property.hpp>

#include <entt/entity/entity.hpp>

#include <cstdint>
#include <vector>

namespace cuexis::behavior {

enum class Easing : std::uint8_t {
    Linear,
    InCubic,
    OutCubic,
    InOutCubic,
};

struct BehaviorKey final {
    double chartTimeMs{};
    world::PropertyValue value{};
    Easing easing{Easing::Linear};
};

struct BehaviorTrack final {
    world::PropertyId property{};
    std::vector<BehaviorKey> keys;
};

struct BehaviorDefinition final {
    std::vector<BehaviorTrack> tracks;
};

struct BehaviorBinding final {
    entt::entity entity{entt::null};
    RuntimeBehaviorIndex behavior;
};

struct BehaviorProgram final {
    std::vector<BehaviorDefinition> definitions;
    std::vector<BehaviorBinding> bindings;
};

} // namespace cuexis::behavior
