#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/animation/animation_diagnostics.hpp>
#include <cuexis/animation/animation_mixer.hpp>
#include <cuexis/animation/animation_system.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/world/property.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/entity.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator = 1)
    -> cuexis::chart::RationalBeat {
    auto value = cuexis::chart::RationalBeat::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto entity(std::uint32_t value) -> entt::entity {
    return static_cast<entt::entity>(value);
}

[[nodiscard]] auto constantScalarClip(cuexis::chart::AnimationProperty property, double value)
    -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .tracks = {{
            .property = property,
            .segments = {{
                .startBeat = beat(0),
                .durationBeats = beat(4),
                .startValue = value,
                .endValue = value,
                .fieldPath = "$/tracks/0/segments/0",
            }},
            .fieldPath = "$/tracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] auto constantVec3Clip(cuexis::chart::AnimationProperty property,
                                    cuexis::core::Vec3 value) -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .tracks = {{
            .property = property,
            .segments = {{
                .startBeat = beat(0),
                .durationBeats = beat(4),
                .startValue = value,
                .endValue = value,
                .fieldPath = "$/tracks/0/segments/0",
            }},
            .fieldPath = "$/tracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] auto constantQuatClip(cuexis::core::Quat value) -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .tracks = {{
            .property = cuexis::chart::AnimationProperty::TransformRotation,
            .segments = {{
                .startBeat = beat(0),
                .durationBeats = beat(4),
                .startValue = value,
                .endValue = value,
                .fieldPath = "$/tracks/0/segments/0",
            }},
            .fieldPath = "$/tracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] auto constantVisibleClip(bool visible) -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = beat(4),
        .stepTracks = {{
            .property = cuexis::chart::AnimationStepProperty::RenderVisible,
            .steps = {{.beat = beat(0), .value = visible, .fieldPath = "$/stepTracks/0/steps/0"}},
            .fieldPath = "$/stepTracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
}

[[nodiscard]] auto makeProgramClip(cuexis::chart::AnimationRecordIdentity identity,
                                   std::string clipId, cuexis::chart::AnimationClip clip)
    -> cuexis::chart::AnimationProgramClip {
    clip.id = std::move(clipId);
    return {.identity = std::move(identity), .clip = std::move(clip)};
}

[[nodiscard]] auto makeInstance(cuexis::chart::AnimationRecordIdentity identity,
                                cuexis::chart::AnimationRecordIdentity clipIdentity, double weight,
                                cuexis::chart::PropertyMask mask)
    -> cuexis::chart::ResolvedClipInstance {
    return {
        .identity = std::move(identity),
        .clipIdentity = std::move(clipIdentity),
        .startBeat = beat(0),
        .durationScale = beat(1),
        .iterations = {.infinite = false, .count = 1},
        .fillMode = cuexis::chart::AnimationFillMode::Hold,
        .weight = weight,
        .propertyMask = std::move(mask),
    };
}

[[nodiscard]] auto compile(cuexis::chart::AnimationProgramInput input)
    -> cuexis::animation::AnimationProgram {
    auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    REQUIRE(compiled.hasValue());
    REQUIRE(compiled.program.has_value());
    return std::move(*compiled.program);
}

[[nodiscard]] auto hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code)
    -> bool {
    return std::ranges::any_of(diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

[[nodiscard]] auto findWrite(const cuexis::world::PropertyWriteBuffer& writes,
                             cuexis::world::PropertyId property)
    -> std::optional<cuexis::world::PropertyWrite> {
    for (const auto& write : writes.writes()) {
        if (write.property == property) {
            return write;
        }
    }
    return {};
}

[[nodiscard]] auto scalarWrite(const cuexis::world::PropertyWriteBuffer& writes,
                               cuexis::world::PropertyId property) -> double {
    const auto write = findWrite(writes, property);
    REQUIRE(write.has_value());
    const auto* value = std::get_if<double>(&write->value);
    REQUIRE(value != nullptr);
    return *value;
}

[[nodiscard]] auto vec3Write(const cuexis::world::PropertyWriteBuffer& writes,
                             cuexis::world::PropertyId property) -> cuexis::core::Vec3 {
    const auto write = findWrite(writes, property);
    REQUIRE(write.has_value());
    const auto* value = std::get_if<cuexis::core::Vec3>(&write->value);
    REQUIRE(value != nullptr);
    return *value;
}

[[nodiscard]] auto quatWrite(const cuexis::world::PropertyWriteBuffer& writes)
    -> cuexis::core::Quat {
    const auto write = findWrite(writes, cuexis::world::PropertyId::TransformRotation);
    REQUIRE(write.has_value());
    const auto* value = std::get_if<cuexis::core::Quat>(&write->value);
    REQUIRE(value != nullptr);
    return *value;
}

[[nodiscard]] auto boolWrite(const cuexis::world::PropertyWriteBuffer& writes) -> bool {
    const auto write = findWrite(writes, cuexis::world::PropertyId::RenderVisible);
    REQUIRE(write.has_value());
    const auto* value = std::get_if<bool>(&write->value);
    REQUIRE(value != nullptr);
    return *value;
}

[[nodiscard]] auto objectId() -> cuexis::chart::ChartObjectId {
    return cuexis::chart::ChartObjectId{"note"};
}

[[nodiscard]] auto mask(std::vector<std::string> properties, std::vector<std::string> prefixes = {})
    -> cuexis::chart::PropertyMask {
    return {.properties = std::move(properties), .prefixes = std::move(prefixes)};
}

[[nodiscard]] auto binding() -> cuexis::animation::AnimationObjectBinding {
    return {.objectId = objectId(), .entity = entity(1)};
}

[[nodiscard]] auto baseline(cuexis::world::PropertyId property, cuexis::world::PropertyValue value)
    -> cuexis::animation::AnimationObjectBaseline {
    return {.objectId = objectId(),
            .properties = {{.property = property, .value = std::move(value)}}};
}

[[nodiscard]] auto evaluate(const cuexis::animation::AnimationProgram& program,
                            std::span<const cuexis::animation::AnimationObjectBinding> bindings,
                            std::span<const cuexis::animation::AnimationObjectBaseline> baselines,
                            cuexis::world::PropertyWriteBuffer& writes)
    -> cuexis::core::Result<cuexis::animation::AnimationEvaluateResult> {
    return cuexis::animation::AnimationSystem::evaluate(program, beat(0), bindings, baselines,
                                                        writes);
}

} // namespace

static_assert(!std::is_default_constructible_v<cuexis::animation::AnimationMixer>);

TEST_CASE("Override lerp uses group and layer weights against the layer input",
          "[animation][mix][override][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip(
        "clip", "clip-id",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 10.0)));
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.weight = 0.5;
    group.instances.push_back(
        makeInstance("instance", "clip", 1.0, mask({"transform.position.x"})));
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    layer.priority = 0;
    layer.weight = 0.5;
    layer.propertyMask = mask({"transform.position.x"});
    layer.blendGroups.push_back(std::move(group));
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformPositionX, 0.0)};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK_FALSE(mixed->hasErrors());
    CHECK(writes.size() == 1);
    CHECK(scalarWrite(writes, cuexis::world::PropertyId::TransformPositionX) == Catch::Approx(2.5));
}

