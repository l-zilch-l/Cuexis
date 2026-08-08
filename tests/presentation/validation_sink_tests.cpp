#include "validation_sink.hpp"

#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

[[nodiscard]] auto fixtureRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project";
}

[[nodiscard]] auto fullCapabilities() -> cuexis::playback::PresentationCapabilities {
    return cuexis::playback::PresentationCapabilities{
        .opaquePass = true,
        .transparentPass = true,
        .linearTexture = true,
        .srgbTexture = true,
        .straightAlphaBlend = true,
        .backFaceCulling = true,
        .doubleSided = true,
        .debugPass = true,
        .maxResourceBytes = 64ULL * 1024ULL * 1024ULL,
        .maxTotalDecodedBytes = 512ULL * 1024ULL * 1024ULL,
        .maxTextureDimension = 8192,
        .maxMeshVertices = 1'048'576,
        .maxMeshIndices = 3'145'728,
    };
}

[[nodiscard]] auto preparePortable(cuexis::playback::PlaybackSession& session)
    -> cuexis::core::Result<cuexis::playback::PreparedPlayback> {
    auto source = cuexis::playback::PlaybackSource::fromFilesystemProject(fixtureRoot());
    if (!source) {
        return cuexis::core::unexpected(std::move(source.error()));
    }
    return session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
}

[[nodiscard]] auto hasDiagnostic(const cuexis::core::Diagnostics& diagnostics,
                                 std::string_view code) -> bool {
    return std::any_of(diagnostics.items().begin(), diagnostics.items().end(),
                       [&](const auto& item) { return item.code() == code; });
}

