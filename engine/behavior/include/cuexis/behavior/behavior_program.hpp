#pragma once

#include <cuexis/behavior/behavior_component.hpp>
#include <cuexis/world/property.hpp>

#include <entt/entity/entity.hpp>

#include <cstdint>
#include <optional>
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

struct BehaviorEvent final {
    double startBeat{};
    double endBeat{};
    world::PropertyValue startValue{};
    world::PropertyValue endValue{};
    double startSlope{};
    double endSlope{};
    bool instantaneous{};
};

struct BehaviorEventTrack final {
    world::PropertyId property{};
    std::vector<BehaviorEvent> events;
};

struct BehaviorStepEvent final {
    double beat{};
    world::PropertyValue value{};
};

struct BehaviorStepTrack final {
    world::PropertyId property{};
    std::vector<BehaviorStepEvent> events;
};

struct BehaviorDefinition final {
    std::vector<BehaviorTrack> tracks;
    std::vector<BehaviorEventTrack> eventTracks;
    std::vector<BehaviorStepTrack> stepTracks;
};

struct PropertyBaseline final {
    world::PropertyId property{};
    world::PropertyValue value{};
};

struct BehaviorBinding final {
    entt::entity entity{entt::null};
    RuntimeBehaviorIndex behavior;
    std::vector<PropertyBaseline> baselines;
};

struct BehaviorSample final {
    double chartTimeMs{};
    double beat{};
    bool inStop{};
    double stopProgress{};
};

struct BehaviorProgram final {
    std::vector<BehaviorDefinition> definitions;
    std::vector<BehaviorBinding> bindings;
};

} // namespace cuexis::behavior
