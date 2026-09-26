#include <cuexis/media_import/media_cache.hpp>

#include "media_internal.hpp"
#include "media_json_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cuexis::media_import {
namespace {

namespace fs = std::filesystem;
using detail::mediaError;

inline constexpr std::string_view recordsDirectoryName = "records";
inline constexpr std::string_view artifactsDirectoryName = "artifacts";
inline constexpr std::string_view recordFileExtension = ".json";
inline constexpr std::string_view artifactFileExtension = ".bin";
inline constexpr std::size_t identityHexBytes = 64;

[[nodiscard]] auto corruptError(std::string message) -> core::Error {
    return mediaError("media.cache.corrupt", std::move(message));
}

[[nodiscard]] auto invalidError(std::string message) -> core::Error {
    return mediaError("media.cache.invalid_request", std::move(message));
}

[[nodiscard]] auto ioError(std::string message, const fs::path& path) -> core::Error {
    return mediaError("media.cache.io_failed", std::move(message))
        .withContext("path", path.generic_string());
}

[[nodiscard]] auto conflictError(std::string message, const fs::path& path) -> core::Error {
    return mediaError("media.cache.conflict", std::move(message))
        .withContext("path", path.generic_string());
}

// The key inputs are exactly the fields that can change canonical bytes, so every one of them must
// be present before a key is derived: an absent input would make unrelated requests share a key.
[[nodiscard]] auto validateKeyInput(const MediaCacheKeyInput& input) -> core::Result<void> {
    if (!detail::isLowerHex(input.rawSourceIdentity, identityHexBytes)) {
        return core::unexpected(
            invalidError("raw source identity must be a lowercase SHA-256 hex string"));
    }
    if (!detail::isLowerHex(input.profileIdentity, identityHexBytes)) {
        return core::unexpected(
            invalidError("profile identity must be a lowercase SHA-256 hex string"));
    }
    if (input.decoderVersion.empty() || !detail::isPrintableAscii(input.decoderVersion)) {
        return core::unexpected(
            invalidError("decoder version must be a non-empty printable ASCII string"));
    }
    if (input.buildOptions.empty() || !detail::isPrintableAscii(input.buildOptions)) {
        return core::unexpected(
            invalidError("build options must be a non-empty printable ASCII string"));
    }
    return {};
}

void appendKeyField(std::string& out, std::string_view value) {
    out += std::to_string(value.size());
    out += ':';
    out.append(value);
}

[[nodiscard]] auto readWholeFile(const fs::path& path) -> core::Result<std::vector<std::byte>> {
    std::error_code status;
    if (!fs::is_regular_file(path, status) || status) {
        return core::unexpected(ioError("cache file is missing or not a regular file", path));
    }
    std::ifstream stream{path, std::ios::binary};
    if (!stream.good()) {
        return core::unexpected(ioError("cache file cannot be opened", path));
    }
    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    if (size < 0) {
        return core::unexpected(ioError("cache file size cannot be determined", path));
    }
    stream.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        if (!stream.good()) {
            return core::unexpected(ioError("cache file cannot be read", path));
        }
    }
    return bytes;
}

[[nodiscard]] auto writeWholeFile(const fs::path& path, std::span<const std::byte> bytes)
    -> core::Result<void> {
    std::error_code status;
    fs::create_directories(path.parent_path(), status);
    if (status) {
        return core::unexpected(ioError("cache directory cannot be created", path.parent_path()));
    }
    // Write to a sibling and rename, so a torn write never becomes a readable record.
    fs::path temporary = path;
    temporary += ".partial";
    {
        std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
        if (!stream.good()) {
            return core::unexpected(ioError("cache file cannot be created", temporary));
        }
        if (!bytes.empty()) {
            stream.write(reinterpret_cast<const char*>(bytes.data()),
                         static_cast<std::streamsize>(bytes.size()));
        }
        stream.flush();
        if (!stream.good()) {
            stream.close();
            fs::remove(temporary, status);
            return core::unexpected(ioError("cache file cannot be written", temporary));
        }
    }
    fs::remove(path, status);
    status.clear();
    fs::rename(temporary, path, status);
    if (status) {
        fs::remove(temporary, status);
        return core::unexpected(ioError("cache file cannot be published", path));
    }
    return {};
}

