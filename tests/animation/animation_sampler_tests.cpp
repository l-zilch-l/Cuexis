#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/animation/animation_diagnostics.hpp>
#include <cuexis/animation/animation_sample.hpp>
#include <cuexis/animation/animation_system.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
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

[[nodiscard]] auto makeScalarSegment(cuexis::chart::RationalBeat start,
                                     cuexis::chart::RationalBeat duration, double startValue,
                                     double endValue, double startSlope = 0.0,
                                     double endSlope = 0.0) -> cuexis::chart::AnimationSegment {
    return {
        .startBeat = start,
        .durationBeats = duration,
        .startValue = startValue,
        .endValue = endValue,
        .startSlope = startSlope,
        .endSlope = endSlope,
        .fieldPath = "$/tracks/0/segments/0",
    };
}

[[nodiscard]] auto makeScalarClip(cuexis::chart::RationalBeat duration,
                                  std::vector<cuexis::chart::AnimationSegment> segments,
                                  std::vector<cuexis::chart::AnimationStepTrack> stepTracks = {})
    -> cuexis::chart::AnimationClip {
    return {
        .id = {},
        .durationBeats = duration,
        .tracks = {{
            .property = cuexis::chart::AnimationProperty::TransformPositionX,
            .segments = std::move(segments),
            .fieldPath = "$/tracks/0",
        }},
        .stepTracks = std::move(stepTracks),
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
                                cuexis::chart::AnimationRecordIdentity clipIdentity,
                                cuexis::chart::RationalBeat startBeat,
                                cuexis::chart::RationalBeat durationScale,
                                cuexis::chart::AnimationIterations iterations,
                                cuexis::chart::AnimationFillMode fillMode)
    -> cuexis::chart::ResolvedClipInstance {
    return {
        .identity = std::move(identity),
        .clipIdentity = std::move(clipIdentity),
        .startBeat = startBeat,
        .durationScale = durationScale,
        .iterations = iterations,
        .fillMode = fillMode,
        .weight = 1.0,
        .propertyMask = {},
    };
}

[[nodiscard]] auto compileOne(cuexis::chart::AnimationProgramClip clip,
                              cuexis::chart::ResolvedClipInstance instance)
    -> cuexis::animation::AnimationProgram {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(std::move(clip));
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = cuexis::chart::ChartObjectId{"note"};
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.instances.push_back(std::move(instance));
    layer.blendGroups.push_back(std::move(group));
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));
    auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    REQUIRE(compiled.hasValue());
    REQUIRE(compiled.program.has_value());
    return std::move(*compiled.program);
}

[[nodiscard]] auto scalarAt(const cuexis::animation::AnimationClipSample& sample) -> double {
    REQUIRE(sample.tracks.size() == 1);
    const auto* value = std::get_if<double>(&sample.tracks.front().value);
    REQUIRE(value != nullptr);
    return *value;
}

} // namespace

static_assert(!std::is_default_constructible_v<cuexis::animation::AnimationSampler>);
static_assert(!std::is_default_constructible_v<cuexis::animation::AnimationSystem>);

TEST_CASE("Local Beat uses startBeat durationScale iterations and fill",
          "[animation][sample][timing][s4-b]") {
    auto clip = makeScalarClip(
        beat(4), {makeScalarSegment(beat(0), beat(4), 0.0, 4.0, 1.0, 1.0)},
        {{
            .property = cuexis::chart::AnimationStepProperty::RenderVisible,
            .steps = {{.beat = beat(2), .value = true, .fieldPath = "$/stepTracks/0/steps/0"}},
            .fieldPath = "$/stepTracks/0",
        }});
    const auto program =
        compileOne(makeProgramClip("local-clip", "shared-id", std::move(clip)),
                   makeInstance("instance", "local-clip", beat(2), beat(1, 2),
                                cuexis::chart::AnimationIterations{.infinite = false, .count = 2},
                                cuexis::chart::AnimationFillMode::Hold));
    const auto* instance = program.findInstance(
        cuexis::chart::AnimationRecordIdentity{std::in_place_type<std::string>, "instance"});
    REQUIRE(instance != nullptr);

    const auto before =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(1));
    REQUIRE(before.has_value());
    CHECK_FALSE(before->has_value());

    const auto firstStart =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(2));
    REQUIRE(firstStart.has_value());
    REQUIRE(firstStart->has_value());
    CHECK((*firstStart)->localBeat == beat(0));
    CHECK(scalarAt(**firstStart) == Catch::Approx(0.0));
    CHECK((*firstStart)->steps.empty());

    const auto firstMid =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(3));
    REQUIRE(firstMid.has_value());
    REQUIRE(firstMid->has_value());
    CHECK((*firstMid)->localBeat == beat(2));
    CHECK(scalarAt(**firstMid) == Catch::Approx(2.0));
    REQUIRE((*firstMid)->steps.size() == 1);
    CHECK(std::get<bool>((*firstMid)->steps.front().value));

    const auto secondStart =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(4));
    REQUIRE(secondStart.has_value());
    REQUIRE(secondStart->has_value());
    CHECK((*secondStart)->localBeat == beat(0));
    CHECK(scalarAt(**secondStart) == Catch::Approx(0.0));

    const auto finiteHold =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(6));
    REQUIRE(finiteHold.has_value());
    REQUIRE(finiteHold->has_value());
    CHECK((*finiteHold)->localBeat == beat(4));
    CHECK(scalarAt(**finiteHold) == Catch::Approx(4.0));

    const auto afterHold =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(7));
    REQUIRE(afterHold.has_value());
    REQUIRE(afterHold->has_value());
    CHECK((*afterHold)->localBeat == beat(4));
    CHECK(scalarAt(**afterHold) == Catch::Approx(4.0));
}

