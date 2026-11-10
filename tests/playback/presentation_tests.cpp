#include "presentation_fixture.hpp"

using namespace presentation_fixture;

TEST_CASE("Portable presentation manifest is provider-independent and owning",
          "[playback][presentation][providers]") {
    const auto fixture = loadFixture();
    std::optional<PresentationResourceManifest> baseline;
    std::vector<PortableResourcePtr> heldAfterSession;

    for (const auto provider :
         {ProviderKind::Filesystem, ProviderKind::Memory, ProviderKind::Host}) {
        auto readCount = std::make_shared<std::size_t>(0);
        cuexis::playback::PlaybackSession session;
        auto source = typedSource(provider, fixture, {"texture.checker"}, readCount);
        REQUIRE(source.has_value());
        auto prepared =
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
        REQUIRE(prepared.has_value());
        REQUIRE(prepared->presentationCandidateToken().has_value());
        const auto* candidateManifest = prepared->presentationManifest();
        REQUIRE(candidateManifest != nullptr);
        REQUIRE(candidateManifest->entries.size() == 4);
        CHECK(candidateManifest->totalEncodedBytes == 295);
        CHECK(candidateManifest->totalDecodedBytes == 601);
        CHECK(candidateManifest->entries[0].reference.assetId == "material.blend");
        CHECK(candidateManifest->entries[1].reference.assetId == "material.opaque");
        CHECK(candidateManifest->entries[2].reference.assetId == "mesh.triangle");
        CHECK(candidateManifest->entries[3].reference.assetId == "texture.checker");

        CHECK(identityHex(candidateManifest->entries[0].reference) ==
              "8e26294412ec07e8737fde2282cc7df98dbcb83dcc7fca9d69592b45b49c3576");
        CHECK(identityHex(candidateManifest->entries[1].reference) ==
              "256ef5a13ed84387eb66c1ee3edb1726489a59345a3baf2a00c827f73dab15c0");
        CHECK(identityHex(candidateManifest->entries[2].reference) ==
              "cd9811f75e8aa95568792bbe05f17ce229a4467d4fcd26ef482f98d093f0e976");
        CHECK(identityHex(candidateManifest->entries[3].reference) ==
              "e9f3790936120626b8adbd45dd86a3a0e1aa2fb9972416ffe63279cafbba4cab");

        const auto* blendEntry = findEntry(*candidateManifest, "material.blend");
        REQUIRE(blendEntry != nullptr);
        REQUIRE(blendEntry->dependencies.size() == 1);
        CHECK(blendEntry->dependencies[0] == candidateManifest->entries[3].reference);

        auto mesh = prepared->acquirePresentationResource(candidateManifest->entries[2].reference);
        REQUIRE(mesh.has_value());
        auto sameMesh =
            prepared->acquirePresentationResource(candidateManifest->entries[2].reference);
        REQUIRE(sameMesh.has_value());
        CHECK(mesh->get() == sameMesh->get());
        const auto* meshValue = std::get_if<cuexis::playback::PortableMesh>(&(*mesh)->value);
        REQUIRE(meshValue != nullptr);
        REQUIRE(meshValue->positions.size() == 9);
        CHECK(meshValue->boundsMin[0] == Catch::Approx(-0.5F));
        CHECK(meshValue->boundsMax[1] == Catch::Approx(0.5F));

        const auto candidateCopy = *candidateManifest;
        REQUIRE(session.commit(std::move(*prepared)).has_value());
        auto activeManifest = session.presentationManifest();
        REQUIRE(activeManifest.has_value());
        CHECK(manifestsEqual(candidateCopy, *activeManifest));
        if (!baseline) {
            baseline = *activeManifest;
        } else {
            CHECK(manifestsEqual(*baseline, *activeManifest));
        }
        auto activeMesh = session.acquirePresentationResource(activeManifest->entries[2].reference);
        REQUIRE(activeMesh.has_value());
        CHECK(activeMesh->get() == mesh->get());

        const auto readsAfterPrepare = *readCount;
        REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
        REQUIRE(session.extractFrame({.width = 1280, .height = 720}).has_value());
        CHECK(*readCount == readsAfterPrepare);

        heldAfterSession.push_back(*mesh);
        REQUIRE(session.unload().has_value());
        CHECK_FALSE(session.presentationManifest().has_value());
        CHECK(std::get<cuexis::playback::PortableMesh>(heldAfterSession.back()->value)
                  .indices.size() == 3);
    }

    REQUIRE(heldAfterSession.size() == 3);
    for (const auto& resource : heldAfterSession) {
        CHECK(resource->reference.assetId == "mesh.triangle");
        CHECK(std::get<cuexis::playback::PortableMesh>(resource->value).positions[0] ==
              Catch::Approx(-0.5F));
    }
}

