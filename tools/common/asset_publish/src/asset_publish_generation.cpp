#include "asset_publish_internal.hpp"

#include "publish_fs_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cuexis::tools {

using detail::publishError;
using detail::publishInvalidCode;
using detail::publishIoCode;
using detail::publishReplaceCode;
using detail::publishStagingCode;
using detail::publishValidationCode;

auto publishGenerationLocked(const GenerationPublishRequest& request, bool acquireLock)
    -> core::Result<GenerationPublishResult> {
    if (request.root.empty()) {
        return core::unexpected(publishError(publishInvalidCode, "Generation root is empty"));
    }
    if (!isPortableToken(request.generationId, maxGenerationIdBytes)) {
        return core::unexpected(
            publishError(publishInvalidCode, "Generation id must be a short portable token")
                .withContext("generation_id", request.generationId));
    }
    if (request.entries.empty()) {
        return core::unexpected(
            publishError(publishInvalidCode, "Generation closure must not be empty"));
    }
    auto closure = listEntries(request.entries);
    if (!closure) {
        return core::unexpected(std::move(closure.error()));
    }
    auto provenance = listEntries(request.provenanceRecords);
    if (!provenance) {
        return core::unexpected(std::move(provenance.error()));
    }

    std::error_code status;
    fs::create_directories(request.root, status);
    if (status || !detail::isDirectory(request.root)) {
        return core::unexpected(publishError(publishIoCode, "Generation root could not be created")
                                    .withContext("path", request.root.generic_string()));
    }

    std::optional<PublicationLock> lock;
    if (acquireLock) {
        auto acquired = PublicationLock::acquire(request.root / pathOf(publicationLockName));
        if (!acquired)
            return core::unexpected(std::move(acquired.error()));
        lock.emplace(std::move(*acquired));
    }

    MarkerDocument marker;
    marker.label = request.generationId;
    marker.identity = generationIdentityOf(marker.label, *closure, *provenance);
    marker.closure = *closure;
    marker.provenance = *provenance;

    const auto publishedRoot = request.root / pathOf(generationPublishedDirectory);
    const auto generationPath = publishedRoot / fs::path{marker.identity};
    if (fs::exists(generationPath, status) && !status) {
        // Content-addressed and immutable: an identical republish is a verified no-op, and a
        // different tree under the same identity is refused instead of overwritten.
        auto existing = readMarker(generationPath);
        if (!existing) {
            return core::unexpected(std::move(existing.error()));
        }
        if (!markersEqual(*existing, marker)) {
            return core::unexpected(
                publishError(publishConflictCode,
                             "Published generation does not match the requested content")
                    .withContext("path", generationPath.generic_string()));
        }
        auto verified = verifyGenerationTree(generationPath, *existing);
        if (!verified) {
            return core::unexpected(std::move(verified.error()));
        }
        return GenerationPublishResult{generationPath, marker.identity,
                                       closureByteTotal(marker.closure), marker.closure.size(),
                                       marker.provenance.size()};
    }

    const auto stagingPath =
        request.root / pathOf(generationStagingDirectory) / fs::path{marker.identity};
    detail::removeTreeQuiet(stagingPath);
    fs::create_directories(stagingPath, status);
    if (status) {
        return core::unexpected(
            publishError(publishStagingCode, "Generation staging directory could not be created")
                .withContext("path", stagingPath.generic_string()));
    }
    const auto abandon = [&stagingPath]() { detail::removeTreeQuiet(stagingPath); };

    auto closureWritten = writeEntryTree(stagingPath, request.entries, fs::path{});
    if (!closureWritten) {
        abandon();
        return core::unexpected(std::move(closureWritten.error()));
    }
    auto provenanceWritten =
        writeEntryTree(stagingPath, request.provenanceRecords, pathOf(provenanceDirectoryName));
    if (!provenanceWritten) {
        abandon();
        return core::unexpected(std::move(provenanceWritten.error()));
    }
    const auto markerText = encodeMarker(marker);
    const auto markerBytes =
        std::as_bytes(std::span<const char>{markerText.data(), markerText.size()});
    auto markerWritten =
        detail::writeFileExclusive(stagingPath / pathOf(generationMarkerName), markerBytes);
    if (!markerWritten) {
        abandon();
        return core::unexpected(std::move(markerWritten.error()));
    }

    if (detail::testFailureEnabled("CUEXIS_ASSET_PUBLISH_FAIL_AFTER_STAGING")) {
        abandon();
        return core::unexpected(
            publishError(publishStagingCode, "Injected failure after staging (test hook)"));
    }

    // The staged tree is re-read and re-hashed before it becomes visible.
    auto stagedMarker = readMarker(stagingPath);
    if (!stagedMarker) {
        abandon();
        return core::unexpected(std::move(stagedMarker.error()));
    }
    auto stagedVerified = verifyGenerationTree(stagingPath, *stagedMarker);
    if (!stagedVerified) {
        abandon();
        return core::unexpected(std::move(stagedVerified.error()));
    }
    auto synced = detail::syncDirectory(stagingPath);
    if (!synced) {
        abandon();
        return core::unexpected(std::move(synced.error()));
    }

    if (detail::testFailureEnabled("CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_PUBLISH")) {
        abandon();
        return core::unexpected(
            publishError(publishReplaceCode, "Injected failure before publish (test hook)"));
    }

    fs::create_directories(publishedRoot, status);
    if (status) {
        abandon();
        return core::unexpected(
            publishError(publishStagingCode, "Generation directory could not be created")
                .withContext("path", publishedRoot.generic_string()));
    }
    // The single visible switch: one rename publishes the whole generation.
    auto published = detail::replaceAtomically(stagingPath, generationPath);
    if (!published) {
        abandon();
        auto existing = readMarker(generationPath);
        if (existing) {
            auto verified = verifyGenerationTree(generationPath, *existing);
            if (verified && markersEqual(*existing, marker)) {
                return GenerationPublishResult{generationPath, marker.identity,
                                               closureByteTotal(marker.closure),
                                               marker.closure.size(), marker.provenance.size()};
            }
        }
        return core::unexpected(std::move(published.error()));
    }
    auto publishedSynced = detail::syncDirectory(publishedRoot);
    if (!publishedSynced) {
        return core::unexpected(std::move(publishedSynced.error()));
    }

    std::uint64_t closureBytes = 0;
    for (const auto& entry : marker.closure) {
        closureBytes += entry.byteCount;
    }
    return GenerationPublishResult{generationPath, marker.identity, closureBytes,
                                   marker.closure.size(), marker.provenance.size()};
}