[[nodiscard]] auto diagnosticContext(const cuexis::core::Diagnostics& diagnostics,
                                     std::string_view code, std::string_view key,
                                     std::string_view value) -> bool {
    for (const auto& item : diagnostics.items()) {
        if (item.code() != code) {
            continue;
        }
        if (std::any_of(item.context().begin(), item.context().end(), [&](const auto& context) {
                return context.key == key && context.value == value;
            })) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto acquireResources(cuexis::playback::PreparedPlayback& prepared,
                                    const cuexis::playback::PresentationResourceManifest& manifest)
    -> std::vector<cuexis::playback::PortableResourcePtr> {
    std::vector<cuexis::playback::PortableResourcePtr> resources;
    resources.reserve(manifest.entries.size());
    for (const auto& entry : manifest.entries) {
        auto resource = prepared.acquirePresentationResource(entry.reference);
        REQUIRE(resource.has_value());
        resources.push_back(std::move(*resource));
    }
    return resources;
}

[[nodiscard]] auto findEntry(const cuexis::playback::PresentationResourceManifest& manifest,
                             std::string_view assetId)
    -> const cuexis::playback::PresentationManifestEntry* {
    const auto found =
        std::find_if(manifest.entries.begin(), manifest.entries.end(),
                     [&](const auto& entry) { return entry.reference.assetId == assetId; });
    return found == manifest.entries.end() ? nullptr : &*found;
}

void makeIdentityMatrix(float (&matrix)[16]) {
    matrix[0] = 1.0F;
    matrix[5] = 1.0F;
    matrix[10] = 1.0F;
    matrix[15] = 1.0F;
}

void addObject(cuexis::playback::FrameSnapshot& snapshot, std::string id,
               const cuexis::playback::PresentationResourceRef& mesh,
               const cuexis::playback::PresentationResourceRef& material, float z,
               double opacity = 1.0) {
    cuexis::playback::FrameSnapshot::ObjectSnapshot object;
    object.id = std::move(id);
    object.hasTransform = true;
    object.mesh = mesh;
    object.material = material;
    object.materialAssetId = material.assetId;
    object.materialOpacity = opacity;
    makeIdentityMatrix(object.worldMatrix);
    object.worldMatrix[14] = z;
    snapshot.objects.push_back(std::move(object));
}

} // namespace

TEST_CASE("Prepared presentation preflight reports capabilities, limits, and Debug degradation",
          "[presentation][validation][capability]") {
    cuexis::playback::PlaybackSession session;
    auto prepared = preparePortable(session);
    REQUIRE(prepared.has_value());

    auto valid = prepared->validatePresentation(fullCapabilities(), {});
    REQUIRE(valid.hasValue());
    REQUIRE(valid.settings.has_value());
    CHECK_FALSE(valid.settings->debugPassEnabled);
    CHECK(valid.diagnostics.empty());

    auto debugCapabilities = fullCapabilities();
    debugCapabilities.debugPass = false;
    auto debug = prepared->validatePresentation(debugCapabilities, {.enableDebugPass = true});
    REQUIRE(debug.hasValue());
    CHECK_FALSE(debug.settings->debugPassEnabled);
    CHECK(debug.diagnostics.hasWarnings());
    CHECK(hasDiagnostic(debug.diagnostics, "playback.presentation.debug_unavailable"));

    auto missing = fullCapabilities();
    missing.opaquePass = false;
    missing.transparentPass = false;
    missing.linearTexture = false;
    missing.srgbTexture = false;
    missing.straightAlphaBlend = false;
    missing.backFaceCulling = false;
    missing.doubleSided = false;
    const auto unsupported = prepared->validatePresentation(missing, {});
    CHECK_FALSE(unsupported.hasValue());
    CHECK(unsupported.diagnostics.count(cuexis::core::DiagnosticSeverity::Error) == 7);
    CHECK(diagnosticContext(unsupported.diagnostics,
                            "playback.presentation.capability.required_missing", "capability",
                            "opaque_pass"));
    CHECK(diagnosticContext(unsupported.diagnostics,
                            "playback.presentation.capability.required_missing", "capability",
                            "transparent_pass"));

    auto versions = fullCapabilities();
    versions.version = 2;
    versions.portableProfileVersion = 2;
    const auto versioned =
        prepared->validatePresentation(versions, {.version = 3, .portableProfileVersion = 3});
    CHECK_FALSE(versioned.hasValue());
    CHECK(versioned.diagnostics.count(cuexis::core::DiagnosticSeverity::Error) == 4);
    CHECK(hasDiagnostic(versioned.diagnostics,
                        "playback.presentation.capability.version_unsupported"));
    CHECK(hasDiagnostic(versioned.diagnostics,
                        "playback.presentation.capability.profile_unsupported"));
}

TEST_CASE("Prepared presentation preflight checks each candidate limit",
          "[presentation][validation][limits]") {
    cuexis::playback::PlaybackSession session;
    auto prepared = preparePortable(session);
    REQUIRE(prepared.has_value());

    const auto rejects = [&](cuexis::playback::PresentationCapabilities capabilities,
                             std::string_view name) {
        const auto result = prepared->validatePresentation(capabilities, {});
        CHECK_FALSE(result.hasValue());
        CHECK(diagnosticContext(result.diagnostics,
                                "playback.presentation.capability.limit_insufficient", "capability",
                                name));
    };

    auto capabilities = fullCapabilities();
    capabilities.maxResourceBytes = 111;
    rejects(capabilities, "max_resource_bytes");
    capabilities = fullCapabilities();
    capabilities.maxTotalDecodedBytes = 600;
    rejects(capabilities, "max_total_decoded_bytes");
    capabilities = fullCapabilities();
    capabilities.maxTextureDimension = 1;
    rejects(capabilities, "max_texture_dimension");
    capabilities = fullCapabilities();
    capabilities.maxMeshVertices = 2;
    rejects(capabilities, "max_mesh_vertices");
    capabilities = fullCapabilities();
    capabilities.maxMeshIndices = 2;
    rejects(capabilities, "max_mesh_indices");
}

TEST_CASE("Presentation preflight preserves legacy candidates and rejects moved-from candidates",
          "[presentation][validation][lifecycle]") {
    cuexis::playback::PlaybackSession legacySession;
    auto legacySource = cuexis::playback::PlaybackSource::fromFilesystemProject(
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage1b_project");
    REQUIRE(legacySource.has_value());
    auto legacy = legacySession.prepareLoad(std::move(*legacySource),
                                            cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(legacy.has_value());
    auto legacyValidation = legacy->validatePresentation({}, {.enableDebugPass = true});
    REQUIRE(legacyValidation.hasValue());
    CHECK_FALSE(legacyValidation.settings->debugPassEnabled);
    CHECK(hasDiagnostic(legacyValidation.diagnostics, "playback.presentation.debug_unavailable"));

    cuexis::playback::PlaybackSession portableSession;
    auto prepared = preparePortable(portableSession);
    REQUIRE(prepared.has_value());
    auto moved = std::move(*prepared);
    const auto invalid = prepared->validatePresentation(fullCapabilities(), {});
    CHECK_FALSE(invalid.hasValue());
    CHECK(hasDiagnostic(invalid.diagnostics, "playback.prepared.invalid"));
    CHECK(moved.validatePresentation(fullCapabilities(), {}).hasValue());
}

TEST_CASE("Validation Sink independently validates resource identity, type, dependency, and order",
          "[presentation][validation][resources]") {
    cuexis::playback::PlaybackSession session;
    auto prepared = preparePortable(session);
    REQUIRE(prepared.has_value());
    const auto* manifest = prepared->presentationManifest();
    REQUIRE(manifest != nullptr);
    const auto resources = acquireResources(*prepared, *manifest);

    auto reversedManifest = *manifest;
    auto reversedResources = resources;
    std::reverse(reversedManifest.entries.begin(), reversedManifest.entries.end());
    std::reverse(reversedResources.begin(), reversedResources.end());
    REQUIRE(cuexis::test_support::validatePresentationData(reversedManifest, reversedResources)
                .has_value());

    SECTION("semantic identity") {
        auto changed = resources;
        const auto texture = std::find_if(changed.begin(), changed.end(), [](const auto& resource) {
            return resource->reference.assetId == "texture.checker";
        });
        REQUIRE(texture != changed.end());
        auto copy = std::make_shared<cuexis::playback::PortableResource>(**texture);
        std::get<cuexis::playback::PortableTexture2D>(copy->value).pixelsRgba8.back() =
            std::byte{0};
        *texture = std::move(copy);
        const auto result = cuexis::test_support::validatePresentationData(*manifest, changed);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.presentation.reference.invalid");
    }
    SECTION("resource type") {
        auto changed = resources;
        auto copy = std::make_shared<cuexis::playback::PortableResource>(*changed.front());
        copy->value = cuexis::playback::PortableMesh{};
        changed.front() = std::move(copy);
        const auto result = cuexis::test_support::validatePresentationData(*manifest, changed);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.presentation.reference.invalid");
    }
    SECTION("dependency") {
        auto changed = *manifest;
        const auto found =
            std::find_if(changed.entries.begin(), changed.entries.end(), [](const auto& entry) {
                return entry.reference.assetId == "material.blend";
            });
        REQUIRE(found != changed.entries.end());
        found->dependencies.clear();
        const auto result = cuexis::test_support::validatePresentationData(changed, resources);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.presentation.reference.invalid");
    }
    SECTION("missing resource") {
        auto changed = resources;
        changed.pop_back();
        const auto result = cuexis::test_support::validatePresentationData(*manifest, changed);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.presentation.reference.invalid");
    }
    SECTION("manifest budget") {
        auto changed = *manifest;
        ++changed.totalDecodedBytes;
        const auto result = cuexis::test_support::validatePresentationData(changed, resources);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.presentation.session.budget_exceeded");
    }
}

TEST_CASE("Validation candidate follows prepare, commit, and noexcept activation order",
          "[presentation][validation][transaction]") {
    cuexis::playback::PlaybackSession session;
    auto first = preparePortable(session);
    auto second = preparePortable(session);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    auto firstAdapter = cuexis::test_support::prepareValidationCandidate(*first, fullCapabilities(),
                                                                         {.enableDebugPass = true});
    auto secondAdapter = cuexis::test_support::prepareValidationCandidate(
        *second, fullCapabilities(), {.enableDebugPass = true});
    REQUIRE(firstAdapter.hasValue());
    REQUIRE(secondAdapter.hasValue());
    const auto secondToken = secondAdapter.candidate->token();

    cuexis::test_support::ValidationSink sink;
    CHECK_FALSE(sink.active());
    REQUIRE(session.commit(std::move(*second)).has_value());
    sink.activate(std::move(*secondAdapter.candidate));
    REQUIRE(sink.activeToken() != nullptr);
    CHECK(*sink.activeToken() == secondToken);

    const auto stale = session.commit(std::move(*first));
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code() == "playback.prepared.stale");
    CHECK(*sink.activeToken() == secondToken);
}

TEST_CASE("Validation Sink follows Playback material steps, stops, visibility, seek, and reload",
          "[presentation][validation][timeline]") {
    cuexis::playback::PlaybackSession session;
    auto prepared = preparePortable(session);
    REQUIRE(prepared.has_value());
    auto candidate =
        cuexis::test_support::prepareValidationCandidate(*prepared, fullCapabilities(), {});
    REQUIRE(candidate.hasValue());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    cuexis::test_support::ValidationSink sink;
    sink.activate(std::move(*candidate.candidate));

    const auto validateAt = [&](double chartTimeMs, std::uint64_t discontinuity = 0) {
        REQUIRE(session
                    .update({.chartTimeMs = chartTimeMs,
                             .simulationDeltaTimeMs = 0.0,
                             .timeDiscontinuityId = discontinuity})
                    .has_value());
        auto frame = session.extractFrame({.width = 1280, .height = 720});
        REQUIRE(frame.has_value());
        cuexis::test_support::ValidationSummary summary;
        REQUIRE(sink.validateFrame(*frame, summary).has_value());
        return summary;
    };

    const auto atZero = validateAt(0.0);
    REQUIRE(atZero.opaque.size() == 1);
    CHECK(atZero.transparent.empty());

    const auto atStopStart = validateAt(500.0);
    const auto duringStop = validateAt(625.0);
    REQUIRE(atStopStart.transparent.size() == 1);
    CHECK(duringStop.digest == atStopStart.digest);
    CHECK(duringStop.transparent.front().effectiveColor[3] < 1.0);

    const auto hidden = validateAt(1250.0);
    CHECK(hidden.opaque.empty());
    CHECK(hidden.transparent.empty());

    const auto sought = validateAt(0.0, 1);
    REQUIRE(sought.opaque.size() == 1);
    CHECK(sought.transparent.empty());

    auto replacementSource = cuexis::playback::PlaybackSource::fromFilesystemProject(fixtureRoot());
    REQUIRE(replacementSource.has_value());
    auto replacement = session.prepareReload(std::move(*replacementSource), {.chartTimeMs = 0.0},
                                             cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE(replacement.has_value());
    auto replacementCandidate =
        cuexis::test_support::prepareValidationCandidate(*replacement, fullCapabilities(), {});
    REQUIRE(replacementCandidate.hasValue());
    REQUIRE(session.commit(std::move(*replacement)).has_value());
    sink.activate(std::move(*replacementCandidate.candidate));
    auto afterReload = session.extractFrame({.width = 1280, .height = 720});
    REQUIRE(afterReload.has_value());
    cuexis::test_support::ValidationSummary reloadedSummary;
    REQUIRE(sink.validateFrame(*afterReload, reloadedSummary).has_value());
    REQUIRE(reloadedSummary.opaque.size() == 1);
    CHECK(reloadedSummary.transparent.empty());
}

TEST_CASE("Validation Sink produces canonical pass order, effective state, and stable digest",
          "[presentation][validation][frame]") {
    cuexis::playback::PlaybackSession session;
    auto prepared = preparePortable(session);
    REQUIRE(prepared.has_value());
    auto candidate = cuexis::test_support::prepareValidationCandidate(*prepared, fullCapabilities(),
                                                                      {.enableDebugPass = true});
    REQUIRE(candidate.hasValue());
    const auto manifest = candidate.candidate->manifest();
    const auto* mesh = findEntry(manifest, "mesh.triangle");
    const auto* opaque = findEntry(manifest, "material.opaque");
    const auto* blend = findEntry(manifest, "material.blend");
    REQUIRE(mesh != nullptr);
    REQUIRE(opaque != nullptr);
    REQUIRE(blend != nullptr);
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    cuexis::test_support::ValidationSink sink;
    sink.activate(std::move(*candidate.candidate));

    cuexis::playback::FrameSnapshot snapshot;
    snapshot.viewportWidth = 1280;
    snapshot.viewportHeight = 720;
    snapshot.camera.active = true;
    makeIdentityMatrix(snapshot.camera.viewMatrix);
    makeIdentityMatrix(snapshot.camera.projectionMatrix);
    addObject(snapshot, "opaque.z", mesh->reference, opaque->reference, 0.0F);
    addObject(snapshot, "opaque.a", mesh->reference, opaque->reference, 0.0F);
    addObject(snapshot, "transparent.near", mesh->reference, blend->reference, -1.0F, 0.5);
    addObject(snapshot, "transparent.tie.b", mesh->reference, blend->reference, -2.0F);
    addObject(snapshot, "transparent.far", mesh->reference, blend->reference, -3.0F);
    addObject(snapshot, "transparent.tie.a", mesh->reference, blend->reference, -2.0F);

    cuexis::test_support::ValidationSummary summary;
    REQUIRE(sink.validateFrame(snapshot, summary).has_value());
    REQUIRE(summary.opaque.size() == 2);
    CHECK(summary.opaque[0].objectId == "opaque.a");
    CHECK(summary.opaque[1].objectId == "opaque.z");
    REQUIRE(summary.transparent.size() == 4);
    CHECK(summary.transparent[0].objectId == "transparent.far");
    CHECK(summary.transparent[1].objectId == "transparent.tie.a");
    CHECK(summary.transparent[2].objectId == "transparent.tie.b");
    CHECK(summary.transparent[3].objectId == "transparent.near");
    CHECK(summary.transparent[0].transparentDepthKey == 12'288);
    CHECK(summary.transparent[1].transparentDepthKey == 8'192);
    CHECK(summary.transparent.back().effectiveColor[3] == Catch::Approx(0.375));
    CHECK(summary.transparent.back().sourceOverBlend);
    CHECK_FALSE(summary.transparent.back().depthWrite);
    CHECK(summary.debugPassEnabled);
    CHECK(summary.digest != 0);

    auto reordered = snapshot;
    std::reverse(reordered.objects.begin(), reordered.objects.end());
    cuexis::test_support::ValidationSummary reorderedSummary;
    REQUIRE(sink.validateFrame(reordered, reorderedSummary).has_value());
    CHECK(reorderedSummary.digest == summary.digest);
}

TEST_CASE("Validation Sink rejects invalid frames without publishing partial commands",
          "[presentation][validation][frame-errors]") {
    cuexis::playback::PlaybackSession session;
    auto prepared = preparePortable(session);
    REQUIRE(prepared.has_value());
    auto candidate =
        cuexis::test_support::prepareValidationCandidate(*prepared, fullCapabilities(), {});
    REQUIRE(candidate.hasValue());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    cuexis::test_support::ValidationSink sink;
    sink.activate(std::move(*candidate.candidate));
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto frame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(frame.has_value());

    cuexis::test_support::ValidationSummary summary;
    auto missingCamera = *frame;
    missingCamera.camera.active = false;
    auto result = sink.validateFrame(missingCamera, summary);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "playback.presentation.frame.camera_required");
    CHECK(summary.opaque.empty());
    CHECK(summary.transparent.empty());
    CHECK(summary.digest == 0);

    auto mismatch = *frame;
    REQUIRE(mismatch.objects.front().material.has_value());
    mismatch.objects.front().material->identity.sha256[0] ^= 0xFFU;
    result = sink.validateFrame(mismatch, summary);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "playback.presentation.frame.resource_mismatch");
    CHECK(summary.opaque.empty());

    auto invisible = missingCamera;
    invisible.objects.front().visible = false;
    REQUIRE(sink.validateFrame(invisible, summary).has_value());
    CHECK(summary.opaque.empty());
    CHECK(summary.transparent.empty());
    CHECK(summary.digest != 0);

    cuexis::playback::FrameSnapshot empty;
    REQUIRE(sink.validateFrame(empty, summary).has_value());
    CHECK(summary.opaque.empty());
    CHECK(summary.transparent.empty());
}