TEST_CASE("Zero layer or group weight does not write", "[animation][mix][weight][s4-c]") {
    auto makeProgram = [](double layerWeight, double groupWeight) {
        cuexis::chart::AnimationProgramInput input;
        input.clips.push_back(makeProgramClip(
            "clip", "clip-id",
            constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 10.0)));
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = "group";
        group.weight = groupWeight;
        group.instances.push_back(
            makeInstance("instance", "clip", 1.0, mask({"transform.position.x"})));
        cuexis::chart::ResolvedAnimationLayer layer;
        layer.identity = "layer";
        layer.weight = layerWeight;
        layer.propertyMask = mask({"transform.position.x"});
        layer.blendGroups.push_back(std::move(group));
        cuexis::chart::ObjectAnimationProgram object;
        object.objectId = objectId();
        object.layers.push_back(std::move(layer));
        input.objects.push_back(std::move(object));
        return compile(std::move(input));
    };

    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformPositionX, 1.0)};
    cuexis::world::PropertyWriteBuffer writes;
    REQUIRE(evaluate(makeProgram(0.0, 1.0), bindings, baselines, writes).has_value());
    CHECK(writes.size() == 0);
    REQUIRE(evaluate(makeProgram(1.0, 0.0), bindings, baselines, writes).has_value());
    CHECK(writes.size() == 0);
}

