#include <cuexis/playback/content_provider.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/playback/presentation.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using cuexis::playback::FrameDigest;
using cuexis::playback::PlaybackContentInfo;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PlaybackSource;
using cuexis::playback::PreparedSemanticIdentity;
using cuexis::playback::RuntimeFrame;

constexpr std::array<RuntimeFrame, 4> sampleFrames{
    RuntimeFrame{.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0},
    RuntimeFrame{.chartTimeMs = 625.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1},
    RuntimeFrame{.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 2},
    RuntimeFrame{.chartTimeMs = 1250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 3},
};

constexpr std::array<RuntimeFrame, 4> animationFrames{
    RuntimeFrame{.chartTimeMs = 4000.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0},
    RuntimeFrame{.chartTimeMs = 4250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1},
    RuntimeFrame{.chartTimeMs = 5000.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 2},
    RuntimeFrame{.chartTimeMs = 6250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 3},
};

constexpr std::uint64_t expectedStopDigest = 11596562486377158370ULL;
constexpr std::string_view expectedSemanticIdentity =
    "6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5";
constexpr std::string_view expectedAnimationIdentity =
    "fb662e259e2146cf68d6ebc763514b3bc2b460f2a865ece6805bda844586e9b4";
constexpr std::array<std::uint64_t, 4> expectedAnimationDigests{
    105060921077611920ULL, 10690198800679353609ULL, 18438846932740715847ULL,
    18147874964077530090ULL};

struct Observation final {
    PreparedSemanticIdentity identity;
    std::array<std::uint64_t, sampleFrames.size()> digests{};
};

[[nodiscard]] auto fixtureRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_FIXTURE_DIR};
}

[[nodiscard]] auto referenceProject() -> std::filesystem::path {
    return fixtureRoot() / "cfu_f_reference_project";
}

[[nodiscard]] auto referencePackage() -> std::filesystem::path {
    return fixtureRoot() / "cfu_f_v4_reference.cxc";
}

[[nodiscard]] auto animatedPackage() -> std::filesystem::path {
    return fixtureRoot() / "cxc_v1_v4_cxt.cxc";
}

[[nodiscard]] auto parameterizedProject() -> std::filesystem::path {
    return fixtureRoot() / "parameterized_project";
}

