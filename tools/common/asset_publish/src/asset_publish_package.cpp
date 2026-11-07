#include "asset_publish_internal.hpp"

#include "publish_fs_internal.hpp"

#include <cuexis/core/diagnostic.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cuexis::tools {

using detail::publishConflictCode;
using detail::publishError;
using detail::publishInvalidCode;
using detail::publishIoCode;
using detail::publishReplaceCode;
using detail::publishStagingCode;
using detail::publishValidationCode;

// --- CXC package publication -------------------------------------------------------------------

namespace {

struct BuiltPackage final {
    std::vector<std::byte> bytes;
    std::string identity;
};

[[nodiscard]] auto buildPackage(const std::vector<cxc::CxcWriteEntry>& entries,
                                const std::string& extensionsJson,
                                const std::vector<cxc::CxcRequiredExtension>& requiredExtensions,
                                const cxc::CxcPackageLimits& limits) -> core::Result<BuiltPackage> {
    cxc::CxcWriteRequest request;
    request.entries = entries;
    request.extensionsJson = extensionsJson;
    request.requiredExtensions = requiredExtensions;
    auto written = cxc::CxcWriter::write(std::move(request), limits);
    if (!written.hasValue() || !written.bytes.has_value()) {
        return core::unexpected(
            publishError(publishValidationCode, "CXC writer rejected the package: " +
                                                    diagnosticsMessage(written.diagnostics)));
    }
    auto loaded = cxc::CxcPackageLoader::loadMemory(
        std::span<const std::byte>{written.bytes->data(), written.bytes->size()}, limits);
    if (!loaded.package.has_value()) {
        return core::unexpected(
            publishError(publishValidationCode, "CXC package self-validation failed: " +
                                                    diagnosticsMessage(loaded.diagnostics)));
    }
    return BuiltPackage{std::move(*written.bytes), loaded.package->identity().hex()};
}

// Writes one package to a same-directory temporary file, re-validates the bytes on disk through the
// production loader, and only then replaces the target in one step. The target is therefore always
// either the previous package or the new one; there is no interval in which it is absent.
[[nodiscard]] auto commitPackage(const fs::path& target, const BuiltPackage& built,
                                 const cxc::CxcPackageLimits& limits, bool& replacedExisting)
    -> core::Result<void> {
    const auto parent = target.parent_path();
    if (!detail::isDirectory(parent)) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package target directory does not exist")
                .withContext("path", parent.generic_string()));
    }
    // A package path must be a file. An existing directory (or any other non-file) is refused
    // instead of being renamed away and deleted, because that would silently destroy a path the
    // caller never identified as a package.
    std::error_code existingStatus;
    if (fs::exists(target, existingStatus) && !existingStatus && !detail::isRegularFile(target)) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package target exists and is not a regular file")
                .withContext("path", target.generic_string()));
    }
    const auto temporary = detail::uniqueSibling(target, publicationRole);
    detail::removeTreeQuiet(temporary);
    if (detail::testFailureTargetMatches("CUEXIS_ASSET_PUBLISH_FAIL_WRITE_TARGET",
                                         target.filename().string())) {
        // A write that the operating system would refuse with a full or read-only volume. The
        // failure is reported through the same code path as a real `writeFileExclusive` failure,
        // so the case proves the transaction cleans up and leaves the previous package in place.
        return core::unexpected(
            publishError(publishIoCode, "Injected package write failure (test hook)")
                .withContext("path", target.generic_string()));
    }
    auto written = detail::writeFileExclusive(temporary, built.bytes);
    if (!written) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(written.error()));
    }
    auto reloaded = cxc::CxcPackageLoader::loadFile(temporary, limits);
    if (!reloaded.package.has_value()) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(
            publishError(publishValidationCode, "Published package bytes failed validation: " +
                                                    diagnosticsMessage(reloaded.diagnostics)));
    }
    if (reloaded.package->identity().hex() != built.identity) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(
            publishError(publishValidationCode, "Published package identity changed on disk")
                .withContext("path", target.generic_string()));
    }

    if (detail::testFailureEnabled("CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_REPLACE") ||
        detail::testFailureTargetMatches("CUEXIS_ASSET_PUBLISH_FAIL_REPLACE_TARGET",
                                         target.filename().string())) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(
            publishError(publishReplaceCode, "Injected failure before replace (test hook)"));
    }

    // A crash in the window this contract forbids would leave the target absent. The single
    // replacement below has no such window, and this probe makes that visible: it removes the
    // temporary file, so the replacement cannot succeed, and the case can then assert that the
    // previous package is still the one on disk rather than a missing path.
    if (detail::testFailureTargetMatches("CUEXIS_ASSET_PUBLISH_FAIL_TEMPORARY_TARGET",
                                         target.filename().string())) {
        detail::removeTreeQuiet(temporary);
    }

    std::error_code status;
    if (fs::exists(target, status) && !status) {
        replacedExisting = true;
    }
    // One atomic replacement, and only one. Moving the previous package aside first would open a
    // window in which the target does not exist: a crash between the two renames would leave
    // neither the new package nor the last valid one, which is exactly the outcome this contract
    // forbids. `replaceAtomically` overwrites the target in a single step, so the target is always
    // either the previous package or the new one, never absent.
    auto committed = detail::replaceAtomically(temporary, target);
    if (!committed) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(committed.error()));
    }
    auto synced = detail::syncDirectory(parent);
    if (!synced) {
        return core::unexpected(std::move(synced.error()));
    }
    return {};
}

