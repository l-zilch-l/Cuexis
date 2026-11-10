#include "presentation_fixture.hpp"

using namespace presentation_fixture;

TEST_CASE("Normalized presentation extraction produces canonical opaque and transparent order",
          "[playback][presentation][order]") {
    const auto fixture = loadFixture();
    cuexis::playback::PlaybackSession session;
    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto manifest = session.presentationManifest();
    REQUIRE(manifest.has_value());
    const auto resources = acquireResources(session, *manifest);
    const auto* mesh = findEntry(*manifest, "mesh.triangle");
    const auto* opaque = findEntry(*manifest, "material.opaque");
    const auto* blend = findEntry(*manifest, "material.blend");
    REQUIRE(mesh != nullptr);
    REQUIRE(opaque != nullptr);
    REQUIRE(blend != nullptr);

    cuexis::playback::FrameSnapshot snapshot;
    snapshot.camera.active = true;
    snapshot.camera.viewMatrix[0] = 1.0F;
    snapshot.camera.viewMatrix[5] = 1.0F;
    snapshot.camera.viewMatrix[10] = 1.0F;
    snapshot.camera.viewMatrix[15] = 1.0F;
    snapshot.camera.projectionMatrix[0] = 1.0F;
    snapshot.camera.projectionMatrix[5] = 1.0F;
    snapshot.camera.projectionMatrix[10] = 1.0F;
    snapshot.camera.projectionMatrix[15] = 1.0F;
    const auto addObject = [&](std::string id, const PresentationResourceRef& material, float z) {
        cuexis::playback::FrameSnapshot::ObjectSnapshot object;
        object.id = std::move(id);
        object.hasTransform = true;
        object.mesh = mesh->reference;
        object.material = material;
        object.materialAssetId = material.assetId;
        object.worldMatrix[0] = 1.0F;
        object.worldMatrix[5] = 1.0F;
        object.worldMatrix[10] = 1.0F;
        object.worldMatrix[15] = 1.0F;
        object.worldMatrix[14] = z;
        snapshot.objects.push_back(std::move(object));
    };
    addObject("opaque.z", opaque->reference, 0.0F);
    addObject("opaque.a", opaque->reference, 0.0F);
    addObject("transparent.near", blend->reference, -1.0F);
    addObject("transparent.tie.b", blend->reference, -2.0F);
    addObject("transparent.far", blend->reference, -3.0F);
    addObject("transparent.tie.a", blend->reference, -2.0F);
    addObject("transparent.half.positive", blend->reference, -0.5F / 4096.0F);
    addObject("transparent.half.negative", blend->reference, 0.5F / 4096.0F);

    cuexis::playback::detail::NormalizedPresentationFrame normalized;
    REQUIRE(cuexis::playback::detail::normalizePresentationFrame(snapshot, *manifest, resources,
                                                                 normalized)
                .has_value());
    REQUIRE(normalized.opaque.size() == 2);
    CHECK(snapshot.objects[normalized.opaque[0].objectIndex].id == "opaque.a");
    CHECK(snapshot.objects[normalized.opaque[1].objectIndex].id == "opaque.z");

    REQUIRE(normalized.transparent.size() == 6);
    CHECK(snapshot.objects[normalized.transparent[0].objectIndex].id == "transparent.far");
    CHECK(snapshot.objects[normalized.transparent[1].objectIndex].id == "transparent.tie.a");
    CHECK(snapshot.objects[normalized.transparent[2].objectIndex].id == "transparent.tie.b");
    CHECK(snapshot.objects[normalized.transparent[3].objectIndex].id == "transparent.near");
    CHECK(normalized.transparent[0].transparentDepthKey == 12'288);
    CHECK(normalized.transparent[1].transparentDepthKey == 8'192);
    CHECK(normalized.transparent[4].transparentDepthKey == 1);
    CHECK(normalized.transparent[5].transparentDepthKey == -1);
}

TEST_CASE("Normalized presentation extraction rejects camera, numeric, and resource mismatches",
          "[playback][presentation][frame-errors]") {
    const auto fixture = loadFixture();
    cuexis::playback::PlaybackSession session;
    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto manifest = session.presentationManifest();
    REQUIRE(manifest.has_value());
    const auto resources = acquireResources(session, *manifest);
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto snapshot = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(snapshot.has_value());
    cuexis::playback::detail::NormalizedPresentationFrame normalized;

    auto missingCamera = *snapshot;
    missingCamera.camera.active = false;
    auto result = cuexis::playback::detail::normalizePresentationFrame(missingCamera, *manifest,
                                                                       resources, normalized);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "playback.presentation.frame.camera_required");
    CHECK(normalized.opaque.empty());
    CHECK(normalized.transparent.empty());

    auto invisible = missingCamera;
    auto* invisibleObject = findObject(invisible, "019f0000-0000-7abc-8def-000000000310");
    REQUIRE(invisibleObject != nullptr);
    invisibleObject->visible = false;
    REQUIRE(cuexis::playback::detail::normalizePresentationFrame(invisible, *manifest, resources,
                                                                 normalized)
                .has_value());
    CHECK(normalized.opaque.empty());
    CHECK(normalized.transparent.empty());

    auto nonFiniteFrame = *snapshot;
    auto* nonFiniteObject = findObject(nonFiniteFrame, "019f0000-0000-7abc-8def-000000000310");
    REQUIRE(nonFiniteObject != nullptr);
    nonFiniteObject->worldMatrix[12] = std::numeric_limits<float>::infinity();
    result = cuexis::playback::detail::normalizePresentationFrame(nonFiniteFrame, *manifest,
                                                                  resources, normalized);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "playback.presentation.frame.non_finite");

    auto mismatchedFrame = *snapshot;
    auto* mismatchedObject = findObject(mismatchedFrame, "019f0000-0000-7abc-8def-000000000310");
    REQUIRE(mismatchedObject != nullptr);
    REQUIRE(mismatchedObject->material.has_value());
    mismatchedObject->material->identity.sha256[0] ^= 0xFFU;
    result = cuexis::playback::detail::normalizePresentationFrame(mismatchedFrame, *manifest,
                                                                  resources, normalized);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "playback.presentation.frame.resource_mismatch");

    cuexis::playback::FrameSnapshot empty;
    REQUIRE(cuexis::playback::detail::normalizePresentationFrame(empty, *manifest, resources,
                                                                 normalized)
                .has_value());
    CHECK(normalized.opaque.empty());
    CHECK(normalized.transparent.empty());
}