TEST_CASE("Portable presentation candidate tokens identify distinct preparations",
          "[playback][presentation][candidate]") {
    const auto fixture = loadFixture();
    cuexis::playback::PlaybackSession session;
    auto firstSource = typedSource(ProviderKind::Memory, fixture);
    auto secondSource = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(firstSource.has_value());
    REQUIRE(secondSource.has_value());
    auto first =
        session.prepareLoad(std::move(*firstSource), cuexis::playback::PlaybackMode::ChartClock);
    auto second =
        session.prepareLoad(std::move(*secondSource), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    const auto firstToken = first->presentationCandidateToken();
    const auto secondToken = second->presentationCandidateToken();
    REQUIRE(firstToken.has_value());
    REQUIRE(secondToken.has_value());
    CHECK(*firstToken != *secondToken);

    auto moved = std::move(*first);
    CHECK_FALSE(first->presentationCandidateToken().has_value());
    REQUIRE(moved.presentationCandidateToken().has_value());
    CHECK(*moved.presentationCandidateToken() == *firstToken);
    REQUIRE(session.commit(std::move(*second)).has_value());
    const auto stale = session.commit(std::move(moved));
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code() == "playback.prepared.stale");
}

TEST_CASE(
    "Portable presentation rejects malformed payloads and dependency mismatches before commit",
    "[playback][presentation][validation]") {
    const auto base = loadFixture();

    SECTION("invalid mesh index") {
        auto fixture = base;
        writeU32(fixture.blobs.at("meshes/triangle.mesh.bin"), 100, 3);
        CHECK(rejectionCode(std::move(fixture)) == "playback.presentation.mesh.index_out_of_range");
    }
    SECTION("non-finite mesh value") {
        auto fixture = base;
        writeU32(fixture.blobs.at("meshes/triangle.mesh.bin"), 40, 0x7FC00000U);
        CHECK(rejectionCode(std::move(fixture)) == "playback.presentation.mesh.value_invalid");
    }
    SECTION("truncated texture") {
        auto fixture = base;
        fixture.blobs.at("textures/checker.texture.bin").pop_back();
        CHECK(rejectionCode(std::move(fixture)) == "playback.presentation.payload.truncated");
    }
    SECTION("payload type mismatch") {
        auto fixture = base;
        writeU32(fixture.blobs.at("meshes/triangle.mesh.bin"), 8, 2);
        CHECK(rejectionCode(std::move(fixture)) == "playback.presentation.payload.type_mismatch");
    }
    SECTION("payload magic mismatch") {
        auto fixture = base;
        fixture.blobs.at("meshes/triangle.mesh.bin")[0] = std::byte{'B'};
        CHECK(rejectionCode(std::move(fixture)) == "playback.presentation.payload.magic_invalid");
    }
    SECTION("payload version mismatch") {
        auto fixture = base;
        writeU32(fixture.blobs.at("meshes/triangle.mesh.bin"), 12, 2);
        CHECK(rejectionCode(std::move(fixture)) ==
              "playback.presentation.payload.version_unsupported");
    }
    SECTION("decoded texture budget") {
        auto fixture = base;
        writeU32(fixture.blobs.at("textures/checker.texture.bin"), 24, 8192);
        writeU32(fixture.blobs.at("textures/checker.texture.bin"), 28, 8192);
        CHECK(rejectionCode(std::move(fixture)) ==
              "playback.presentation.resource.budget_exceeded");
    }
    SECTION("encoded provider budget") {
        auto blobs = std::make_shared<decltype(base.blobs)>(base.blobs);
        auto provider = cuexis::content::HostContentProvider::create(
            [blobs = std::move(blobs)](const cuexis::content::ContentRequest& request)
                -> cuexis::core::Result<cuexis::content::ContentBlob> {
                if (request.source == "meshes/triangle.mesh.bin") {
                    return cuexis::core::unexpected(cuexis::core::Error{
                        "content.provider.too_large", "Fixture simulates an oversized blob"});
                }
                const auto found = blobs->find(request.source);
                if (found == blobs->end()) {
                    return cuexis::core::unexpected(cuexis::core::Error{
                        "test.content.source_missing", "Requested fixture source does not exist"});
                }
                return cuexis::content::ContentBlob{found->second, 11};
            });
        REQUIRE(provider.has_value());
        auto source = PlaybackSource::fromTypedProject({.sourceId = "stage3-encoded-budget",
                                                        .chartJson = base.chartJson,
                                                        .assets = descriptors()},
                                                       std::move(*provider));
        REQUIRE(source.has_value());

        cuexis::playback::PlaybackSession session;
        auto prepared =
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
        REQUIRE_FALSE(prepared.has_value());
        CHECK(prepared.error().code() == "playback.presentation.resource.budget_exceeded");
        CHECK(errorContextValue(prepared.error(), "asset_id") == "mesh.triangle");
        CHECK(errorContextValue(prepared.error(), "resource_type") == "mesh");
        CHECK(errorContextValue(prepared.error(), "limit") == "67108864");
        CHECK(errorContextValue(prepared.error(), "actual") == "greater_than_limit");
    }
    SECTION("material dependency mismatch") {
        CHECK(rejectionCode(base, {}) == "playback.presentation.dependency.mismatch");
    }
    SECTION("dependency cycle") {
        std::vector<cuexis::content::MemoryContentEntry> entries;
        for (const auto& [source, bytes] : base.blobs) {
            entries.push_back({.rootId = "main", .source = source, .bytes = bytes});
        }
        auto provider = cuexis::content::MemoryContentProvider::create(std::move(entries));
        REQUIRE(provider.has_value());
        auto assets = descriptors();
        assets[0].dependencies = {"material.blend"};
        auto source = PlaybackSource::fromTypedProject(
            {.sourceId = "stage3-cycle", .chartJson = base.chartJson, .assets = std::move(assets)},
            std::move(*provider));
        REQUIRE_FALSE(source.has_value());
        CHECK(source.error().code() == "playback.presentation.dependency.cycle");
        CHECK(errorContextValue(source.error(), "cycle").has_value());
        REQUIRE(source.error().cause() != nullptr);
        CHECK(source.error().cause()->code() == "assets.database.dependency_cycle");
    }
}

