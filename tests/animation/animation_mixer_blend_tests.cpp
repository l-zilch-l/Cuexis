#include "animation_mixer_test_fixture.hpp"

// Override lerp, layer capture, discrete, additive and quaternion blend cases.

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

TEST_CASE("Layer contribution capture does not change mixed writes",
          "[animation][mix][semantics][s4-g]") {
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
    cuexis::world::PropertyWriteBuffer silent;
    cuexis::world::PropertyWriteBuffer captured;
    const auto withoutContributions = evaluate(program, bindings, baselines, silent);
    const auto withContributions = cuexis::animation::AnimationSystem::evaluate(
        program, beat(0), bindings, baselines, captured, true);
    REQUIRE(withoutContributions.has_value());
    REQUIRE(withContributions.has_value());
    CHECK_FALSE(withoutContributions->hasErrors());
    CHECK_FALSE(withContributions->hasErrors());
    CHECK(withContributions->layerContributions.size() == 1);
    REQUIRE(silent.size() == 1);
    REQUIRE(captured.size() == 1);
    CHECK(scalarWrite(silent, cuexis::world::PropertyId::TransformPositionX) ==
          scalarWrite(captured, cuexis::world::PropertyId::TransformPositionX));
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