TEST_CASE("Normalized presentation extraction rejects malformed tables and renderable contracts",
          "[playback][presentation][frame-errors][branch-coverage]") {
    const auto fixture = loadFixture();
    cuexis::playback::PlaybackSession session;
    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto manifest = session.presentationManifest();
    REQUIRE(manifest.has_value());
    const auto resources = acquireResources(session, *manifest);
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto snapshot = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(snapshot.has_value());
    constexpr std::string_view objectId = "019f0000-0000-7abc-8def-000000000310";

    const auto expectMismatch = [&](const cuexis::playback::FrameSnapshot& candidate,
                                    const PresentationResourceManifest& candidateManifest,
                                    const std::vector<PortableResourcePtr>& candidateResources) {
        cuexis::playback::detail::NormalizedPresentationFrame normalized;
        normalized.opaque.push_back({});
        normalized.transparent.push_back({});
        const auto result = cuexis::playback::detail::normalizePresentationFrame(
            candidate, candidateManifest, candidateResources, normalized);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.presentation.frame.resource_mismatch");
        CHECK(normalized.opaque.empty());
        CHECK(normalized.transparent.empty());
    };

    SECTION("manifest and resource tables must stay aligned and typed") {
        auto shortResources = resources;
        shortResources.pop_back();
        expectMismatch(*snapshot, *manifest, shortResources);

        auto nullResource = resources;
        nullResource.front().reset();
        expectMismatch(*snapshot, *manifest, nullResource);

        auto wrongValue = resources;
        auto meshAsTexture = std::make_shared<cuexis::playback::PortableResource>(*wrongValue[0]);
        meshAsTexture->value = cuexis::playback::PortableTexture2D{};
        wrongValue[0] = std::move(meshAsTexture);
        expectMismatch(*snapshot, *manifest, wrongValue);
    }

    SECTION("snapshot renderable references must remain paired, compatible, and resolvable") {
        auto missingMaterial = *snapshot;
        auto* missingMaterialObject = findObject(missingMaterial, objectId);
        REQUIRE(missingMaterialObject != nullptr);
        missingMaterialObject->material.reset();
        expectMismatch(missingMaterial, *manifest, resources);

        auto incompatibleMaterial = *snapshot;
        auto* incompatibleMaterialObject = findObject(incompatibleMaterial, objectId);
        REQUIRE(incompatibleMaterialObject != nullptr);
        REQUIRE(incompatibleMaterialObject->material.has_value());
        incompatibleMaterialObject->material->type = PresentationResourceType::Texture2D;
        expectMismatch(incompatibleMaterial, *manifest, resources);

        auto missingMesh = *snapshot;
        auto* missingMeshObject = findObject(missingMesh, objectId);
        REQUIRE(missingMeshObject != nullptr);
        REQUIRE(missingMeshObject->mesh.has_value());
        missingMeshObject->mesh->assetId = "mesh.missing";
        expectMismatch(missingMesh, *manifest, resources);

        auto unexpectedDependencies = *manifest;
        const auto opaque = findEntry(unexpectedDependencies, "material.opaque");
        REQUIRE(opaque != nullptr);
        const auto opaqueIndex =
            static_cast<std::size_t>(opaque - unexpectedDependencies.entries.data());
        unexpectedDependencies.entries[opaqueIndex].dependencies.push_back(
            unexpectedDependencies.entries.front().reference);
        expectMismatch(*snapshot, unexpectedDependencies, resources);
    }
}