auto adoptGenerationLocked(const fs::path& root, std::string_view generationId, bool acquireLock,
                           bool* commitVisible) -> core::Result<void> {
    if (root.empty() || !detail::isLowerHex(generationId, 64)) {
        return core::unexpected(
            publishError(publishInvalidCode, "Adoption requires a root and a generation identity"));
    }
    const auto generationPath =
        root / pathOf(generationPublishedDirectory) / fs::path{std::string{generationId}};
    auto marker = readMarker(generationPath);
    if (!marker) {
        return core::unexpected(std::move(marker.error()));
    }
    if (marker->identity != generationId) {
        return core::unexpected(
            publishError(publishValidationCode, "Generation marker identity differs from its path")
                .withContext("path", generationPath.generic_string()));
    }
    auto verified = verifyGenerationTree(generationPath, *marker);
    if (!verified) {
        return core::unexpected(std::move(verified.error()));
    }
    std::string text{adoptedGenerationFormat};
    text += ' ';
    text += std::to_string(adoptedGenerationVersion);
    text += '\n';
    text += std::string{generationId};
    text += '\n';
    const auto bytes = std::as_bytes(std::span<const char>{text.data(), text.size()});

    std::optional<PublicationLock> lock;
    if (acquireLock) {
        auto acquired = PublicationLock::acquire(root / pathOf(publicationLockName));
        if (!acquired)
            return core::unexpected(std::move(acquired.error()));
        lock.emplace(std::move(*acquired));
    }
    const auto target = root / pathOf(adoptedGenerationName);
    const auto temporary = detail::uniqueSibling(target, publicationRole);
    auto written = detail::writeFileExclusive(temporary, bytes);
    if (!written) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(written.error()));
    }
    if (detail::testFailureEnabled("CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_ADOPT")) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(
            publishError(publishReplaceCode, "Injected failure before adoption"));
    }
    auto replaced = detail::replaceAtomically(temporary, target);
    if (!replaced) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(replaced.error()));
    }
    if (commitVisible)
        *commitVisible = true;
    if (detail::testFailureEnabled("CUEXIS_ASSET_PUBLISH_FAIL_AFTER_ADOPT")) {
        return core::unexpected(publishError(publishIoCode, "Injected adoption durability failure")
                                    .withContext("commitVisible", "true"));
    }
    auto synced = detail::syncDirectory(root);
    if (!synced)
        synced.error().withContext("commitVisible", "true");
    return synced;
}

} // namespace cuexis::tools
