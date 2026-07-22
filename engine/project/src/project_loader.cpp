//  ProjectLoader 实现 — 项目配置加载与原子保存
//  固定文件：<project-root>/cuexis.project.json，format: "cuexis.project"
//  load()：定位文件 → 物理 containment 校验 → typed-read ProjectConfig → 解析资产根
//  saveAtomic()：序列化 → 校验 → 独占临时文件写入 → 重新加载验证 → 原子替换
//  便携路径规则：相对路径、正斜杠、禁止 dot segment/Windows 保留名/非法字符

#include <cuexis/project/project_loader.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/uuid.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

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

namespace cuexis::project {
namespace {

using core::DiagnosticSeverity;
namespace fs = std::filesystem;

[[nodiscard]] core::Diagnostics makeDiagnostics(const ProjectLimits& limits) {
    return core::Diagnostics{
        limits.maxDiagnostics,
        core::Diagnostic{DiagnosticSeverity::Error, "project.diagnostics.limit_exceeded",
                         "Project diagnostic count exceeds the configured limit", "$"}};
}

void addDiagnostic(core::Diagnostics& diagnostics, DiagnosticSeverity severity, std::string code,
                   std::string message, std::string fieldPath) {
    static_cast<void>(diagnostics.add(
        core::Diagnostic{severity, std::move(code), std::move(message), std::move(fieldPath)}));
}

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string fieldPath) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Error, std::move(code), std::move(message),
                  std::move(fieldPath));
}

void addError(core::Diagnostics& diagnostics, const core::Error& error, std::string fieldPath) {
    auto diagnostic = core::Diagnostic{DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(fieldPath)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

void addWarning(core::Diagnostics& diagnostics, std::string code, std::string message,
                std::string fieldPath) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Warning, std::move(code), std::move(message),
                  std::move(fieldPath));
}

[[nodiscard]] bool validateLimits(const ProjectLimits& limits, core::Diagnostics& diagnostics) {
    if (limits.maxInputBytes == 0 || limits.maxNestingDepth == 0 || limits.maxStringBytes == 0 ||
        limits.maxPortablePathBytes == 0 || limits.maxDiagnostics == 0 ||
        limits.maxAssetRoots == 0 || limits.maxExtensions == 0) {
        addError(diagnostics, "project.limits.invalid",
                 "Project limits must all be greater than zero", "$");
        return false;
    }
    return true;
}

[[nodiscard]] bool isAsciiAlphaNumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9');
}

[[nodiscard]] bool isAssetRootId(std::string_view value) noexcept {
    if (value.empty() || value.size() > 64 || value.front() < 'a' || value.front() > 'z') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
               character == '.' || character == '_' || character == '-';
    });
}

[[nodiscard]] std::string asciiUpper(std::string_view text) {
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(), [](char character) {
        if (character >= 'a' && character <= 'z') {
            return static_cast<char>(character - 'a' + 'A');
        }
        return character;
    });
    return result;
}

[[nodiscard]] bool isWindowsReservedSegment(std::string_view segment) {
    const auto dot = segment.find('.');
    const std::string stem = asciiUpper(segment.substr(0, dot));
    constexpr std::array reserved{std::string_view{"CON"},    std::string_view{"PRN"},
                                  std::string_view{"AUX"},    std::string_view{"NUL"},
                                  std::string_view{"CLOCK$"}, std::string_view{"CONIN$"},
                                  std::string_view{"CONOUT$"}};
    if (std::find(reserved.begin(), reserved.end(), stem) != reserved.end()) {
        return true;
    }
    if (stem.size() == 4 && stem[3] >= '1' && stem[3] <= '9') {
        return stem.starts_with("COM") || stem.starts_with("LPT");
    }
    return false;
}