TEST_CASE(
    "Normalized presentation extraction clears output on numeric failures throughout the frame",
    "[playback][presentation][frame-errors][branch-coverage]") {
    const auto fixture = loadFixture();
    cuexis::playback::PlaybackSession session;
    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto manifest = session.presentationManifest();
    REQUIRE(manifest.has_value());
    const auto resources = acquireResources(session, *manifest);
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto snapshot = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(snapshot.has_value());
    constexpr std::string_view objectId = "019f0000-0000-7abc-8def-000000000310";

    const auto expectNonFinite = [&](const cuexis::playback::FrameSnapshot& candidate,
                                     const std::vector<PortableResourcePtr>& candidateResources,
                                     std::string_view field) {
        cuexis::playback::detail::NormalizedPresentationFrame normalized;
        normalized.opaque.push_back({});
        const auto result = cuexis::playback::detail::normalizePresentationFrame(
            candidate, *manifest, candidateResources, normalized);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.presentation.frame.non_finite");
        CHECK(errorContextValue(result.error(), "field") == field);
        CHECK(normalized.opaque.empty());
        CHECK(normalized.transparent.empty());
    };

    auto invalidCamera = *snapshot;
    invalidCamera.camera.viewMatrix[0] = std::numeric_limits<float>::infinity();
    expectNonFinite(invalidCamera, resources, "camera_matrix");

    auto invalidOpacity = *snapshot;
    auto* opacityObject = findObject(invalidOpacity, objectId);
    REQUIRE(opacityObject != nullptr);
    opacityObject->materialOpacity = std::numeric_limits<float>::quiet_NaN();
    expectNonFinite(invalidOpacity, resources, "material_opacity");

    auto invalidTint = *snapshot;
    auto* tintObject = findObject(invalidTint, objectId);
    REQUIRE(tintObject != nullptr);
    tintObject->materialTint[1] = std::numeric_limits<float>::infinity();
    expectNonFinite(invalidTint, resources, "effective_rgb");

    const auto mesh = findEntry(*manifest, "mesh.triangle");
    REQUIRE(mesh != nullptr);
    const auto meshIndex = static_cast<std::size_t>(mesh - manifest->entries.data());
    auto invalidBounds = resources;
    auto meshCopy = std::make_shared<cuexis::playback::PortableResource>(*invalidBounds[meshIndex]);
    std::get<cuexis::playback::PortableMesh>(meshCopy->value).boundsMin[0] =
        std::numeric_limits<float>::infinity();
    invalidBounds[meshIndex] = std::move(meshCopy);
    expectNonFinite(*snapshot, invalidBounds, "mesh_bounds");

    const auto opaque = findEntry(*manifest, "material.opaque");
    REQUIRE(opaque != nullptr);
    const auto opaqueIndex = static_cast<std::size_t>(opaque - manifest->entries.data());
    auto invalidBaseColor = resources;
    auto materialCopy =
        std::make_shared<cuexis::playback::PortableResource>(*invalidBaseColor[opaqueIndex]);
    std::get<cuexis::playback::PortableUnlitMaterial>(materialCopy->value).baseColor[3] =
        std::numeric_limits<float>::quiet_NaN();
    invalidBaseColor[opaqueIndex] = std::move(materialCopy);
    expectNonFinite(*snapshot, invalidBaseColor, "effective_alpha");

    auto invalidDepth = *snapshot;
    auto* depthObject = findObject(invalidDepth, objectId);
    REQUIRE(depthObject != nullptr);
    depthObject->worldMatrix[14] = std::numeric_limits<float>::max();
    expectNonFinite(invalidDepth, resources, "depth");
}

