#include <cuexis/tools/cxc_tool.hpp>

#include "cxc_tool_internal.hpp"

#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/filesystem/secure_file.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <utility>

namespace cuexis::tools {
namespace {

[[nodiscard]] auto prepareTarget(const std::filesystem::path& outputPath)
    -> std::variant<bool, CxcToolResult> {
    const auto parent =
        outputPath.parent_path().empty() ? std::filesystem::path{"."} : outputPath.parent_path();
    if (!detail::isSafeExistingPath(parent, true)) {
        return detail::failure("cxc.unpack.output_parent_unavailable", "$/output",
                               "Unpack output parent must be a stable directory");
    }
    std::error_code error;
    const auto status = std::filesystem::symlink_status(outputPath, error);
    if (error) {
        if (error == std::errc::no_such_file_or_directory) {
            return false;
        }
        return detail::failure("cxc.unpack.output_inspection_failed", "$/output",
                               "Unpack output target could not be inspected");
    }
    if (!std::filesystem::exists(status)) {
        return false;
    }
    if (!std::filesystem::is_directory(status) || !detail::isSafeExistingPath(outputPath, true)) {
        return detail::failure("cxc.unpack.output_invalid", "$/output",
                               "Existing unpack output must be a stable directory");
    }
    const auto iterator = std::filesystem::directory_iterator(outputPath, error);
    if (error) {
        return detail::failure("cxc.unpack.output_inspection_failed", "$/output",
                               "Unpack output target could not be enumerated");
    }
    if (iterator != std::filesystem::directory_iterator{}) {
        return detail::failure("cxc.unpack.output_not_empty", "$/output",
                               "Unpack output target must be empty");
    }
    return true;
}

[[nodiscard]] auto writeStagingTree(const cxc::CxcPackage& package,
                                    const std::filesystem::path& staging)
    -> std::optional<CxcToolResult> {
    std::error_code error;
    if (!std::filesystem::create_directory(staging, error) || error ||
        !detail::isSafeExistingPath(staging, true)) {
        return detail::failure("cxc.unpack.staging_create_failed", "$/output",
                               "Unpack staging directory could not be created");
    }

    std::size_t writtenEntries = 0;
    for (const auto& entry : package.manifest().entries) {
        const auto bytes = package.entryBytes(entry.path);
        if (!bytes) {
            return detail::failure("cxc.unpack.entry_missing", "$/entries",
                                   "Validated package entry bytes are unavailable", "path",
                                   entry.path);
        }
        const auto target = staging / std::filesystem::path{entry.path};
        if (!detail::containsPath(staging, target)) {
            return detail::failure("cxc.unpack.path_escape", "$/entries",
                                   "Package entry escaped the staging directory", "path",
                                   entry.path);
        }
        std::filesystem::create_directories(target.parent_path(), error);
        if (error || !detail::isSafeExistingPath(target.parent_path(), true) ||
            !detail::writeBytes(target, *bytes)) {
            return detail::failure("cxc.unpack.entry_write_failed", "$/entries",
                                   "Package entry could not be written", "path", entry.path);
        }
        auto verified = filesystem::readBoundedFile(
            target, {.root = staging,
                     .maxBytes = std::max<std::size_t>(bytes->size(), 1U),
                     .errors = {.rootUnavailable = "cxc.unpack.staging_changed",
                                .rootChanged = "cxc.unpack.staging_changed",
                                .openFailed = "cxc.unpack.entry_verify_failed",
                                .outsideRoot = "cxc.unpack.path_escape",
                                .notRegular = "cxc.unpack.entry_verify_failed",
                                .tooLarge = "cxc.unpack.entry_verify_failed",
                                .readFailed = "cxc.unpack.entry_verify_failed",
                                .changedDuringRead = "cxc.unpack.entry_verify_failed"}});
        if (!verified || !std::ranges::equal(verified->bytes, *bytes)) {
            return detail::failure("cxc.unpack.entry_verify_failed", "$/entries",
                                   "Package entry failed write verification", "path", entry.path);
        }
        ++writtenEntries;
        if (writtenEntries == 1U &&
            detail::testFailureEnabled("CUEXIS_CXC_UNPACK_FAIL_AFTER_ENTRY")) {
            return detail::failure("cxc.unpack.staging_failed", "$/output",
                                   "Unpack staging failed");
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool restoreOutputBackup(const std::filesystem::path& backup,
                                       const std::filesystem::path& outputPath) {
    if (detail::testFailureEnabled("CUEXIS_CXC_UNPACK_FAIL_RESTORE")) {
        return false;
    }
    std::error_code error;
    std::filesystem::rename(backup, outputPath, error);
    return !error;
}

[[nodiscard]] auto outputRestoreFailure() -> CxcToolResult {
    return detail::failure(
        "cxc.unpack.output_restore_failed", "$/output",
        "Original unpack output could not be restored; its backup was preserved");
}

[[nodiscard]] auto commitStaging(const std::filesystem::path& staging,
                                 const std::filesystem::path& outputPath, bool existed)
    -> std::optional<CxcToolResult> {
    const auto backup = detail::uniqueSibling(outputPath, "unpack-backup");
    std::error_code error;
    if (existed) {
        if (backup.empty()) {
            return detail::failure("cxc.unpack.output_commit_failed", "$/output",
                                   "Unpack output backup path could not be reserved");
        }
        std::filesystem::rename(outputPath, backup, error);
        if (error) {
            return detail::failure("cxc.unpack.output_commit_failed", "$/output",
                                   "Existing empty output could not be staged for replacement");
        }
        if (detail::testFailureEnabled("CUEXIS_CXC_UNPACK_FAIL_AFTER_BACKUP")) {
            if (!restoreOutputBackup(backup, outputPath)) {
                return outputRestoreFailure();
            }
            return detail::failure("cxc.unpack.output_commit_failed", "$/output",
                                   "Unpack output commit failed");
        }
    }

    std::filesystem::rename(staging, outputPath, error);
    if (error) {
        if (existed && !restoreOutputBackup(backup, outputPath)) {
            return outputRestoreFailure();
        }
        return detail::failure("cxc.unpack.output_commit_failed", "$/output",
                               "Unpack staging directory could not be committed");
    }
    if (existed) {
        detail::removeTree(backup);
    }
    return std::nullopt;
}

} // namespace

auto unpackCxc(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath)
    -> CxcToolResult {
    if (inputPath.empty() || outputPath.empty()) {
        return detail::failure("cxc.unpack.arguments_invalid", "$",
                               "Unpack requires input and output paths");
    }
    if (detail::samePath(inputPath, outputPath)) {
        return detail::failure("cxc.unpack.path_conflict", "$/output",
                               "Unpack input and output paths must be distinct");
    }
    auto loaded = detail::loadPackage(inputPath);
    if (loaded.failure) {
        return std::move(*loaded.failure);
    }
    auto target = prepareTarget(outputPath);
    if (auto* failure = std::get_if<CxcToolResult>(&target)) {
        return std::move(*failure);
    }
    const bool existed = std::get<bool>(target);
    const auto staging = detail::uniqueSibling(outputPath, "unpack-staging");
    if (staging.empty()) {
        return detail::failure("cxc.unpack.staging_create_failed", "$/output",
                               "Unpack staging path could not be reserved");
    }
    if (auto failure = writeStagingTree(*loaded.package, staging)) {
        detail::removeTree(staging);
        return std::move(*failure);
    }
    if (auto failure = commitStaging(staging, outputPath, existed)) {
        detail::removeTree(staging);
        return std::move(*failure);
    }

    std::size_t byteCount = 0;
    for (const auto& entry : loaded.package->manifest().entries) {
        byteCount += static_cast<std::size_t>(entry.byteCount);
    }
    return detail::success(
        "Unpacked CXC: entries=" + std::to_string(loaded.package->manifest().entries.size()) +
        " bytes=" + std::to_string(byteCount) + " identity=" + loaded.package->identity().hex());
}

} // namespace cuexis::tools