TEST_CASE("Portable presentation validates envelope, mesh, and texture structural boundaries",
          "[playback][presentation][payload][branch-coverage]") {
    const auto base = loadFixture();

    const auto meshCode = [&](auto mutate) {
        auto fixture = base;
        mutate(fixture.blobs.at("meshes/triangle.mesh.bin"));
        return rejectionCode(std::move(fixture));
    };
    const auto textureCode = [&](auto mutate) {
        auto fixture = base;
        mutate(fixture.blobs.at("textures/checker.texture.bin"));
        return rejectionCode(std::move(fixture));
    };

    SECTION("envelope distinguishes a short body, a short declaration, and trailing bytes") {
        CHECK(meshCode([](auto& bytes) { bytes.resize(23); }) ==
              "playback.presentation.payload.truncated");
        CHECK(meshCode([](auto& bytes) { writeU64(bytes, 16, bytes.size() + 1U); }) ==
              "playback.presentation.payload.truncated");
        CHECK(meshCode([](auto& bytes) { writeU64(bytes, 16, bytes.size() - 1U); }) ==
              "playback.presentation.payload.size_mismatch");
        CHECK(meshCode([](auto& bytes) {
                  bytes.push_back(std::byte{0});
                  writeU64(bytes, 16, bytes.size());
              }) == "playback.presentation.payload.size_mismatch");
    }

    SECTION("mesh fixed fields reject invalid counts and reserved bits") {
        CHECK(meshCode([](auto& bytes) { writeU32(bytes, 24, 0); }) ==
              "playback.presentation.mesh.vertex_count_invalid");
        CHECK(meshCode([](auto& bytes) { writeU32(bytes, 28, 2); }) ==
              "playback.presentation.mesh.index_count_invalid");
        CHECK(meshCode([](auto& bytes) { writeU32(bytes, 32, 2); }) ==
              "playback.presentation.payload.reserved_nonzero");
        CHECK(meshCode([](auto& bytes) { writeU32(bytes, 36, 1); }) ==
              "playback.presentation.payload.reserved_nonzero");
    }

    SECTION("mesh rejects repeated and zero-area triangles") {
        CHECK(meshCode([](auto& bytes) { writeU32(bytes, 100, 1); }) ==
              "playback.presentation.mesh.degenerate_triangle");
        CHECK(meshCode([](auto& bytes) {
                  writeU32(bytes, 40, 0);
                  writeU32(bytes, 44, 0);
                  writeU32(bytes, 48, 0);
                  writeU32(bytes, 52, 0);
                  writeU32(bytes, 56, 0);
                  writeU32(bytes, 60, 0);
              }) == "playback.presentation.mesh.degenerate_triangle");
    }

    SECTION("texture fixed fields reject invalid metadata and encoded size") {
        CHECK(textureCode([](auto& bytes) { writeU32(bytes, 24, 0); }) ==
              "playback.presentation.texture.dimension_invalid");
        CHECK(textureCode([](auto& bytes) { writeU32(bytes, 32, 3); }) ==
              "playback.presentation.texture.color_space_unsupported");
        CHECK(textureCode([](auto& bytes) { writeU32(bytes, 36, 1); }) ==
              "playback.presentation.payload.reserved_nonzero");
        CHECK(textureCode([](auto& bytes) {
                  bytes.push_back(std::byte{0});
                  writeU64(bytes, 16, bytes.size());
              }) == "playback.presentation.texture.pixel_size_invalid");
    }
}