TEST_CASE(
    "Normalized presentation extraction checks every renderable reference and dependency shape",
    "[playback][presentation][frame-errors][branch-coverage]") {
    const auto fixture = loadFixture();
    cuexis::playback::PlaybackSession session;
    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto manifest = session.presentationManifest();
    REQUIRE(manifest.has_value());
    const auto resources = acquireResources(session, *manifest);
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto snapshot = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(snapshot.has_value());
    constexpr std::string_view objectId = "019f0000-0000-7abc-8def-000000000310";

    const auto expectMismatch = [&](const cuexis::playback::FrameSnapshot& candidate,
                                    const PresentationResourceManifest& candidateManifest,
                                    const std::vector<PortableResourcePtr>& candidateResources) {
        cuexis::playback::detail::NormalizedPresentationFrame normalized;
        const auto result = cuexis::playback::detail::normalizePresentationFrame(
            candidate, candidateManifest, candidateResources, normalized);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.presentation.frame.resource_mismatch");
        CHECK(normalized.opaque.empty());
        CHECK(normalized.transparent.empty());
    };

    SECTION("paired refs and AssetIds are mandatory") {
        auto materialOnly = *snapshot;
        auto* materialOnlyObject = findObject(materialOnly, objectId);
        REQUIRE(materialOnlyObject != nullptr);
        materialOnlyObject->mesh.reset();
        expectMismatch(materialOnly, *manifest, resources);

        auto wrongAssetId = *snapshot;
        auto* wrongAssetIdObject = findObject(wrongAssetId, objectId);
        REQUIRE(wrongAssetIdObject != nullptr);
        wrongAssetIdObject->materialAssetId = "material.other";
        expectMismatch(wrongAssetId, *manifest, resources);

        auto missingMaterial = *snapshot;
        auto* missingMaterialObject = findObject(missingMaterial, objectId);
        REQUIRE(missingMaterialObject != nullptr);
        REQUIRE(missingMaterialObject->material.has_value());
        missingMaterialObject->material->assetId = "material.missing";
        missingMaterialObject->materialAssetId = "material.missing";
        expectMismatch(missingMaterial, *manifest, resources);
    }

    SECTION("unlit texture dependency must agree with both manifest and resource table") {
        auto candidate = *snapshot;
        auto* candidateObject = findObject(candidate, objectId);
        REQUIRE(candidateObject != nullptr);
        const auto* blend = findEntry(*manifest, "material.blend");
        REQUIRE(blend != nullptr);
        candidateObject->material = blend->reference;
        candidateObject->materialAssetId = blend->reference.assetId;

        auto wrongDependencyManifest = *manifest;
        const auto blendIndex = static_cast<std::size_t>(blend - manifest->entries.data());
        wrongDependencyManifest.entries[blendIndex].dependencies.clear();
        expectMismatch(candidate, wrongDependencyManifest, resources);

        auto wrongTextureTypeManifest = *manifest;
        auto& dependency = wrongTextureTypeManifest.entries[blendIndex].dependencies.front();
        dependency.type = PresentationResourceType::Mesh;
        expectMismatch(candidate, wrongTextureTypeManifest, resources);

        auto wrongTextureValue = resources;
        const auto* texture = findEntry(*manifest, "texture.checker");
        REQUIRE(texture != nullptr);
        const auto textureIndex = static_cast<std::size_t>(texture - manifest->entries.data());
        auto textureCopy =
            std::make_shared<cuexis::playback::PortableResource>(*wrongTextureValue[textureIndex]);
        textureCopy->value = cuexis::playback::PortableMesh{};
        wrongTextureValue[textureIndex] = std::move(textureCopy);
        expectMismatch(candidate, *manifest, wrongTextureValue);
    }
}