[[nodiscard]] auto asBytes(std::string_view text) -> std::vector<std::byte> {
    const auto* data = reinterpret_cast<const std::byte*>(text.data());
    return std::vector<std::byte>{data, data + text.size()};
}

[[nodiscard]] auto asText(std::span<const std::byte> bytes) -> std::string {
    return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

} // namespace

auto mediaCacheKey(const MediaCacheKeyInput& input) -> std::string {
    // Domain separated and length prefixed, so no field boundary is ambiguous and no other record
    // family can collide with a cache key.
    std::string preimage{"cuexis.media.cache.key.v1"};
    appendKeyField(preimage, input.rawSourceIdentity);
    appendKeyField(preimage, input.profileIdentity);
    appendKeyField(preimage, input.decoderVersion);
    appendKeyField(preimage, input.buildOptions);
    return contentIdentity(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(preimage.data()), preimage.size()});
}

auto encodeMediaCacheRecord(const MediaCacheRecord& record) -> std::string {
    std::string out;
    out.reserve(512);
    out += "{\"format\":";
    detail::appendJsonString(out, mediaCacheFormat);
    out += ",\"version\":";
    out += std::to_string(mediaCacheVersion);
    out += ",\"key\":";
    detail::appendJsonString(out, record.key);
    out += ",\"kind\":";
    detail::appendJsonString(out, mediaArtifactKindName(record.kind));
    out += ",\"rawSourceIdentity\":";
    detail::appendJsonString(out, record.input.rawSourceIdentity);
    out += ",\"profileIdentity\":";
    detail::appendJsonString(out, record.input.profileIdentity);
    out += ",\"decoderVersion\":";
    detail::appendJsonString(out, record.input.decoderVersion);
    out += ",\"buildOptions\":";
    detail::appendJsonString(out, record.input.buildOptions);
    out += ",\"decoder\":";
    detail::appendJsonString(out, record.decoder);
    out += ",\"artifactIdentity\":";
    detail::appendJsonString(out, record.artifactIdentity);
    out += ",\"artifactBytes\":";
    out += std::to_string(record.artifactBytes);
    out += "}\n";
    return out;
}

auto decodeMediaCacheRecord(std::string_view text) -> core::Result<MediaCacheRecord> {
    if (text.size() > mediaCacheMaxRecordBytes) {
        return core::unexpected(corruptError("cache record exceeds the byte limit"));
    }
    auto reader = detail::JsonObjectReader::parse(text, std::string{"media.cache.corrupt"});
    if (!reader) {
        return core::unexpected(std::move(reader.error()));
    }
    auto format = reader->takeString("format");
    if (!format) {
        return core::unexpected(std::move(format.error()));
    }
    if (*format != mediaCacheFormat) {
        return core::unexpected(
            mediaError("media.cache.unsupported", "unsupported cache record format")
                .withContext("format", *format));
    }
    auto version = reader->takeNumber("version");
    if (!version) {
        return core::unexpected(std::move(version.error()));
    }
    if (*version != mediaCacheVersion) {
        return core::unexpected(
            mediaError("media.cache.unsupported", "unsupported cache record version")
                .withContext("version", std::to_string(*version)));
    }

    MediaCacheRecord record;
    auto key = reader->takeString("key");
    if (!key) {
        return core::unexpected(std::move(key.error()));
    }
    record.key = std::move(*key);
    auto kind = reader->takeString("kind");
    if (!kind) {
        return core::unexpected(std::move(kind.error()));
    }
    const auto parsedKind = parseMediaArtifactKind(*kind);
    if (!parsedKind) {
        return core::unexpected(corruptError("cache record has an unknown artifact kind"));
    }
    record.kind = *parsedKind;
    auto rawSourceIdentity = reader->takeString("rawSourceIdentity");
    if (!rawSourceIdentity) {
        return core::unexpected(std::move(rawSourceIdentity.error()));
    }
    record.input.rawSourceIdentity = std::move(*rawSourceIdentity);
    auto profileIdentity = reader->takeString("profileIdentity");
    if (!profileIdentity) {
        return core::unexpected(std::move(profileIdentity.error()));
    }
    record.input.profileIdentity = std::move(*profileIdentity);
    auto decoderVersion = reader->takeString("decoderVersion");
    if (!decoderVersion) {
        return core::unexpected(std::move(decoderVersion.error()));
    }
    record.input.decoderVersion = std::move(*decoderVersion);
    auto buildOptions = reader->takeString("buildOptions");
    if (!buildOptions) {
        return core::unexpected(std::move(buildOptions.error()));
    }
    record.input.buildOptions = std::move(*buildOptions);
    auto decoder = reader->takeString("decoder");
    if (!decoder) {
        return core::unexpected(std::move(decoder.error()));
    }
    record.decoder = std::move(*decoder);
    auto artifactIdentity = reader->takeString("artifactIdentity");
    if (!artifactIdentity) {
        return core::unexpected(std::move(artifactIdentity.error()));
    }
    record.artifactIdentity = std::move(*artifactIdentity);
    auto artifactBytes = reader->takeNumber("artifactBytes");
    if (!artifactBytes) {
        return core::unexpected(std::move(artifactBytes.error()));
    }
    record.artifactBytes = *artifactBytes;

    if (reader->remaining() != 0) {
        return core::unexpected(corruptError("cache record has an unknown key '" +
                                             std::string{reader->firstRemainingKey()} + "'"));
    }
    auto validation = validateMediaCacheRecord(record);
    if (!validation) {
        return core::unexpected(std::move(validation.error()));
    }
    return record;
}

