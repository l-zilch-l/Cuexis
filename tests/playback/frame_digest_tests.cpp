#include <cuexis/playback/frame_digest.hpp>

#include "frame_digest_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace {

[[nodiscard]] auto digestSnapshot() -> cuexis::playback::FrameSnapshot {
    cuexis::playback::FrameSnapshot snapshot;
    snapshot.viewportWidth = 1280;
    snapshot.viewportHeight = 720;
    snapshot.objects.push_back({.id = "object.a", .hasTransform = true});
    snapshot.objects[0].worldMatrix[0] = 1.0F;
    snapshot.objects[0].worldMatrix[5] = 1.0F;
    snapshot.objects[0].worldMatrix[10] = 1.0F;
    snapshot.objects[0].worldMatrix[15] = 1.0F;
    return snapshot;
}

[[nodiscard]] auto reference(cuexis::playback::PresentationResourceType type, std::string assetId,
                             bool reverseIdentity = false)
    -> cuexis::playback::PresentationResourceRef {
    cuexis::playback::PresentationResourceRef reference;
    reference.type = type;
    reference.assetId = std::move(assetId);
    for (std::size_t index = 0; index < reference.identity.sha256.size(); ++index) {
        reference.identity.sha256[index] = static_cast<std::uint8_t>(
            reverseIdentity ? reference.identity.sha256.size() - 1 - index : index);
    }
    return reference;
}

} // namespace

TEST_CASE("FrameDigest preserves v1 and v2 historical definitions",
          "[playback][frame-digest][compatibility]") {
    const cuexis::playback::RuntimeFrame frame{100.0, 16.0, 3};
    const auto snapshot = digestSnapshot();
    const auto v1 = cuexis::playback::detail::computeFrameDigestVersion(1, frame, snapshot);
    const auto v2 = cuexis::playback::detail::computeFrameDigestVersion(2, frame, snapshot);
    REQUIRE(v1.has_value());
    REQUIRE(v2.has_value());
    CHECK(v1->algorithmVersion == 1);
    CHECK(v1->value == 7174372098521454160ULL);
    CHECK(v2->algorithmVersion == 2);
    CHECK(v2->value == 7850652359432829177ULL);
}

TEST_CASE("FrameDigest fixed-version entry rejects unsupported algorithm versions",
          "[playback][frame-digest][version]") {
    const cuexis::playback::RuntimeFrame frame{100.0, 16.0, 3};
    const auto snapshot = digestSnapshot();

    // The internal fixed-version entry is the compatibility seam for historical digests. It must
    // reject an unknown version explicitly instead of silently hashing with the current one.
    const auto future = cuexis::playback::detail::computeFrameDigestVersion(4, frame, snapshot);
    REQUIRE_FALSE(future.has_value());
    CHECK(future.error().code() == "playback.frame_digest.version_unsupported");
    const auto zero = cuexis::playback::detail::computeFrameDigestVersion(0, frame, snapshot);
    REQUIRE_FALSE(zero.has_value());
    CHECK(zero.error().code() == "playback.frame_digest.version_unsupported");

    // The public default version and the historical versions keep their algorithms.
    const auto publicDigest = cuexis::playback::computeFrameDigest(frame, snapshot);
    REQUIRE(publicDigest.has_value());
    CHECK(publicDigest->algorithmVersion == 3);
    const auto fixedV3 = cuexis::playback::detail::computeFrameDigestVersion(3, frame, snapshot);
    REQUIRE(fixedV3.has_value());
    CHECK(fixedV3->value == publicDigest->value);
}

TEST_CASE("FrameDigest v3 hashes portable ref presence, type, ID, and identity",
          "[playback][frame-digest][portable]") {
    const cuexis::playback::RuntimeFrame frame{100.0, 16.0, 3};
    auto snapshot = digestSnapshot();
    snapshot.objects[0].materialAssetId = "material.a";
    snapshot.objects[0].mesh =
        reference(cuexis::playback::PresentationResourceType::Mesh, "mesh.a");
    snapshot.objects[0].material =
        reference(cuexis::playback::PresentationResourceType::UnlitMaterial, "material.a", true);

    const auto digest = cuexis::playback::computeFrameDigest(frame, snapshot);
    REQUIRE(digest.has_value());
    CHECK(digest->algorithmVersion == 3);
    CHECK(digest->value == 9547878342828936930ULL);
    const auto golden = digest->value;

    auto changed = snapshot;
    changed.objects[0].mesh.reset();
    CHECK(cuexis::playback::computeFrameDigest(frame, changed)->value != golden);
    changed = snapshot;
    changed.objects[0].mesh->type = cuexis::playback::PresentationResourceType::Texture2D;
    CHECK(cuexis::playback::computeFrameDigest(frame, changed)->value != golden);
    changed = snapshot;
    changed.objects[0].mesh->assetId = "mesh.b";
    CHECK(cuexis::playback::computeFrameDigest(frame, changed)->value != golden);
    changed = snapshot;
    changed.objects[0].mesh->identity.sha256[7] ^= 0xFFU;
    CHECK(cuexis::playback::computeFrameDigest(frame, changed)->value != golden);
    changed = snapshot;
    changed.objects[0].material->identity.sha256[23] ^= 0xFFU;
    CHECK(cuexis::playback::computeFrameDigest(frame, changed)->value != golden);
}