TEST_CASE("Finite none fill and infinite wrap rebuild from absolute time",
          "[animation][sample][fill][s4-b]") {
    auto clip = makeScalarClip(beat(4), {makeScalarSegment(beat(0), beat(4), 0.0, 4.0, 1.0, 1.0)});
    const auto noneProgram =
        compileOne(makeProgramClip("clip", "clip-id", clip),
                   makeInstance("instance", "clip", beat(0), beat(1),
                                cuexis::chart::AnimationIterations{.infinite = false, .count = 1},
                                cuexis::chart::AnimationFillMode::None));
    const auto* noneInstance = noneProgram.findInstance(
        cuexis::chart::AnimationRecordIdentity{std::in_place_type<std::string>, "instance"});
    REQUIRE(noneInstance != nullptr);
    const auto noneEnd =
        cuexis::animation::AnimationSampler::sampleInstance(noneProgram, *noneInstance, beat(4));
    REQUIRE(noneEnd.has_value());
    CHECK_FALSE(noneEnd->has_value());

    const auto infiniteProgram =
        compileOne(makeProgramClip("clip", "clip-id", std::move(clip)),
                   makeInstance("instance", "clip", beat(-2), beat(1),
                                cuexis::chart::AnimationIterations{.infinite = true, .count = 1},
                                cuexis::chart::AnimationFillMode::None));
    const auto* infiniteInstance = infiniteProgram.findInstance(
        cuexis::chart::AnimationRecordIdentity{std::in_place_type<std::string>, "instance"});
    REQUIRE(infiniteInstance != nullptr);
    const auto wrap = cuexis::animation::AnimationSampler::sampleInstance(
        infiniteProgram, *infiniteInstance, beat(2));
    REQUIRE(wrap.has_value());
    REQUIRE(wrap->has_value());
    CHECK((*wrap)->localBeat == beat(0));
    CHECK(scalarAt(**wrap) == Catch::Approx(0.0));
}

TEST_CASE("Continuous gaps hold the previous endValue and zero-duration segments snap",
          "[animation][sample][curve][s4-b]") {
    auto clip = makeScalarClip(beat(8), {makeScalarSegment(beat(1), beat(2), 1.0, 3.0, 1.0, 1.0),
                                         makeScalarSegment(beat(4), beat(0), 9.0, 9.0)});
    const auto program =
        compileOne(makeProgramClip("clip", "clip-id", std::move(clip)),
                   makeInstance("instance", "clip", beat(0), beat(1),
                                cuexis::chart::AnimationIterations{.infinite = false, .count = 1},
                                cuexis::chart::AnimationFillMode::Hold));
    const auto* instance = program.findInstance(
        cuexis::chart::AnimationRecordIdentity{std::in_place_type<std::string>, "instance"});
    REQUIRE(instance != nullptr);

    const auto before =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(0));
    REQUIRE(before.has_value());
    REQUIRE(before->has_value());
    CHECK((*before)->tracks.empty());

    const auto gap =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(3));
    REQUIRE(gap.has_value());
    REQUIRE(gap->has_value());
    CHECK(scalarAt(**gap) == Catch::Approx(3.0));

    const auto snap =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(4));
    REQUIRE(snap.has_value());
    REQUIRE(snap->has_value());
    CHECK(scalarAt(**snap) == Catch::Approx(9.0));
}

