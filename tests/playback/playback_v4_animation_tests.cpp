#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view animationNoteId = "019f0000-0000-7abc-8def-000000000412";
constexpr std::string_view bindingLeftId = "019f0000-0000-7abc-8def-000000000453";
constexpr std::string_view bindingRightId = "019f0000-0000-7abc-8def-000000000454";
constexpr std::string_view templateNoteId = "019f0000-0000-7abc-8def-000000000482";

constexpr std::array<cuexis::playback::RuntimeFrame, 4> animationFrames{
    cuexis::playback::RuntimeFrame{
        .chartTimeMs = 4000.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0},
    cuexis::playback::RuntimeFrame{
        .chartTimeMs = 4250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1},
    cuexis::playback::RuntimeFrame{
        .chartTimeMs = 5000.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 2},
    cuexis::playback::RuntimeFrame{
        .chartTimeMs = 6250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 3},
};

constexpr std::array<std::uint64_t, animationFrames.size()> expectedAnimationDigests{
    105060921077611920ULL, 10690198800679353609ULL, 18438846932740715847ULL,
    18147874964077530090ULL};

struct AnimationObservation final {
    cuexis::playback::PreparedSemanticIdentity identity;
    std::array<std::uint64_t, animationFrames.size()> digests{};
    std::vector<std::string> objectIds;
};

[[nodiscard]] auto fixture(std::string_view relative) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           relative;
}

[[nodiscard]] auto stage3Asset(std::string_view relative) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project" /
           "assets" / relative;
}

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"Could not read S4-E fixture: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    const auto text = readText(path);
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

[[nodiscard]] auto unusedProvider() -> std::shared_ptr<cuexis::content::IContentProvider> {
    auto provider = cuexis::content::HostContentProvider::create(
        [](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            return cuexis::core::unexpected(cuexis::core::Error{
                "test.content.unexpected", "The S4-E provider must not be read"});
        });
    if (!provider) {
        throw std::runtime_error{"Could not create S4-E content provider"};
    }
    return *provider;
}

[[nodiscard]] auto trimmedCapabilities() -> cuexis::playback::PlaybackCapabilitySet {
    return {
        .version = 1,
        .ids = {std::string{cuexis::playback::capabilityBehaviorEventV1},
                std::string{cuexis::playback::capabilityChartV3},
                std::string{cuexis::playback::capabilityChartV4},
                std::string{cuexis::playback::capabilityMaterialSnapshotV1},
                std::string{cuexis::playback::capabilityRenderVisibilityV1},
                std::string{cuexis::playback::capabilitySourceCxcV1},
                std::string{cuexis::playback::capabilitySourceCxtV1}},
    };
}

[[nodiscard]] auto typedChartSource(std::string_view relative, std::string sourceId)
    -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    return cuexis::playback::PlaybackSource::fromTypedProjectSource(
        {.sourceId = std::move(sourceId),
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json",
                               .utf8Text = readText(fixture(relative))}},
         .assets = {}},
        unusedProvider());
}

[[nodiscard]] auto typedCxtSource() -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    return cuexis::playback::PlaybackSource::fromTypedProjectSource(
        {.sourceId = "s4-e-cxt",
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json",
                               .utf8Text =
                                   readText(fixture("valid/chart_v4_cxt_template_binding.json"))},
                              {.path = "templates/move-y.cxt",
                               .utf8Text = readText(fixture("valid/templates/move-y.cxt"))}},
         .assets = {}},
        unusedProvider());
}

[[nodiscard]] auto typedSourceProject() -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    return cuexis::playback::PlaybackSource::fromTypedProjectSource(
        {.sourceId = "s4-f-typed-source-project",
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json",
                               .utf8Text = readText(
                                   fixture("source_project/assets/charts/main.cuexis.chart.json"))},
                              {.path = "templates/move-y.cxt",
                               .utf8Text =
                                   readText(fixture("source_project/templates/move-y.cxt"))}},
         .assets = {}},
        unusedProvider());
}

