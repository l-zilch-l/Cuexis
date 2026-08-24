#include <cuexis/tools/cxc_tool.hpp>

#include "cxc_tool_internal.hpp"

#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/cxc/cxc_writer.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/project/asset_index_reader.hpp>
#include <cuexis/project/project_loader.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::tools {
namespace {

struct SourceSnapshot final {
    cxc::CxcWriteRequest request;
    std::size_t totalBytes{};
    std::set<std::string, std::less<>> paths;
};

[[nodiscard]] auto textFromBytes(std::span<const std::byte> bytes) -> std::string {
    std::string result(bytes.size(), '\0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index] = static_cast<char>(std::to_integer<unsigned char>(bytes[index]));
    }
    return result;
}

[[nodiscard]] auto joinPath(std::string_view left, std::string_view right) -> std::string {
    if (left.empty()) {
        return std::string{right};
    }
    std::string result;
    result.reserve(left.size() + right.size() + 1U);
    result.append(left);
    result.push_back('/');
    result.append(right);
    return result;
}

[[nodiscard]] bool hasDiagnostic(const core::Diagnostics& diagnostics, std::string_view code) {
    return std::ranges::any_of(diagnostics.items(), [code](const core::Diagnostic& diagnostic) {
        return diagnostic.code() == code;
    });
}

[[nodiscard]] auto sourceReadFailure(const core::Error& error, std::string_view logicalPath)
    -> CxcToolResult {
    return detail::failure(std::string{error.code()}, "$/source", std::string{error.message()},
                           "path", logicalPath);
}

[[nodiscard]] auto readSourceFile(const std::filesystem::path& root, std::string_view logicalPath,
                                  std::size_t maxBytes, const cxc::CxcPackageLimits& limits,
                                  SourceSnapshot& snapshot)
    -> std::variant<std::vector<std::byte>, CxcToolResult> {
    if (snapshot.request.entries.size() + 1U >= limits.maxEntries) {
        return detail::failure("cxc.budget.exceeded", "$/source",
                               "Source Project entry count exceeds the package limit", "path",
                               logicalPath);
    }
    auto contents = filesystem::readBoundedFile(
        root / std::filesystem::path{logicalPath},
        {.root = root,
         .maxBytes = std::min(maxBytes, limits.maxEntryBytes),
         .errors = {.rootUnavailable = "cxc.pack.source_root_unavailable",
                    .rootChanged = "cxc.pack.source_root_changed",
                    .openFailed = "cxc.pack.source_open_failed",
                    .outsideRoot = "cxc.pack.source_outside_root",
                    .notRegular = "cxc.pack.source_not_regular",
                    .tooLarge = "cxc.budget.exceeded",
                    .readFailed = "cxc.pack.source_read_failed",
                    .changedDuringRead = "cxc.pack.source_changed"}});
    if (!contents) {
        return sourceReadFailure(contents.error(), logicalPath);
    }
    if (snapshot.totalBytes > limits.manifest.maxListedBytes ||
        contents->bytes.size() > limits.manifest.maxListedBytes - snapshot.totalBytes) {
        return detail::failure("cxc.budget.exceeded", "$/source",
                               "Source Project declared bytes exceed the package limit", "path",
                               logicalPath);
    }
    snapshot.totalBytes += contents->bytes.size();
    return std::move(contents->bytes);
}

[[nodiscard]] auto addSourceEntry(SourceSnapshot& snapshot, std::string logicalPath,
                                  std::vector<std::byte> bytes) -> std::optional<CxcToolResult> {
    if (!snapshot.paths.emplace(logicalPath).second) {
        return std::nullopt;
    }
    snapshot.request.entries.push_back(
        cxc::CxcWriteEntry{std::move(logicalPath), std::move(bytes)});
    return std::nullopt;
}

[[nodiscard]] auto readAndAdd(const std::filesystem::path& root, std::string logicalPath,
                              std::size_t maxBytes, const cxc::CxcPackageLimits& limits,
                              SourceSnapshot& snapshot)
    -> std::variant<std::vector<std::byte>, CxcToolResult> {
    if (snapshot.paths.contains(logicalPath)) {
        const auto existing =
            std::ranges::find(snapshot.request.entries, logicalPath, &cxc::CxcWriteEntry::path);
        if (existing != snapshot.request.entries.end()) {
            return existing->bytes;
        }
        return detail::failure("cxc.pack.snapshot_internal", "$/source",
                               "Source snapshot lost a previously read entry", "path", logicalPath);
    }
    auto contents = readSourceFile(root, logicalPath, maxBytes, limits, snapshot);
    if (auto* failure = std::get_if<CxcToolResult>(&contents)) {
        return std::move(*failure);
    }
    auto bytes = std::move(std::get<std::vector<std::byte>>(contents));
    auto parserBytes = bytes;
    static_cast<void>(addSourceEntry(snapshot, std::move(logicalPath), std::move(bytes)));
    return parserBytes;
}

[[nodiscard]] auto snapshotSourceProject(const std::filesystem::path& root,
                                         const cxc::CxcPackageLimits& limits)
    -> std::variant<SourceSnapshot, CxcToolResult> {
    SourceSnapshot snapshot;
    auto projectBytes =
        readAndAdd(root, "cuexis.project.json", limits.project.maxInputBytes, limits, snapshot);
    if (auto* failure = std::get_if<CxcToolResult>(&projectBytes)) {
        return std::move(*failure);
    }
    const auto projectText = textFromBytes(std::get<std::vector<std::byte>>(projectBytes));
    auto projectResult = project::ProjectConfigReader::read(projectText, limits.project);
    if (!projectResult.hasValue()) {
        return detail::invalid(projectResult.diagnostics, "cuexis.project.json");
    }
    const auto& project = *projectResult.config;

    std::map<std::string, std::string, std::less<>> roots;
    for (const auto& rootDeclaration : project.assetRoots) {
        roots.emplace(rootDeclaration.id, rootDeclaration.path);
        const auto indexPath = joinPath(rootDeclaration.path, project::assetIndexFileName);
        auto indexBytes =
            readAndAdd(root, indexPath, limits.assetIndex.maxInputBytes, limits, snapshot);
        if (auto* failure = std::get_if<CxcToolResult>(&indexBytes)) {
            return std::move(*failure);
        }
        const auto indexText = textFromBytes(std::get<std::vector<std::byte>>(indexBytes));
        auto indexResult = project::AssetIndexReader::read(indexText, limits.assetIndex);
        if (!indexResult.hasValue()) {
            return detail::invalid(indexResult.diagnostics, indexPath);
        }
        for (const auto& record : indexResult.document->assets) {
            const auto sourcePath = joinPath(rootDeclaration.path, record.source);
            if (snapshot.paths.contains(sourcePath)) {
                continue;
            }
            auto sourceBytes = readAndAdd(root, sourcePath, limits.maxEntryBytes, limits, snapshot);
            if (auto* failure = std::get_if<CxcToolResult>(&sourceBytes)) {
                return std::move(*failure);
            }
        }
    }

    const auto chartRoot = roots.find(project.entry.chart.root);
    if (chartRoot == roots.end()) {
        core::Diagnostics diagnostics;
        static_cast<void>(diagnostics.add(
            core::Diagnostic{core::DiagnosticSeverity::Error, "project.entry.root_missing",
                             "Entry root does not name a declared asset root", "$/entry/chart/root"}
                .withContext("root", project.entry.chart.root)));
        return detail::invalid(diagnostics, "cuexis.project.json");
    }
    const auto chartPath = joinPath(chartRoot->second, project.entry.chart.path);
    auto chartBytes = readAndAdd(root, chartPath, limits.chart.maxInputBytes, limits, snapshot);
    if (auto* failure = std::get_if<CxcToolResult>(&chartBytes)) {
        return std::move(*failure);
    }
    const auto chartText = textFromBytes(std::get<std::vector<std::byte>>(chartBytes));

    auto v4 = chart::ChartV4Loader::load(chartText, limits.chart);
    if (v4.hasValue()) {
        for (const auto& import : v4.document->animationTemplateImports) {
            if (snapshot.paths.contains(import.source)) {
                continue;
            }
            auto cxtBytes =
                readAndAdd(root, import.source, limits.chart.maxInputBytes, limits, snapshot);
            if (auto* failure = std::get_if<CxcToolResult>(&cxtBytes)) {
                return std::move(*failure);
            }
        }
        return snapshot;
    }

    auto legacy = chart::ChartLoader::load(chartText, limits.chart);
    if (legacy.hasValue()) {
        return snapshot;
    }
    const bool v4HasWrongVersion = hasDiagnostic(v4.diagnostics, "chart.version.unsupported");
    const bool legacyHasWrongVersion =
        hasDiagnostic(legacy.diagnostics, "chart.version.unsupported");
    return !v4HasWrongVersion && legacyHasWrongVersion
               ? std::variant<SourceSnapshot, CxcToolResult>{detail::invalid(v4.diagnostics,
                                                                             chartPath)}
               : std::variant<SourceSnapshot, CxcToolResult>{
                     detail::invalid(legacy.diagnostics, chartPath)};
}

[[nodiscard]] bool restorePackageBackup(const std::filesystem::path& backup,
                                        const std::filesystem::path& outputPath) {
    if (detail::testFailureEnabled("CUEXIS_CXC_PACK_FAIL_RESTORE")) {
        return false;
    }
    std::error_code error;
    std::filesystem::rename(backup, outputPath, error);
    return !error;
}

[[nodiscard]] auto packageRestoreFailure() -> CxcToolResult {
    return detail::failure(
        "cxc.pack.output_restore_failed", "$/output",
        "Original package output could not be restored; its backup was preserved");
}

[[nodiscard]] auto commitPackage(const std::filesystem::path& outputPath,
                                 std::span<const std::byte> bytes,
                                 const cxc::CxcPackageLimits& limits)
    -> std::variant<cxc::CxcPackageIdentity, CxcToolResult> {
    const auto temporary = detail::uniqueSibling(outputPath, "pack-staging", ".cxc");
    const auto backup = detail::uniqueSibling(outputPath, "pack-backup", ".cxc");
    if (temporary.empty() || backup.empty() || !detail::writeBytes(temporary, bytes)) {
        detail::removeFile(temporary);
        return detail::failure("cxc.pack.output_write_failed", "$/output",
                               "Package staging file could not be written");
    }
    auto staged = cxc::CxcPackageLoader::loadFile(temporary, limits);
    if (!staged.hasValue()) {
        detail::removeFile(temporary);
        return detail::failure("cxc.pack.output_validation_failed", "$/output",
                               "Package staging file failed reload validation");
    }
    const auto identity = staged.package->identity();
    if (detail::testFailureEnabled("CUEXIS_CXC_PACK_FAIL_BEFORE_COMMIT")) {
        detail::removeFile(temporary);
        return detail::failure("cxc.pack.output_commit_failed", "$/output",
                               "Package output commit failed");
    }

    std::error_code error;
    const bool existed = std::filesystem::exists(outputPath, error) && !error;
    if (error) {
        detail::removeFile(temporary);
        return detail::failure("cxc.pack.output_commit_failed", "$/output",
                               "Package output target could not be inspected");
    }
    if (existed) {
        std::filesystem::rename(outputPath, backup, error);
        if (error) {
            detail::removeFile(temporary);
            return detail::failure("cxc.pack.output_commit_failed", "$/output",
                                   "Existing package output could not be staged for replacement");
        }
        if (detail::testFailureEnabled("CUEXIS_CXC_PACK_FAIL_AFTER_BACKUP")) {
            detail::removeFile(temporary);
            if (!restorePackageBackup(backup, outputPath)) {
                return packageRestoreFailure();
            }
            return detail::failure("cxc.pack.output_commit_failed", "$/output",
                                   "Package output commit failed");
        }
    }

    std::filesystem::rename(temporary, outputPath, error);
    if (error) {
        detail::removeFile(temporary);
        if (existed && !restorePackageBackup(backup, outputPath)) {
            return packageRestoreFailure();
        }
        return detail::failure("cxc.pack.output_commit_failed", "$/output",
                               "Package output could not be committed");
    }
    if (existed) {
        detail::removeFile(backup);
    }
    return identity;
}

} // namespace

