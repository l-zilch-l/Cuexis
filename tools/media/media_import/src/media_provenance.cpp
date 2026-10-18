#include <cuexis/media_import/media_provenance.hpp>

#include "media_internal.hpp"
#include "media_json_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::media_import {
namespace {

using detail::mediaError;

inline constexpr std::size_t identityHexBytes = 64;
inline constexpr std::size_t maxDecoderVersionBytes = 128;
inline constexpr std::size_t maxRawSourceLocatorBytes = 4096;
inline constexpr std::size_t maxProvenanceFileNameBytes = 512;

[[nodiscard]] auto invalidError(std::string message) -> core::Error {
    return mediaError("media.provenance.invalid", std::move(message));
}

[[nodiscard]] auto unsupportedError(std::string message) -> core::Error {
    return mediaError("media.provenance.unsupported", std::move(message));
}

} // namespace

auto mediaArtifactKindName(MediaArtifactKind kind) noexcept -> std::string_view {
    switch (kind) {
    case MediaArtifactKind::Texture:
        return "texture";
    case MediaArtifactKind::Audio:
        return "audio";
    }
    return "texture";
}

auto parseMediaArtifactKind(std::string_view name) noexcept -> std::optional<MediaArtifactKind> {
    if (name == "texture") {
        return MediaArtifactKind::Texture;
    }
    if (name == "audio") {
        return MediaArtifactKind::Audio;
    }
    return std::nullopt;
}

auto encodeProvenance(const MediaProvenance& provenance) -> std::string {
    std::string out;
    out.reserve(512);
    out += "{\"format\":";
    detail::appendJsonString(out, mediaProvenanceFormat);
    out += ",\"version\":";
    out += std::to_string(mediaProvenanceVersion);
    out += ",\"kind\":";
    detail::appendJsonString(out, mediaArtifactKindName(provenance.kind));
    out += ",\"rawSourceIdentity\":";
    detail::appendJsonString(out, provenance.rawSourceIdentity);
    out += ",\"rawSourceBytes\":";
    out += std::to_string(provenance.rawSourceBytes);
    out += ",\"rawSourceLocator\":";
    detail::appendJsonString(out, provenance.rawSourceLocator);
    out += ",\"profileIdentity\":";
    detail::appendJsonString(out, provenance.profileIdentity);
    out += ",\"decoderVersion\":";
    detail::appendJsonString(out, provenance.decoderVersion);
    out += ",\"artifactIdentity\":";
    detail::appendJsonString(out, provenance.artifactIdentity);
    out += ",\"artifactBytes\":";
    out += std::to_string(provenance.artifactBytes);
    out += ",\"assetId\":";
    detail::appendJsonString(out, provenance.assetId);
    out += "}\n";
    return out;
}

auto validateProvenance(const MediaProvenance& provenance) -> core::Result<void> {
    if (!detail::isLowerHex(provenance.rawSourceIdentity, identityHexBytes)) {
        return core::unexpected(
            invalidError("rawSourceIdentity must be 64 lowercase hexadecimal digits"));
    }
    if (provenance.rawSourceBytes == 0) {
        return core::unexpected(invalidError("rawSourceBytes must not be zero"));
    }
    if (provenance.rawSourceLocator.size() > maxRawSourceLocatorBytes) {
        return core::unexpected(invalidError("rawSourceLocator is too long"));
    }
    if (!detail::isLowerHex(provenance.profileIdentity, identityHexBytes)) {
        return core::unexpected(
            invalidError("profileIdentity must be 64 lowercase hexadecimal digits"));
    }
    if (provenance.decoderVersion.empty() ||
        provenance.decoderVersion.size() > maxDecoderVersionBytes ||
        !detail::isPrintableAscii(provenance.decoderVersion)) {
        return core::unexpected(
            invalidError("decoderVersion must be a short printable ASCII token"));
    }
    if (!detail::isLowerHex(provenance.artifactIdentity, identityHexBytes)) {
        return core::unexpected(
            invalidError("artifactIdentity must be 64 lowercase hexadecimal digits"));
    }
    if (provenance.artifactBytes == 0) {
        return core::unexpected(invalidError("artifactBytes must not be zero"));
    }
    if (!detail::isPortableAssetId(provenance.assetId, mediaProvenanceMaxAssetIdBytes)) {
        return core::unexpected(invalidError("assetId must be a portable asset id, not a path"));
    }
    return {};
}