auto validateMediaCacheRecord(const MediaCacheRecord& record) -> core::Result<void> {
    if (!detail::isLowerHex(record.key, identityHexBytes)) {
        return core::unexpected(corruptError("cache record key must be a lowercase SHA-256 hex"));
    }
    auto input = validateKeyInput(record.input);
    if (!input) {
        return core::unexpected(corruptError(std::string{input.error().message()}));
    }
    if (!detail::isLowerHex(record.artifactIdentity, identityHexBytes)) {
        return core::unexpected(
            corruptError("cache artifact identity must be a lowercase SHA-256 hex"));
    }
    if (record.decoder.empty() || !detail::isPrintableAscii(record.decoder)) {
        return core::unexpected(
            corruptError("cache record must name a non-empty printable ASCII decoder"));
    }
    if (record.artifactBytes == 0 || record.artifactBytes > mediaCacheMaxArtifactBytes) {
        return core::unexpected(corruptError("cache artifact byte count is out of range"));
    }
    // The key is a pure function of the recorded inputs, so a record whose key does not match its
    // own inputs was edited or produced by a different key derivation.
    if (mediaCacheKey(record.input) != record.key) {
        return core::unexpected(
            corruptError("cache record key does not match the inputs it records"));
    }
    return {};
}

MediaCacheStore::MediaCacheStore(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

auto MediaCacheStore::directory() const noexcept -> const std::filesystem::path& {
    return directory_;
}

auto MediaCacheStore::recordPath(std::string_view key) const -> std::filesystem::path {
    return directory_ / recordsDirectoryName /
           (std::string{key} + std::string{recordFileExtension});
}

auto MediaCacheStore::artifactPath(std::string_view artifactIdentity) const
    -> std::filesystem::path {
    return directory_ / artifactsDirectoryName /
           (std::string{artifactIdentity} + std::string{artifactFileExtension});
}

auto MediaCacheStore::load(const MediaCacheKeyInput& input) const -> core::Result<MediaCacheHit> {
    auto validation = validateKeyInput(input);
    if (!validation) {
        return core::unexpected(std::move(validation.error()));
    }
    const auto key = mediaCacheKey(input);
    const auto path = recordPath(key);
    std::error_code status;
    if (!fs::exists(path, status) || status) {
        return core::unexpected(
            mediaError("media.cache.missing", "no cache record for this import request")
                .withContext("key", key));
    }
    auto recordBytes = readWholeFile(path);
    if (!recordBytes) {
        return core::unexpected(std::move(recordBytes.error()));
    }
    auto record = decodeMediaCacheRecord(asText(*recordBytes));
    if (!record) {
        return core::unexpected(std::move(record.error()));
    }
    // Revalidate against the request, not just against itself.
    if (record->key != key || record->input.rawSourceIdentity != input.rawSourceIdentity ||
        record->input.profileIdentity != input.profileIdentity ||
        record->input.decoderVersion != input.decoderVersion ||
        record->input.buildOptions != input.buildOptions) {
        return core::unexpected(corruptError("cache record does not describe the requested import")
                                    .withContext("key", key));
    }
    const auto artifact = artifactPath(record->artifactIdentity);
    auto bytes = readWholeFile(artifact);
    if (!bytes) {
        return core::unexpected(corruptError("cache artifact referenced by the record is missing")
                                    .withContext("path", artifact.generic_string()));
    }
    if (static_cast<std::uint64_t>(bytes->size()) != record->artifactBytes) {
        return core::unexpected(corruptError("cached artifact size differs from the record")
                                    .withContext("expected", std::to_string(record->artifactBytes))
                                    .withContext("actual", std::to_string(bytes->size())));
    }
    const auto identity = contentIdentity(*bytes);
    if (identity != record->artifactIdentity) {
        return core::unexpected(corruptError("cached artifact identity differs from the record")
                                    .withContext("expected", record->artifactIdentity)
                                    .withContext("actual", identity));
    }
    return MediaCacheHit{std::move(*record), std::move(*bytes)};
}

auto MediaCacheStore::store(const MediaCacheRecord& record,
                            std::span<const std::byte> artifact) const -> core::Result<void> {
    auto validation = validateMediaCacheRecord(record);
    if (!validation) {
        return core::unexpected(std::move(validation.error()));
    }
    if (static_cast<std::uint64_t>(artifact.size()) != record.artifactBytes) {
        return core::unexpected(invalidError("artifact size does not match the cache record")
                                    .withContext("expected", std::to_string(record.artifactBytes))
                                    .withContext("actual", std::to_string(artifact.size())));
    }
    const auto identity = contentIdentity(artifact);
    if (identity != record.artifactIdentity) {
        return core::unexpected(invalidError("artifact identity does not match the cache record")
                                    .withContext("expected", record.artifactIdentity)
                                    .withContext("actual", identity));
    }

    const auto artifactPathValue = artifactPath(record.artifactIdentity);
    std::error_code status;
    if (fs::exists(artifactPathValue, status) && !status) {
        // Content addressed artifacts are immutable: an existing one is verified, never replaced.
        auto existing = readWholeFile(artifactPathValue);
        if (!existing) {
            return core::unexpected(std::move(existing.error()));
        }
        if (contentIdentity(*existing) != record.artifactIdentity) {
            return core::unexpected(conflictError(
                "existing cache artifact does not match its content address", artifactPathValue));
        }
    } else {
        auto written = writeWholeFile(artifactPathValue, artifact);
        if (!written) {
            return core::unexpected(std::move(written.error()));
        }
    }

    const auto recordPathValue = recordPath(record.key);
    if (fs::exists(recordPathValue, status) && !status) {
        auto existing = readWholeFile(recordPathValue);
        if (!existing) {
            return core::unexpected(std::move(existing.error()));
        }
        if (asText(*existing) != encodeMediaCacheRecord(record)) {
            // A different result for the same inputs means one of the two is wrong. Refuse and let
            // the caller discard explicitly rather than overwrite evidence.
            return core::unexpected(conflictError(
                "cache already holds a different record for these inputs", recordPathValue));
        }
        return {};
    }
    const auto encoded = encodeMediaCacheRecord(record);
    auto written = writeWholeFile(recordPathValue, asBytes(encoded));
    if (!written) {
        return core::unexpected(std::move(written.error()));
    }
    return {};
}

auto MediaCacheStore::discard(const MediaCacheKeyInput& input) const -> core::Result<bool> {
    auto validation = validateKeyInput(input);
    if (!validation) {
        return core::unexpected(std::move(validation.error()));
    }
    const auto path = recordPath(mediaCacheKey(input));
    std::error_code status;
    const bool removed = fs::remove(path, status);
    if (status) {
        return core::unexpected(ioError("cache record cannot be removed", path));
    }
    return removed;
}

auto MediaCacheStore::recordCount() const -> std::size_t {
    std::error_code status;
    const auto records = directory_ / recordsDirectoryName;
    if (!fs::is_directory(records, status) || status) {
        return 0;
    }
    std::size_t count = 0;
    for (const auto& entry : fs::directory_iterator{records, status}) {
        if (status) {
            return count;
        }
        if (entry.is_regular_file(status) && !status) {
            ++count;
        }
    }
    return count;
}

} // namespace cuexis::media_import