auto packCxc(const std::filesystem::path& sourceRoot, const std::filesystem::path& outputPath)
    -> CxcToolResult {
    if (sourceRoot.empty() || outputPath.empty() || outputPath.extension().string() != ".cxc") {
        return detail::failure("cxc.pack.arguments_invalid", "$",
                               "Pack requires a Source Project root and .cxc output");
    }
    if (!detail::isSafeExistingPath(sourceRoot, true)) {
        return detail::failure("cxc.pack.source_root_unavailable", "$/input",
                               "Source Project root must be a stable directory");
    }
    if (detail::containsPath(sourceRoot, outputPath)) {
        return detail::failure("cxc.pack.path_conflict", "$/output",
                               "Package output must be outside the Source Project root");
    }
    const auto outputParent =
        outputPath.parent_path().empty() ? std::filesystem::path{"."} : outputPath.parent_path();
    if (!detail::isSafeExistingPath(outputParent, true)) {
        return detail::failure("cxc.pack.output_parent_unavailable", "$/output",
                               "Package output parent must be a stable directory");
    }
    std::error_code outputError;
    const auto outputStatus = std::filesystem::symlink_status(outputPath, outputError);
    if (!outputError && std::filesystem::exists(outputStatus) &&
        (!std::filesystem::is_regular_file(outputStatus) ||
         !detail::isSafeExistingPath(outputPath, false))) {
        return detail::failure("cxc.pack.output_invalid", "$/output",
                               "Existing package output must be a regular non-link file");
    }

    const cxc::CxcPackageLimits limits;
    auto snapshot = snapshotSourceProject(sourceRoot, limits);
    if (auto* failure = std::get_if<CxcToolResult>(&snapshot)) {
        return std::move(*failure);
    }
    auto source = std::move(std::get<SourceSnapshot>(snapshot));
    const auto entryCount = source.request.entries.size();
    auto written = cxc::CxcWriter::write(std::move(source.request), limits);
    if (!written.hasValue()) {
        if (hasDiagnostic(written.diagnostics, "cxc.internal.failure")) {
            return detail::failure("cxc.pack.internal_failure", "$",
                                   "CXC Writer failed while packing the Source Project");
        }
        return detail::invalid(written.diagnostics);
    }
    const auto byteCount = written.bytes->size();
    auto committed = commitPackage(outputPath, *written.bytes, limits);
    if (auto* failure = std::get_if<CxcToolResult>(&committed)) {
        return std::move(*failure);
    }
    return detail::success("Packed CXC: entries=" + std::to_string(entryCount) +
                           " bytes=" + std::to_string(byteCount) +
                           " identity=" + std::get<cxc::CxcPackageIdentity>(committed).hex());
}

} // namespace cuexis::tools
