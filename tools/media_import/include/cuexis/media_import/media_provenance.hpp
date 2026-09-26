#pragma once

// Resource identity separation and provenance for Stage 6 E3.
//
// One imported resource has four independent identities, and the pipeline must not conflate them:
//
//   raw source identity   SHA-256 of the original encoded bytes (PNG/JPEG/MP3/Ogg Vorbis/FLAC)
//   profile identity      mediaProfileIdentity(): the versioned conversion profile plus the exact
//                         decoder builds and build options that can change canonical bytes
//   artifact identity     SHA-256 of the canonical artifact (CXPRES01 payload or RIFF/WAVE PCM)
//   resource AssetId      the id the project asset index addresses the resource by
//
// The provenance record is an author-side audit sidecar. It is written next to the published
// generation, it is never part of the runtime resource closure, and Playback never reads it. The
// original encoded source stays on the author side: this record keeps a locator and an identity for
// traceability, while a distributed package carries only the runtime closure.
//
// This is an internal tool library: it is not installed and is not part of the Cuexis SDK.

#include <cuexis/core/result.hpp>
#include <cuexis/media_import/media_import.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace cuexis::media_import {

inline constexpr std::string_view mediaProvenanceFormat = "cuexis.media-provenance";
inline constexpr std::uint32_t mediaProvenanceVersion = 1;

// Longest portable AssetId the provenance sidecar accepts, matching the asset index limit.
inline constexpr std::size_t mediaProvenanceMaxAssetIdBytes = 256;

enum class MediaArtifactKind {
    Texture,
    Audio,
};

[[nodiscard]] auto mediaArtifactKindName(MediaArtifactKind kind) noexcept -> std::string_view;
[[nodiscard]] auto parseMediaArtifactKind(std::string_view name) noexcept
    -> std::optional<MediaArtifactKind>;

struct MediaProvenance final {
    MediaArtifactKind kind{MediaArtifactKind::Texture};

    // Raw input identity. The encoded bytes stay author-side.
    std::string rawSourceIdentity;
    std::uint64_t rawSourceBytes{};
    std::string rawSourceLocator;

    // Conversion identity.
    std::string profileIdentity;
    std::string decoderVersion;

    // Canonical output identity.
    std::string artifactIdentity;
    std::uint64_t artifactBytes{};

    // Resource identity owned by the project asset index.
    std::string assetId;

    friend bool operator==(const MediaProvenance&, const MediaProvenance&) = default;
};

// Canonical JSON: fixed key order, one line, trailing newline. Deterministic for a given record.
[[nodiscard]] auto encodeProvenance(const MediaProvenance& provenance) -> std::string;

// Strict decode. Unknown or duplicate keys, missing keys, malformed identities, a bad kind, a
// non-canonical number, a wrong format or an unsupported version are rejected instead of defaulted.
[[nodiscard]] auto decodeProvenance(std::string_view text) -> core::Result<MediaProvenance>;

// Validates the record on its own: identities are lowercase SHA-256 hex, the profile identity is
// present, and the AssetId is a portable name rather than a path.
[[nodiscard]] auto validateProvenance(const MediaProvenance& provenance) -> core::Result<void>;

// Re-checks a record against the artifact bytes that are about to be published or served from the
// cache: byte count and artifact identity must match.
[[nodiscard]] auto verifyProvenanceArtifact(const MediaProvenance& provenance,
                                            std::span<const std::byte> artifact)
    -> core::Result<void>;

// Provenance records for one finished import. The AssetId and the author-side locator are supplied
// by the caller because neither belongs to the decoder.
[[nodiscard]] auto makeImageProvenance(std::span<const std::byte> source,
                                       const ImageImportResult& result, std::string assetId,
                                       std::string rawSourceLocator) -> MediaProvenance;
[[nodiscard]] auto makeAudioProvenance(std::span<const std::byte> source,
                                       const AudioImportResult& result, std::string assetId,
                                       std::string rawSourceLocator) -> MediaProvenance;

// Single-segment file name for one AssetId: every byte outside [A-Za-z0-9._-] is percent-encoded,
// so `textures/checker` becomes `textures%2Fchecker`. The result is never a path. A non-portable
// AssetId is refused. Publication uses this to derive runtime entry paths from the AssetId.
[[nodiscard]] auto encodedAssetIdName(std::string_view assetId) -> core::Result<std::string>;

// Sidecar file name for one AssetId: the encoded asset id plus `.provenance.json`, so
// `textures/checker` becomes `textures%2Fchecker.provenance.json`.
[[nodiscard]] auto provenanceFileName(std::string_view assetId) -> core::Result<std::string>;

} // namespace cuexis::media_import