[[nodiscard]] auto animatedProject() -> std::filesystem::path {
    return fixtureRoot() / "source_project";
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

[[nodiscard]] auto fail(std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

[[nodiscard]] auto fail(std::string_view operation, const cuexis::core::Error& error) -> int {
    std::cerr << operation << " failed: " << error.code() << ": " << error.message() << '\n';
    return 1;
}

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        std::cerr << "Could not open fixture: " << path << '\n';
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path)
    -> std::optional<std::vector<std::byte>> {
    const auto raw = readText(path);
    if (!raw) {
        return std::nullopt;
    }
    std::vector<std::byte> bytes;
    bytes.reserve(raw->size());
    for (const unsigned char value : *raw) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

[[nodiscard]] auto identityHex(const PreparedSemanticIdentity& identity) -> std::string {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : identity.sha256) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}

[[nodiscard]] auto capabilities() -> cuexis::playback::PresentationCapabilities {
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

[[nodiscard]] auto validResource(const cuexis::playback::PortableResource& resource) -> bool {
    using cuexis::playback::PresentationResourceType;
    switch (resource.reference.type) {
    case PresentationResourceType::Mesh: {
        const auto* mesh = std::get_if<cuexis::playback::PortableMesh>(&resource.value);
        return mesh != nullptr && !mesh->positions.empty() && !mesh->indices.empty();
    }
    case PresentationResourceType::Texture2D: {
        const auto* texture = std::get_if<cuexis::playback::PortableTexture2D>(&resource.value);
        return texture != nullptr && texture->width > 0 && texture->height > 0 &&
               texture->pixelsRgba8.size() ==
                   static_cast<std::size_t>(texture->width) * texture->height * 4U;
    }
    case PresentationResourceType::UnlitMaterial: {
        const auto* material =
            std::get_if<cuexis::playback::PortableUnlitMaterial>(&resource.value);
        return material != nullptr &&
               (!material->baseColorTexture || !material->baseColorTexture->assetId.empty());
    }
    case PresentationResourceType::Shader: {
        const auto* shader = std::get_if<cuexis::playback::PortableShader>(&resource.value);
        return shader != nullptr && !shader->vertexSource.empty() &&
               !shader->fragmentSource.empty();
    }
    case PresentationResourceType::ParameterizedMaterial: {
        const auto* material =
            std::get_if<cuexis::playback::PortableParameterizedMaterial>(&resource.value);
        return material != nullptr && !material->shader.assetId.empty();
    }
    }
    return false;
}

[[nodiscard]] auto referenceAssets() -> std::vector<cuexis::playback::PlaybackAssetDescriptor> {
    using cuexis::playback::PlaybackAssetDescriptor;
    using cuexis::playback::PlaybackAssetType;
    return {
        PlaybackAssetDescriptor{.id = "texture.checker",
                                .type = PlaybackAssetType::Texture,
                                .rootId = "main",
                                .logicalSource = "textures/checker.texture.bin",
                                .dependencies = {}},
        PlaybackAssetDescriptor{.id = "material.blend",
                                .type = PlaybackAssetType::Material,
                                .rootId = "main",
                                .logicalSource = "materials/blend.material.bin",
                                .dependencies = {"texture.checker"}},
        PlaybackAssetDescriptor{.id = "mesh.triangle",
                                .type = PlaybackAssetType::Mesh,
                                .rootId = "main",
                                .logicalSource = "meshes/triangle.mesh.bin",
                                .dependencies = {}},
        PlaybackAssetDescriptor{.id = "material.opaque",
                                .type = PlaybackAssetType::Material,
                                .rootId = "main",
                                .logicalSource = "materials/opaque.material.bin",
                                .dependencies = {}},
    };
}

[[nodiscard]] auto typedReferenceSource() -> std::optional<PlaybackSource> {
    const auto chart = readText(referenceProject() / "assets/charts/main.cuexis.chart.json");
    if (!chart) {
        return std::nullopt;
    }
    auto provider = cuexis::playback::FilesystemContentProvider::create(
        {{.id = "main", .path = referenceProject() / "assets"}});
    if (!provider) {
        static_cast<void>(fail("typed reference provider", provider.error()));
        return std::nullopt;
    }
    auto source = PlaybackSource::fromTypedProjectSource(
        {.sourceId = "external-cfu-f2-typed-reference",
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json", .utf8Text = *chart}},
         .assets = referenceAssets()},
        std::move(*provider));
    if (!source) {
        static_cast<void>(fail("typed reference source", source.error()));
        return std::nullopt;
    }
    return std::move(*source);
}

[[nodiscard]] auto typedAnimatedSource() -> std::optional<PlaybackSource> {
    const auto chart = readText(animatedProject() / "assets/charts/main.cuexis.chart.json");
    const auto cxt = readText(animatedProject() / "templates/move-y.cxt");
    if (!chart || !cxt) {
        return std::nullopt;
    }
    auto provider = cuexis::playback::MemoryContentProvider::create({});
    if (!provider) {
        static_cast<void>(fail("typed animation provider", provider.error()));
        return std::nullopt;
    }
    auto source = PlaybackSource::fromTypedProjectSource(
        {.sourceId = "s4-f-typed-source-project",
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json", .utf8Text = *chart},
                              {.path = "templates/move-y.cxt", .utf8Text = *cxt}},
         .assets = {}},
        std::move(*provider));
    if (!source) {
        static_cast<void>(fail("typed animation source", source.error()));
        return std::nullopt;
    }
    return std::move(*source);
}

[[nodiscard]] auto observe(PlaybackSource&& source, std::string_view sourceName)
    -> std::optional<Observation> {
    PlaybackSession session;
    auto prepared = session.prepareLoad(std::move(source), PlaybackMode::ChartClock);
    if (!prepared) {
        static_cast<void>(fail(std::string{sourceName} + " prepare", prepared.error()));
        return std::nullopt;
    }
    const auto candidateIdentity = prepared->semanticIdentity();
    const auto* manifest = prepared->presentationManifest();
    if (!candidateIdentity || manifest == nullptr || manifest->entries.size() != 4U) {
        static_cast<void>(fail(std::string{sourceName} + " candidate is incomplete"));
        return std::nullopt;
    }
    const auto validation = prepared->validatePresentation(
        capabilities(), cuexis::playback::PresentationRequest{.enableDebugPass = true});
    if (!validation.hasValue() || !validation.settings || !validation.settings->debugPassEnabled) {
        static_cast<void>(fail(std::string{sourceName} + " presentation preflight failed"));
        return std::nullopt;
    }

    cuexis::playback::PortableResourcePtr retained;
    for (const auto& entry : manifest->entries) {
        auto resource = prepared->acquirePresentationResource(entry.reference);
        if (!resource || !*resource || (*resource)->reference != entry.reference ||
            !validResource(**resource)) {
            static_cast<void>(fail(std::string{sourceName} + " resource acquisition failed"));
            return std::nullopt;
        }
        if (!retained) {
            retained = *resource;
        }
    }
    auto committed = session.commit(std::move(*prepared));
    if (!committed) {
        static_cast<void>(fail(std::string{sourceName} + " commit", committed.error()));
        return std::nullopt;
    }
    const auto activeIdentity = session.semanticIdentity();
    if (!activeIdentity || *activeIdentity != *candidateIdentity) {
        static_cast<void>(fail(std::string{sourceName} + " active identity mismatch"));
        return std::nullopt;
    }

    Observation observation{.identity = *activeIdentity};
    for (std::size_t index = 0; index < sampleFrames.size(); ++index) {
        auto updated = session.update(sampleFrames[index]);
        if (!updated) {
            static_cast<void>(fail(std::string{sourceName} + " update", updated.error()));
            return std::nullopt;
        }
        auto snapshot = session.extractFrame({.width = 1280, .height = 720});
        if (!snapshot || snapshot->objects.size() != 2U) {
            static_cast<void>(fail(std::string{sourceName} + " frame extraction failed"));
            return std::nullopt;
        }
        auto digest = cuexis::playback::computeFrameDigest(sampleFrames[index], *snapshot);
        if (!digest || digest->algorithmVersion != 3U) {
            static_cast<void>(fail(std::string{sourceName} + " FrameDigest failed"));
            return std::nullopt;
        }
        observation.digests[index] = digest->value;
    }
    if (!session.unload() || !retained || !validResource(*retained)) {
        static_cast<void>(fail(std::string{sourceName} + " owning resource lifetime failed"));
        return std::nullopt;
    }
    return observation;
}

[[nodiscard]] auto observeAnimation(PlaybackSource&& source, std::string_view sourceName)
    -> std::optional<Observation> {
    PlaybackSession session;
    auto prepared = session.prepareLoad(std::move(source), PlaybackMode::ChartClock);
    if (!prepared) {
        static_cast<void>(fail(std::string{sourceName} + " prepare", prepared.error()));
        return std::nullopt;
    }
    const auto candidateIdentity = prepared->semanticIdentity();
    if (!candidateIdentity) {
        static_cast<void>(fail(std::string{sourceName} + " candidate identity is missing"));
        return std::nullopt;
    }
    auto committed = session.commit(std::move(*prepared));
    if (!committed) {
        static_cast<void>(fail(std::string{sourceName} + " commit", committed.error()));
        return std::nullopt;
    }
    const auto activeIdentity = session.semanticIdentity();
    if (!activeIdentity || *activeIdentity != *candidateIdentity) {
        static_cast<void>(fail(std::string{sourceName} + " active identity mismatch"));
        return std::nullopt;
    }

    Observation observation{.identity = *activeIdentity};
    std::vector<std::string> objectIds;
    for (std::size_t index = 0; index < animationFrames.size(); ++index) {
        auto updated = session.update(animationFrames[index]);
        if (!updated) {
            static_cast<void>(fail(std::string{sourceName} + " update", updated.error()));
            return std::nullopt;
        }
        auto snapshot = session.extractFrame({.width = 1280, .height = 720});
        if (!snapshot || snapshot->objects.empty()) {
            static_cast<void>(fail(std::string{sourceName} + " frame extraction failed"));
            return std::nullopt;
        }
        if (index == 0) {
            objectIds.reserve(snapshot->objects.size());
            for (const auto& object : snapshot->objects) {
                objectIds.push_back(object.id);
            }
            if (!std::is_sorted(objectIds.begin(), objectIds.end())) {
                static_cast<void>(fail(std::string{sourceName} + " object order is not sorted"));
                return std::nullopt;
            }
        } else if (objectIds.size() != snapshot->objects.size()) {
            static_cast<void>(fail(std::string{sourceName} + " object permutation changed"));
            return std::nullopt;
        }
        auto digest = cuexis::playback::computeFrameDigest(animationFrames[index], *snapshot);
        if (!digest || digest->algorithmVersion != 3U) {
            static_cast<void>(fail(std::string{sourceName} + " FrameDigest failed"));
            return std::nullopt;
        }
        observation.digests[index] = digest->value;
    }
    if (observation.digests[0] == observation.digests[1]) {
        static_cast<void>(fail(std::string{sourceName} + " animation FrameDigest did not change"));
        return std::nullopt;
    }
    return observation;
}

[[nodiscard]] auto numberOptions(double x, double scaleY, double fov)
    -> cuexis::playback::PlaybackPrepareOptions {
    return {
        .parameters = {
            .values = {
                {.id = "layout.x", .value = cuexis::playback::ChartParameterNumber{x}},
                {.id = "layout.scale-y", .value = cuexis::playback::ChartParameterNumber{scaleY}},
                {.id = "camera.fov", .value = cuexis::playback::ChartParameterNumber{fov}}}}};
}

[[nodiscard]] auto verifyNumberOptions() -> bool {
    auto source = PlaybackSource::fromFilesystemProject(parameterizedProject());
    if (!source) {
        static_cast<void>(fail("parameterized filesystem source", source.error()));
        return false;
    }
    auto options = numberOptions(-4.0, 2.0, 75.0);
    PlaybackSession session;
    auto prepared = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock, options);
    if (!prepared) {
        static_cast<void>(fail("parameterized prepare", prepared.error()));
        return false;
    }
    const auto candidateIdentity = prepared->semanticIdentity();
    std::get<cuexis::playback::ChartParameterNumber>(options.parameters.values[0].value).value =
        99.0;
    if (!candidateIdentity || !session.commit(std::move(*prepared)) ||
        !session.update({.chartTimeMs = 0.0})) {
        static_cast<void>(fail("parameterized commit or update failed"));
        return false;
    }
    const auto activeIdentity = session.semanticIdentity();
    const auto frame = session.extractFrame({.width = 640, .height = 480});
    if (!activeIdentity || *activeIdentity != *candidateIdentity || !frame ||
        frame->objects.size() != 1U || frame->objects[0].worldMatrix[12] != -4.0F ||
        frame->objects[0].worldMatrix[5] != 2.0F || frame->camera.fovY != 75.0) {
        static_cast<void>(fail("prepare options were not frozen into the prepared candidate"));
        return false;
    }
    return true;
}

