#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/playback/presentation.hpp>

#include "presentation_extraction.hpp"
#include "presentation_internal.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using cuexis::playback::PlaybackAssetDescriptor;
using cuexis::playback::PlaybackAssetType;
using cuexis::playback::PlaybackSource;
using cuexis::playback::PortableResourcePtr;
using cuexis::playback::PresentationResourceManifest;
using cuexis::playback::PresentationResourceRef;
using cuexis::playback::PresentationResourceType;

enum class ProviderKind {
    Filesystem,
    Memory,
    Host,
};

struct Stage3Fixture final {
    std::string chartJson;
    std::map<std::string, std::vector<std::byte>, std::less<>> blobs;
};

[[nodiscard]] auto fixtureRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project";
}

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open fixture text: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
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

[[nodiscard]] auto loadFixture() -> Stage3Fixture {
    const auto root = fixtureRoot() / "assets";
    return Stage3Fixture{
        .chartJson = readText(root / "charts" / "stage3_example.cuexis.chart.json"),
        .blobs = {{"materials/blend.material.bin",
                   readBytes(root / "materials" / "blend.material.bin")},
                  {"materials/opaque.material.bin",
                   readBytes(root / "materials" / "opaque.material.bin")},
                  {"meshes/triangle.mesh.bin", readBytes(root / "meshes" / "triangle.mesh.bin")},
                  {"textures/checker.texture.bin",
                   readBytes(root / "textures" / "checker.texture.bin")}},
    };
}

[[nodiscard]] auto addIrrelevantMaterialBehaviors(std::string chartJson,
                                                  bool bindCameraBehavior = false) -> std::string {
    const auto objects = chartJson.find("\"objects\": [");
    REQUIRE(objects != std::string::npos);
    const auto behaviorsEnd = chartJson.rfind("],", objects);
    REQUIRE(behaviorsEnd != std::string::npos);
    chartJson.insert(behaviorsEnd, R"json(,
    {
      "id": "stage3.unbound.material",
      "type": "behavior.event",
      "version": 1,
      "events": [],
      "stepEvents": [
        {
          "property": "render.material",
          "beat": { "numerator": 0, "denominator": 1 },
          "value": { "domain": "asset", "id": "material.unbound.missing" }
        }
      ]
    },
    {
      "id": "stage3.camera.material",
      "type": "behavior.event",
      "version": 1,
      "events": [],
      "stepEvents": [
        {
          "property": "render.material",
          "beat": { "numerator": 0, "denominator": 1 },
          "value": { "domain": "asset", "id": "material.camera.missing" }
        }
      ]
    }
)json");

    if (bindCameraBehavior) {
        const auto camera = chartJson.find("\"cuexis.camera\": {");
        REQUIRE(camera != std::string::npos);
        const auto cameraLine = chartJson.rfind('\n', camera);
        REQUIRE(cameraLine != std::string::npos);
        chartJson.insert(cameraLine + 1, R"json(        "cuexis.behavior": {
          "version": 1,
          "behavior": { "domain": "behavior", "id": "stage3.camera.material" }
        },
)json");
    }
    return chartJson;
}

[[nodiscard]] auto descriptors(std::vector<std::string> blendDependencies = {"texture.checker"})
    -> std::vector<PlaybackAssetDescriptor> {
    return {
        {.id = "texture.checker",
         .type = PlaybackAssetType::Texture,
         .rootId = "main",
         .logicalSource = "textures/checker.texture.bin",
         .dependencies = {}},
        {.id = "material.blend",
         .type = PlaybackAssetType::Material,
         .rootId = "main",
         .logicalSource = "materials/blend.material.bin",
         .dependencies = std::move(blendDependencies)},
        {.id = "mesh.triangle",
         .type = PlaybackAssetType::Mesh,
         .rootId = "main",
         .logicalSource = "meshes/triangle.mesh.bin",
         .dependencies = {}},
        {.id = "material.opaque",
         .type = PlaybackAssetType::Material,
         .rootId = "main",
         .logicalSource = "materials/opaque.material.bin",
         .dependencies = {}},
    };
}