TEST_CASE("Presentation resource table accepts all portable value variants when aligned",
          "[playback][presentation][frame-errors][branch-coverage]") {
    cuexis::playback::detail::PreparedPresentation presentation;
    const auto addResource = [&](PresentationResourceType type, std::string assetId,
                                 cuexis::playback::PortableResourceValue value) {
        PresentationResourceRef reference{type, std::move(assetId), {}};
        auto resource = std::make_shared<const cuexis::playback::PortableResource>(
            cuexis::playback::PortableResource{reference, std::move(value)});
        presentation.manifest.entries.push_back(
            cuexis::playback::PresentationManifestEntry{reference, 1, 1, {}});
        presentation.orderedResources.push_back(resource);
    };
    addResource(PresentationResourceType::Mesh, "a.mesh", cuexis::playback::PortableMesh{});
    addResource(PresentationResourceType::Texture2D, "b.texture",
                cuexis::playback::PortableTexture2D{});
    addResource(PresentationResourceType::UnlitMaterial, "c.material",
                cuexis::playback::PortableUnlitMaterial{});
    addResource(PresentationResourceType::Shader, "d.shader", cuexis::playback::PortableShader{});
    addResource(PresentationResourceType::ParameterizedMaterial, "e.parameterized",
                cuexis::playback::PortableParameterizedMaterial{});
    std::sort(presentation.manifest.entries.begin(), presentation.manifest.entries.end(),
              [](const auto& left, const auto& right) {
                  return std::tie(left.reference.assetId, left.reference.type) <
                         std::tie(right.reference.assetId, right.reference.type);
              });
    std::vector<PortableResourcePtr> resources;
    resources.reserve(presentation.orderedResources.size());
    for (const auto& resource : presentation.orderedResources) {
        resources.push_back(resource);
    }
    cuexis::playback::FrameSnapshot empty;
    cuexis::playback::detail::NormalizedPresentationFrame normalized;
    CHECK(cuexis::playback::detail::normalizePresentationFrame(empty, presentation.manifest,
                                                               resources, normalized)
              .has_value());
}