// Puts a package target back to the bytes captured before a pair publication. A target that did not
// exist before is removed; a target that existed is restored byte for byte.
[[nodiscard]] auto rollbackPackage(const fs::path& target,
                                   const std::optional<std::vector<std::byte>>& previous)
    -> core::Result<void> {
    if (!previous) {
        detail::removeTreeQuiet(target);
        return {};
    }
    auto temporary = detail::uniqueSibling(target, "rollback");
    auto written = detail::writeFileExclusive(temporary, *previous);
    if (!written) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(written.error()));
    }
    auto replaced = detail::replaceAtomically(temporary, target);
    if (!replaced) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(replaced.error()));
    }
    return detail::syncDirectory(target.parent_path());
}

} // namespace

auto publishPackage(const PackagePublishRequest& request) -> core::Result<PackagePublishResult> {
    if (request.target.empty() || request.entries.empty()) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package publication requires a target and entries"));
    }
    if (!detail::isDirectory(request.target.parent_path())) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package target directory does not exist")
                .withContext("path", request.target.parent_path().generic_string()));
    }
    auto built = buildPackage(request.entries, request.extensionsJson, request.requiredExtensions,
                              request.limits);
    if (!built) {
        return core::unexpected(std::move(built.error()));
    }
    auto lock =
        PublicationLock::acquire(request.target.parent_path() / pathOf(publicationLockName));
    if (!lock) {
        return core::unexpected(std::move(lock.error()));
    }
    bool replacedExisting = false;
    auto committed = commitPackage(request.target, *built, request.limits, replacedExisting);
    if (!committed) {
        return core::unexpected(std::move(committed.error()));
    }
    return PackagePublishResult{request.target, built->identity,
                                static_cast<std::uint64_t>(built->bytes.size()), replacedExisting};
}

auto publishPackagePair(const PackagePairRequest& request) -> core::Result<PackagePairResult> {
    if (request.v4Target.empty() || request.candidateTarget.empty() || request.entries.empty()) {
        return core::unexpected(publishError(
            publishInvalidCode, "Package pair publication requires two targets and entries"));
    }
    if (request.v4Target == request.candidateTarget) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package pair targets must differ"));
    }
    if (!detail::isDirectory(request.v4Target.parent_path())) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package target directory does not exist")
                .withContext("path", request.v4Target.parent_path().generic_string()));
    }
    // One batch, two packages: the candidate closure is the shared closure plus the entries its own
    // extension declares, and both packages are built and validated before either target is
    // touched.
    auto v4 = buildPackage(request.entries, "{}", request.requiredExtensions, request.limits);
    if (!v4) {
        return core::unexpected(std::move(v4.error()));
    }
    std::vector<cxc::CxcWriteEntry> candidateEntries = request.entries;
    candidateEntries.insert(candidateEntries.end(), request.candidateEntries.begin(),
                            request.candidateEntries.end());
    auto candidate = buildPackage(candidateEntries, request.candidateExtensionsJson,
                                  request.requiredExtensions, request.limits);
    if (!candidate) {
        return core::unexpected(std::move(candidate.error()));
    }
    // The transaction writes both targets, and they need not share a parent directory. Locking only
    // the v4 parent would leave a concurrent writer free to replace the candidate target inside
    // this transaction. Lock the whole target set, in a deterministic order.
    auto lock = PublicationLock::acquireAll(
        {request.v4Target.parent_path() / pathOf(publicationLockName),
         request.candidateTarget.parent_path() / pathOf(publicationLockName)});
    if (!lock) {
        return core::unexpected(std::move(lock.error()));
    }

    // The pair is transactional, so the previous v4 package is captured before it is replaced. A
    // failure of the second replacement restores those exact bytes: the last valid package must
    // survive the failure, and deleting it would be as wrong as leaving a half-updated pair.
    std::optional<std::vector<std::byte>> previousV4;
    if (detail::isRegularFile(request.v4Target)) {
        auto previous = detail::readFileBytes(request.v4Target, request.limits.maxPackageBytes);
        if (!previous) {
            return core::unexpected(std::move(previous.error()));
        }
        previousV4 = std::move(*previous);
    }

    bool v4Replaced = false;
    auto v4Committed = commitPackage(request.v4Target, *v4, request.limits, v4Replaced);
    if (!v4Committed) {
        return core::unexpected(std::move(v4Committed.error()));
    }
    bool candidateReplaced = false;
    auto candidateCommitted =
        commitPackage(request.candidateTarget, *candidate, request.limits, candidateReplaced);
    if (!candidateCommitted) {
        auto rolledBack = rollbackPackage(request.v4Target, previousV4);
        if (!rolledBack) {
            return core::unexpected(
                publishError(publishReplaceCode,
                             "Candidate package publication failed and the v4 package could not be "
                             "rolled back")
                    .withCause(std::move(candidateCommitted.error())));
        }
        return core::unexpected(
            publishError(publishReplaceCode,
                         "Candidate package publication failed; the v4 package was rolled back")
                .withCause(std::move(candidateCommitted.error())));
    }
    PackagePairResult result;
    result.v4 = PackagePublishResult{request.v4Target, v4->identity,
                                     static_cast<std::uint64_t>(v4->bytes.size()), v4Replaced};
    result.candidate = PackagePublishResult{request.candidateTarget, candidate->identity,
                                            static_cast<std::uint64_t>(candidate->bytes.size()),
                                            candidateReplaced};
    return result;
}

} // namespace cuexis::tools