[[nodiscard]] auto typedSource(ProviderKind kind, const Stage3Fixture& fixture,
                               std::vector<std::string> blendDependencies = {"texture.checker"},
                               std::shared_ptr<std::size_t> readCount = {})
    -> cuexis::core::Result<PlaybackSource> {
    if (kind == ProviderKind::Filesystem) {
        return PlaybackSource::fromFilesystemProject(fixtureRoot());
    }

    std::shared_ptr<cuexis::content::IContentProvider> provider;
    if (kind == ProviderKind::Memory) {
        std::vector<cuexis::content::MemoryContentEntry> entries;
        entries.reserve(fixture.blobs.size());
        for (const auto& [source, bytes] : fixture.blobs) {
            entries.push_back({.rootId = "main", .source = source, .bytes = bytes, .revision = 7});
        }
        auto created = cuexis::content::MemoryContentProvider::create(std::move(entries));
        if (!created) {
            return cuexis::core::unexpected(std::move(created.error()));
        }
        provider = std::move(*created);
    } else {
        auto blobs = std::make_shared<decltype(fixture.blobs)>(fixture.blobs);
        auto created = cuexis::content::HostContentProvider::create(
            [blobs = std::move(blobs),
             readCount = std::move(readCount)](const cuexis::content::ContentRequest& request)
                -> cuexis::core::Result<cuexis::content::ContentBlob> {
                if (request.rootId != "main") {
                    return cuexis::core::unexpected(cuexis::core::Error{
                        "test.content.root_missing", "Requested fixture root does not exist"});
                }
                const auto found = blobs->find(request.source);
                if (found == blobs->end()) {
                    return cuexis::core::unexpected(cuexis::core::Error{
                        "test.content.source_missing", "Requested fixture source does not exist"});
                }
                if (found->second.size() > request.maxBytes) {
                    return cuexis::core::unexpected(cuexis::core::Error{
                        "test.content.limit", "Requested fixture source exceeds the byte limit"});
                }
                if (readCount) {
                    ++*readCount;
                }
                return cuexis::content::ContentBlob{found->second, 11};
            });
        if (!created) {
            return cuexis::core::unexpected(std::move(created.error()));
        }
        provider = std::move(*created);
    }

    cuexis::playback::TypedPlaybackProject project{
        .sourceId = "stage3-portable-test",
        .chartJson = fixture.chartJson,
        .assets = descriptors(std::move(blendDependencies)),
    };
    return PlaybackSource::fromTypedProject(std::move(project), std::move(provider));
}