[[nodiscard]] auto taggedSource() -> std::optional<PlaybackSource> {
    const auto chart = readText(fixtureRoot() / "chart_v4_parameterized_rational.json");
    if (!chart) {
        return std::nullopt;
    }
    auto provider = cuexis::playback::MemoryContentProvider::create({});
    if (!provider) {
        static_cast<void>(fail("tagged memory provider", provider.error()));
        return std::nullopt;
    }
    auto source = PlaybackSource::fromTypedProjectSource(
        {.sourceId = "external-cfu-f2-tagged-options",
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json", .utf8Text = *chart}},
         .assets = {}},
        std::move(*provider));
    if (!source) {
        static_cast<void>(fail("tagged typed source", source.error()));
        return std::nullopt;
    }
    return std::move(*source);
}

[[nodiscard]] auto taggedIdentity(std::int64_t numerator, std::int64_t denominator)
    -> std::optional<PreparedSemanticIdentity> {
    auto source = taggedSource();
    if (!source) {
        return std::nullopt;
    }
    const cuexis::playback::PlaybackPrepareOptions options{
        .parameters = {
            .values = {
                {.id = "motion.duration-scale",
                 .value = cuexis::playback::ChartParameterRational{numerator, denominator}},
                {.id = "motion.weight", .value = cuexis::playback::ChartParameterWeight{0.5}}}}};
    PlaybackSession session;
    auto prepared = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock, options);
    if (!prepared) {
        static_cast<void>(fail("tagged parameter prepare", prepared.error()));
        return std::nullopt;
    }
    const auto candidateIdentity = prepared->semanticIdentity();
    if (!candidateIdentity || !session.commit(std::move(*prepared))) {
        static_cast<void>(fail("tagged parameter commit failed"));
        return std::nullopt;
    }
    const auto activeIdentity = session.semanticIdentity();
    if (!activeIdentity || *activeIdentity != *candidateIdentity) {
        static_cast<void>(fail("tagged parameter active identity mismatch"));
        return std::nullopt;
    }
    return *activeIdentity;
}