auto decodeProvenance(std::string_view text) -> core::Result<MediaProvenance> {
    auto reader = detail::JsonObjectReader::parse(text, "media.provenance.invalid");
    if (!reader) {
        return core::unexpected(std::move(reader.error()));
    }
    auto format = reader->takeString("format");
    if (!format) {
        return core::unexpected(std::move(format.error()));
    }
    if (*format != mediaProvenanceFormat) {
        return core::unexpected(
            unsupportedError("provenance document has an unknown format '" + *format + "'"));
    }
    auto version = reader->takeNumber("version");
    if (!version) {
        return core::unexpected(std::move(version.error()));
    }
    if (*version != mediaProvenanceVersion) {
        return core::unexpected(unsupportedError("provenance document version " +
                                                 std::to_string(*version) + " is not supported"));
    }
    auto kind = reader->takeString("kind");
    if (!kind) {
        return core::unexpected(std::move(kind.error()));
    }
    const auto parsedKind = parseMediaArtifactKind(*kind);
    if (!parsedKind.has_value()) {
        return core::unexpected(
            unsupportedError("provenance document has an unknown kind '" + *kind + "'"));
    }

    MediaProvenance provenance;
    provenance.kind = *parsedKind;
    auto rawSourceIdentity = reader->takeString("rawSourceIdentity");
    if (!rawSourceIdentity) {
        return core::unexpected(std::move(rawSourceIdentity.error()));
    }
    provenance.rawSourceIdentity = std::move(*rawSourceIdentity);
    auto rawSourceBytes = reader->takeNumber("rawSourceBytes");
    if (!rawSourceBytes) {
        return core::unexpected(std::move(rawSourceBytes.error()));
    }
    provenance.rawSourceBytes = *rawSourceBytes;
    auto rawSourceLocator = reader->takeString("rawSourceLocator");
    if (!rawSourceLocator) {
        return core::unexpected(std::move(rawSourceLocator.error()));
    }
    provenance.rawSourceLocator = std::move(*rawSourceLocator);
    auto profileIdentity = reader->takeString("profileIdentity");
    if (!profileIdentity) {
        return core::unexpected(std::move(profileIdentity.error()));
    }
    provenance.profileIdentity = std::move(*profileIdentity);
    auto decoderVersion = reader->takeString("decoderVersion");
    if (!decoderVersion) {
        return core::unexpected(std::move(decoderVersion.error()));
    }
    provenance.decoderVersion = std::move(*decoderVersion);
    auto artifactIdentity = reader->takeString("artifactIdentity");
    if (!artifactIdentity) {
        return core::unexpected(std::move(artifactIdentity.error()));
    }
    provenance.artifactIdentity = std::move(*artifactIdentity);
    auto artifactBytes = reader->takeNumber("artifactBytes");
    if (!artifactBytes) {
        return core::unexpected(std::move(artifactBytes.error()));
    }
    provenance.artifactBytes = *artifactBytes;
    auto assetId = reader->takeString("assetId");
    if (!assetId) {
        return core::unexpected(std::move(assetId.error()));
    }
    provenance.assetId = std::move(*assetId);

    if (reader->remaining() != 0) {
        return core::unexpected(invalidError("provenance document has an unknown key '" +
                                             std::string{reader->firstRemainingKey()} + "'"));
    }
    auto validation = validateProvenance(provenance);
    if (!validation) {
        return core::unexpected(std::move(validation.error()));
    }
    return provenance;
}

