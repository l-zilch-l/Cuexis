// Internal project loader helpers.
#pragma once

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

namespace cuexis::project::project_internal {

// Validation, JSON reading and path containment for the loader and reader.
using core::DiagnosticSeverity;
namespace fs = std::filesystem;

[[nodiscard]] inline core::Diagnostics makeDiagnostics(const ProjectLimits& limits) {
    return core::Diagnostics{
        limits.maxDiagnostics,
        core::Diagnostic{DiagnosticSeverity::Error, "project.diagnostics.limit_exceeded",
                         "Project diagnostic count exceeds the configured limit", "$"}};
}

inline void addDiagnostic(core::Diagnostics& diagnostics, DiagnosticSeverity severity,
                          std::string code, std::string message, std::string fieldPath) {
    static_cast<void>(diagnostics.add(
        core::Diagnostic{severity, std::move(code), std::move(message), std::move(fieldPath)}));
}

inline void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
                     std::string fieldPath) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Error, std::move(code), std::move(message),
                  std::move(fieldPath));
}

inline void addError(core::Diagnostics& diagnostics, const core::Error& error,
                     std::string fieldPath) {
    auto diagnostic = core::Diagnostic{DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(fieldPath)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

inline void addWarning(core::Diagnostics& diagnostics, std::string code, std::string message,
                       std::string fieldPath) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Warning, std::move(code), std::move(message),
                  std::move(fieldPath));
}

[[nodiscard]] inline bool validateLimits(const ProjectLimits& limits,
                                         core::Diagnostics& diagnostics) {
    if (limits.maxInputBytes == 0 || limits.maxNestingDepth == 0 || limits.maxStringBytes == 0 ||
        limits.maxPortablePathBytes == 0 || limits.maxDiagnostics == 0 ||
        limits.maxAssetRoots == 0 || limits.maxExtensions == 0) {
        addError(diagnostics, "project.limits.invalid",
                 "Project limits must all be greater than zero", "$");
        return false;
    }
    return true;
}

[[nodiscard]] inline bool isAsciiAlphaNumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9');
}

[[nodiscard]] inline bool isAssetRootId(std::string_view value) noexcept {
    if (value.empty() || value.size() > 64 || value.front() < 'a' || value.front() > 'z') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
               character == '.' || character == '_' || character == '-';
    });
}

[[nodiscard]] inline bool isWindowsReservedSegment(std::string_view segment) {
    return core::detail::isWindowsReservedSegment(segment);
}

[[nodiscard]] inline bool validatePortablePathInternal(std::string_view value, std::size_t maxBytes,
                                                       core::Diagnostics& diagnostics,
                                                       std::string_view fieldPath) {
    const std::string path{fieldPath};
    if (value.empty()) {
        addError(diagnostics, "project.path.empty", "Portable path cannot be empty", path);
        return false;
    }
    if (value.size() > maxBytes) {
        addError(diagnostics, "project.path.length_limit",
                 "Portable path exceeds the configured byte limit", path);
        return false;
    }
    if (value.front() == '/' ||
        (value.size() >= 2 && isAsciiAlphaNumeric(value.front()) && value[1] == ':')) {
        addError(diagnostics, "project.path.absolute", "Portable path must be relative", path);
        return false;
    }
    if (value.find('\\') != std::string_view::npos) {
        addError(diagnostics, "project.path.backslash",
                 "Portable path must use forward slash separators", path);
        return false;
    }

    std::size_t segmentBegin = 0;
    while (segmentBegin <= value.size()) {
        const auto separator = value.find('/', segmentBegin);
        const auto segment = value.substr(segmentBegin, separator == std::string_view::npos
                                                            ? value.size() - segmentBegin
                                                            : separator - segmentBegin);
        if (segment.empty()) {
            addError(diagnostics, "project.path.empty_segment",
                     "Portable path cannot contain empty segments", path);
            return false;
        }
        if (segment == "." || segment == "..") {
            addError(diagnostics, "project.path.dot_segment",
                     "Portable path cannot contain dot segments", path);
            return false;
        }
        if (segment.back() == '.' || segment.back() == ' ') {
            addError(diagnostics, "project.path.trailing_character",
                     "Portable path segments cannot end in a dot or space", path);
            return false;
        }
        if (isWindowsReservedSegment(segment)) {
            addError(diagnostics, "project.path.reserved_name",
                     "Portable path contains a reserved platform name", path);
            return false;
        }
        for (const unsigned char character : segment) {
            const bool alphaNumeric = (character >= static_cast<unsigned char>('A') &&
                                       character <= static_cast<unsigned char>('Z')) ||
                                      (character >= static_cast<unsigned char>('a') &&
                                       character <= static_cast<unsigned char>('z')) ||
                                      (character >= static_cast<unsigned char>('0') &&
                                       character <= static_cast<unsigned char>('9'));
            if (!alphaNumeric && character != static_cast<unsigned char>('.') &&
                character != static_cast<unsigned char>('_') &&
                character != static_cast<unsigned char>('-')) {
                addError(diagnostics, "project.path.non_portable_character",
                         "Portable path contains a character outside its portable ASCII subset",
                         path);
                return false;
            }
        }
        if (separator == std::string_view::npos) {
            break;
        }
        segmentBegin = separator + 1;
    }
    return true;
}