[[nodiscard]] auto contextValue(const cuexis::core::Diagnostic& diagnostic, std::string_view key)
    -> std::string_view {
    const auto found =
        std::ranges::find(diagnostic.context(), key, &cuexis::core::DiagnosticContext::key);
    return found == diagnostic.context().end() ? std::string_view{} : found->value;
}

[[nodiscard]] auto diagnosticSignature(const cuexis::core::Diagnostics& diagnostics)
    -> std::string {
    std::ostringstream output;
    bool first = true;
    for (const auto& diagnostic : diagnostics.items()) {
        if (!first) {
            output << '|';
        }
        first = false;
        output << diagnostic.code() << '@' << diagnostic.fieldPath();
        const auto capability = contextValue(diagnostic, "capability");
        if (!capability.empty()) {
            output << '#' << capability;
        }
    }
    return output.str();
}

[[nodiscard]] auto diagnosticFingerprint(const cuexis::core::Diagnostics& diagnostics)
    -> std::string {
    std::ostringstream output;
    output << diagnostics.size() << ':' << diagnostics.hasErrors() << ':'
           << diagnostics.hasWarnings() << ':' << diagnostics.limitReached();
    for (const auto& diagnostic : diagnostics.items()) {
        output << '|' << static_cast<int>(diagnostic.severity()) << ':' << diagnostic.code() << ':'
               << diagnostic.message() << ':' << diagnostic.fieldPath();
        for (const auto& context : diagnostic.context()) {
            output << ':' << context.key << '=' << context.value;
        }
    }
    return output.str();
}