auto verifyProvenanceArtifact(const MediaProvenance& provenance,
                              std::span<const std::byte> artifact) -> core::Result<void> {
    auto validation = validateProvenance(provenance);
    if (!validation) {
        return core::unexpected(std::move(validation.error()));
    }
    if (static_cast<std::uint64_t>(artifact.size()) != provenance.artifactBytes) {
        return core::unexpected(
            mediaError("media.provenance.artifact_mismatch", "canonical artifact size differs")
                .withContext("expected", std::to_string(provenance.artifactBytes))
                .withContext("actual", std::to_string(artifact.size())));
    }
    const auto identity = contentIdentity(artifact);
    if (identity != provenance.artifactIdentity) {
        return core::unexpected(
            mediaError("media.provenance.artifact_mismatch",
                       "canonical artifact identity differs from the provenance record")
                .withContext("expected", provenance.artifactIdentity)
                .withContext("actual", identity));
    }
    return {};
}

auto makeImageProvenance(std::span<const std::byte> source, const ImageImportResult& result,
                         std::string assetId, std::string rawSourceLocator) -> MediaProvenance {
    MediaProvenance provenance;
    provenance.kind = MediaArtifactKind::Texture;
    provenance.rawSourceIdentity = contentIdentity(source);
    provenance.rawSourceBytes = static_cast<std::uint64_t>(source.size());
    provenance.rawSourceLocator = std::move(rawSourceLocator);
    provenance.profileIdentity = mediaProfileIdentity();
    provenance.decoderVersion = result.info.decoder;
    provenance.artifactIdentity = contentIdentity(result.portableTexture);
    provenance.artifactBytes = static_cast<std::uint64_t>(result.portableTexture.size());
    provenance.assetId = std::move(assetId);
    return provenance;
}

auto makeAudioProvenance(std::span<const std::byte> source, const AudioImportResult& result,
                         std::string assetId, std::string rawSourceLocator) -> MediaProvenance {
    MediaProvenance provenance;
    provenance.kind = MediaArtifactKind::Audio;
    provenance.rawSourceIdentity = contentIdentity(source);
    provenance.rawSourceBytes = static_cast<std::uint64_t>(source.size());
    provenance.rawSourceLocator = std::move(rawSourceLocator);
    provenance.profileIdentity = mediaProfileIdentity();
    provenance.decoderVersion = result.info.decoder;
    provenance.artifactIdentity = contentIdentity(result.canonicalWav);
    provenance.artifactBytes = static_cast<std::uint64_t>(result.canonicalWav.size());
    provenance.assetId = std::move(assetId);
    return provenance;
}

auto encodedAssetIdName(std::string_view assetId) -> core::Result<std::string> {
    if (!detail::isPortableAssetId(assetId, mediaProvenanceMaxAssetIdBytes)) {
        return core::unexpected(invalidError("assetId must be a portable asset id, not a path"));
    }
    constexpr std::string_view hexDigits = "0123456789ABCDEF";
    std::string name;
    name.reserve(assetId.size());
    for (const char character : assetId) {
        const bool safe = (character >= 'A' && character <= 'Z') ||
                          (character >= 'a' && character <= 'z') ||
                          (character >= '0' && character <= '9') || character == '.' ||
                          character == '_' || character == '-';
        if (safe) {
            name.push_back(character);
            continue;
        }
        const auto byte = static_cast<unsigned char>(character);
        name.push_back('%');
        name.push_back(hexDigits[byte >> 4]);
        name.push_back(hexDigits[byte & 0x0FU]);
    }
    if (name.size() > maxProvenanceFileNameBytes) {
        return core::unexpected(invalidError("encoded asset id is too long"));
    }
    return name;
}

auto provenanceFileName(std::string_view assetId) -> core::Result<std::string> {
    auto name = encodedAssetIdName(assetId);
    if (!name) {
        return core::unexpected(std::move(name.error()));
    }
    name->append(".provenance.json");
    if (name->size() > maxProvenanceFileNameBytes) {
        return core::unexpected(invalidError("provenance file name is too long"));
    }
    return std::move(*name);
}

} // namespace cuexis::media_import