TEST_CASE("Seek stop and different frame rates sample the same absolute Beat",
          "[animation][sample][seek][s4-b]") {
    auto clip = makeScalarClip(beat(4), {makeScalarSegment(beat(0), beat(4), 0.0, 4.0, 1.0, 1.0)});
    const auto program =
        compileOne(makeProgramClip("clip", "clip-id", std::move(clip)),
                   makeInstance("instance", "clip", beat(0), beat(1),
                                cuexis::chart::AnimationIterations{.infinite = false, .count = 2},
                                cuexis::chart::AnimationFillMode::None));

    const auto seek = cuexis::animation::AnimationSystem::sample(program, beat(3));
    REQUIRE(seek.has_value());
    REQUIRE(seek->size() == 1);
    REQUIRE(seek->front().clip.has_value());
    CHECK(scalarAt(*seek->front().clip) == Catch::Approx(3.0));

    auto previous = cuexis::animation::AnimationSystem::sample(program, beat(0));
    REQUIRE(previous.has_value());
    for (std::int64_t chartBeat = 1; chartBeat <= 3; ++chartBeat) {
        previous = cuexis::animation::AnimationSystem::sample(program, beat(chartBeat));
        REQUIRE(previous.has_value());
    }
    REQUIRE(previous->front().clip.has_value());
    CHECK(scalarAt(*previous->front().clip) == Catch::Approx(3.0));

    const auto coarse = cuexis::animation::AnimationSystem::sample(program, beat(3));
    const auto fine = cuexis::animation::AnimationSystem::sample(program, beat(6, 2));
    REQUIRE(coarse.has_value());
    REQUIRE(fine.has_value());
    REQUIRE(coarse->front().clip.has_value());
    REQUIRE(fine->front().clip.has_value());
    CHECK(coarse->front().clip->localBeat == fine->front().clip->localBeat);
    CHECK(scalarAt(*coarse->front().clip) == Catch::Approx(scalarAt(*fine->front().clip)));
}

TEST_CASE("Sample lookup rejects missing identity and embedded clip.id",
          "[animation][sample][identity][s4-b]") {
    auto clip = makeScalarClip(beat(4), {makeScalarSegment(beat(0), beat(4), 0.0, 1.0)});
    const auto program =
        compileOne(makeProgramClip("local-clip", "shared-id", std::move(clip)),
                   makeInstance("instance", "local-clip", beat(0), beat(1),
                                cuexis::chart::AnimationIterations{.infinite = false, .count = 1},
                                cuexis::chart::AnimationFillMode::None));

    auto missing = makeInstance("other", "missing-clip", beat(0), beat(1),
                                cuexis::chart::AnimationIterations{.infinite = false, .count = 1},
                                cuexis::chart::AnimationFillMode::None);
    const auto missingResult =
        cuexis::animation::AnimationSampler::sampleInstance(program, missing, beat(0));
    REQUIRE_FALSE(missingResult.has_value());
    CHECK(missingResult.error().code() == cuexis::animation::sampleClipMissing);

    auto byClipId = makeInstance("other", "shared-id", beat(0), beat(1),
                                 cuexis::chart::AnimationIterations{.infinite = false, .count = 1},
                                 cuexis::chart::AnimationFillMode::None);
    const auto forbidden =
        cuexis::animation::AnimationSampler::sampleInstance(program, byClipId, beat(0));
    REQUIRE_FALSE(forbidden.has_value());
    CHECK(forbidden.error().code() == cuexis::animation::sampleClipIdLookupForbidden);
}

TEST_CASE("Quaternion tracks use shortest-path slerp", "[animation][sample][quaternion][s4-b]") {
    cuexis::chart::AnimationClip clip{
        .id = {},
        .durationBeats = beat(2),
        .tracks = {{
            .property = cuexis::chart::AnimationProperty::TransformRotation,
            .segments = {{
                .startBeat = beat(0),
                .durationBeats = beat(2),
                .startValue = cuexis::core::Quat{0.0F, 0.0F, 0.0F, 1.0F},
                .endValue = cuexis::core::Quat{0.0F, 0.0F, 0.0F, -1.0F},
                .startSlope = 1.0,
                .endSlope = 1.0,
                .fieldPath = "$/tracks/0/segments/0",
            }},
            .fieldPath = "$/tracks/0",
        }},
        .fieldPath = "$/animationClips/0",
    };
    const auto program =
        compileOne(makeProgramClip("clip", "clip-id", std::move(clip)),
                   makeInstance("instance", "clip", beat(0), beat(1),
                                cuexis::chart::AnimationIterations{.infinite = false, .count = 1},
                                cuexis::chart::AnimationFillMode::Hold));
    const auto* instance = program.findInstance(
        cuexis::chart::AnimationRecordIdentity{std::in_place_type<std::string>, "instance"});
    REQUIRE(instance != nullptr);
    const auto sampled =
        cuexis::animation::AnimationSampler::sampleInstance(program, *instance, beat(1));
    REQUIRE(sampled.has_value());
    REQUIRE(sampled->has_value());
    REQUIRE((*sampled)->tracks.size() == 1);
    const auto* rotation = std::get_if<cuexis::core::Quat>(&(*sampled)->tracks.front().value);
    REQUIRE(rotation != nullptr);
    CHECK(rotation->x == Catch::Approx(0.0F));
    CHECK(rotation->y == Catch::Approx(0.0F));
    CHECK(rotation->z == Catch::Approx(0.0F));
    CHECK(rotation->w == Catch::Approx(1.0F));
}
