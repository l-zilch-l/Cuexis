#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/playback/presentation.hpp>

#include <filesystem>
#include <iostream>
#include <string_view>
#include <variant>

namespace {

[[nodiscard]] auto fail(std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
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
        return material != nullptr && (!material->baseColorTexture.has_value() ||
                                       !material->baseColorTexture->assetId.empty());
    }
    }
    return false;
}

} // namespace

int main() {
    const auto project = std::filesystem::path{CUEXIS_FIXTURE_DIR} / "stage3_project";
    auto source = cuexis::playback::PlaybackSource::fromFilesystemProject(project);
    if (!source) {
        return fail("Playback-only consumer could not create the stage3 source");
    }

    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    if (!prepared) {
        return fail("Playback-only consumer prepare failed");
    }
    const auto* manifest = prepared->presentationManifest();
    if (manifest == nullptr || manifest->entries.size() != 4U) {
        return fail("Playback-only consumer did not receive the portable manifest");
    }

    const auto validation = prepared->validatePresentation(
        capabilities(), cuexis::playback::PresentationRequest{.enableDebugPass = true});
    if (!validation.hasValue() || !validation.settings || !validation.settings->debugPassEnabled) {
        return fail("Playback-only consumer presentation preflight failed");
    }

    for (const auto& entry : manifest->entries) {
        auto resource = prepared->acquirePresentationResource(entry.reference);
        if (!resource || !*resource || (*resource)->reference != entry.reference ||
            !validResource(**resource)) {
            return fail("Playback-only consumer resource acquisition failed");
        }
    }

    if (!session.commit(std::move(*prepared))) {
        return fail("Playback-only consumer commit failed");
    }
    const auto activeManifest = session.presentationManifest();
    if (!activeManifest || activeManifest->entries.size() != 4U) {
        return fail("Playback-only consumer active manifest failed");
    }

    auto retained = session.acquirePresentationResource(activeManifest->entries.front().reference);
    if (!retained || !*retained || !validResource(**retained)) {
        return fail("Playback-only consumer active resource acquisition failed");
    }

    if (!session.update({.chartTimeMs = 625.0})) {
        return fail("Playback-only consumer update failed");
    }
    const auto frame = session.extractFrame({.width = 1280, .height = 720});
    if (!frame || frame->objects.size() != 2U) {
        return fail("Playback-only consumer frame extraction failed");
    }
    const auto digest = cuexis::playback::computeFrameDigest({.chartTimeMs = 625.0}, *frame);
    const auto repeatDigest = cuexis::playback::computeFrameDigest({.chartTimeMs = 625.0}, *frame);
    constexpr std::uint64_t expectedDigest = 8424169740673868033ULL;
    if (!digest || !repeatDigest || digest->algorithmVersion != 3U ||
        digest->algorithmVersion != repeatDigest->algorithmVersion ||
        digest->value != expectedDigest || digest->value != repeatDigest->value) {
        return fail("Playback-only consumer digest parity failed");
    }

    if (!session.unload() || !validResource(**retained)) {
        return fail("Playback-only consumer owning resource lifetime failed");
    }

    std::cout << "Cuexis Playback-only consumer passed digest=" << digest->value << '\n';
    return 0;
}
