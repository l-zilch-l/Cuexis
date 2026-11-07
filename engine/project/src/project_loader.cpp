//  ProjectLoader 实现 — 项目配置加载与原子保存
//  固定文件：<project-root>/cuexis.project.json，format: "cuexis.project"
//  load()：定位文件 → 物理 containment 校验 → typed-read ProjectConfig → 解析资产根
//  saveAtomic()：序列化 → 校验 → 独占临时文件写入 → 重新加载验证 → 原子替换
//  便携路径规则：相对路径、正斜杠、禁止 dot segment/Windows 保留名/非法字符

#include <cuexis/project/project_loader.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/uuid.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>
#include <cuexis_internal/portable_path.hpp>

#include "project_validation.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "project_loader_read_helpers.hpp"

#include "project_loader_write_helpers.hpp"

namespace cuexis::project {

using namespace project_internal;

namespace detail {

bool validatePortablePath(std::string_view value, std::size_t maxBytes,
                          core::Diagnostics& diagnostics, std::string_view fieldPath) {
    return validatePortablePathInternal(value, maxBytes, diagnostics, fieldPath);
}

} // namespace detail

const PreparedAssetRoot* PreparedProject::findAssetRoot(std::string_view id) const noexcept {
    const auto iterator =
        std::find_if(assetRoots.begin(), assetRoots.end(),
                     [id](const PreparedAssetRoot& root) { return root.declaration.id == id; });
    return iterator == assetRoots.end() ? nullptr : &*iterator;
}

ProjectConfigResult ProjectConfigReader::read(std::string_view jsonText,
                                              const ProjectLimits& limits) {
    auto diagnostics = makeDiagnostics(limits);
    if (!validateLimits(limits, diagnostics)) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addError(diagnostics, parsed.error(), "$");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    const json::Reader root{*parsed, diagnostics};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    constexpr std::array rootFields{std::string_view{"format"},    std::string_view{"version"},
                                    std::string_view{"projectId"}, std::string_view{"assetRoots"},
                                    std::string_view{"entry"},     std::string_view{"extensions"}};
    root.rejectUnknownFields(rootFields);

    ProjectConfig config;
    const auto formatReader = root.requiredField("format");
    const auto versionReader = root.requiredField("version");
    const auto projectIdReader = root.requiredField("projectId");
    const auto rootsReader = root.requiredField("assetRoots");
    const auto entryReader = root.requiredField("entry");
    const auto extensionsReader = root.requiredField("extensions");

    if (formatReader) {
        const auto value = readString(*formatReader);
        if (value) {
            config.format = *value;
            if (*value != projectFormat) {
                addError(diagnostics, "project.format.unsupported", "Project format is unsupported",
                         std::string{formatReader->fieldPath()});
            }
        }
    }
    if (versionReader) {
        const auto value = versionReader->readInt64();
        if (value && *value != projectFormatVersion) {
            addError(diagnostics, "project.version.unsupported",
                     "Project format version is unsupported",
                     std::string{versionReader->fieldPath()});
        } else if (value) {
            config.version = static_cast<std::uint32_t>(*value);
        }
    }
    if (projectIdReader) {
        const auto value = readString(*projectIdReader);
        if (value) {
            config.projectId = *value;
            if (!core::isUuidV7(*value)) {
                addError(diagnostics, "project.id.invalid",
                         "Project ID must be a canonical lowercase UUIDv7",
                         std::string{projectIdReader->fieldPath()});
            }
        }
    }

    std::set<std::string, std::less<>> rootIds;
    if (rootsReader) {
        const auto* roots = rootsReader->readArray();
        if (roots != nullptr) {
            if (roots->empty()) {
                addError(diagnostics, "project.asset_roots.empty",
                         "Project must declare at least one asset root",
                         std::string{rootsReader->fieldPath()});
            }
            if (roots->size() > limits.maxAssetRoots) {
                addError(diagnostics, "project.asset_roots.limit",
                         "Asset root count exceeds the configured limit",
                         std::string{rootsReader->fieldPath()});
            }
            const auto count = std::min(roots->size(), limits.maxAssetRoots);
            config.assetRoots.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                const auto item = rootsReader->element(index);
                if (!item || item->readObject() == nullptr) {
                    continue;
                }
                constexpr std::array fields{std::string_view{"id"}, std::string_view{"path"}};
                item->rejectUnknownFields(fields);
                const auto idReader = item->requiredField("id");
                const auto pathReader = item->requiredField("path");
                if (!idReader || !pathReader) {
                    continue;
                }
                const auto id = readString(*idReader);
                const auto path = readString(*pathReader);
                if (!id || !path) {
                    continue;
                }
                if (!isAssetRootId(*id)) {
                    addError(diagnostics, "project.asset_root.id_invalid",
                             "Asset root ID contains unsupported characters",
                             std::string{idReader->fieldPath()});
                }
                if (!rootIds.insert(*id).second) {
                    addError(diagnostics, "project.asset_root.id_duplicate",
                             "Asset root IDs must be unique", std::string{idReader->fieldPath()});
                }
                static_cast<void>(detail::validatePortablePath(
                    *path, limits.maxPortablePathBytes, diagnostics, pathReader->fieldPath()));
                config.assetRoots.push_back(AssetRoot{*id, *path});
            }
        }
    }