[[nodiscard]] inline std::optional<std::string> readString(const json::Reader& reader) {
    const auto value = reader.readString();
    return value ? std::optional<std::string>{std::string{*value}} : std::nullopt;
}

[[nodiscard]] inline core::Result<std::string> readBoundedFile(const fs::path& file,
                                                               std::size_t maxBytes) {
    auto contents = filesystem::readBoundedTextFile(
        file, {.root = file.parent_path(),
               .maxBytes = maxBytes,
               .errors = {.rootUnavailable = "project.file.open_failed",
                          .rootChanged = "project.file.root_changed",
                          .openFailed = "project.file.open_failed",
                          .outsideRoot = "project.file.outside_root",
                          .notRegular = "project.file.not_regular",
                          .tooLarge = "project.file.size_limit",
                          .readFailed = "project.file.read_failed",
                          .changedDuringRead = "project.file.changed_during_read"}});
    if (!contents) {
        return core::unexpected(std::move(contents.error()));
    }
    return std::move(contents->text);
}

#if defined(_WIN32)
[[nodiscard]] inline bool componentEqual(const fs::path& left, const fs::path& right) {
    const auto& leftText = left.native();
    const auto& rightText = right.native();
    return leftText.size() == rightText.size() &&
           std::equal(leftText.begin(), leftText.end(), rightText.begin(),
                      [](wchar_t leftCharacter, wchar_t rightCharacter) {
                          return std::towlower(leftCharacter) == std::towlower(rightCharacter);
                      });
}
#else
[[nodiscard]] inline bool componentEqual(const fs::path& left, const fs::path& right) {
    return left.native() == right.native();
}
#endif