TEST_CASE("Presentation resource lookup remains exact across manifest sizes and resource types",
          "[playback][presentation][lookup][pb12]") {
    const auto lookup =
        [](const cuexis::playback::detail::PreparedPresentation& presentation,
           std::string_view assetId,
           PresentationResourceType type) -> const cuexis::playback::PortableResourcePtr* {
        const auto found = presentation.resources.find(
            cuexis::playback::detail::PresentationResourceKeyView{assetId, type});
        return found == presentation.resources.end() ? nullptr : &found->second;
    };
    for (const auto resourceCount : {std::size_t{1}, std::size_t{2}, std::size_t{65'536}}) {
        auto presentation = syntheticPresentation(resourceCount);
        const auto firstId = syntheticAssetId(0);
        const auto lastId = syntheticAssetId(resourceCount - 1U);
        const auto first = lookup(presentation, firstId, PresentationResourceType::Mesh);
        REQUIRE(first != nullptr);
        CHECK((*first)->reference.assetId == firstId);
        const auto last =
            lookup(presentation, std::string_view{lastId}, PresentationResourceType::Mesh);
        REQUIRE(last != nullptr);
        CHECK((*last)->reference.assetId == lastId);
        const auto direct =
            presentation.resources.find(cuexis::playback::detail::PresentationResourceKeyView{
                lastId, PresentationResourceType::Mesh});
        REQUIRE(direct != presentation.resources.end());
        CHECK(direct->second->reference.assetId == lastId);

        const auto missing = lookup(presentation, std::string_view{"synthetic.missing"},
                                    PresentationResourceType::Mesh);
        CHECK(missing == nullptr);

        auto wrongType = (*first)->reference;
        wrongType.type = PresentationResourceType::Texture2D;
        const auto wrongTypeFound =
            presentation.resources.find(cuexis::playback::detail::PresentationResourceKeyView{
                wrongType.assetId, wrongType.type});
        CHECK(wrongTypeFound == presentation.resources.end());
        auto wrongIdentity = (*first)->reference;
        wrongIdentity.identity.sha256[0] ^= 0xFFU;
        const auto wrongIdentityFound =
            presentation.resources.find(cuexis::playback::detail::PresentationResourceKeyView{
                wrongIdentity.assetId, wrongIdentity.type});
        CHECK(wrongIdentityFound != presentation.resources.end());
        CHECK(wrongIdentityFound->second->reference != wrongIdentity);
    }
}

TEST_CASE("Presentation lookup distinguishes same AssetId across resource types",
          "[playback][presentation][lookup][pb12]") {
    cuexis::playback::detail::PreparedPresentation presentation;
    const std::string assetId = "shared.asset";
    PresentationResourceRef meshReference{PresentationResourceType::Mesh, assetId, {}};
    PresentationResourceRef textureReference{PresentationResourceType::Texture2D, assetId, {}};
    auto mesh = std::make_shared<const cuexis::playback::PortableResource>(
        cuexis::playback::PortableResource{meshReference, cuexis::playback::PortableMesh{}});
    auto texture = std::make_shared<const cuexis::playback::PortableResource>(
        cuexis::playback::PortableResource{textureReference,
                                           cuexis::playback::PortableTexture2D{}});
    presentation.resources.emplace(
        cuexis::playback::detail::PresentationResourceKey{assetId, meshReference.type}, mesh);
    presentation.resources.emplace(
        cuexis::playback::detail::PresentationResourceKey{assetId, textureReference.type}, texture);

    const auto foundMesh =
        presentation.resources.find(cuexis::playback::detail::PresentationResourceKeyView{
            assetId, PresentationResourceType::Mesh});
    const auto foundTexture =
        presentation.resources.find(cuexis::playback::detail::PresentationResourceKeyView{
            assetId, PresentationResourceType::Texture2D});
    REQUIRE(foundMesh != presentation.resources.end());
    REQUIRE(foundTexture != presentation.resources.end());
    CHECK(foundMesh->second->reference.type == PresentationResourceType::Mesh);
    CHECK(foundTexture->second->reference.type == PresentationResourceType::Texture2D);
}

TEST_CASE("Duplicate presentation manifest keys are rejected by table validation",
          "[playback][presentation][lookup][pb12]") {
    const auto fixture = loadFixture();
    cuexis::playback::PlaybackSession session;
    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto manifest = session.presentationManifest();
    REQUIRE(manifest.has_value());
    auto duplicate = *manifest;
    REQUIRE(duplicate.entries.size() >= 2);
    const auto duplicateEntry = duplicate.entries.front();
    const auto originalEntries = duplicate.entries;
    duplicate.entries.clear();
    duplicate.entries.reserve(originalEntries.size() + 1);
    duplicate.entries.push_back(duplicateEntry);
    duplicate.entries.push_back(duplicateEntry);
    for (std::size_t index = 1; index < originalEntries.size(); ++index) {
        duplicate.entries.push_back(originalEntries[index]);
    }
    auto resources = acquireResources(session, *manifest);
    REQUIRE(resources.size() >= 2);
    const auto originalResources = resources;
    const auto duplicateResource = resources.front();
    resources.clear();
    resources.reserve(originalResources.size() + 1);
    resources.push_back(duplicateResource);
    resources.push_back(duplicateResource);
    for (std::size_t index = 1; index < originalResources.size(); ++index) {
        resources.push_back(originalResources[index]);
    }
    cuexis::playback::FrameSnapshot empty;
    cuexis::playback::detail::NormalizedPresentationFrame normalized;
    const auto result = cuexis::playback::detail::normalizePresentationFrame(empty, duplicate,
                                                                             resources, normalized);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "playback.presentation.frame.resource_mismatch");
}

TEST_CASE("Presentation extraction rejects an AssetId reused with a different resource type",
          "[playback][presentation][lookup][pb12]") {
    auto fixture = loadFixture();
    constexpr std::string_view original =
        R"json("material": { "domain": "asset", "id": "material.opaque" })json";
    constexpr std::string_view replacement =
        R"json("material": { "domain": "asset", "id": "mesh.triangle" })json";
    const auto position = fixture.chartJson.find(original);
    REQUIRE(position != std::string::npos);
    fixture.chartJson.replace(position, original.size(), replacement);

    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    const auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(prepared.has_value());
    CHECK(prepared.error().code() == "playback.presentation.reference.invalid");
}

TEST_CASE("Presentation manifest order and semantic identity are stable under asset input reorder",
          "[playback][presentation][determinism][pb12]") {
    const auto fixture = loadFixture();
    auto firstSource = typedSource(ProviderKind::Memory, fixture);
    auto secondSource = reorderedMemorySource(fixture);
    REQUIRE(firstSource.has_value());
    REQUIRE(secondSource.has_value());

    cuexis::playback::PlaybackSession firstSession;
    cuexis::playback::PlaybackSession secondSession;
    auto firstPrepared = firstSession.prepareLoad(std::move(*firstSource),
                                                  cuexis::playback::PlaybackMode::ChartClock);
    auto secondPrepared = secondSession.prepareLoad(std::move(*secondSource),
                                                    cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(firstPrepared.has_value());
    REQUIRE(secondPrepared.has_value());
    REQUIRE(firstPrepared->presentationManifest() != nullptr);
    REQUIRE(secondPrepared->presentationManifest() != nullptr);
    CHECK(manifestsEqual(*firstPrepared->presentationManifest(),
                         *secondPrepared->presentationManifest()));
    REQUIRE(firstPrepared->semanticIdentity().has_value());
    REQUIRE(secondPrepared->semanticIdentity().has_value());
    CHECK(*firstPrepared->semanticIdentity() == *secondPrepared->semanticIdentity());
}