TEST_CASE("Portable presentation resource closure failures retain the active manifest and frame",
          "[playback][presentation][closure][rollback][branch-coverage]") {
    const auto base = loadFixture();

    const auto failedReload = [&](Stage3Fixture replacement,
                                  std::vector<PlaybackAssetDescriptor> replacementAssets,
                                  std::string_view expectedOuter,
                                  std::string_view expectedDiagnostic) {
        cuexis::playback::PlaybackSession session;
        auto initial = typedSource(ProviderKind::Memory, base);
        REQUIRE(initial.has_value());
        REQUIRE(session.load(std::move(*initial), cuexis::playback::PlaybackMode::ChartClock)
                    .has_value());
        REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
        const auto manifestBefore = session.presentationManifest();
        const auto frameBefore = session.extractFrame({.width = 640, .height = 480});
        REQUIRE(manifestBefore.has_value());
        REQUIRE(frameBefore.has_value());

        auto source = memorySourceWithAssets(replacement, std::move(replacementAssets));
        REQUIRE(source.has_value());
        const auto prepared = session.prepareReload(std::move(*source), {.chartTimeMs = 0.0},
                                                    cuexis::playback::ReloadPolicy::KeepChartTime);
        REQUIRE_FALSE(prepared.has_value());
        const auto operationDiagnostics = session.lastOperationDiagnostics();
        REQUIRE(operationDiagnostics.has_value());
        const auto diagnosticCode = errorContextValue(prepared.error(), "diagnostic_code");
        INFO("outer=" << prepared.error().code()
                      << "; diagnostic_code=" << (diagnosticCode ? *diagnosticCode : "<none>")
                      << "; diagnostics=" << diagnosticCodes(*operationDiagnostics));
        CHECK(prepared.error().code() == expectedOuter);
        CHECK(std::any_of(operationDiagnostics->items().begin(),
                          operationDiagnostics->items().end(),
                          [&](const auto& item) { return item.code() == expectedDiagnostic; }));

        const auto manifestAfter = session.presentationManifest();
        const auto frameAfter = session.extractFrame({.width = 640, .height = 480});
        REQUIRE(manifestAfter.has_value());
        REQUIRE(frameAfter.has_value());
        CHECK(manifestsEqual(*manifestBefore, *manifestAfter));
        REQUIRE(frameAfter->objects.size() == frameBefore->objects.size());
        CHECK(frameAfter->objects[0].materialAssetId == frameBefore->objects[0].materialAssetId);
    };

    SECTION(
        "missing and incompatible indexed resources fail Runtime preparation before publication") {
        auto missing = descriptors();
        missing.erase(
            std::remove_if(missing.begin(), missing.end(),
                           [](const auto& asset) { return asset.id == "material.blend"; }),
            missing.end());
        auto missingSource = memorySourceWithAssets(base, std::move(missing));
        REQUIRE(missingSource.has_value());
        cuexis::playback::PlaybackSession missingSession;
        const auto missingPrepared = missingSession.prepareLoad(
            std::move(*missingSource), cuexis::playback::PlaybackMode::ChartClock);
        REQUIRE_FALSE(missingPrepared.has_value());
        const auto missingDiagnostics = missingSession.lastOperationDiagnostics();
        REQUIRE(missingDiagnostics.has_value());
        const auto missingDiagnosticCode =
            errorContextValue(missingPrepared.error(), "diagnostic_code");
        INFO("outer=" << missingPrepared.error().code() << "; diagnostic_code="
                      << (missingDiagnosticCode ? *missingDiagnosticCode : "<none>")
                      << "; diagnostics=" << diagnosticCodes(*missingDiagnostics));
        CHECK(missingPrepared.error().code() == "playback.session.prepare_failed");
        CHECK(std::any_of(
            missingDiagnostics->items().begin(), missingDiagnostics->items().end(),
            [](const auto& item) { return item.code() == "assets.resource.required_failed"; }));

        auto incompatible = descriptors();
        const auto material =
            std::find_if(incompatible.begin(), incompatible.end(),
                         [](const auto& asset) { return asset.id == "material.blend"; });
        REQUIRE(material != incompatible.end());
        material->type = PlaybackAssetType::Mesh;
        auto incompatibleSource = memorySourceWithAssets(base, std::move(incompatible));
        REQUIRE(incompatibleSource.has_value());
        cuexis::playback::PlaybackSession incompatibleSession;
        const auto incompatiblePrepared = incompatibleSession.prepareLoad(
            std::move(*incompatibleSource), cuexis::playback::PlaybackMode::ChartClock);
        REQUIRE_FALSE(incompatiblePrepared.has_value());
        const auto incompatibleDiagnostics = incompatibleSession.lastOperationDiagnostics();
        REQUIRE(incompatibleDiagnostics.has_value());
        const auto incompatibleDiagnosticCode =
            errorContextValue(incompatiblePrepared.error(), "diagnostic_code");
        INFO("outer=" << incompatiblePrepared.error().code() << "; diagnostic_code="
                      << (incompatibleDiagnosticCode ? *incompatibleDiagnosticCode : "<none>")
                      << "; diagnostics=" << diagnosticCodes(*incompatibleDiagnostics));
        CHECK(incompatiblePrepared.error().code() == "playback.session.prepare_failed");
        CHECK(std::any_of(
            incompatibleDiagnostics->items().begin(), incompatibleDiagnostics->items().end(),
            [](const auto& item) { return item.code() == "assets.resource.required_failed"; }));
    }

    SECTION("a missing content blob and non-leaf Mesh are both stable preparation failures") {
        auto missingBlob = base;
        missingBlob.blobs.erase("meshes/triangle.mesh.bin");
        failedReload(std::move(missingBlob), descriptors(),
                     "playback.presentation.resource.missing",
                     "playback.presentation.resource.missing");

        auto meshDependency = descriptors();
        const auto mesh =
            std::find_if(meshDependency.begin(), meshDependency.end(),
                         [](const auto& asset) { return asset.id == "mesh.triangle"; });
        REQUIRE(mesh != meshDependency.end());
        mesh->dependencies = {"texture.checker"};
        failedReload(base, std::move(meshDependency), "playback.presentation.dependency.mismatch",
                     "playback.presentation.dependency.mismatch");
    }

    SECTION("material texture routing verifies the Asset Index type and Texture leaf contract") {
        auto wrongTextureType = descriptors();
        const auto texture =
            std::find_if(wrongTextureType.begin(), wrongTextureType.end(),
                         [](const auto& asset) { return asset.id == "texture.checker"; });
        REQUIRE(texture != wrongTextureType.end());
        texture->type = PlaybackAssetType::Mesh;
        auto wrongTypeSource = memorySourceWithAssets(base, std::move(wrongTextureType));
        REQUIRE(wrongTypeSource.has_value());
        cuexis::playback::PlaybackSession wrongTypeSession;
        const auto wrongTypePrepared = wrongTypeSession.prepareLoad(
            std::move(*wrongTypeSource), cuexis::playback::PlaybackMode::ChartClock);
        REQUIRE_FALSE(wrongTypePrepared.has_value());
        CHECK(wrongTypePrepared.error().code() == "playback.presentation.dependency.mismatch");

        auto textureDependency = descriptors();
        const auto textureWithDependency =
            std::find_if(textureDependency.begin(), textureDependency.end(),
                         [](const auto& asset) { return asset.id == "texture.checker"; });
        REQUIRE(textureWithDependency != textureDependency.end());
        textureWithDependency->dependencies = {"mesh.triangle"};
        auto textureDependencySource = memorySourceWithAssets(base, std::move(textureDependency));
        REQUIRE(textureDependencySource.has_value());
        cuexis::playback::PlaybackSession textureDependencySession;
        const auto textureDependencyPrepared = textureDependencySession.prepareLoad(
            std::move(*textureDependencySource), cuexis::playback::PlaybackMode::ChartClock);
        REQUIRE_FALSE(textureDependencyPrepared.has_value());
        CHECK(textureDependencyPrepared.error().code() ==
              "playback.presentation.dependency.mismatch");
    }
}