[[nodiscard]] auto sameContent(const PlaybackContentInfo& left, const PlaybackContentInfo& right)
    -> bool {
    return left.chartId == right.chartId && left.chartFormatVersion == right.chartFormatVersion &&
           left.timingOffsetMs == right.timingOffsetMs && left.mode == right.mode &&
           left.mainMusicAssetId == right.mainMusicAssetId;
}

[[nodiscard]] auto frameDigest(PlaybackSession& session, const RuntimeFrame& frame)
    -> std::optional<FrameDigest> {
    auto snapshot = session.extractFrame({.width = 1280, .height = 720});
    if (!snapshot) {
        static_cast<void>(fail("failure-path frame extraction", snapshot.error()));
        return std::nullopt;
    }
    auto digest = cuexis::playback::computeFrameDigest(frame, *snapshot);
    if (!digest) {
        static_cast<void>(fail("failure-path FrameDigest", digest.error()));
        return std::nullopt;
    }
    return *digest;
}

[[nodiscard]] auto verifyFailedReload() -> bool {
    auto source = PlaybackSource::fromCxcFile(referencePackage());
    if (!source) {
        static_cast<void>(fail("failure-path reference source", source.error()));
        return false;
    }
    PlaybackSession session{trimmedCapabilities()};
    if (!session.load(std::move(*source), PlaybackMode::ChartClock) ||
        !session.update(sampleFrames[1])) {
        static_cast<void>(fail("failure-path reference load failed"));
        return false;
    }
    const auto identityBefore = session.semanticIdentity();
    const auto contentBefore = session.contentInfo();
    const auto diagnosticsBefore = session.diagnostics();
    const auto digestBefore = frameDigest(session, sampleFrames[1]);
    if (!identityBefore || !contentBefore || !diagnosticsBefore || !digestBefore) {
        static_cast<void>(fail("failure-path active state is incomplete"));
        return false;
    }

    const auto bytes = readBytes(animatedPackage());
    if (!bytes) {
        return false;
    }
    auto replacement = PlaybackSource::fromCxcMemory(*bytes);
    if (!replacement) {
        static_cast<void>(fail("animated replacement source", replacement.error()));
        return false;
    }
    const auto rejected = session.reload(std::move(*replacement), sampleFrames[1],
                                         cuexis::playback::ReloadPolicy::KeepChartTime);
    if (rejected || rejected.error().code() != "playback.capability.preflight_failed") {
        static_cast<void>(fail("animated CXC reload was not rejected"));
        return false;
    }
    const auto operationDiagnostics = session.lastOperationDiagnostics();
    constexpr std::string_view expectedSignature =
        "playback.capability.unsupported@$/animationClips#cuexis.animation.clip.v1|"
        "playback.capability.unsupported@$/objects#cuexis.animation.layers.v1";
    if (!operationDiagnostics || diagnosticSignature(*operationDiagnostics) != expectedSignature) {
        static_cast<void>(fail("animated CXC diagnostic signature mismatch"));
        return false;
    }

    const auto identityAfter = session.semanticIdentity();
    const auto contentAfter = session.contentInfo();
    const auto diagnosticsAfter = session.diagnostics();
    const auto digestAfter = frameDigest(session, sampleFrames[1]);
    return identityAfter && *identityAfter == *identityBefore && contentAfter &&
           sameContent(*contentAfter, *contentBefore) && diagnosticsAfter &&
           diagnosticFingerprint(*diagnosticsAfter) == diagnosticFingerprint(*diagnosticsBefore) &&
           digestAfter && digestAfter->algorithmVersion == digestBefore->algorithmVersion &&
           digestAfter->value == digestBefore->value;
}

} // namespace

