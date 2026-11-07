#pragma once

// Fixtures shared by the portable presentation test units: the stage 3 project fixture, the
// typed sources and the manifest helpers.

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

namespace presentation_fixture {

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

[[nodiscard]] inline auto fixtureRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project";
}

[[nodiscard]] inline auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open fixture text: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] inline auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    const auto text = readText(path);
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

[[nodiscard]] inline auto loadFixture() -> Stage3Fixture {
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

[[nodiscard]] inline auto addIrrelevantMaterialBehaviors(std::string chartJson,
                                                         bool bindCameraBehavior = false)
    -> std::string {
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

[[nodiscard]] inline auto
descriptors(std::vector<std::string> blendDependencies = {"texture.checker"})
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

[[nodiscard]] inline auto
typedSource(ProviderKind kind, const Stage3Fixture& fixture,
            std::vector<std::string> blendDependencies = {"texture.checker"},
            std::shared_ptr<std::size_t> readCount = {}) -> cuexis::core::Result<PlaybackSource> {
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

[[nodiscard]] inline auto memorySourceWithAssets(const Stage3Fixture& fixture,
                                                 std::vector<PlaybackAssetDescriptor> assets)
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
    return PlaybackSource::fromTypedProject({.sourceId = "stage3-presentation-closure",
                                             .chartJson = fixture.chartJson,
                                             .assets = std::move(assets)},
                                            std::move(*provider));
}

[[nodiscard]] inline auto manifestsEqual(const PresentationResourceManifest& left,
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

[[nodiscard]] inline auto identityHex(const PresentationResourceRef& reference) -> std::string {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const auto byte : reference.identity.sha256) {
        stream << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

[[nodiscard]] inline auto findEntry(const PresentationResourceManifest& manifest,
                                    std::string_view assetId)
    -> const cuexis::playback::PresentationManifestEntry* {
    const auto found =
        std::find_if(manifest.entries.begin(), manifest.entries.end(),
                     [&](const auto& entry) { return entry.reference.assetId == assetId; });
    return found == manifest.entries.end() ? nullptr : &*found;
}

[[nodiscard]] inline auto syntheticAssetId(std::size_t index) -> std::string {
    std::ostringstream stream;
    stream << "synthetic." << std::setfill('0') << std::setw(5) << index;
    return stream.str();
}

[[nodiscard]] inline auto syntheticPresentation(std::size_t resourceCount)
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

[[nodiscard]] inline auto reorderedMemorySource(const Stage3Fixture& fixture)
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

[[nodiscard]] inline auto errorContextValue(const cuexis::core::Error& error, std::string_view key)
    -> std::optional<std::string_view> {
    const auto found = std::find_if(error.context().begin(), error.context().end(),
                                    [&](const auto& context) { return context.key == key; });
    return found == error.context().end() ? std::nullopt
                                          : std::optional<std::string_view>{found->value};
}

[[nodiscard]] inline auto diagnosticCodes(const cuexis::core::Diagnostics& diagnostics)
    -> std::string {
    std::string result;
    for (const auto& item : diagnostics.items()) {
        if (!result.empty()) {
            result += ", ";
        }
        result += std::string{item.code()};
    }
    return result;
}

inline void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    REQUIRE(offset + 4 <= bytes.size());
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xFFU);
    }
}

inline void writeU64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value) {
    REQUIRE(offset + 8 <= bytes.size());
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xFFU);
    }
}

[[nodiscard]] inline auto rejectionCode(Stage3Fixture fixture,
                                        std::vector<std::string> blendDependencies = {
                                            "texture.checker"}) -> std::string {
    cuexis::playback::PlaybackSession session;
    auto source = typedSource(ProviderKind::Memory, fixture, std::move(blendDependencies));
    REQUIRE(source.has_value());
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(prepared.has_value());
    return std::string{prepared.error().code()};
}

[[nodiscard]] inline auto acquireResources(cuexis::playback::PlaybackSession& session,
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

[[nodiscard]] inline auto findObject(cuexis::playback::FrameSnapshot& snapshot,
                                     std::string_view objectId)
    -> cuexis::playback::FrameSnapshot::ObjectSnapshot* {
    const auto found = std::find_if(snapshot.objects.begin(), snapshot.objects.end(),
                                    [&](const auto& object) { return object.id == objectId; });
    return found == snapshot.objects.end() ? nullptr : &*found;
}

} // namespace presentation_fixture
