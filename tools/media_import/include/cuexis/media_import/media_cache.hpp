#pragma once

// Media import cache with full identity revalidation.
//
// The cache key covers every input that can change canonical bytes: the raw source identity, the
// importer profile identity, the exact decoder version and the output-affecting build policy. A hit
// is only accepted after the record is re-derived and the stored artifact is re-hashed, so a
// corrupted or mismatched entry is refused with a stable code instead of being silently
// re-imported. Rebuilding is an explicit offline action, never an implicit fallback.
//
// This is an internal developer tool library: it is not installed and Playback never links it.

#include <cuexis/core/result.hpp>
#include <cuexis/media_import/media_import.hpp>
#include <cuexis/media_import/media_provenance.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::media_import {

inline constexpr std::string_view mediaCacheFormat = "cuexis.media-cache";
inline constexpr std::uint32_t mediaCacheVersion = 1;
// Output-affecting build policy: scalar minimp3, precise floating point and no contraction.
inline constexpr std::string_view mediaCacheBuildOptions =
    "minimp3-no-simd;fp-precise;contract-off";
inline constexpr std::size_t mediaCacheMaxRecordBytes = 64U * 1024U;
inline constexpr std::size_t mediaCacheMaxArtifactBytes = 512ULL * 1024ULL * 1024ULL;

// Every input that can change the canonical artifact bytes.
struct MediaCacheKeyInput final {
    std::string rawSourceIdentity;
    std::string profileIdentity;
    std::string decoderVersion;
    std::string buildOptions{std::string{mediaCacheBuildOptions}};
};

// The key of a request: SHA-256 over a domain-separated, length-prefixed preimage of every input.
[[nodiscard]] auto mediaCacheKey(const MediaCacheKeyInput& input) -> std::string;

struct MediaCacheRecord final {
    std::string key;
    MediaArtifactKind kind{MediaArtifactKind::Texture};
    MediaCacheKeyInput input;
    // The exact decoder that produced the artifact. Not part of the key, because the profile
    // identity already pins every decoder build; it is recorded so a cache hit can still report
    // which decoder produced the bytes it serves.
    std::string decoder;
    std::string artifactIdentity;
    std::uint64_t artifactBytes{};
};

// One accepted cache hit: the validated record and the canonical artifact it describes.
struct MediaCacheHit final {
    MediaCacheRecord record;
    std::vector<std::byte> artifact;
};

[[nodiscard]] auto encodeMediaCacheRecord(const MediaCacheRecord& record) -> std::string;
[[nodiscard]] auto decodeMediaCacheRecord(std::string_view text) -> core::Result<MediaCacheRecord>;
[[nodiscard]] auto validateMediaCacheRecord(const MediaCacheRecord& record) -> core::Result<void>;

class MediaCacheStore final {
  public:
    explicit MediaCacheStore(std::filesystem::path directory);

    [[nodiscard]] auto directory() const noexcept -> const std::filesystem::path&;
    [[nodiscard]] auto recordPath(std::string_view key) const -> std::filesystem::path;
    [[nodiscard]] auto artifactPath(std::string_view artifactIdentity) const
        -> std::filesystem::path;

    // Returns the cached record and artifact for a request. Missing entries report
    // `media.cache.missing`; a record or artifact that does not revalidate reports
    // `media.cache.corrupt` and is never treated as a hit.
    [[nodiscard]] auto load(const MediaCacheKeyInput& input) const -> core::Result<MediaCacheHit>;

    // Stores an artifact and its record. Artifacts are content addressed and immutable: an existing
    // artifact is verified rather than overwritten, and an existing record with different bytes is
    // a conflict that only an explicit discard can clear.
    [[nodiscard]] auto store(const MediaCacheRecord& record,
                             std::span<const std::byte> artifact) const -> core::Result<void>;

    // Explicit offline rebuild support: removes the record for one key. The content-addressed
    // artifact stays, because another key may reference the same bytes.
    [[nodiscard]] auto discard(const MediaCacheKeyInput& input) const -> core::Result<bool>;

    [[nodiscard]] auto recordCount() const -> std::size_t;

  private:
    std::filesystem::path directory_;
};

} // namespace cuexis::media_import