[[nodiscard]] auto animationChartSource()
    -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    auto provider = cuexis::content::MemoryContentProvider::create(
        {{.rootId = "main",
          .source = "meshes/note.mesh.bin",
          .bytes = readBytes(stage3Asset("meshes/triangle.mesh.bin"))},
         {.rootId = "main",
          .source = "materials/note.material.bin",
          .bytes = readBytes(stage3Asset("materials/opaque.material.bin"))}});
    if (!provider) {
        return cuexis::core::unexpected(std::move(provider.error()));
    }
    return cuexis::playback::PlaybackSource::fromTypedProjectSource(
        {.sourceId = "s4-e-animation",
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json",
                               .utf8Text = readText(fixture("valid/chart_v4_animation.json"))}},
         .assets = {{.id = "mesh.note",
                     .type = cuexis::playback::PlaybackAssetType::Mesh,
                     .rootId = "main",
                     .logicalSource = "meshes/note.mesh.bin"},
                    {.id = "material.note",
                     .type = cuexis::playback::PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "materials/note.material.bin"}}},
        std::move(*provider));
}

[[nodiscard]] auto findObject(const cuexis::playback::FrameSnapshot& snapshot, std::string_view id)
    -> const cuexis::playback::FrameSnapshot::ObjectSnapshot* {
    const auto found = std::ranges::find_if(snapshot.objects,
                                            [id](const auto& object) { return object.id == id; });
    return found == snapshot.objects.end() ? nullptr : &*found;
}

[[nodiscard]] auto load(cuexis::playback::PlaybackSession& session,
                        cuexis::core::Result<cuexis::playback::PlaybackSource> source)
    -> cuexis::core::Result<void> {
    if (!source) {
        return cuexis::core::unexpected(std::move(source.error()));
    }
    return session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
}

[[nodiscard]] auto snapshotAt(cuexis::playback::PlaybackSession& session, double chartTimeMs)
    -> cuexis::core::Result<cuexis::playback::FrameSnapshot> {
    auto updated = session.update({.chartTimeMs = chartTimeMs});
    if (!updated) {
        return cuexis::core::unexpected(std::move(updated.error()));
    }
    return session.extractFrame({.width = 640, .height = 480});
}

[[nodiscard]] auto observeAnimation(cuexis::core::Result<cuexis::playback::PlaybackSource> source)
    -> cuexis::core::Result<AnimationObservation> {
    if (!source) {
        return cuexis::core::unexpected(std::move(source.error()));
    }
    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    if (!prepared) {
        return cuexis::core::unexpected(std::move(prepared.error()));
    }
    const auto candidate = prepared->semanticIdentity();
    if (!candidate) {
        return cuexis::core::unexpected(cuexis::core::Error{"test.animation.identity_missing",
                                                            "S4-F candidate identity is missing"});
    }
    auto committed = session.commit(std::move(*prepared));
    if (!committed) {
        return cuexis::core::unexpected(std::move(committed.error()));
    }
    AnimationObservation observation{.identity = *candidate};
    for (std::size_t index = 0; index < animationFrames.size(); ++index) {
        auto updated = session.update(animationFrames[index]);
        if (!updated) {
            return cuexis::core::unexpected(std::move(updated.error()));
        }
        auto snapshot = session.extractFrame({.width = 1280, .height = 720});
        if (!snapshot) {
            return cuexis::core::unexpected(std::move(snapshot.error()));
        }
        auto digest = cuexis::playback::computeFrameDigest(animationFrames[index], *snapshot);
        if (!digest || digest->algorithmVersion != 3U) {
            return cuexis::core::unexpected(cuexis::core::Error{
                "test.animation.digest_failed", "S4-F FrameDigest v3 observation failed"});
        }
        observation.digests[index] = digest->value;
        if (index == 0) {
            observation.objectIds.reserve(snapshot->objects.size());
            for (const auto& object : snapshot->objects) {
                observation.objectIds.push_back(object.id);
            }
        }
    }
    return observation;
}