TEST_CASE("Discrete override selects max weight then minimum instance identity",
          "[animation][mix][discrete][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip("true-clip", "true", constantVisibleClip(true)));
    input.clips.push_back(makeProgramClip("false-clip", "false", constantVisibleClip(false)));
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.instances.push_back(makeInstance("b", "false-clip", 0.4, mask({"render.visible"})));
    group.instances.push_back(makeInstance("a", "true-clip", 0.4, mask({"render.visible"})));
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    layer.propertyMask = mask({"render.visible"});
    layer.blendGroups.push_back(std::move(group));
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::RenderVisible, false)};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK_FALSE(mixed->hasErrors());
    CHECK(boolWrite(writes));
}

TEST_CASE("Additive position sums weighted deltas without a second layer mix",
          "[animation][mix][additive][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip(
        "clip-a", "a",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 2.0)));
    input.clips.push_back(makeProgramClip(
        "clip-b", "b",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 5.0)));
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.mode = cuexis::chart::AnimationBlendMode::Additive;
    group.weight = 0.5;
    group.instances.push_back(
        makeInstance("instance-b", "clip-b", 1.0, mask({"transform.position.x"})));
    group.instances.push_back(
        makeInstance("instance-a", "clip-a", 1.0, mask({"transform.position.x"})));
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    layer.weight = 0.5;
    layer.propertyMask = mask({}, {"transform."});
    layer.blendGroups.push_back(std::move(group));
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformPositionX, 1.0)};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK_FALSE(mixed->hasErrors());
    CHECK(scalarWrite(writes, cuexis::world::PropertyId::TransformPositionX) ==
          Catch::Approx(2.75));
}

TEST_CASE("Additive permutation golden does not depend on instance insertion order",
          "[animation][mix][additive][order][s4-c]") {
    auto makeProgram = [](bool reverse) {
        cuexis::chart::AnimationProgramInput input;
        input.clips.push_back(makeProgramClip(
            "clip-a", "a",
            constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 2.0)));
        input.clips.push_back(makeProgramClip(
            "clip-b", "b",
            constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 5.0)));
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = "group";
        group.mode = cuexis::chart::AnimationBlendMode::Additive;
        auto first = makeInstance("instance-a", "clip-a", 1.0, mask({"transform.position.x"}));
        auto second = makeInstance("instance-b", "clip-b", 1.0, mask({"transform.position.x"}));
        if (reverse) {
            group.instances.push_back(std::move(second));
            group.instances.push_back(std::move(first));
        } else {
            group.instances.push_back(std::move(first));
            group.instances.push_back(std::move(second));
        }
        cuexis::chart::ResolvedAnimationLayer layer;
        layer.identity = "layer";
        layer.propertyMask = mask({"transform.position.x"});
        layer.blendGroups.push_back(std::move(group));
        cuexis::chart::ObjectAnimationProgram object;
        object.objectId = objectId();
        object.layers.push_back(std::move(layer));
        input.objects.push_back(std::move(object));
        return compile(std::move(input));
    };

    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformPositionX, 1.0)};
    cuexis::world::PropertyWriteBuffer firstWrites;
    cuexis::world::PropertyWriteBuffer secondWrites;
    REQUIRE(evaluate(makeProgram(false), bindings, baselines, firstWrites).has_value());
    REQUIRE(evaluate(makeProgram(true), bindings, baselines, secondWrites).has_value());
    CHECK(scalarWrite(firstWrites, cuexis::world::PropertyId::TransformPositionX) ==
          Catch::Approx(8.0));
    CHECK(scalarWrite(secondWrites, cuexis::world::PropertyId::TransformPositionX) ==
          Catch::Approx(8.0));
}