[[nodiscard]] bool validatePortablePathInternal(std::string_view value, std::size_t maxBytes,
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

[[nodiscard]] std::optional<std::string> readString(const json::Reader& reader) {
    const auto value = reader.readString();
    return value ? std::optional<std::string>{std::string{*value}} : std::nullopt;
}

[[nodiscard]] core::Result<std::string> readBoundedFile(const fs::path& file,
                                                        std::size_t maxBytes) {
    std::ifstream stream{file, std::ios::binary};
    if (!stream) {
        return core::unexpected(
            core::Error{"project.file.open_failed", "Project file could not be opened"});
    }

    std::string text;
    text.reserve(std::min<std::size_t>(maxBytes, 64U * 1024U));
    std::array<char, 8192> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<std::size_t>(stream.gcount());
        if (count > maxBytes - std::min(text.size(), maxBytes)) {
            return core::unexpected(core::Error{"project.file.size_limit",
                                                "Project file exceeds the configured byte limit"}
                                        .withContext("max_bytes", std::to_string(maxBytes)));
        }
        text.append(buffer.data(), count);
    }
    if (!stream.eof()) {
        return core::unexpected(
            core::Error{"project.file.read_failed", "Project file could not be read completely"});
    }
    return text;
}

#if defined(_WIN32)
[[nodiscard]] bool componentEqual(const fs::path& left, const fs::path& right) {
    const auto& leftText = left.native();
    const auto& rightText = right.native();
    return leftText.size() == rightText.size() &&
           std::equal(leftText.begin(), leftText.end(), rightText.begin(),
                      [](wchar_t leftCharacter, wchar_t rightCharacter) {
                          return std::towlower(leftCharacter) == std::towlower(rightCharacter);
                      });
}
#else
[[nodiscard]] bool componentEqual(const fs::path& left, const fs::path& right) {
    return left.native() == right.native();
}
#endif

[[nodiscard]] bool containsPath(const fs::path& base, const fs::path& candidate) {
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

[[nodiscard]] bool strictlyContainsPath(const fs::path& base, const fs::path& candidate) {
    if (!containsPath(base, candidate)) {
        return false;
    }
    return static_cast<std::size_t>(std::distance(base.begin(), base.end())) <
           static_cast<std::size_t>(std::distance(candidate.begin(), candidate.end()));
}

[[nodiscard]] std::optional<fs::path>
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

[[nodiscard]] ProjectLoadResult prepareProject(ProjectConfig config, const fs::path& projectFile,
                                               const fs::path& projectRoot,
                                               core::Diagnostics diagnostics,
                                               const ProjectLimits& limits) {
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

[[nodiscard]] json::Value projectConfigValue(const ProjectConfig& config,
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

[[nodiscard]] core::Result<std::string> serializeProjectConfig(const ProjectConfig& config,
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

[[nodiscard]] core::Error invalidConfigError(const core::Diagnostics& diagnostics,
                                             std::string_view code, std::string_view message) {
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

[[nodiscard]] core::Result<fs::path> saveTarget(const fs::path& locator) {
    if (locator.empty()) {
        return core::unexpected(
            core::Error{"project.locator.empty", "Project locator cannot be empty"});
    }
    std::error_code error;
    const auto status = fs::status(locator, error);
    fs::path target;
    if (!error && fs::is_directory(status)) {
        target = locator / std::string{projectFileName};
    } else if (locator.filename() == fs::path{projectFileName}) {
        target = locator;
    } else {
        return core::unexpected(core::Error{
            "project.locator.invalid",
            "Project locator must be a directory or the exact cuexis.project.json file"});
    }
    const auto parentStatus = fs::status(target.parent_path(), error);
    if (error || !fs::is_directory(parentStatus)) {
        return core::unexpected(
            core::Error{"project.root.invalid", "Project root must be an existing directory"});
    }
    return target;
}

[[nodiscard]] fs::path temporarySibling(const fs::path& target, std::uint64_t attempt) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto ticks =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto token = ticks ^ sequence.fetch_add(1, std::memory_order_relaxed) ^ attempt;
    return target.parent_path() /
           (target.filename().native() + fs::path{".tmp." + std::to_string(token)}.native());
}

#if defined(_WIN32)
[[nodiscard]] core::Result<void> writeExclusive(const fs::path& file, std::string_view text) {
    const HANDLE handle = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::unexpected(
            core::Error{"project.save.temp_create_failed", "Temporary project file creation failed"}
                .withContext("platform_error", std::to_string(GetLastError())));
    }

    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto remaining = std::min<std::size_t>(text.size() - offset, 0x7FFFFFFFU);
        DWORD written = 0;
        if (WriteFile(handle, text.data() + offset, static_cast<DWORD>(remaining), &written,
                      nullptr) == 0 ||
            written == 0) {
            succeeded = false;
            break;
        }
        offset += written;
    }
    if (succeeded && FlushFileBuffers(handle) == 0) {
        succeeded = false;
    }
    const bool closed = CloseHandle(handle) != 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            core::Error{"project.save.temp_write_failed", "Temporary project file write failed"}
                .withContext("platform_error", std::to_string(GetLastError())));
    }
    return {};
}

