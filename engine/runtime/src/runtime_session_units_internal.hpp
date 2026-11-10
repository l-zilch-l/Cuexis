#pragma once

// The runtime session builds one evaluation state, runs the frame update against it, and applies
// the override and reload transactions. The units share the evaluation state and the property
// conversions, so both live here.

#include <cuexis/runtime/runtime_session.hpp>

#include <cuexis/animation/animation_system.hpp>
#include <cuexis/behavior/behavior_system.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/world/property.hpp>
#include <cuexis/world/resolver_access.hpp>
#include <cuexis/world/transform_system.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::runtime {

class RuntimeEvaluationState final {
  public:
    explicit RuntimeEvaluationState(std::size_t requiredWrites, std::size_t animationWrites)
        : writes(requiredWrites), animationWrites(animationWrites) {}

    struct CameraEntry final {
        entt::entity entity{entt::null};
        double baselineFovY{60.0};
        double candidateFovY{60.0};
        render::CameraComponent* slot{};
    };

    struct AppearanceEntry final {
        entt::entity entity{entt::null};
        render::AppearanceComponent baseline;
        render::AppearanceComponent candidate;
        render::AppearanceComponent* slot{};
    };

    behavior::BehaviorProgram program;
    world::PropertyWriteBuffer writes;
    world::PropertyWriteBuffer animationWrites;
    world::PropertyResolver resolver;
    std::optional<animation::AnimationProgram> animation;
    std::vector<animation::AnimationObjectBinding> animationBindings;
    std::vector<animation::AnimationObjectBaseline> animationBaselines;
    std::vector<animation::AnimationLayerContribution> animationLayerContributions;
    std::vector<world::OverrideToken> hostOverrides;
    std::vector<world::OverrideToken> previewOverrides;
    std::vector<world::PropertyWrite> gameplayWrites;
    std::uint64_t nextOverrideId{1};
    std::vector<CameraEntry> cameras;
    std::vector<AppearanceEntry> appearances;
    std::vector<world::detail::TransformWrite> transformWrites;
};

constexpr std::array<world::PropertyId, world::propertyCount> allPropertyIds{
    world::PropertyId::TransformPositionX, world::PropertyId::TransformPositionY,
    world::PropertyId::TransformPositionZ, world::PropertyId::TransformRotation,
    world::PropertyId::TransformScale,     world::PropertyId::CameraFovY,
    world::PropertyId::RenderVisible,      world::PropertyId::RenderMaterial,
    world::PropertyId::MaterialOpacity,    world::PropertyId::MaterialTint,
};

[[nodiscard]] auto allocateSessionToken() noexcept -> std::uint64_t;

void addSessionError(core::Diagnostics& diagnostics, std::string code, std::string message);

void addSessionError(core::Diagnostics& diagnostics, const core::Error& error);

[[nodiscard]] auto baselineFor(const chart::RuntimeObject& object, world::PropertyId property)
    -> core::Result<world::PropertyValue>;

[[nodiscard]] auto overrideIsActive(const world::OverrideToken& token,
                                    const RuntimeFrame& frame) noexcept -> bool;

[[nodiscard]] auto rebuildAnimationBaselines(RuntimeEvaluationState& state) -> core::Result<void>;

[[nodiscard]] auto applyResolvedPresentation(RuntimeEvaluationState& state) -> core::Result<void>;

[[nodiscard]] auto buildEvaluationState(chart::ChartRuntime& runtime,
                                        const ObjectEntityMap& objects, world::World& world,
                                        animation::AnimationProgram animation)
    -> core::Result<std::unique_ptr<RuntimeEvaluationState>>;

[[nodiscard]] auto preparePresentationSlots(RuntimeEvaluationState& state,
                                            world::detail::TransformUpdate& plan)
    -> core::Result<void>;

void installRuntimeState(RuntimeEvaluationState& state, world::detail::TransformUpdate& plan,
                         const RuntimeFrame& frame, bool tickLifetimes) noexcept;

[[nodiscard]] auto validateFrame(const RuntimeFrame& frame,
                                 const std::optional<RuntimeFrame>& previous) -> core::Result<void>;

[[nodiscard]] auto identityLabel(const chart::AnimationRecordIdentity& identity) -> std::string;

[[nodiscard]] auto objectIdFor(const ObjectEntityMap& objects, entt::entity entity)
    -> chart::ChartObjectId;

[[nodiscard]] auto baselineFor(const behavior::BehaviorBinding& binding, world::PropertyId property)
    -> world::PropertyValue;

[[nodiscard]] auto owningValue(const world::PropertyWriteValue& value) -> world::PropertyValue;

[[nodiscard]] auto resolvedValue(const RuntimeEvaluationState& state, entt::entity entity,
                                 world::PropertyId property) -> std::optional<world::PropertyValue>;

} // namespace cuexis::runtime