TEST_CASE("Additive scale uses the weighted positive product",
          "[animation][mix][additive][scale][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(
        makeProgramClip("clip-a", "a",
                        constantVec3Clip(cuexis::chart::AnimationProperty::TransformScale,
                                         cuexis::core::Vec3{2.0F, 1.0F, 1.0F})));
    input.clips.push_back(
        makeProgramClip("clip-b", "b",
                        constantVec3Clip(cuexis::chart::AnimationProperty::TransformScale,
                                         cuexis::core::Vec3{1.0F, 3.0F, 1.0F})));
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.mode = cuexis::chart::AnimationBlendMode::Additive;
    group.instances.push_back(makeInstance("instance-b", "clip-b", 1.0, mask({"transform.scale"})));
    group.instances.push_back(makeInstance("instance-a", "clip-a", 1.0, mask({"transform.scale"})));
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    layer.propertyMask = mask({"transform.scale"});
    layer.blendGroups.push_back(std::move(group));
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{
        baseline(cuexis::world::PropertyId::TransformScale, cuexis::core::Vec3{1.0F, 1.0F, 1.0F})};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK_FALSE(mixed->hasErrors());
    const auto scale = vec3Write(writes, cuexis::world::PropertyId::TransformScale);
    CHECK(scale.x == Catch::Approx(2.0F));
    CHECK(scale.y == Catch::Approx(3.0F));
    CHECK(scale.z == Catch::Approx(1.0F));
}

TEST_CASE("Override quaternion mixes after aligning to the minimum instance identity hemisphere",
          "[animation][mix][quaternion][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip(
        "clip-a", "a", constantQuatClip(cuexis::core::Quat{0.0F, 0.0F, 0.0F, 1.0F})));
    input.clips.push_back(makeProgramClip(
        "clip-b", "b", constantQuatClip(cuexis::core::Quat{0.0F, 0.0F, 0.0F, -1.0F})));
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.instances.push_back(
        makeInstance("instance-b", "clip-b", 1.0, mask({"transform.rotation"})));
    group.instances.push_back(
        makeInstance("instance-a", "clip-a", 1.0, mask({"transform.rotation"})));
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    layer.propertyMask = mask({"transform.rotation"});
    layer.blendGroups.push_back(std::move(group));
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformRotation,
                                               cuexis::core::Quat{0.0F, 0.0F, 0.0F, 1.0F})};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK_FALSE(mixed->hasErrors());
    const auto rotation = quatWrite(writes);
    CHECK(rotation.x == Catch::Approx(0.0F));
    CHECK(rotation.y == Catch::Approx(0.0F));
    CHECK(rotation.z == Catch::Approx(0.0F));
    CHECK(rotation.w == Catch::Approx(1.0F));
}

TEST_CASE("Same-priority overlapping layers discard the conflict write",
          "[animation][mix][overlap][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip(
        "clip-a", "a",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 3.0)));
    input.clips.push_back(makeProgramClip(
        "clip-b", "b",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 9.0)));
    auto makeLayer = [](std::string identity, std::string clipIdentity, std::int64_t priority) {
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = identity + "-group";
        group.instances.push_back(makeInstance(identity + "-instance", std::move(clipIdentity), 1.0,
                                               mask({"transform.position.x"})));
        cuexis::chart::ResolvedAnimationLayer layer;
        layer.identity = std::move(identity);
        layer.priority = priority;
        layer.propertyMask = mask({"transform.position.x"});
        layer.blendGroups.push_back(std::move(group));
        return layer;
    };
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(makeLayer("layer-b", "clip-b", 0));
    object.layers.push_back(makeLayer("layer-a", "clip-a", 0));
    input.objects.push_back(std::move(object));

    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformPositionX, 1.0)};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK(mixed->hasErrors());
    CHECK(hasCode(mixed->diagnostics, cuexis::animation::mixPriorityOverlap));
    CHECK(writes.size() == 0);
}