[[nodiscard]] core::Result<void> replaceAtomically(const fs::path& source, const fs::path& target) {
    if (MoveFileExW(source.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        return core::unexpected(
            core::Error{"project.save.replace_failed", "Project file atomic replacement failed"}
                .withContext("platform_error", std::to_string(GetLastError())));
    }
    return {};
}
#else
[[nodiscard]] core::Result<void> writeExclusive(const fs::path& file, std::string_view text) {
    const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) {
        return core::unexpected(
            core::Error{"project.save.temp_create_failed", "Temporary project file creation failed"}
                .withContext("platform_error", std::to_string(errno)));
    }

    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto written = ::write(descriptor, text.data() + offset, text.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            succeeded = false;
            break;
        }
        offset += static_cast<std::size_t>(written);
    }
    if (succeeded && ::fsync(descriptor) != 0) {
        succeeded = false;
    }
    const bool closed = ::close(descriptor) == 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            core::Error{"project.save.temp_write_failed", "Temporary project file write failed"}
                .withContext("platform_error", std::to_string(errno)));
    }
    return {};
}

[[nodiscard]] core::Result<void> replaceAtomically(const fs::path& source, const fs::path& target) {
    if (::rename(source.c_str(), target.c_str()) != 0) {
        return core::unexpected(
            core::Error{"project.save.replace_failed", "Project file atomic replacement failed"}
                .withContext("platform_error", std::to_string(errno)));
    }
    return {};
}
#endif

} // namespace

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

core::Result<void> ProjectLoader::saveAtomic(const ProjectConfig& config, const fs::path& locator,
                                             const ProjectLimits& limits) {
    auto serialized = serializeProjectConfig(config, limits);
    if (!serialized) {
        return core::unexpected(serialized.error());
    }
    auto validated = ProjectConfigReader::read(*serialized, limits);
    if (!validated.hasValue()) {
        return core::unexpected(invalidConfigError(validated.diagnostics,
                                                   "project.save.config_invalid",
                                                   "ProjectConfig is invalid and was not saved"));
    }
    serialized = serializeProjectConfig(*validated.config, limits);
    if (!serialized) {
        return core::unexpected(serialized.error());
    }

    const auto target = saveTarget(locator);
    if (!target) {
        return core::unexpected(target.error());
    }

    fs::path temporary;
    core::Result<void> written = core::unexpected(
        core::Error{"project.save.temp_create_failed", "Temporary project file creation failed"});
    constexpr std::uint64_t maxAttempts = 128;
    for (std::uint64_t attempt = 0; attempt < maxAttempts; ++attempt) {
        temporary = temporarySibling(*target, attempt);
        written = writeExclusive(temporary, *serialized);
        if (written) {
            break;
        }
        std::error_code ignored;
        fs::remove(temporary, ignored);
    }
    if (!written) {
        return core::unexpected(written.error());
    }

    const auto cleanup = [&temporary]() {
        std::error_code ignored;
        fs::remove(temporary, ignored);
    };
    const auto reloadedText = readBoundedFile(temporary, limits.maxInputBytes);
    if (!reloadedText) {
        cleanup();
        return core::unexpected(reloadedText.error());
    }
    auto reloaded = ProjectConfigReader::read(*reloadedText, limits);
    if (!reloaded.hasValue() || *reloaded.config != *validated.config) {
        const auto error =
            invalidConfigError(reloaded.diagnostics, "project.save.reload_failed",
                               "Temporary ProjectConfig failed full reload validation");
        cleanup();
        return core::unexpected(error);
    }
    auto prepared = prepareProject(*reloaded.config, *target, target->parent_path(),
                                   std::move(reloaded.diagnostics), limits);
    if (!prepared.hasValue()) {
        const auto error =
            invalidConfigError(prepared.diagnostics, "project.save.reload_failed",
                               "Temporary ProjectConfig failed full reload validation");
        cleanup();
        return core::unexpected(error);
    }

    auto replaced = replaceAtomically(temporary, *target);
    if (!replaced) {
        cleanup();
        return core::unexpected(replaced.error());
    }
    return {};
}

} // namespace cuexis::project