    if (entryReader && entryReader->readObject() != nullptr) {
        constexpr std::array fields{std::string_view{"chart"}};
        entryReader->rejectUnknownFields(fields);
        const auto chartReader = entryReader->requiredField("chart");
        if (chartReader && chartReader->readObject() != nullptr) {
            constexpr std::array chartFields{std::string_view{"root"}, std::string_view{"path"}};
            chartReader->rejectUnknownFields(chartFields);
            const auto rootIdReader = chartReader->requiredField("root");
            const auto pathReader = chartReader->requiredField("path");
            if (rootIdReader) {
                const auto value = readString(*rootIdReader);
                if (value) {
                    config.entry.chart.root = *value;
                    if (!isAssetRootId(*value)) {
                        addError(diagnostics, "project.entry.root_invalid",
                                 "Entry root must be a valid asset root ID",
                                 std::string{rootIdReader->fieldPath()});
                    } else if (!rootIds.contains(*value)) {
                        addError(diagnostics, "project.entry.root_missing",
                                 "Entry root does not name a declared asset root",
                                 std::string{rootIdReader->fieldPath()});
                    }
                }
            }
            if (pathReader) {
                const auto value = readString(*pathReader);
                if (value) {
                    config.entry.chart.path = *value;
                    static_cast<void>(detail::validatePortablePath(
                        *value, limits.maxPortablePathBytes, diagnostics, pathReader->fieldPath()));
                }
            }
        }
    }

    if (extensionsReader) {
        const auto* object = extensionsReader->readObject();
        if (object != nullptr) {
            if (object->size() > limits.maxExtensions) {
                addError(diagnostics, "project.extensions.limit",
                         "Project extension count exceeds the configured limit",
                         std::string{extensionsReader->fieldPath()});
            }
            auto serialized = json::serialize(extensionsReader->value());
            if (!serialized) {
                addError(diagnostics, serialized.error(),
                         std::string{extensionsReader->fieldPath()});
            } else {
                config.extensions.canonicalText = std::move(*serialized);
            }
            if (!object->empty()) {
                addWarning(diagnostics, "project.extensions.opaque",
                           "Project extensions are preserved without v1 runtime behavior",
                           std::string{extensionsReader->fieldPath()});
            }
        }
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        return {std::nullopt, std::move(diagnostics)};
    }
    return {std::move(config), std::move(diagnostics)};
}

ProjectLoadResult ProjectLoader::load(const fs::path& locator, const ProjectLimits& limits) {
    auto diagnostics = makeDiagnostics(limits);
    if (!validateLimits(limits, diagnostics)) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    const auto located = locateProjectFile(locator);
    if (!located) {
        addError(diagnostics, located.error(), "$");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    std::error_code error;
    const auto canonicalFile = fs::canonical(*located, error);
    const auto canonicalRoot = fs::canonical(located->parent_path(), error);
    if (error || !containsPath(canonicalRoot, canonicalFile.parent_path()) ||
        !containsPath(canonicalFile.parent_path(), canonicalRoot)) {
        addError(diagnostics, "project.file.outside_root",
                 "Project file cannot resolve outside its project root", "$");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    const auto text = readBoundedFile(canonicalFile, limits.maxInputBytes);
    if (!text) {
        addError(diagnostics, text.error(), "$");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    auto read = ProjectConfigReader::read(*text, limits);
    if (!read.config) {
        return {std::nullopt, std::move(read.diagnostics)};
    }
    return prepareProject(std::move(*read.config), canonicalFile, canonicalRoot,
                          std::move(read.diagnostics), limits);
}

ProjectLoadResult ProjectLoader::loadText(std::string_view jsonText, const fs::path& projectRoot,
                                          const ProjectLimits& limits) {
    auto read = ProjectConfigReader::read(jsonText, limits);
    if (!read.config) {
        return {std::nullopt, std::move(read.diagnostics)};
    }
    return prepareProject(std::move(*read.config), projectRoot / std::string{projectFileName},
                          projectRoot, std::move(read.diagnostics), limits);
}

core::Result<fs::path> ProjectLoader::locateProjectFile(const fs::path& locator) {
    if (locator.empty()) {
        return core::unexpected(
            core::Error{"project.locator.empty", "Project locator cannot be empty"});
    }

    std::error_code error;
    const auto status = fs::status(locator, error);
    if (error || !fs::exists(status)) {
        return core::unexpected(
            core::Error{"project.locator.not_found", "Project locator does not exist"});
    }

    fs::path file;
    if (fs::is_directory(status)) {
        file = locator / std::string{projectFileName};
    } else if (fs::is_regular_file(status) && locator.filename() == fs::path{projectFileName}) {
        file = locator;
    } else {
        return core::unexpected(core::Error{
            "project.locator.invalid",
            "Project locator must be a directory or the exact cuexis.project.json file"});
    }

    const auto fileStatus = fs::status(file, error);
    if (error || !fs::is_regular_file(fileStatus)) {
        return core::unexpected(core::Error{"project.file.not_regular",
                                            "ProjectConfig must be an existing regular file"});
    }
    return file;
}
} // namespace cuexis::project