TEST_CASE("Later priority replaces the earlier layer write", "[animation][mix][priority][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip(
        "clip-low", "low",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 3.0)));
    input.clips.push_back(makeProgramClip(
        "clip-high", "high",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 9.0)));
    auto makeLayer = [](std::string identity, std::string clipIdentity, std::int64_t priority) {
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = identity + "-group";
        group.instances.push_back(makeInstance(identity + "-instance", std::move(clipIdentity), 1.0,
                                               mask({"transform.position.x"})));
        cuexis::chart::ResolvedAnimationLayer layer;
        layer.identity = std::move(identity);
        layer.priority = priority;
        layer.propertyMask = mask({"transform.position.x"});
        layer.blendGroups.push_back(std::move(group));
        return layer;
    };
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(makeLayer("high", "clip-high", 10));
    object.layers.push_back(makeLayer("low", "clip-low", 0));
    input.objects.push_back(std::move(object));

    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformPositionX, 1.0)};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK_FALSE(mixed->hasErrors());
    CHECK(scalarWrite(writes, cuexis::world::PropertyId::TransformPositionX) == Catch::Approx(9.0));
}

TEST_CASE("Overlapping blend groups discard the property and keep a stable diagnostic",
          "[animation][mix][group][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip(
        "clip", "clip-id",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 4.0)));
    auto makeGroup = [](std::string identity) {
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = identity;
        group.instances.push_back(
            makeInstance(identity + "-instance", "clip", 1.0, mask({"transform.position.x"})));
        return group;
    };
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    layer.propertyMask = mask({"transform.position.x"});
    layer.blendGroups.push_back(makeGroup("group-b"));
    layer.blendGroups.push_back(makeGroup("group-a"));
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformPositionX, 1.0)};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK(mixed->hasErrors());
    CHECK(hasCode(mixed->diagnostics, cuexis::animation::mixGroupOverlap));
    CHECK(writes.size() == 0);
}

TEST_CASE("Discrete partial weight and additive material are defensive mix errors",
          "[animation][mix][diagnostics][s4-c]") {
    SECTION("discrete weight") {
        cuexis::chart::AnimationProgramInput input;
        input.clips.push_back(makeProgramClip("clip", "clip-id", constantVisibleClip(true)));
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = "group";
        group.weight = 0.5;
        group.instances.push_back(makeInstance("instance", "clip", 1.0, mask({"render.visible"})));
        cuexis::chart::ResolvedAnimationLayer layer;
        layer.identity = "layer";
        layer.propertyMask = mask({"render.visible"});
        layer.blendGroups.push_back(std::move(group));
        cuexis::chart::ObjectAnimationProgram object;
        object.objectId = objectId();
        object.layers.push_back(std::move(layer));
        input.objects.push_back(std::move(object));
        const auto program = compile(std::move(input));
        const auto bindings = std::array{binding()};
        const auto baselines =
            std::array{baseline(cuexis::world::PropertyId::RenderVisible, false)};
        cuexis::world::PropertyWriteBuffer writes;
        const auto mixed = evaluate(program, bindings, baselines, writes);
        REQUIRE(mixed.has_value());
        CHECK(hasCode(mixed->diagnostics, cuexis::animation::mixDiscreteWeightUnsupported));
        CHECK(writes.size() == 0);
    }
    SECTION("additive opacity") {
        cuexis::chart::AnimationProgramInput input;
        input.clips.push_back(makeProgramClip(
            "clip", "clip-id",
            constantScalarClip(cuexis::chart::AnimationProperty::MaterialOpacity, 0.25)));
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = "group";
        group.mode = cuexis::chart::AnimationBlendMode::Additive;
        group.instances.push_back(
            makeInstance("instance", "clip", 1.0, mask({"material.opacity"})));
        cuexis::chart::ResolvedAnimationLayer layer;
        layer.identity = "layer";
        layer.propertyMask = mask({"material.opacity"});
        layer.blendGroups.push_back(std::move(group));
        cuexis::chart::ObjectAnimationProgram object;
        object.objectId = objectId();
        object.layers.push_back(std::move(layer));
        input.objects.push_back(std::move(object));
        const auto program = compile(std::move(input));
        const auto bindings = std::array{binding()};
        const auto baselines =
            std::array{baseline(cuexis::world::PropertyId::MaterialOpacity, 1.0)};
        cuexis::world::PropertyWriteBuffer writes;
        const auto mixed = evaluate(program, bindings, baselines, writes);
        REQUIRE(mixed.has_value());
        CHECK(hasCode(mixed->diagnostics, cuexis::animation::mixAdditiveUnsupported));
        CHECK(writes.size() == 0);
    }
}

