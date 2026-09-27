#include <cuexis/playback/playback_source.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <utility>

namespace {

[[nodiscard]] auto fixtureRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project";
}

void identityMatrix(float (&matrix)[16]) {
    std::memset(matrix, 0, sizeof(matrix));
    matrix[0] = 1.0F;
    matrix[5] = 1.0F;
    matrix[10] = 1.0F;
    matrix[15] = 1.0F;
}

[[nodiscard]] auto resource(cuexis::playback::PresentationResourceType type, std::string assetId,
                            cuexis::playback::PortableResourceValue value)
    -> cuexis::playback::PortableResourcePtr {
    cuexis::playback::PortableResource portable;
    portable.reference.type = type;
    portable.reference.assetId = std::move(assetId);
    portable.value = std::move(value);
    return std::make_shared<const cuexis::playback::PortableResource>(std::move(portable));
}

[[nodiscard]] auto prepareProject(cuexis::playback::PlaybackSession& session)
    -> cuexis::core::Result<cuexis::playback::PreparedPlayback> {
    auto source = cuexis::playback::PlaybackSource::fromFilesystemProject(fixtureRoot());
    if (!source) {
        return cuexis::core::unexpected(std::move(source.error()));
    }
    return session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
}

} // namespace

TEST_CASE("Neutral commands sort opaque ids and transparent depth",
          "[presentation][renderer][commands]") {
    cuexis::playback::PortableMesh mesh;
    mesh.boundsMin[2] = -5.0F;
    mesh.boundsMax[2] = -5.0F;
    cuexis::playback::PortableUnlitMaterial opaqueMaterial;
    cuexis::playback::PortableUnlitMaterial farMaterial;
    farMaterial.alphaMode = cuexis::playback::PresentationAlphaMode::Blend;
    cuexis::playback::PortableUnlitMaterial nearMaterial = farMaterial;

    const auto farMesh =
        resource(cuexis::playback::PresentationResourceType::Mesh, "mesh.far", mesh);
    mesh.boundsMin[2] = -1.0F;
    mesh.boundsMax[2] = -1.0F;
    const auto nearMesh =
        resource(cuexis::playback::PresentationResourceType::Mesh, "mesh.near", mesh);
    const auto opaqueMesh =
        resource(cuexis::playback::PresentationResourceType::Mesh, "mesh.opaque", mesh);
    const auto far =
        resource(cuexis::playback::PresentationResourceType::UnlitMaterial, "mat.far", farMaterial);
    const auto near = resource(cuexis::playback::PresentationResourceType::UnlitMaterial,
                               "mat.near", nearMaterial);
    const auto opaque = resource(cuexis::playback::PresentationResourceType::UnlitMaterial,
                                 "mat.opaque", opaqueMaterial);
    std::array<cuexis::playback::PortableResourcePtr, 6> resources{farMesh, nearMesh, opaqueMesh,
                                                                   far,     near,     opaque};

    cuexis::playback::FrameSnapshot snapshot;
    identityMatrix(snapshot.camera.viewMatrix);
    identityMatrix(snapshot.camera.projectionMatrix);
    snapshot.camera.active = true;
    snapshot.viewportWidth = 16;
    snapshot.viewportHeight = 9;
    snapshot.objects.resize(4);
    snapshot.objects[0].id = "b";
    snapshot.objects[1].id = "a";
    snapshot.objects[2].id = "far";
    snapshot.objects[3].id = "near";
    for (auto& object : snapshot.objects) {
        identityMatrix(object.worldMatrix);
        object.visible = true;
    }
    snapshot.objects[0].mesh = opaqueMesh->reference;
    snapshot.objects[0].material = opaque->reference;
    snapshot.objects[0].materialAssetId = "mat.opaque";
    snapshot.objects[1].mesh = opaqueMesh->reference;
    snapshot.objects[1].material = opaque->reference;
    snapshot.objects[1].materialAssetId = "mat.opaque";
    snapshot.objects[2].mesh = farMesh->reference;
    snapshot.objects[2].material = far->reference;
    snapshot.objects[2].materialAssetId = "mat.far";
    snapshot.objects[3].mesh = nearMesh->reference;
    snapshot.objects[3].material = near->reference;
    snapshot.objects[3].materialAssetId = "mat.near";

    const auto first = cuexis::presentation_renderer::buildPresentationCommands(
        snapshot, resources, false, false, nullptr);
    const auto second = cuexis::presentation_renderer::buildPresentationCommands(
        snapshot, resources, false, false, nullptr);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(first->digest == second->digest);
    REQUIRE(first->opaque.size() == 2);
    CHECK(first->opaque[0].objectId == "a");
    CHECK(first->opaque[1].objectId == "b");
    REQUIRE(first->transparent.size() == 2);
    CHECK(first->transparent[0].objectId == "far");
    CHECK(first->transparent[1].objectId == "near");
    CHECK(first->transparent[0].transparentDepthKey > first->transparent[1].transparentDepthKey);
    CHECK(first->transparent[0].sourceOverBlend);
    CHECK_FALSE(first->opaque[0].sourceOverBlend);

    snapshot.objects[0].id = "c";
    const auto changed = cuexis::presentation_renderer::buildPresentationCommands(
        snapshot, resources, false, false, nullptr);
    REQUIRE(changed.has_value());
    CHECK(changed->digest != first->digest);
}