[[nodiscard]] inline bool containsPath(const fs::path& base, const fs::path& candidate) {
    auto baseIterator = base.begin();
    auto candidateIterator = candidate.begin();
    for (; baseIterator != base.end(); ++baseIterator, ++candidateIterator) {
        if (candidateIterator == candidate.end() ||
            !componentEqual(*baseIterator, *candidateIterator)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool strictlyContainsPath(const fs::path& base, const fs::path& candidate) {
    if (!containsPath(base, candidate)) {
        return false;
    }
    return static_cast<std::size_t>(std::distance(base.begin(), base.end())) <
           static_cast<std::size_t>(std::distance(candidate.begin(), candidate.end()));
}

[[nodiscard]] inline std::optional<fs::path>
canonicalDirectory(const fs::path& path, core::Diagnostics& diagnostics, std::string fieldPath,
                   std::string_view missingCode, std::string_view outsideMessage = {}) {
    std::error_code error;
    const auto status = fs::status(path, error);
    if (error || !fs::is_directory(status)) {
        addError(diagnostics, std::string{missingCode},
                 "Declared path must be an existing directory", std::move(fieldPath));
        return std::nullopt;
    }
    const auto canonical = fs::canonical(path, error);
    if (error) {
        addError(diagnostics, std::string{missingCode},
                 outsideMessage.empty() ? "Declared directory could not be resolved"
                                        : std::string{outsideMessage},
                 std::move(fieldPath));
        return std::nullopt;
    }
    return canonical;
}

[[nodiscard]] inline ProjectLoadResult
prepareProject(ProjectConfig config, const fs::path& projectFile, const fs::path& projectRoot,
               core::Diagnostics diagnostics, const ProjectLimits& limits) {
    static_cast<void>(limits);
    const auto canonicalRoot =
        canonicalDirectory(projectRoot, diagnostics, "$", "project.root.invalid");
    if (!canonicalRoot) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    struct IndexedRoot final {
        PreparedAssetRoot root;
        std::size_t configIndex;
    };
    std::vector<IndexedRoot> preparedRoots;
    preparedRoots.reserve(config.assetRoots.size());
    for (std::size_t index = 0; index < config.assetRoots.size(); ++index) {
        const auto fieldPath =
            json::appendFieldPath(json::appendIndexPath("$/assetRoots", index), "path");
        const auto resolved =
            canonicalDirectory(*canonicalRoot / config.assetRoots[index].path, diagnostics,
                               fieldPath, "project.asset_root.invalid");
        if (!resolved) {
            continue;
        }
        if (!strictlyContainsPath(*canonicalRoot, *resolved)) {
            addError(diagnostics, "project.asset_root.outside_project",
                     "Asset root must resolve inside the project root", fieldPath);
            continue;
        }
        preparedRoots.push_back(
            IndexedRoot{PreparedAssetRoot{config.assetRoots[index], *resolved,
                                          *resolved / std::string{assetIndexFileName}},
                        index});
    }

    for (std::size_t left = 0; left < preparedRoots.size(); ++left) {
        for (std::size_t right = left + 1; right < preparedRoots.size(); ++right) {
            const auto& leftPath = preparedRoots[left].root.absolutePath;
            const auto& rightPath = preparedRoots[right].root.absolutePath;
            if (!containsPath(leftPath, rightPath) && !containsPath(rightPath, leftPath)) {
                continue;
            }
            const auto fieldPath = json::appendFieldPath(
                json::appendIndexPath("$/assetRoots", preparedRoots[right].configIndex), "path");
            auto diagnostic =
                core::Diagnostic{DiagnosticSeverity::Error, "project.asset_root.overlap",
                                 "Asset roots cannot overlap physically", fieldPath};
            diagnostic.withContext("root", preparedRoots[right].root.declaration.id)
                .withContext("conflicting_root", preparedRoots[left].root.declaration.id);
            static_cast<void>(diagnostics.add(std::move(diagnostic)));
        }
    }

    fs::path chartFile;
    const auto rootIterator =
        std::find_if(preparedRoots.begin(), preparedRoots.end(), [&](const IndexedRoot& root) {
            return root.root.declaration.id == config.entry.chart.root;
        });
    if (rootIterator != preparedRoots.end()) {
        const auto chartPath = rootIterator->root.absolutePath / config.entry.chart.path;
        std::error_code error;
        const auto status = fs::status(chartPath, error);
        if (error || !fs::is_regular_file(status)) {
            addError(diagnostics, "project.entry.chart.invalid",
                     "Entry Chart must be an existing regular file", "$/entry/chart/path");
        } else {
            chartFile = fs::canonical(chartPath, error);
            if (error || !strictlyContainsPath(rootIterator->root.absolutePath, chartFile)) {
                addError(diagnostics, "project.entry.chart.outside_root",
                         "Entry Chart must resolve inside its declared asset root",
                         "$/entry/chart/path");
                chartFile.clear();
            }
        }
    }

    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    std::vector<PreparedAssetRoot> roots;
    roots.reserve(preparedRoots.size());
    for (auto& root : preparedRoots) {
        roots.push_back(std::move(root.root));
    }
    PreparedProject project{std::move(config), projectFile, *canonicalRoot, std::move(roots),
                            std::move(chartFile)};
    diagnostics.sortDeterministically();
    return {std::move(project), std::move(diagnostics)};
}

[[nodiscard]] inline json::Value projectConfigValue(const ProjectConfig& config,
                                                    const ProjectLimits& limits,
                                                    core::Result<void>& extensionStatus) {
    json::Value::Array roots;
    roots.reserve(config.assetRoots.size());
    for (const auto& root : config.assetRoots) {
        json::Value::Object value;
        value.emplace("id", json::Value{root.id});
        value.emplace("path", json::Value{root.path});
        roots.emplace_back(json::Value{std::move(value)});
    }

    json::Value::Object chart;
    chart.emplace("root", json::Value{config.entry.chart.root});
    chart.emplace("path", json::Value{config.entry.chart.path});
    json::Value::Object entry;
    entry.emplace("chart", json::Value{std::move(chart)});

    auto extensions = json::parse(
        config.extensions.canonicalText,
        json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes});
    if (!extensions || extensions->object() == nullptr) {
        extensionStatus = core::unexpected(core::Error{"project.save.extensions_invalid",
                                                       "Project extensions must be a JSON object"});
        extensions = json::Value{json::Value::Object{}};
    }

    json::Value::Object root;
    root.emplace("format", json::Value{config.format});
    root.emplace("version", json::Value{static_cast<std::uint64_t>(config.version)});
    root.emplace("projectId", json::Value{config.projectId});
    root.emplace("assetRoots", json::Value{std::move(roots)});
    root.emplace("entry", json::Value{std::move(entry)});
    root.emplace("extensions", std::move(*extensions));
    return json::Value{std::move(root)};
}

[[nodiscard]] inline core::Result<std::string> serializeProjectConfig(const ProjectConfig& config,
                                                                      const ProjectLimits& limits) {
    core::Result<void> extensionStatus{};
    const auto value = projectConfigValue(config, limits, extensionStatus);
    if (!extensionStatus) {
        return core::unexpected(extensionStatus.error());
    }
    auto serialized = json::serialize(value, json::SerializeStyle::Pretty);
    if (!serialized) {
        return core::unexpected(serialized.error());
    }
    serialized->push_back('\n');
    if (serialized->size() > limits.maxInputBytes) {
        return core::unexpected(core::Error{
            "project.save.size_limit", "Serialized ProjectConfig exceeds the input byte limit"});
    }
    return serialized;
}

[[nodiscard]] inline core::Error invalidConfigError(const core::Diagnostics& diagnostics,
                                                    std::string_view code,
                                                    std::string_view message) {
    core::Error error{std::string{code}, std::string{message}};
    const auto diagnostic = std::find_if(
        diagnostics.items().begin(), diagnostics.items().end(),
        [](const core::Diagnostic& item) { return item.severity() == DiagnosticSeverity::Error; });
    if (diagnostic != diagnostics.items().end()) {
        error.withContext("diagnostic_code", std::string{diagnostic->code()})
            .withContext("field_path", std::string{diagnostic->fieldPath()});
    }
    return error;
}

} // namespace cuexis::project::project_internal