TEST_CASE("Portable manifest excludes Material Steps that cannot affect a Renderable object",
          "[playback][presentation][closure]") {
    auto fixture = loadFixture();
    fixture.chartJson = addIrrelevantMaterialBehaviors(std::move(fixture.chartJson));
    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());

    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    if (!prepared) {
        std::ostringstream details;
        details << prepared.error().code() << ": " << prepared.error().message();
        const auto diagnostics = session.lastOperationDiagnostics();
        if (diagnostics) {
            for (const auto& item : diagnostics->items()) {
                details << " | " << item.code() << ": " << item.message();
            }
        }
        FAIL(details.str());
    }
    REQUIRE(prepared.has_value());
    const auto* manifest = prepared->presentationManifest();
    REQUIRE(manifest != nullptr);
    CHECK(manifest->entries.size() == 4);
    CHECK(findEntry(*manifest, "material.unbound.missing") == nullptr);
    CHECK(findEntry(*manifest, "material.camera.missing") == nullptr);

    REQUIRE(session.commit(std::move(*prepared)).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto frame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(frame.has_value());
    REQUIRE(frame->objects.size() == 2);
    CHECK(frame->objects.front().materialAssetId == "material.opaque");
}

TEST_CASE("Render Material behavior bound to a non-Renderable object fails before presentation",
          "[playback][presentation][closure]") {
    auto fixture = loadFixture();
    fixture.chartJson = addIrrelevantMaterialBehaviors(std::move(fixture.chartJson), true);
    auto source = typedSource(ProviderKind::Memory, fixture);
    REQUIRE(source.has_value());

    cuexis::playback::PlaybackSession session;
    const auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(prepared.has_value());
    CHECK(prepared.error().code() == "playback.session.prepare_failed");
    const auto diagnostics = session.lastOperationDiagnostics();
    REQUIRE(diagnostics.has_value());
    CHECK(
        std::any_of(diagnostics->items().begin(), diagnostics->items().end(), [](const auto& item) {
            return item.code() == "runtime.chart.behavior_renderable_missing";
        }));
}