int main() {
    const auto packageBytes = readBytes(referencePackage());
    if (!packageBytes) {
        return 1;
    }
    auto filesystemSource = PlaybackSource::fromFilesystemProject(referenceProject());
    auto fileSource = PlaybackSource::fromCxcFile(referencePackage());
    auto memorySource = PlaybackSource::fromCxcMemory(*packageBytes);
    auto typedSource = typedReferenceSource();
    if (!filesystemSource || !fileSource || !memorySource || !typedSource) {
        return fail("Playback-only consumer could not construct all reference sources");
    }

    const auto filesystem = observe(std::move(*filesystemSource), "filesystem");
    const auto file = observe(std::move(*fileSource), "CXC file");
    const auto memory = observe(std::move(*memorySource), "CXC memory");
    const auto typed = observe(std::move(*typedSource), "typed project");
    if (!filesystem || !file || !memory || !typed) {
        return 1;
    }
    if (filesystem->identity != file->identity || filesystem->identity != memory->identity ||
        filesystem->identity != typed->identity || filesystem->digests != file->digests ||
        filesystem->digests != memory->digests || filesystem->digests != typed->digests) {
        return fail("Playback-only reference sources produced different semantic observations");
    }
    if (identityHex(file->identity) != expectedSemanticIdentity ||
        file->digests[1] != expectedStopDigest) {
        return fail("Playback-only reference golden mismatch");
    }

    const auto rationalTwoHalves = taggedIdentity(2, 2);
    const auto rationalOneWhole = taggedIdentity(1, 1);
    if (!verifyNumberOptions() || !rationalTwoHalves || !rationalOneWhole ||
        *rationalTwoHalves != *rationalOneWhole) {
        return fail("Playback-only prepare options contract failed");
    }
    if (!verifyFailedReload()) {
        return fail("Playback-only failed reload changed active state");
    }

    auto animatedFilesystemSource = PlaybackSource::fromFilesystemProject(animatedProject());
    auto animatedFileSource = PlaybackSource::fromCxcFile(animatedPackage());
    const auto animatedBytes = readBytes(animatedPackage());
    auto animatedTyped = typedAnimatedSource();
    if (!animatedFilesystemSource || !animatedFileSource || !animatedBytes || !animatedTyped) {
        return fail("Playback-only consumer could not construct animated sources");
    }
    auto animatedMemorySource = PlaybackSource::fromCxcMemory(*animatedBytes);
    if (!animatedMemorySource) {
        return fail("animated CXC memory source", animatedMemorySource.error());
    }
    const auto animatedFilesystem =
        observeAnimation(std::move(*animatedFilesystemSource), "animated filesystem");
    const auto animatedFile = observeAnimation(std::move(*animatedFileSource), "animated CXC file");
    const auto animatedMemory =
        observeAnimation(std::move(*animatedMemorySource), "animated CXC memory");
    const auto animatedTypedObs =
        observeAnimation(std::move(*animatedTyped), "animated typed project");
    if (!animatedFilesystem || !animatedFile || !animatedMemory || !animatedTypedObs) {
        return 1;
    }
    if (animatedFilesystem->identity != animatedFile->identity ||
        animatedFilesystem->identity != animatedMemory->identity ||
        animatedFilesystem->identity != animatedTypedObs->identity ||
        animatedFilesystem->digests != animatedFile->digests ||
        animatedFilesystem->digests != animatedMemory->digests ||
        animatedFilesystem->digests != animatedTypedObs->digests) {
        return fail("Playback-only animated sources produced different semantic observations");
    }
    if (identityHex(animatedFile->identity) != expectedAnimationIdentity ||
        animatedFile->digests != expectedAnimationDigests) {
        return fail("Playback-only animated golden mismatch");
    }

    std::cout << "Cuexis Playback-only CFU-F2 consumer passed identity="
              << identityHex(file->identity) << " stop_digest=" << file->digests[1] << '\n';
    std::cout << "S4-F animation_identity=" << identityHex(animatedFile->identity)
              << " animation_digests=" << animatedFile->digests[0] << ','
              << animatedFile->digests[1] << ',' << animatedFile->digests[2] << ','
              << animatedFile->digests[3] << '\n';
    return 0;
}
