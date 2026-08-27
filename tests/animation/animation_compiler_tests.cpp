#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/animation/animation_diagnostics.hpp>
#include <cuexis/animation/animation_program.hpp>
#include <cuexis/animation/animation_system.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/world/property.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator = 1)
    -> cuexis::chart::RationalBeat {
    auto value = cuexis::chart::RationalBeat::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto makeClip(cuexis::chart::AnimationRecordIdentity identity, std::string clipId,
                            std::string fieldPath) -> cuexis::chart::AnimationProgramClip {
    return {.identity = std::move(identity),
            .clip = {
                .id = std::move(clipId),
                .durationBeats = beat(4),
                .fieldPath = std::move(fieldPath),
            }};
}

[[nodiscard]] auto makeInstance(cuexis::chart::AnimationRecordIdentity identity,
                                cuexis::chart::AnimationRecordIdentity clipIdentity)
    -> cuexis::chart::ResolvedClipInstance {
    return {
        .identity = std::move(identity),
        .clipIdentity = std::move(clipIdentity),
        .startBeat = beat(0),
        .durationScale = beat(1),
        .iterations = {},
        .fillMode = cuexis::chart::AnimationFillMode::None,
        .weight = 1.0,
        .propertyMask = {},
    };
}

[[nodiscard]] auto hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code)
    -> bool {
    return std::ranges::any_of(diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

[[nodiscard]] auto contextValue(const cuexis::core::Diagnostic& diagnostic, std::string_view key)
    -> std::string {
    for (const auto& context : diagnostic.context()) {
        if (context.key == key) {
            return context.value;
        }
    }
    return {};
}

} // namespace

static_assert(!std::is_default_constructible_v<cuexis::animation::AnimationSystem>);

TEST_CASE("Empty AnimationProgramInput compiles to an owning empty program",
          "[animation][compile][s4-a]") {
    const auto compiled =
        cuexis::animation::AnimationCompiler::compile(cuexis::chart::AnimationProgramInput{});
    REQUIRE(compiled.hasValue());
    REQUIRE(compiled.program.has_value());
    CHECK(compiled.program->empty());
    CHECK(compiled.program->clipCount() == 0);
    CHECK(compiled.program->objectCount() == 0);
    CHECK_FALSE(compiled.diagnostics.hasErrors());
}

TEST_CASE("Compile lookup uses AnimationRecordIdentity and ignores embedded clip.id",
          "[animation][compile][identity][s4-a]") {
    cuexis::chart::AnimationProgramInput input;
    const cuexis::chart::GeneratedAnimationIdentity firstGenerated{
        .objectId = "note-a",
        .bindingId = "bind-a",
        .templateId = "move-y",
        .recordKind = cuexis::chart::GeneratedRecordKind::Clip,
    };
    const cuexis::chart::GeneratedAnimationIdentity secondGenerated{
        .objectId = "note-b",
        .bindingId = "bind-b",
        .templateId = "move-y",
        .recordKind = cuexis::chart::GeneratedRecordKind::Clip,
    };
    input.clips.push_back(makeClip("local-clip", "shared-id", "$/animationClips/0"));
    input.clips.push_back(makeClip(firstGenerated, "shared-id", "$/animationClips/generated/0"));
    input.clips.push_back(makeClip(secondGenerated, "shared-id", "$/animationClips/generated/1"));

    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = cuexis::chart::ChartObjectId{"note-a"};
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer-a";
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group-a";
    group.instances.push_back(makeInstance("instance-local", "local-clip"));
    group.instances.push_back(makeInstance("instance-generated", firstGenerated));
    layer.blendGroups.push_back(std::move(group));
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    REQUIRE(compiled.hasValue());
    REQUIRE(compiled.program.has_value());
    CHECK(compiled.program->clipCount() == 3);
    CHECK(compiled.program->objectCount() == 1);

    const auto* local = compiled.program->findClip(
        cuexis::chart::AnimationRecordIdentity{std::in_place_type<std::string>, "local-clip"});
    REQUIRE(local != nullptr);
    CHECK(local->clip.id == "shared-id");

    const auto* generated = compiled.program->findClip(firstGenerated);
    REQUIRE(generated != nullptr);
    CHECK(generated != local);
    CHECK(generated->clip.id == "shared-id");

    const auto* otherGenerated = compiled.program->findClip(secondGenerated);
    REQUIRE(otherGenerated != nullptr);
    CHECK(otherGenerated != generated);

    CHECK(compiled.program->findClip(cuexis::chart::AnimationRecordIdentity{
              std::in_place_type<std::string>, "shared-id"}) == nullptr);
    CHECK(compiled.program->findClip(cuexis::chart::AnimationRecordIdentity{
              std::in_place_type<std::string>, "missing"}) == nullptr);
    CHECK(compiled.program->findInstance(cuexis::chart::AnimationRecordIdentity{
              std::in_place_type<std::string>, "instance-local"}) != nullptr);
    CHECK(compiled.program->findInstance(firstGenerated) == nullptr);
}

TEST_CASE("Duplicate clip identity and missing clip identity produce frozen diagnostics",
          "[animation][compile][diagnostics][s4-a]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeClip("dup", "a", "$/animationClips/0"));
    input.clips.push_back(makeClip("dup", "b", "$/animationClips/1"));

    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = cuexis::chart::ChartObjectId{"note"};
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.instances.push_back(makeInstance("instance", "missing-clip"));
    layer.blendGroups.push_back(std::move(group));
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    CHECK_FALSE(compiled.hasValue());
    CHECK_FALSE(compiled.program.has_value());
    REQUIRE(compiled.diagnostics.hasErrors());
    CHECK(hasCode(compiled.diagnostics, cuexis::animation::diagnosticIdentityDuplicate));
    CHECK(hasCode(compiled.diagnostics, cuexis::animation::diagnosticClipMissing));

    REQUIRE(compiled.diagnostics.items().size() >= 2);
    CHECK(compiled.diagnostics.items()[0].fieldPath() <=
          compiled.diagnostics.items()[1].fieldPath());
    CHECK(contextValue(compiled.diagnostics.items()[0], cuexis::animation::contextChartLocalId) ==
          "dup");
}

TEST_CASE("Compile diagnostics keep generated identity context and frozen codes",
          "[animation][compile][generated][s4-a]") {
    const cuexis::chart::GeneratedAnimationIdentity generatedClip{
        .objectId = "note-a",
        .bindingId = "bind-a",
        .templateId = "move-y",
        .recordKind = cuexis::chart::GeneratedRecordKind::Clip,
    };
    cuexis::chart::AnimationProgramInput input;
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = cuexis::chart::ChartObjectId{"note-a"};
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.instances.push_back(makeInstance("instance", generatedClip));
    layer.blendGroups.push_back(std::move(group));
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    CHECK_FALSE(compiled.hasValue());
    REQUIRE(compiled.diagnostics.items().size() == 1);
    const auto& diagnostic = compiled.diagnostics.items().front();
    CHECK(diagnostic.code() == cuexis::animation::diagnosticClipMissing);
    CHECK(contextValue(diagnostic, cuexis::animation::contextObjectId) == "note-a");
    CHECK(contextValue(diagnostic, cuexis::animation::contextBindingId) == "bind-a");
    CHECK(contextValue(diagnostic, cuexis::animation::contextTemplateId) == "move-y");
    CHECK(contextValue(diagnostic, cuexis::animation::contextRecordKind) == "clip");
    CHECK(cuexis::animation::generatedRecordKindName(cuexis::chart::GeneratedRecordKind::Layer) ==
          "layer");
    CHECK(cuexis::animation::diagnosticClipIdLookupForbidden ==
          "animation.compile.clip_id_lookup_forbidden");
}

TEST_CASE("Bounded compile diagnostics replace the last accepted item with the sentinel",
          "[animation][compile][truncation][s4-a]") {
    cuexis::chart::AnimationProgramInput input;
    cuexis::chart::ObjectAnimationProgram object;
    object.objectId = cuexis::chart::ChartObjectId{"note"};
    cuexis::chart::ResolvedAnimationLayer layer;
    layer.identity = "layer";
    cuexis::chart::ResolvedBlendGroup group;
    group.identity = "group";
    group.instances.reserve(cuexis::animation::maxDiagnostics + 1);
    for (std::size_t index = 0; index < cuexis::animation::maxDiagnostics + 1; ++index) {
        group.instances.push_back(
            makeInstance("instance-" + std::to_string(index), "missing-" + std::to_string(index)));
    }
    layer.blendGroups.push_back(std::move(group));
    object.layers.push_back(std::move(layer));
    input.objects.push_back(std::move(object));

    const auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    CHECK_FALSE(compiled.hasValue());
    CHECK(compiled.diagnostics.limitReached());
    REQUIRE(compiled.diagnostics.size() == cuexis::animation::maxDiagnostics);
    CHECK(hasCode(compiled.diagnostics, cuexis::animation::diagnosticLimitExceeded));
}

TEST_CASE("Compiled clip totals accept exact max and reject plus one",
          "[animation][compile][limits][s4-g]") {
    cuexis::chart::AnimationProgramInput exact;
    exact.clips.push_back(makeClip("clip-a", "a", "$/owned/clip-a"));
    auto limits = cuexis::chart::ChartLimits{};
    limits.maxAnimationClips = 1;
    const auto accepted = cuexis::animation::AnimationCompiler::compile(std::move(exact), limits);
    REQUIRE(accepted.hasValue());
    CHECK(accepted.program->clipCount() == 1);

    cuexis::chart::AnimationProgramInput over;
    over.clips.push_back(makeClip("clip-a", "a", "$/owned/clip-a"));
    over.clips.push_back(makeClip("clip-b", "b", "$/owned/clip-b"));
    const auto rejected = cuexis::animation::AnimationCompiler::compile(std::move(over), limits);
    CHECK_FALSE(rejected.hasValue());
    CHECK(hasCode(rejected.diagnostics, cuexis::animation::diagnosticGeneratedLimit));
    CHECK(rejected.diagnostics.items().front().fieldPath() == "$/owned/clip-b");
}

TEST_CASE("Compiled track and segment totals accept exact max and reject plus one",
          "[animation][compile][limits][s4-g]") {
    auto clipWithTracks = [](std::string identity, std::size_t tracks,
                             std::size_t segments) -> cuexis::chart::AnimationProgramClip {
        auto clip = makeClip(std::move(identity), "shared", {});
        clip.clip.tracks.reserve(tracks);
        for (std::size_t trackIndex = 0; trackIndex < tracks; ++trackIndex) {
            cuexis::chart::AnimationTrack track;
            track.segments.reserve(segments);
            for (std::size_t segmentIndex = 0; segmentIndex < segments; ++segmentIndex) {
                track.segments.push_back({
                    .startBeat = beat(0),
                    .durationBeats = beat(1),
                    .startValue = 0.0,
                    .endValue = 0.0,
                });
            }
            clip.clip.tracks.push_back(std::move(track));
        }
        return clip;
    };

    cuexis::chart::AnimationProgramInput exactTracks;
    exactTracks.clips.push_back(clipWithTracks("clip", 2, 1));
    auto trackLimits = cuexis::chart::ChartLimits{};
    trackLimits.maxAnimationTracksPerClip = 2;
    trackLimits.maxAnimationTracks = 2;
    const auto acceptedTracks =
        cuexis::animation::AnimationCompiler::compile(std::move(exactTracks), trackLimits);
    REQUIRE(acceptedTracks.hasValue());

    cuexis::chart::AnimationProgramInput overTracks;
    overTracks.clips.push_back(clipWithTracks("clip", 3, 1));
    const auto rejectedTracks =
        cuexis::animation::AnimationCompiler::compile(std::move(overTracks), trackLimits);
    CHECK_FALSE(rejectedTracks.hasValue());
    CHECK(hasCode(rejectedTracks.diagnostics, cuexis::animation::diagnosticGeneratedLimit));
    CHECK(rejectedTracks.diagnostics.items().front().fieldPath() ==
          cuexis::animation::fallbackFieldPath);

    cuexis::chart::AnimationProgramInput exactSegments;
    exactSegments.clips.push_back(clipWithTracks("clip", 1, 2));
    auto segmentLimits = cuexis::chart::ChartLimits{};
    segmentLimits.maxAnimationSegmentsOrStepsPerTrack = 2;
    segmentLimits.maxAnimationSegmentsAndSteps = 2;
    const auto acceptedSegments =
        cuexis::animation::AnimationCompiler::compile(std::move(exactSegments), segmentLimits);
    REQUIRE(acceptedSegments.hasValue());

    cuexis::chart::AnimationProgramInput overSegments;
    overSegments.clips.push_back(clipWithTracks("clip", 1, 3));
    const auto rejectedSegments =
        cuexis::animation::AnimationCompiler::compile(std::move(overSegments), segmentLimits);
    CHECK_FALSE(rejectedSegments.hasValue());
    CHECK(hasCode(rejectedSegments.diagnostics, cuexis::animation::diagnosticGeneratedLimit));
}

TEST_CASE("Compiled generated records accept exact max and reject plus one",
          "[animation][compile][limits][generated][s4-g]") {
    const cuexis::chart::GeneratedAnimationIdentity first{
        .objectId = "note-a",
        .bindingId = "bind-a",
        .templateId = "move-y",
        .recordKind = cuexis::chart::GeneratedRecordKind::Clip,
    };
    const cuexis::chart::GeneratedAnimationIdentity second{
        .objectId = "note-b",
        .bindingId = "bind-b",
        .templateId = "move-y",
        .recordKind = cuexis::chart::GeneratedRecordKind::Clip,
    };
    auto limits = cuexis::chart::ChartLimits{};
    limits.maxGeneratedAnimationRecords = 1;

    cuexis::chart::AnimationProgramInput exact;
    exact.clips.push_back(makeClip(first, "shared-id", {}));
    const auto accepted = cuexis::animation::AnimationCompiler::compile(std::move(exact), limits);
    REQUIRE(accepted.hasValue());

    cuexis::chart::AnimationProgramInput over;
    over.clips.push_back(makeClip(first, "shared-id", {}));
    over.clips.push_back(makeClip(second, "shared-id", {}));
    const auto rejected = cuexis::animation::AnimationCompiler::compile(std::move(over), limits);
    CHECK_FALSE(rejected.hasValue());
    CHECK(hasCode(rejected.diagnostics, cuexis::animation::diagnosticGeneratedLimit));
    CHECK(rejected.diagnostics.items().front().fieldPath() == cuexis::animation::fallbackFieldPath);
    CHECK(contextValue(rejected.diagnostics.items().front(), cuexis::animation::contextObjectId) ==
          "note-b");
}

TEST_CASE("Compiled Property Write budget accepts exact max and rejects plus one",
          "[animation][compile][limits][writes][s4-g]") {
    cuexis::chart::AnimationProgramInput input;
    cuexis::chart::ObjectAnimationProgram first;
    first.objectId = cuexis::chart::ChartObjectId{"note-a"};
    cuexis::chart::ObjectAnimationProgram second;
    second.objectId = cuexis::chart::ChartObjectId{"note-b"};
    input.objects.push_back(std::move(first));
    input.objects.push_back(std::move(second));

    const auto exact = cuexis::animation::AnimationCompiler::compile(
        input, cuexis::chart::ChartLimits{}, cuexis::world::propertyCount * 2);
    REQUIRE(exact.hasValue());
    CHECK(exact.program->objectCount() == 2);

    const auto over = cuexis::animation::AnimationCompiler::compile(
        std::move(input), cuexis::chart::ChartLimits{}, cuexis::world::propertyCount * 2 - 1);
    CHECK_FALSE(over.hasValue());
    CHECK(hasCode(over.diagnostics, cuexis::animation::diagnosticWriteLimit));
    CHECK(over.diagnostics.items().front().fieldPath() == cuexis::animation::fallbackFieldPath);
}

TEST_CASE("Stricter fitting ChartLimits do not change a successful compile",
          "[animation][compile][limits][semantics][s4-g]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeClip("local-clip", "clip", "$/owned/clip"));
    auto limits = cuexis::chart::ChartLimits{};
    limits.maxAnimationClips = 1;
    const auto def = cuexis::animation::AnimationCompiler::compile(input);
    const auto tight = cuexis::animation::AnimationCompiler::compile(std::move(input), limits);
    REQUIRE(def.hasValue());
    REQUIRE(tight.hasValue());
    CHECK(def.program->clipCount() == tight.program->clipCount());
    CHECK(def.program->objectCount() == tight.program->objectCount());
}

TEST_CASE("Compile diagnostics keep owned field paths and never forge JSON clip indexes",
          "[animation][compile][diagnostics][s4-g]") {
    cuexis::chart::AnimationProgramInput input;
    input.clips.push_back(makeClip("dup", "a", ""));
    input.clips.push_back(makeClip("dup", "b", ""));
    const auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
    CHECK_FALSE(compiled.hasValue());
    REQUIRE_FALSE(compiled.diagnostics.items().empty());
    for (const auto& item : compiled.diagnostics.items()) {
        CHECK(item.fieldPath() == cuexis::animation::fallbackFieldPath);
        CHECK(item.fieldPath().find("animationClips") == std::string_view::npos);
    }
}
