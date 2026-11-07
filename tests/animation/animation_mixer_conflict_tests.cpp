#include "animation_mixer_test_fixture.hpp"

// Priority, group conflict, defensive mix and diagnostic bound cases.

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

TEST_CASE("Bounded mix diagnostics replace the last accepted item with the sentinel",
          "[animation][mix][truncation][s4-g]") {
    constexpr std::size_t overflowCount = cuexis::animation::maxDiagnostics + 1;
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeProgramClip(
        "clip", "clip-id",
        constantScalarClip(cuexis::chart::AnimationProperty::TransformPositionX, 4.0)));
    auto makeLayer = [](std::string identity) {
        cuexis::chart::ResolvedBlendGroup group;
        group.identity = identity + "-group";
        group.instances.push_back(
            makeInstance(identity + "-instance", "clip", 1.0, mask({"transform.position.x"})));
        cuexis::chart::ResolvedAnimationLayer layer;
        layer.identity = std::move(identity);
        layer.priority = 0;
        layer.propertyMask = mask({"transform.position.x"});
        layer.blendGroups.push_back(std::move(group));
        return layer;
    };
    std::vector<cuexis::animation::AnimationObjectBinding> bindings;
    std::vector<cuexis::animation::AnimationObjectBaseline> baselines;
    bindings.reserve(overflowCount);
    baselines.reserve(overflowCount);
    input.objects.reserve(overflowCount);
    for (std::size_t index = 0; index < overflowCount; ++index) {
        const auto id = "note-" + std::to_string(index);
        cuexis::chart::ObjectAnimationProgram object;
        object.objectId = cuexis::chart::ChartObjectId{id};
        object.layers.push_back(makeLayer("layer-a-" + std::to_string(index)));
        object.layers.push_back(makeLayer("layer-b-" + std::to_string(index)));
        bindings.push_back(
            {.objectId = object.objectId, .entity = entity(static_cast<std::uint32_t>(index + 1))});
        baselines.push_back(
            {.objectId = object.objectId,
             .properties = {
                 {.property = cuexis::world::PropertyId::TransformPositionX, .value = 0.0}}});
        input.objects.push_back(std::move(object));
    }

    const auto program = compile(std::move(input));
    cuexis::world::PropertyWriteBuffer writes;
    const auto mixed = evaluate(program, bindings, baselines, writes);
    REQUIRE(mixed.has_value());
    CHECK(mixed->diagnostics.limitReached());
    REQUIRE(mixed->diagnostics.size() == cuexis::animation::maxDiagnostics);
    CHECK(hasCode(mixed->diagnostics, cuexis::animation::diagnosticLimitExceeded));
    CHECK(hasCode(mixed->diagnostics, cuexis::animation::mixPriorityOverlap));
    for (const auto& item : mixed->diagnostics.items()) {
        CHECK(item.fieldPath() == cuexis::animation::fallbackFieldPath);
        CHECK(item.fieldPath().find("animationClips") == std::string_view::npos);
    }
}