[[nodiscard]] auto manifestsEqual(const PresentationResourceManifest& left,
                                  const PresentationResourceManifest& right) -> bool {
    if (left.version != right.version || left.totalEncodedBytes != right.totalEncodedBytes ||
        left.totalDecodedBytes != right.totalDecodedBytes ||
        left.entries.size() != right.entries.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.entries.size(); ++index) {
        const auto& leftEntry = left.entries[index];
        const auto& rightEntry = right.entries[index];
        if (leftEntry.reference != rightEntry.reference ||
            leftEntry.encodedByteCount != rightEntry.encodedByteCount ||
            leftEntry.decodedByteCount != rightEntry.decodedByteCount ||
            leftEntry.dependencies != rightEntry.dependencies) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto identityHex(const PresentationResourceRef& reference) -> std::string {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const auto byte : reference.identity.sha256) {
        stream << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

[[nodiscard]] auto findEntry(const PresentationResourceManifest& manifest, std::string_view assetId)
    -> const cuexis::playback::PresentationManifestEntry* {
    const auto found =
        std::find_if(manifest.entries.begin(), manifest.entries.end(),
                     [&](const auto& entry) { return entry.reference.assetId == assetId; });
    return found == manifest.entries.end() ? nullptr : &*found;
}

[[nodiscard]] auto syntheticAssetId(std::size_t index) -> std::string {
    std::ostringstream stream;
    stream << "synthetic." << std::setfill('0') << std::setw(5) << index;
    return stream.str();
}

[[nodiscard]] auto syntheticPresentation(std::size_t resourceCount)
    -> cuexis::playback::detail::PreparedPresentation {
    cuexis::playback::detail::PreparedPresentation presentation;
    presentation.manifest.entries.reserve(resourceCount);
    presentation.orderedResources.reserve(resourceCount);
    for (std::size_t index = 0; index < resourceCount; ++index) {
        auto assetId = syntheticAssetId(index);
        PresentationResourceRef reference{PresentationResourceType::Mesh, assetId, {}};
        reference.identity.sha256[0] = static_cast<std::uint8_t>(index & 0xFFU);
        reference.identity.sha256[1] = static_cast<std::uint8_t>((index >> 8U) & 0xFFU);
        auto resource = std::make_shared<const cuexis::playback::PortableResource>(
            cuexis::playback::PortableResource{reference, cuexis::playback::PortableMesh{}});
        const cuexis::playback::detail::PresentationResourceKey key{assetId, reference.type};
        presentation.resources.emplace(key, resource);
        presentation.manifest.entries.push_back(
            cuexis::playback::PresentationManifestEntry{reference, 1, 1, {}});
        presentation.orderedResources.push_back(std::move(resource));
    }
    presentation.manifest.totalEncodedBytes = resourceCount;
    presentation.manifest.totalDecodedBytes = resourceCount;
    return presentation;
}

[[nodiscard]] auto reorderedMemorySource(const Stage3Fixture& fixture)
    -> cuexis::core::Result<PlaybackSource> {
    std::vector<cuexis::content::MemoryContentEntry> entries;
    entries.reserve(fixture.blobs.size());
    for (const auto& [source, bytes] : fixture.blobs) {
        entries.push_back({.rootId = "main", .source = source, .bytes = bytes, .revision = 7});
    }
    auto provider = cuexis::content::MemoryContentProvider::create(std::move(entries));
    if (!provider) {
        return cuexis::core::unexpected(std::move(provider.error()));
    }
    auto assets = descriptors();
    std::reverse(assets.begin(), assets.end());
    cuexis::playback::TypedPlaybackProject project{
        .sourceId = "stage3-portable-test",
        .chartJson = fixture.chartJson,
        .assets = std::move(assets),
    };
    return PlaybackSource::fromTypedProject(std::move(project), std::move(*provider));
}

[[nodiscard]] auto errorContextValue(const cuexis::core::Error& error, std::string_view key)
    -> std::optional<std::string_view> {
    const auto found = std::find_if(error.context().begin(), error.context().end(),
                                    [&](const auto& context) { return context.key == key; });
    return found == error.context().end() ? std::nullopt
                                          : std::optional<std::string_view>{found->value};
}

void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    REQUIRE(offset + 4 <= bytes.size());
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xFFU);
    }
}

[[nodiscard]] auto rejectionCode(Stage3Fixture fixture,
                                 std::vector<std::string> blendDependencies = {"texture.checker"})
    -> std::string {
    cuexis::playback::PlaybackSession session;
    auto source = typedSource(ProviderKind::Memory, fixture, std::move(blendDependencies));
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(prepared.has_value());
    return std::string{prepared.error().code()};
}

[[nodiscard]] auto acquireResources(cuexis::playback::PlaybackSession& session,
                                    const PresentationResourceManifest& manifest)
    -> std::vector<PortableResourcePtr> {
    std::vector<PortableResourcePtr> resources;
    resources.reserve(manifest.entries.size());
    for (const auto& entry : manifest.entries) {
        auto resource = session.acquirePresentationResource(entry.reference);
        REQUIRE(resource.has_value());
        resources.push_back(std::move(*resource));
    }
    return resources;
}

[[nodiscard]] auto findObject(cuexis::playback::FrameSnapshot& snapshot, std::string_view objectId)
    -> cuexis::playback::FrameSnapshot::ObjectSnapshot* {
    const auto found = std::find_if(snapshot.objects.begin(), snapshot.objects.end(),
                                    [&](const auto& object) { return object.id == objectId; });
    return found == snapshot.objects.end() ? nullptr : &*found;
}

} // namespace

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
    duplicate.entries.insert(duplicate.entries.begin() + 1, duplicate.entries.front());
    auto resources = acquireResources(session, *manifest);
    resources.insert(resources.begin() + 1, resources.front());
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