TEST_CASE("Test renderer transactions reject failure and stale tokens",
          "[presentation][renderer][transaction]") {
    cuexis::presentation_renderer::TestPresentationRenderer renderer;
    cuexis::playback::PlaybackSession session;
    auto prepared = prepareProject(session);
    REQUIRE(prepared.has_value());

    cuexis::playback::PresentationRequest unsupported;
    unsupported.version = 3;
    unsupported.portableProfileVersion = 3;
    const auto capabilityFailed = renderer.prepare(*prepared, unsupported);
    REQUIRE_FALSE(capabilityFailed.has_value());
    CHECK(capabilityFailed.error().code() == "presentation.renderer.capability_failed");
    CHECK_FALSE(renderer.status().candidateOutstanding);

    auto candidate = renderer.prepare(*prepared, {.enableDebugPass = true});
    REQUIRE(candidate.has_value());
    CHECK(renderer.status().candidateOutstanding);
    const auto outstanding = renderer.prepare(*prepared);
    REQUIRE_FALSE(outstanding.has_value());
    CHECK(outstanding.error().code() == "presentation.renderer.candidate.outstanding");

    const auto generation = renderer.identity().generation;
    REQUIRE(renderer.resize(0, 8).has_value());
    CHECK(renderer.identity().generation == generation);
    CHECK(renderer.status().surfaceWidth == 0);
    const auto before = prepared->semanticIdentity();
    REQUIRE(before.has_value());

    REQUIRE(renderer.rebuild().has_value());
    CHECK(renderer.identity().generation == generation + 1);
    CHECK_FALSE(renderer.status().active);
    CHECK(renderer.status().activeResourceCount == 0);
    const auto after = prepared->semanticIdentity();
    REQUIRE(after.has_value());
    CHECK(before->sha256 == after->sha256);
    const auto stale = renderer.accepts(*candidate);
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code() == "presentation.renderer.token.stale");
    renderer.discard(std::move(*candidate));
    CHECK_FALSE(renderer.status().candidateOutstanding);

    auto replacement = renderer.prepare(*prepared, {.enableDebugPass = true});
    REQUIRE(replacement.has_value());
    REQUIRE(renderer.accepts(*replacement).has_value());
    const auto playbackIdentity = before->sha256;
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    const auto committed = session.semanticIdentity();
    REQUIRE(committed.has_value());
    CHECK(committed->sha256 == playbackIdentity);
    renderer.activate(std::move(*replacement));
    CHECK(renderer.hasActivePresentation());
    REQUIRE(renderer.resize(0, 8).has_value());
    const auto suspended = renderer.submit(cuexis::playback::FrameSnapshot{}, nullptr);
    REQUIRE_FALSE(suspended.has_value());
    CHECK(suspended.error().code() == "presentation.renderer.surface.zero_size");
    CHECK_FALSE(renderer.status().frameSubmitted);
    REQUIRE(renderer.resize(16, 16).has_value());
    CHECK(committed->sha256 == playbackIdentity);

    REQUIRE(
        session.update({.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0})
            .has_value());
    auto frame = session.extractFrame({.width = 16, .height = 16});
    REQUIRE(frame.has_value());
    cuexis::render::RenderScene debugScene;
    REQUIRE(
        debugScene.addDebugLine({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F, 1.0F})
            .has_value());
    auto summary = renderer.submit(*frame, &debugScene);
    REQUIRE(summary.has_value());
    CHECK(summary->debugPassEnabled);
    CHECK(summary->debugCommandCount == 1);
    CHECK(summary->digest != 0);
    const auto duplicate = renderer.submit(*frame, nullptr);
    REQUIRE_FALSE(duplicate.has_value());
    CHECK(duplicate.error().code() == "presentation.renderer.frame.already_submitted");

    renderer.failNextPresent(cuexis::presentation_renderer::PresentationFailureClass::Recoverable);
    const auto presentFailed = renderer.present();
    REQUIRE_FALSE(presentFailed.has_value());
    CHECK(presentFailed.error().code() == "presentation.renderer.present.failed");
    CHECK(renderer.hasActivePresentation());
    CHECK(committed->sha256 == playbackIdentity);
    const auto missingSubmit = renderer.present();
    REQUIRE_FALSE(missingSubmit.has_value());
    CHECK(missingSubmit.error().code() == "presentation.renderer.frame.not_submitted");

    REQUIRE(renderer.submit(*frame, nullptr).has_value());
    renderer.failNextPresent(
        cuexis::presentation_renderer::PresentationFailureClass::Unrecoverable);
    const auto deviceLost = renderer.present();
    REQUIRE_FALSE(deviceLost.has_value());
    CHECK(deviceLost.error().code() == "presentation.renderer.present.device_lost");
    CHECK(renderer.status().deviceLost);
    CHECK(renderer.hasActivePresentation());
    const auto blocked = renderer.submit(*frame, nullptr);
    REQUIRE_FALSE(blocked.has_value());
    CHECK(blocked.error().code() == "presentation.renderer.surface.lost");

    REQUIRE(renderer.rebuild().has_value());
    CHECK_FALSE(renderer.status().deviceLost);
    CHECK_FALSE(renderer.hasActivePresentation());
    const auto inactive = renderer.submit(*frame, nullptr);
    REQUIRE_FALSE(inactive.has_value());
    CHECK(inactive.error().code() == "presentation.renderer.inactive");

    cuexis::playback::PreparedPlayback empty;
    const auto invalid = renderer.prepare(empty);
    REQUIRE_FALSE(invalid.has_value());
    CHECK(invalid.error().code() == "presentation.renderer.prepare.invalid");

    REQUIRE(renderer.close().has_value());
    CHECK(renderer.status().closed);
    CHECK_FALSE(renderer.hasActivePresentation());
    cuexis::playback::PlaybackSession reopenedSession;
    auto reopened = prepareProject(reopenedSession);
    REQUIRE(reopened.has_value());
    const auto closed = renderer.prepare(*reopened);
    REQUIRE_FALSE(closed.has_value());
    CHECK(closed.error().code() == "presentation.renderer.closed");
    REQUIRE(renderer.rebuild().has_value());
    auto restored = renderer.prepare(*reopened);
    REQUIRE(restored.has_value());
    const auto closeBlocked = renderer.close();
    REQUIRE_FALSE(closeBlocked.has_value());
    CHECK(closeBlocked.error().code() == "presentation.renderer.close.candidate_outstanding");
    renderer.discard(std::move(*restored));
    REQUIRE(renderer.close().has_value());
}