TEST_CASE("Portable presentation reload is atomic and propagates semantic identities",
          "[playback][presentation][reload]") {
    const auto base = loadFixture();
    cuexis::playback::PlaybackSession session;
    auto initialSource = typedSource(ProviderKind::Memory, base);
    REQUIRE(initialSource.has_value());
    REQUIRE(session.load(std::move(*initialSource), cuexis::playback::PlaybackMode::ChartClock)
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    auto oldManifest = session.presentationManifest();
    REQUIRE(oldManifest.has_value());
    const auto* oldTextureEntry = findEntry(*oldManifest, "texture.checker");
    const auto* oldBlendEntry = findEntry(*oldManifest, "material.blend");
    const auto* oldMeshEntry = findEntry(*oldManifest, "mesh.triangle");
    const auto* oldOpaqueEntry = findEntry(*oldManifest, "material.opaque");
    REQUIRE(oldTextureEntry != nullptr);
    REQUIRE(oldBlendEntry != nullptr);
    REQUIRE(oldMeshEntry != nullptr);
    REQUIRE(oldOpaqueEntry != nullptr);
    auto oldTexture = session.acquirePresentationResource(oldTextureEntry->reference);
    REQUIRE(oldTexture.has_value());
    const auto oldFrame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(oldFrame.has_value());

    auto invalid = base;
    writeU32(invalid.blobs.at("meshes/triangle.mesh.bin"), 40, 0x7F800000U);
    auto invalidSource = typedSource(ProviderKind::Memory, invalid);
    REQUIRE(invalidSource.has_value());
    const auto failed = session.prepareReload(std::move(*invalidSource), {.chartTimeMs = 0.0},
                                              cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "playback.presentation.mesh.value_invalid");
    auto afterFailure = session.presentationManifest();
    REQUIRE(afterFailure.has_value());
    CHECK(manifestsEqual(*oldManifest, *afterFailure));
    auto afterFailureFrame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(afterFailureFrame.has_value());
    REQUIRE(afterFailureFrame->objects.size() == oldFrame->objects.size());
    CHECK(afterFailureFrame->objects[0].materialAssetId == oldFrame->objects[0].materialAssetId);

    auto changed = base;
    changed.blobs.at("textures/checker.texture.bin").back() = std::byte{0};
    auto changedSource = typedSource(ProviderKind::Memory, changed);
    REQUIRE(changedSource.has_value());
    auto replacement = session.prepareReload(std::move(*changedSource), {.chartTimeMs = 0.0},
                                             cuexis::playback::ReloadPolicy::KeepChartTime);
    REQUIRE(replacement.has_value());
    const auto* candidateManifest = replacement->presentationManifest();
    REQUIRE(candidateManifest != nullptr);
    const auto* newTextureEntry = findEntry(*candidateManifest, "texture.checker");
    const auto* newBlendEntry = findEntry(*candidateManifest, "material.blend");
    const auto* newMeshEntry = findEntry(*candidateManifest, "mesh.triangle");
    const auto* newOpaqueEntry = findEntry(*candidateManifest, "material.opaque");
    REQUIRE(newTextureEntry != nullptr);
    REQUIRE(newBlendEntry != nullptr);
    REQUIRE(newMeshEntry != nullptr);
    REQUIRE(newOpaqueEntry != nullptr);
    CHECK(newTextureEntry->reference.identity != oldTextureEntry->reference.identity);
    CHECK(newBlendEntry->reference.identity != oldBlendEntry->reference.identity);
    CHECK(newMeshEntry->reference.identity == oldMeshEntry->reference.identity);
    CHECK(newOpaqueEntry->reference.identity == oldOpaqueEntry->reference.identity);
    CHECK(manifestsEqual(*oldManifest, *session.presentationManifest()));
    const auto newTextureIdentity = newTextureEntry->reference.identity;

    REQUIRE(session.commit(std::move(*replacement)).has_value());
    auto active = session.presentationManifest();
    REQUIRE(active.has_value());
    const auto* activeTexture = findEntry(*active, "texture.checker");
    REQUIRE(activeTexture != nullptr);
    CHECK(activeTexture->reference.identity == newTextureIdentity);
    REQUIRE(session.unload().has_value());
    const auto& oldPixels =
        std::get<cuexis::playback::PortableTexture2D>((*oldTexture)->value).pixelsRgba8;
    REQUIRE(oldPixels.size() == 16);
    CHECK(oldPixels.back() == std::byte{255});
}

TEST_CASE("Legacy opaque Stage 1B resources remain loadable without a portable manifest",
          "[playback][presentation][legacy]") {
    const auto root =
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage1b_project";
    auto source = PlaybackSource::fromFilesystemProject(root);
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    CHECK(prepared->presentationManifest() == nullptr);
    CHECK_FALSE(prepared->presentationCandidateToken().has_value());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    CHECK_FALSE(session.presentationManifest().has_value());
}

TEST_CASE("Portable snapshots bind refs and preserve behavior, stop, seek, and lifetime semantics",
          "[playback][presentation][frame]") {
    constexpr std::string_view renderableId = "019f0000-0000-7abc-8def-000000000310";
    const auto fixture = loadFixture();
    auto readCount = std::make_shared<std::size_t>(0);
    std::optional<cuexis::playback::FrameSnapshot> heldAfterSession;

    {
        cuexis::playback::PlaybackSession session;
        auto source = typedSource(ProviderKind::Host, fixture, {"texture.checker"}, readCount);
        REQUIRE(source.has_value());
        REQUIRE(session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock)
                    .has_value());
        auto manifest = session.presentationManifest();
        REQUIRE(manifest.has_value());
        const auto resources = acquireResources(session, *manifest);
        const auto readsAfterPrepare = *readCount;
        const auto* meshEntry = findEntry(*manifest, "mesh.triangle");
        const auto* opaqueEntry = findEntry(*manifest, "material.opaque");
        const auto* blendEntry = findEntry(*manifest, "material.blend");
        REQUIRE(meshEntry != nullptr);
        REQUIRE(opaqueEntry != nullptr);
        REQUIRE(blendEntry != nullptr);

        REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
        auto opaqueFrame = session.extractFrame({.width = 1280, .height = 720});
        REQUIRE(opaqueFrame.has_value());
        const auto* opaqueObject = findObject(*opaqueFrame, renderableId);
        REQUIRE(opaqueObject != nullptr);
        REQUIRE(opaqueObject->mesh.has_value());
        REQUIRE(opaqueObject->material.has_value());
        CHECK(*opaqueObject->mesh == meshEntry->reference);
        CHECK(*opaqueObject->material == opaqueEntry->reference);
        CHECK(opaqueObject->materialAssetId == opaqueObject->material->assetId);
        CHECK(opaqueObject->materialOpacity == Catch::Approx(1.0));
        CHECK(opaqueObject->materialTint[0] == Catch::Approx(1.0F));

        cuexis::playback::detail::NormalizedPresentationFrame normalized;
        REQUIRE(cuexis::playback::detail::normalizePresentationFrame(*opaqueFrame, *manifest,
                                                                     resources, normalized)
                    .has_value());
        REQUIRE(normalized.opaque.size() == 1);
        CHECK(normalized.transparent.empty());
        CHECK(normalized.opaque[0].pass ==
              cuexis::playback::detail::NormalizedPresentationPass::Opaque);
        CHECK(normalized.opaque[0].depthTest);
        CHECK(normalized.opaque[0].depthWrite);
        CHECK_FALSE(normalized.opaque[0].sourceOverBlend);
        CHECK(normalized.opaque[0].depthMeters == Catch::Approx(5.0));

        REQUIRE(session.update({.chartTimeMs = 500.0, .simulationDeltaTimeMs = 500.0}).has_value());
        auto blendFrame = session.extractFrame({.width = 1280, .height = 720});
        REQUIRE(blendFrame.has_value());
        const auto* blendObject = findObject(*blendFrame, renderableId);
        REQUIRE(blendObject != nullptr);
        REQUIRE(blendObject->material.has_value());
        CHECK(*blendObject->material == blendEntry->reference);
        CHECK(blendObject->materialAssetId == "material.blend");
        CHECK(blendObject->materialOpacity == Catch::Approx(0.75));
        CHECK(blendObject->materialTint[0] == Catch::Approx(0.75F));
        CHECK(blendObject->materialTint[1] == Catch::Approx(0.625F));
        CHECK(blendObject->materialTint[2] == Catch::Approx(1.0F));

        REQUIRE(cuexis::playback::detail::normalizePresentationFrame(*blendFrame, *manifest,
                                                                     resources, normalized)
                    .has_value());
        CHECK(normalized.opaque.empty());
        REQUIRE(normalized.transparent.size() == 1);
        const auto& blendRecord = normalized.transparent[0];
        CHECK(blendRecord.pass ==
              cuexis::playback::detail::NormalizedPresentationPass::Transparent);
        CHECK(blendRecord.depthTest);
        CHECK_FALSE(blendRecord.depthWrite);
        CHECK(blendRecord.sourceOverBlend);
        const auto materialResource = session.acquirePresentationResource(blendEntry->reference);
        REQUIRE(materialResource.has_value());
        const auto& material =
            std::get<cuexis::playback::PortableUnlitMaterial>((*materialResource)->value);
        CHECK(blendRecord.effectiveRgb[0] ==
              Catch::Approx(static_cast<double>(material.baseColor[0]) * 0.75));
        CHECK(blendRecord.effectiveRgb[1] ==
              Catch::Approx(static_cast<double>(material.baseColor[1]) * 0.625));
        CHECK(blendRecord.effectiveAlpha ==
              Catch::Approx(static_cast<double>(material.baseColor[3]) * 0.75));
        const auto blendEffectiveAlpha = blendRecord.effectiveAlpha;

        REQUIRE(session.update({.chartTimeMs = 625.0, .simulationDeltaTimeMs = 0.0}).has_value());
        auto stoppedFrame = session.extractFrame({.width = 1280, .height = 720});
        REQUIRE(stoppedFrame.has_value());
        const auto* stoppedObject = findObject(*stoppedFrame, renderableId);
        REQUIRE(stoppedObject != nullptr);
        CHECK(stoppedObject->material == blendObject->material);
        CHECK(stoppedObject->materialOpacity == blendObject->materialOpacity);
        CHECK(stoppedObject->materialTint[0] == blendObject->materialTint[0]);
        REQUIRE(cuexis::playback::detail::normalizePresentationFrame(*stoppedFrame, *manifest,
                                                                     resources, normalized)
                    .has_value());
        REQUIRE(normalized.transparent.size() == 1);
        CHECK(normalized.transparent[0].effectiveAlpha == blendEffectiveAlpha);

        REQUIRE(
            session.update({.chartTimeMs = 1250.0, .simulationDeltaTimeMs = 625.0}).has_value());
        auto hiddenFrame = session.extractFrame({.width = 1280, .height = 720});
        REQUIRE(hiddenFrame.has_value());
        const auto* hiddenObject = findObject(*hiddenFrame, renderableId);
        REQUIRE(hiddenObject != nullptr);
        CHECK_FALSE(hiddenObject->visible);
        REQUIRE(cuexis::playback::detail::normalizePresentationFrame(*hiddenFrame, *manifest,
                                                                     resources, normalized)
                    .has_value());
        CHECK(normalized.opaque.empty());
        CHECK(normalized.transparent.empty());

        REQUIRE(
            session
                .update(
                    {.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1})
                .has_value());
        auto soughtFrame = session.extractFrame({.width = 1280, .height = 720});
        REQUIRE(soughtFrame.has_value());
        const auto* soughtObject = findObject(*soughtFrame, renderableId);
        REQUIRE(soughtObject != nullptr);
        CHECK(soughtObject->visible);
        CHECK(soughtObject->material == opaqueObject->material);
        CHECK(*readCount == readsAfterPrepare);

        heldAfterSession = *blendFrame;
        auto replacementSource = typedSource(ProviderKind::Memory, fixture);
        REQUIRE(replacementSource.has_value());
        REQUIRE(session
                    .reload(std::move(*replacementSource), {.chartTimeMs = 500.0},
                            cuexis::playback::ReloadPolicy::KeepChartTime)
                    .has_value());
        CHECK(findObject(*heldAfterSession, renderableId)->material == blendEntry->reference);
        REQUIRE(session.unload().has_value());
        CHECK(findObject(*heldAfterSession, renderableId)->mesh == meshEntry->reference);
    }

    REQUIRE(heldAfterSession.has_value());
    const auto* heldObject = findObject(*heldAfterSession, renderableId);
    REQUIRE(heldObject != nullptr);
    REQUIRE(heldObject->mesh.has_value());
    REQUIRE(heldObject->material.has_value());
    CHECK(heldObject->mesh->assetId == "mesh.triangle");
    CHECK(heldObject->material->assetId == "material.blend");
}