[[nodiscard]] auto framesEquivalent(const cuexis::playback::FrameSnapshot& left,
                                    const cuexis::playback::FrameSnapshot& right) -> bool {
    if (left.objects.size() != right.objects.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.objects.size(); ++index) {
        if (left.objects[index].id != right.objects[index].id ||
            left.objects[index].visible != right.objects[index].visible ||
            left.objects[index].materialAssetId != right.objects[index].materialAssetId ||
            left.objects[index].materialOpacity !=
                Catch::Approx(right.objects[index].materialOpacity)) {
            return false;
        }
        for (std::size_t component = 0; component < 16; ++component) {
            if (left.objects[index].worldMatrix[component] !=
                Catch::Approx(right.objects[index].worldMatrix[component])) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

TEST_CASE("Trimmed Playback still rejects nonempty animation before compile",
          "[playback][v4][animation][capability][s4-f]") {
    cuexis::playback::PlaybackSession session{trimmedCapabilities()};
    const auto capabilities = session.capabilities();
    REQUIRE(capabilities.has_value());
    CHECK_FALSE(std::ranges::any_of(capabilities->ids, [](const std::string& id) {
        return id == cuexis::playback::capabilityAnimationClipV1;
    }));
    CHECK_FALSE(std::ranges::any_of(capabilities->ids, [](const std::string& id) {
        return id == cuexis::playback::capabilityAnimationLayersV1;
    }));

    CHECK_FALSE(session
                    .prepareLoad(readText(fixture("valid/chart_v4_animation.json")),
                                 cuexis::playback::PlaybackMode::ChartClock)
                    .has_value());
    REQUIRE(session.lastOperationDiagnostics().has_value());
    CHECK(std::ranges::any_of(session.lastOperationDiagnostics()->items(), [](const auto& item) {
        return item.code() == "playback.capability.unsupported";
    }));
}

TEST_CASE("Default Playback compiles and commits empty Chart v4 animation state",
          "[playback][v4][animation][empty][s4-e][s4-f]") {
    const auto staticChart = readText(fixture("valid/chart_v4_static_migration.json"));
    cuexis::playback::PlaybackSession session;
    auto prepared = session.prepareLoad(staticChart, cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto candidateIdentity = prepared->semanticIdentity();
    REQUIRE(candidateIdentity.has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto frame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(frame.has_value());
    REQUIRE_FALSE(frame->objects.empty());
    const auto activeIdentity = session.semanticIdentity();
    REQUIRE(activeIdentity.has_value());
    CHECK(*activeIdentity == *candidateIdentity);
}

TEST_CASE("Default Playback evaluates chart_v4_animation.json through FrameSnapshot",
          "[playback][v4][animation][clip][s4-e][s4-f]") {
    auto source = animationChartSource();
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto candidateIdentity = prepared->semanticIdentity();
    REQUIRE(candidateIdentity.has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    const auto activeIdentity = session.semanticIdentity();
    REQUIRE(activeIdentity.has_value());
    CHECK(*activeIdentity == *candidateIdentity);

    auto before = snapshotAt(session, 0.0);
    REQUIRE(before.has_value());
    const auto* beforeNote = findObject(*before, animationNoteId);
    REQUIRE(beforeNote != nullptr);
    CHECK(beforeNote->worldMatrix[0] == Catch::Approx(1.0F));
    CHECK(beforeNote->worldMatrix[5] == Catch::Approx(1.0F));
    CHECK(beforeNote->materialOpacity == Catch::Approx(1.0));

    auto active = snapshotAt(session, 8500.0);
    REQUIRE(active.has_value());
    const auto* activeNote = findObject(*active, animationNoteId);
    REQUIRE(activeNote != nullptr);
    CHECK(activeNote->worldMatrix[0] == Catch::Approx(1.125F));
    CHECK(activeNote->worldMatrix[5] == Catch::Approx(1.125F));
    CHECK(activeNote->materialOpacity == Catch::Approx(0.8828125));

    REQUIRE(
        session.update({.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1})
            .has_value());
    auto seekStart = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(seekStart.has_value());
    CHECK(framesEquivalent(*seekStart, *before));

    REQUIRE(
        session
            .update({.chartTimeMs = 8500.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 2})
            .has_value());
    auto seekActive = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(seekActive.has_value());
    CHECK(framesEquivalent(*seekActive, *active));

    auto after = snapshotAt(session, 13000.0);
    REQUIRE(after.has_value());
    const auto* afterNote = findObject(*after, animationNoteId);
    REQUIRE(afterNote != nullptr);
    CHECK(afterNote->worldMatrix[0] == Catch::Approx(1.0F));
    CHECK(afterNote->materialOpacity == Catch::Approx(1.0));

    const auto identityAfterUpdate = session.semanticIdentity();
    REQUIRE(identityAfterUpdate.has_value());
    CHECK(*identityAfterUpdate == *candidateIdentity);
}

TEST_CASE("Default Playback evaluates CXT Binding and template animator",
          "[playback][v4][animation][cxt][template][s4-e][s4-f]") {
    SECTION("CXT Binding") {
        auto source = typedCxtSource();
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session;
        REQUIRE(load(session, std::move(source)).has_value());

        auto start = snapshotAt(session, 4000.0);
        REQUIRE(start.has_value());
        const auto* leftStart = findObject(*start, bindingLeftId);
        const auto* rightStart = findObject(*start, bindingRightId);
        REQUIRE(leftStart != nullptr);
        REQUIRE(rightStart != nullptr);
        CHECK(leftStart->worldMatrix[13] == Catch::Approx(0.0F));
        CHECK(rightStart->worldMatrix[13] == Catch::Approx(0.0F));

        auto mid = snapshotAt(session, 4250.0);
        REQUIRE(mid.has_value());
        const auto* leftMid = findObject(*mid, bindingLeftId);
        REQUIRE(leftMid != nullptr);
        CHECK(leftMid->worldMatrix[13] == Catch::Approx(0.5F));
        CHECK(findObject(*mid, bindingRightId)->worldMatrix[13] == Catch::Approx(0.0F));

        auto held = snapshotAt(session, 5000.0);
        REQUIRE(held.has_value());
        CHECK(findObject(*held, bindingLeftId)->worldMatrix[13] == Catch::Approx(1.0F));

        auto rightMid = snapshotAt(session, 6250.0);
        REQUIRE(rightMid.has_value());
        CHECK(findObject(*rightMid, bindingLeftId)->worldMatrix[13] == Catch::Approx(1.0F));
        CHECK(findObject(*rightMid, bindingRightId)->worldMatrix[13] == Catch::Approx(0.5F));
    }

    SECTION("template animator") {
        auto source = typedChartSource("valid/chart_v4_template_animator.json", "s4-e-template");
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session;
        REQUIRE(load(session, std::move(source)).has_value());

        auto start = snapshotAt(session, 0.0);
        REQUIRE(start.has_value());
        const auto* note = findObject(*start, templateNoteId);
        REQUIRE(note != nullptr);
        CHECK(note->worldMatrix[0] == Catch::Approx(1.0F));

        auto mid = snapshotAt(session, 500.0);
        REQUIRE(mid.has_value());
        CHECK(findObject(*mid, templateNoteId)->worldMatrix[0] == Catch::Approx(1.1F));

        auto wrap = snapshotAt(session, 1000.0);
        REQUIRE(wrap.has_value());
        CHECK(findObject(*wrap, templateNoteId)->worldMatrix[0] == Catch::Approx(1.0F));
    }
}

TEST_CASE("Default Playback failed reload keeps compiled animation state",
          "[playback][v4][animation][reload][s4-e][s4-f]") {
    auto source = typedCxtSource();
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto identity = prepared->semanticIdentity();
    REQUIRE(identity.has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    auto before = snapshotAt(session, 4250.0);
    REQUIRE(before.has_value());
    const auto infoBefore = session.contentInfo();
    REQUIRE(infoBefore.has_value());

    CHECK_FALSE(session
                    .reload(readText(fixture("invalid/chart_v4_mask_conflict.json")),
                            {.chartTimeMs = 4250.0}, cuexis::playback::ReloadPolicy::KeepChartTime)
                    .has_value());
    const auto identityAfter = session.semanticIdentity();
    const auto infoAfter = session.contentInfo();
    auto after = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(identityAfter.has_value());
    REQUIRE(infoAfter.has_value());
    REQUIRE(after.has_value());
    CHECK(*identityAfter == *identity);
    CHECK(infoAfter->chartId == infoBefore->chartId);
    CHECK(framesEquivalent(*after, *before));
}

TEST_CASE("Animation evaluation does not rewrite Prepared semantic identity",
          "[playback][v4][animation][identity][s4-e]") {
    auto typed = typedCxtSource();
    REQUIRE(typed.has_value());
    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*typed), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto candidate = prepared->semanticIdentity();
    REQUIRE(candidate.has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    REQUIRE(snapshotAt(session, 4250.0).has_value());
    const auto active = session.semanticIdentity();
    REQUIRE(active.has_value());
    CHECK(*active == *candidate);

    auto filesystem =
        cuexis::playback::PlaybackSource::fromFilesystemProject(fixture("source_project"));
    REQUIRE(filesystem.has_value());
    cuexis::playback::PlaybackSession filesystemSession;
    auto filesystemPrepared = filesystemSession.prepareLoad(
        std::move(*filesystem), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(filesystemPrepared.has_value());
    const auto filesystemIdentity = filesystemPrepared->semanticIdentity();
    REQUIRE(filesystemIdentity.has_value());
    CHECK(*filesystemIdentity == *candidate);
}

TEST_CASE("Default Playback advertises animation capabilities",
          "[playback][v4][animation][capability][s4-f]") {
    cuexis::playback::PlaybackSession session;
    const auto capabilities = session.capabilities();
    REQUIRE(capabilities.has_value());
    CHECK(capabilities->version == 1);
    CHECK(std::ranges::any_of(capabilities->ids, [](const std::string& id) {
        return id == cuexis::playback::capabilityAnimationClipV1;
    }));
    CHECK(std::ranges::any_of(capabilities->ids, [](const std::string& id) {
        return id == cuexis::playback::capabilityAnimationLayersV1;
    }));
}

TEST_CASE("Default Playback CXT sources share identity, FrameDigest v3, and object order",
          "[playback][v4][animation][consumer][digest][s4-f]") {
    auto filesystem = observeAnimation(
        cuexis::playback::PlaybackSource::fromFilesystemProject(fixture("source_project")));
    auto file = observeAnimation(
        cuexis::playback::PlaybackSource::fromCxcFile(fixture("golden/cxc_v1_v4_cxt.cxc")));
    auto memory = observeAnimation(cuexis::playback::PlaybackSource::fromCxcMemory(
        readBytes(fixture("golden/cxc_v1_v4_cxt.cxc"))));
    auto typed = observeAnimation(typedSourceProject());
    REQUIRE(filesystem.has_value());
    REQUIRE(file.has_value());
    REQUIRE(memory.has_value());
    REQUIRE(typed.has_value());
    CHECK(filesystem->identity == file->identity);
    CHECK(filesystem->identity == memory->identity);
    CHECK(filesystem->identity == typed->identity);
    CHECK(filesystem->digests == file->digests);
    CHECK(filesystem->digests == memory->digests);
    CHECK(filesystem->digests == typed->digests);
    CHECK(filesystem->objectIds == file->objectIds);
    CHECK(filesystem->objectIds == memory->objectIds);
    CHECK(filesystem->objectIds == typed->objectIds);
    REQUIRE_FALSE(filesystem->objectIds.empty());
    CHECK(std::ranges::is_sorted(filesystem->objectIds));
    CHECK(filesystem->digests == expectedAnimationDigests);
}

TEST_CASE("Default Playback seek, stop, discontinuity, and frame rate keep FrameDigest v3",
          "[playback][v4][animation][seek][stop][s4-f]") {
    auto source = typedCxtSource();
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(load(session, std::move(source)).has_value());

    auto start = snapshotAt(session, 4000.0);
    auto mid = snapshotAt(session, 4250.0);
    auto held = snapshotAt(session, 5000.0);
    REQUIRE(start.has_value());
    REQUIRE(mid.has_value());
    REQUIRE(held.has_value());
    const auto startDigest = cuexis::playback::computeFrameDigest({.chartTimeMs = 4000.0}, *start);
    const auto midDigest = cuexis::playback::computeFrameDigest({.chartTimeMs = 4250.0}, *mid);
    const auto heldDigest = cuexis::playback::computeFrameDigest({.chartTimeMs = 5000.0}, *held);
    REQUIRE(startDigest.has_value());
    REQUIRE(midDigest.has_value());
    REQUIRE(heldDigest.has_value());
    CHECK(startDigest->algorithmVersion == 3U);
    CHECK(heldDigest->algorithmVersion == 3U);
    CHECK(startDigest->value != midDigest->value);
    CHECK(heldDigest->value != startDigest->value);

    REQUIRE(
        session
            .update(
                {.chartTimeMs = 4000.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 11})
            .has_value());
    auto seekStart = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(seekStart.has_value());
    CHECK(framesEquivalent(*seekStart, *start));
    CHECK(cuexis::playback::computeFrameDigest({.chartTimeMs = 4000.0}, *seekStart)->value ==
          startDigest->value);

    REQUIRE(
        session
            .update(
                {.chartTimeMs = 4250.0, .simulationDeltaTimeMs = 33.0, .timeDiscontinuityId = 11})
            .has_value());
    auto seekMid = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(seekMid.has_value());
    CHECK(framesEquivalent(*seekMid, *mid));
    CHECK(cuexis::playback::computeFrameDigest({.chartTimeMs = 4250.0}, *seekMid)->value ==
          midDigest->value);

    REQUIRE(
        session
            .update(
                {.chartTimeMs = 4250.0, .simulationDeltaTimeMs = 8.0, .timeDiscontinuityId = 11})
            .has_value());
    auto sameTime = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(sameTime.has_value());
    CHECK(framesEquivalent(*sameTime, *mid));

    auto after = snapshotAt(session, 8000.0);
    REQUIRE(after.has_value());
    CHECK(findObject(*after, bindingLeftId)->worldMatrix[13] == Catch::Approx(1.0F));
    CHECK(findObject(*after, bindingRightId)->worldMatrix[13] == Catch::Approx(1.0F));
}

TEST_CASE("Invalid Chart v4 format still fails before AnimationSystem",
          "[playback][v4][animation][invalid][s4-f]") {
    cuexis::playback::PlaybackSession session;
    CHECK_FALSE(session
                    .prepareLoad(readText(fixture("invalid/chart_v4_mask_conflict.json")),
                                 cuexis::playback::PlaybackMode::ChartClock)
                    .has_value());
    REQUIRE(session.lastOperationDiagnostics().has_value());
    CHECK(std::ranges::any_of(session.lastOperationDiagnostics()->items(), [](const auto& item) {
        return item.code() == "chart.animation.mask_conflict" ||
               item.code() == "chart.animation.clip_invalid" || item.code() == "chart.v4.invalid";
    }));
    CHECK_FALSE(
        std::ranges::any_of(session.lastOperationDiagnostics()->items(), [](const auto& item) {
            return item.code() == "playback.capability.unsupported" ||
                   item.code() == "playback.animation.compile_failed";
        }));
}

TEST_CASE("Playback HostOverride restores the lower-layer result after release",
          "[playback][override][s4-d]") {
    auto source = animationChartSource();
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(load(session, std::move(source)).has_value());
    auto baseline = snapshotAt(session, 0.0);
    REQUIRE(baseline.has_value());
    const auto* before = findObject(*baseline, animationNoteId);
    REQUIRE(before != nullptr);
    const auto baselineX = before->worldMatrix[12];

    const auto mask =
        cuexis::playback::hostPropertyBit(cuexis::playback::HostPropertyId::TransformPositionX);
    const auto token = session.acquireHostOverride(
        "host", 1, mask, {},
        std::array{cuexis::playback::HostOverrideWrite{
            .objectId = std::string{animationNoteId},
            .property = cuexis::playback::HostPropertyId::TransformPositionX,
            .value = 9.0,
        }});
    REQUIRE(token.has_value());
    auto overridden = snapshotAt(session, 0.0);
    REQUIRE(overridden.has_value());
    CHECK(findObject(*overridden, animationNoteId)->worldMatrix[12] == Catch::Approx(9.0F));

    REQUIRE(session.releaseHostOverride(*token).has_value());
    auto restored = snapshotAt(session, 0.0);
    REQUIRE(restored.has_value());
    CHECK(findObject(*restored, animationNoteId)->worldMatrix[12] == Catch::Approx(baselineX));
}

TEST_CASE("Playback same-priority HostOverride discards the conflict write",
          "[playback][override][s4-d]") {
    auto source = animationChartSource();
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(load(session, std::move(source)).has_value());
    auto baseline = snapshotAt(session, 0.0);
    REQUIRE(baseline.has_value());
    const auto baselineX = findObject(*baseline, animationNoteId)->worldMatrix[12];
    const auto mask =
        cuexis::playback::hostPropertyBit(cuexis::playback::HostPropertyId::TransformPositionX);
    const auto write = [&](double value) {
        return cuexis::playback::HostOverrideWrite{
            .objectId = std::string{animationNoteId},
            .property = cuexis::playback::HostPropertyId::TransformPositionX,
            .value = value,
        };
    };
    REQUIRE(session.acquireHostOverride("left", 4, mask, {}, std::array{write(8.0)}).has_value());
    REQUIRE(session.acquireHostOverride("right", 4, mask, {}, std::array{write(9.0)}).has_value());
    auto conflicted = snapshotAt(session, 0.0);
    REQUIRE(conflicted.has_value());
    CHECK(findObject(*conflicted, animationNoteId)->worldMatrix[12] == Catch::Approx(baselineX));
}

TEST_CASE("Playback RemainingFrames HostOverride expires on the next frame",
          "[playback][override][s4-d]") {
    auto source = animationChartSource();
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(load(session, std::move(source)).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto mask =
        cuexis::playback::hostPropertyBit(cuexis::playback::HostPropertyId::TransformPositionX);
    REQUIRE(session
                .acquireHostOverride(
                    "host", 1, mask,
                    {.kind = cuexis::playback::HostOverrideLifetimeKind::RemainingFrames,
                     .remainingFrames = 1},
                    std::array{cuexis::playback::HostOverrideWrite{
                        .objectId = std::string{animationNoteId},
                        .property = cuexis::playback::HostPropertyId::TransformPositionX,
                        .value = 4.0,
                    }})
                .has_value());
    auto active = snapshotAt(session, 0.0);
    REQUIRE(active.has_value());
    CHECK(findObject(*active, animationNoteId)->worldMatrix[12] == Catch::Approx(4.0F));
    auto expired = snapshotAt(session, 0.0);
    REQUIRE(expired.has_value());
    CHECK(findObject(*expired, animationNoteId)->worldMatrix[12] == Catch::Approx(0.0F));
}

TEST_CASE("Playback HostOverride missing object does not change the active frame",
          "[playback][override][s4-d]") {
    auto source = animationChartSource();
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(load(session, std::move(source)).has_value());
    auto baseline = snapshotAt(session, 0.0);
    REQUIRE(baseline.has_value());
    const auto mask =
        cuexis::playback::hostPropertyBit(cuexis::playback::HostPropertyId::TransformPositionX);
    const auto token = session.acquireHostOverride(
        "host", 1, mask, {},
        std::array{cuexis::playback::HostOverrideWrite{
            .objectId = "missing.object",
            .property = cuexis::playback::HostPropertyId::TransformPositionX,
            .value = 9.0,
        }});
    REQUIRE_FALSE(token.has_value());
    CHECK(token.error().code() == "playback.override.object_missing");
    auto after = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(after.has_value());
    CHECK(framesEquivalent(*after, *baseline));
}

TEST_CASE("Trimmed Playback Session still accepts HostOverride",
          "[playback][override][capability][s4-d]") {
    constexpr std::string_view staticNoteId = "019f0000-0000-7abc-8def-000000000411";
    cuexis::playback::PlaybackSession session{trimmedCapabilities()};
    auto prepared = session.prepareLoad(readText(fixture("valid/chart_v4_static_migration.json")),
                                        cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    const auto mask =
        cuexis::playback::hostPropertyBit(cuexis::playback::HostPropertyId::TransformPositionX);
    REQUIRE(session
                .acquireHostOverride(
                    "host", 1, mask, {},
                    std::array{cuexis::playback::HostOverrideWrite{
                        .objectId = std::string{staticNoteId},
                        .property = cuexis::playback::HostPropertyId::TransformPositionX,
                        .value = 3.0,
                    }})
                .has_value());
    auto overridden = snapshotAt(session, 0.0);
    REQUIRE(overridden.has_value());
    CHECK(findObject(*overridden, staticNoteId)->worldMatrix[12] == Catch::Approx(3.0F));
}

TEST_CASE("Playback HostOverride requires at least one write", "[playback][override][s4-d]") {
    auto source = animationChartSource();
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(load(session, std::move(source)).has_value());
    const auto mask =
        cuexis::playback::hostPropertyBit(cuexis::playback::HostPropertyId::TransformPositionX);
    const auto token = session.acquireHostOverride(
        "host", 1, mask, {}, std::array<cuexis::playback::HostOverrideWrite, 0>{});
    REQUIRE_FALSE(token.has_value());
    CHECK(token.error().code() == "playback.override.empty");
}

TEST_CASE("Empty Playback Session rejects HostOverride", "[playback][override][s4-d]") {
    cuexis::playback::PlaybackSession session;
    const auto mask =
        cuexis::playback::hostPropertyBit(cuexis::playback::HostPropertyId::TransformPositionX);
    const auto token = session.acquireHostOverride(
        "host", 1, mask, {},
        std::array{cuexis::playback::HostOverrideWrite{
            .objectId = "object",
            .property = cuexis::playback::HostPropertyId::TransformPositionX,
            .value = 1.0,
        }});
    REQUIRE_FALSE(token.has_value());
    CHECK(token.error().code() == "playback.session.not_ready");
}