TEST_CASE("Additive rotation permutation golden is independent of instance order",
          "[animation][mix][additive][quaternion][s4-c]") {
    const auto identity = cuexis::core::Quat{0.0F, 0.0F, 0.0F, 1.0F};
    const auto quarterZ = cuexis::core::Quat{0.0F, 0.0F, 0.70710677F, 0.70710677F};
    auto makeProgram = [&](bool reverse) {
        cuexis::chart::AnimationProgramInput input;
        input.clips.push_back(makeProgramClip("clip-id", "id", constantQuatClip(identity)));
        input.clips.push_back(makeProgramClip("clip-z", "z", constantQuatClip(quarterZ)));
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = "group";
        group.mode = cuexis::chart::AnimationBlendMode::Additive;
        auto first = makeInstance("instance-id", "clip-id", 1.0, mask({"transform.rotation"}));
        auto second = makeInstance("instance-z", "clip-z", 1.0, mask({"transform.rotation"}));
        if (reverse) {
            group.instances.push_back(std::move(second));
            group.instances.push_back(std::move(first));
        } else {
            group.instances.push_back(std::move(first));
            group.instances.push_back(std::move(second));
        }
        cuexis::chart::ResolvedAnimationLayer layer;
        layer.identity = "layer";
        layer.propertyMask = mask({"transform.rotation"});
        layer.blendGroups.push_back(std::move(group));
        cuexis::chart::ObjectAnimationProgram object;
        object.objectId = objectId();
        object.layers.push_back(std::move(layer));
        input.objects.push_back(std::move(object));
        return compile(std::move(input));
    };

    const auto bindings = std::array{binding()};
    const auto baselines =
        std::array{baseline(cuexis::world::PropertyId::TransformRotation, identity)};
    cuexis::world::PropertyWriteBuffer firstWrites;
    cuexis::world::PropertyWriteBuffer secondWrites;
    REQUIRE(evaluate(makeProgram(false), bindings, baselines, firstWrites).has_value());
    REQUIRE(evaluate(makeProgram(true), bindings, baselines, secondWrites).has_value());
    const auto first = quatWrite(firstWrites);
    const auto second = quatWrite(secondWrites);
    CHECK(first.x == Catch::Approx(second.x).margin(1.0e-5));
    CHECK(first.y == Catch::Approx(second.y).margin(1.0e-5));
    CHECK(first.z == Catch::Approx(second.z).margin(1.0e-5));
    CHECK(first.w == Catch::Approx(second.w).margin(1.0e-5));
    CHECK(first.z == Catch::Approx(0.70710677F).margin(1.0e-4));
    CHECK(first.w == Catch::Approx(0.70710677F).margin(1.0e-4));
}

TEST_CASE("Empty mask does not write and missing entity binding is a hard error",
          "[animation][mix][mask][binding][s4-c]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip(
        "clip", "clip-id",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 4.0)));
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.instances.push_back(
        makeInstance("instance", "clip", 1.0, mask({"transform.position.x"})));
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    layer.propertyMask = mask({});
    layer.blendGroups.push_back(std::move(group));
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = objectId();
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));
    const auto program = compile(std::move(input));
    const auto bindings = std::array{binding()};
    const auto baselines = std::array{baseline(cuexis::world::PropertyId::TransformPositionX, 1.0)};
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK(writes.size() == 0);

    const auto unbound =
        cuexis::animation::AnimationMixer::evaluate(program, beat(0), {}, baselines, writes);
    REQUIRE_FALSE(unbound.has_value());
    CHECK(unbound.error().code() == cuexis::animation::mixBindingMissing);
}
