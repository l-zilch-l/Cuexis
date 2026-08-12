#include <cuexis/cxc/cxc_manifest_loader.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::cxc {
namespace {

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(path)});
}

[[nodiscard]] auto makeDiagnostics(const CxcManifestLimits& limits) -> core::Diagnostics {
    if (limits.maxDiagnostics == 0) {
        return {};
    }
    return core::Diagnostics{
        limits.maxDiagnostics,
        core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.budget.exceeded",
                         "CXC diagnostics reached the configured limit", "$"}
            .withContext("max_diagnostics", std::to_string(limits.maxDiagnostics))};
}

void addParseError(core::Diagnostics& diagnostics, const core::Error& error) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, "$"};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto isAsciiAlphaNumeric(char character) noexcept -> bool {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}

[[nodiscard]] auto isPortableStableId(std::string_view value) noexcept -> bool {
    return !value.empty() && value.size() <= 256 && isAsciiAlphaNumeric(value.front()) &&
           std::ranges::all_of(value.substr(1), [](char character) {
               return isAsciiAlphaNumeric(character) || character == '.' || character == '_' ||
                      character == '-';
           });
}

[[nodiscard]] auto foldAscii(std::string_view value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(character >= 'A' && character <= 'Z'
                             ? static_cast<char>(character - 'A' + 'a')
                             : character);
    }
    return result;
}

[[nodiscard]] auto isWindowsReservedSegment(std::string_view segment) -> bool {
    const auto dot = segment.find('.');
    const auto stem = foldAscii(segment.substr(0, dot));
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") {
        return true;
    }
    if (stem.size() == 4 && stem[3] >= '1' && stem[3] <= '9') {
        return stem.starts_with("com") || stem.starts_with("lpt");
    }
    return false;
}

[[nodiscard]] auto isPortablePath(std::string_view path) -> bool {
    if (path.empty() || path.size() > 4096 || path.front() == '/' || path.back() == '/' ||
        path.find("//") != std::string_view::npos) {
        return false;
    }
    std::size_t segmentStart = 0;
    while (segmentStart < path.size()) {
        const auto separator = path.find('/', segmentStart);
        const auto segment = path.substr(segmentStart, separator - segmentStart);
        if (segment.empty() || segment == "." || segment == ".." || segment.back() == '.' ||
            isWindowsReservedSegment(segment) || !std::ranges::all_of(segment, [](char character) {
                return isAsciiAlphaNumeric(character) || character == '.' || character == '_' ||
                       character == '-';
            })) {
            return false;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        segmentStart = separator + 1;
    }
    return true;
}

[[nodiscard]] auto isSha256(std::string_view value) noexcept -> bool {
    return value.size() == 64 && std::ranges::all_of(value, [](char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

[[nodiscard]] auto readRequiredExtensions(const json::Reader& reader,
                                          const CxcManifestLimits& limits,
                                          core::Diagnostics& diagnostics)
    -> std::vector<CxcRequiredExtension> {
    std::vector<CxcRequiredExtension> result;
    const auto* items = reader.readArray();
    if (items == nullptr) {
        return result;
    }
    if (items->size() > limits.maxExtensions) {
        addError(diagnostics, "cxc.budget.exceeded",
                 "CXC required extension count exceeds the limit", std::string{reader.fieldPath()});
    }
    std::set<std::string, std::less<>> ids;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{std::string_view{"id"}, std::string_view{"version"}};
        item->rejectUnknownFields(fields);
        const auto idReader = item->requiredField("id");
        const auto versionReader = item->requiredField("version");
        auto id = std::optional<std::string_view>{};
        auto version = std::optional<std::uint64_t>{};
        if (idReader) {
            id = idReader->readString();
        }
        if (versionReader) {
            version = versionReader->readUInt64();
        }
        if (id && !isPortableStableId(*id)) {
            addError(diagnostics, "cxc.project.invalid", "CXC extension ID is invalid",
                     std::string{idReader->fieldPath()});
        }
        if (version && (*version == 0 || *version > std::numeric_limits<std::uint32_t>::max())) {
            addError(diagnostics, "cxc.version.unsupported", "CXC extension version is invalid",
                     std::string{versionReader->fieldPath()});
        }
        if (!id || !isPortableStableId(*id) || !version || *version == 0 ||
            *version > std::numeric_limits<std::uint32_t>::max()) {
            continue;
        }
        const auto idValue = *id;
        const auto versionValue = *version;
        if (!ids.emplace(idValue).second) {
            addError(diagnostics, "cxc.entry.duplicate", "CXC extension ID is duplicated",
                     std::string{idReader->fieldPath()});
            continue;
        }
        result.push_back(
            CxcRequiredExtension{std::string{idValue}, static_cast<std::uint32_t>(versionValue)});
    }
    std::ranges::sort(result, {}, &CxcRequiredExtension::id);
    return result;
}

[[nodiscard]] auto readEntries(const json::Reader& reader, const CxcManifestLimits& limits,
                               core::Diagnostics& diagnostics) -> std::vector<CxcManifestEntry> {
    std::vector<CxcManifestEntry> result;
    const auto* items = reader.readArray();
    if (items == nullptr) {
        return result;
    }
    if (items->empty()) {
        addError(diagnostics, "cxc.entry.missing", "CXC manifest must list project content",
                 std::string{reader.fieldPath()});
    }
    if (items->size() > limits.maxEntries) {
        addError(diagnostics, "cxc.budget.exceeded", "CXC entry count exceeds the limit",
                 std::string{reader.fieldPath()});
    }

    std::set<std::string, std::less<>> exactPaths;
    std::set<std::string, std::less<>> foldedPaths;
    std::optional<std::string> previousPath;
    std::uint64_t listedBytes = 0;
    bool hasProject = false;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{std::string_view{"path"}, std::string_view{"byteCount"},
                                    std::string_view{"sha256"}};
        item->rejectUnknownFields(fields);
        const auto pathReader = item->requiredField("path");
        const auto byteCountReader = item->requiredField("byteCount");
        const auto shaReader = item->requiredField("sha256");
        auto path = std::optional<std::string_view>{};
        auto byteCount = std::optional<std::uint64_t>{};
        auto sha = std::optional<std::string_view>{};
        if (pathReader) {
            path = pathReader->readString();
        }
        if (byteCountReader) {
            byteCount = byteCountReader->readUInt64();
        }
        if (shaReader) {
            sha = shaReader->readString();
        }
        const auto pathValid = path && isPortablePath(*path) && *path != "cuexis.cxc.json";
        const auto byteCountValid = byteCount && *byteCount <= limits.maxEntryBytes;
        const auto shaValid = sha && isSha256(*sha);
        if (path && !pathValid) {
            addError(diagnostics, "cxc.entry.path_invalid", "CXC entry path is not portable",
                     std::string{pathReader->fieldPath()});
        }
        if (byteCount && !byteCountValid) {
            addError(diagnostics, "cxc.budget.exceeded", "CXC entry byteCount exceeds the limit",
                     std::string{byteCountReader->fieldPath()});
        }
        if (sha && !shaValid) {
            addError(diagnostics, "cxc.entry.hash_mismatch",
                     "CXC entry SHA-256 must be lowercase hexadecimal",
                     std::string{shaReader->fieldPath()});
        }
        if (!pathValid || !byteCountValid || !shaValid) {
            continue;
        }

        const auto pathText = std::string{*path};
        const auto byteCountValue = *byteCount;
        const auto shaText = std::string{*sha};
        const auto foldedPath = foldAscii(*path);
        if (!exactPaths.emplace(pathText).second || !foldedPaths.emplace(foldedPath).second) {
            addError(diagnostics, "cxc.entry.duplicate",
                     "CXC entry path is duplicated or has an ASCII case conflict",
                     std::string{pathReader->fieldPath()});
        }
        if (previousPath && pathText <= *previousPath) {
            addError(diagnostics, "cxc.entry.order_invalid",
                     "CXC entries must be sorted by portable path bytes",
                     std::string{pathReader->fieldPath()});
        }
        previousPath = pathText;
        hasProject = hasProject || pathText == "cuexis.project.json";
        if (listedBytes > limits.maxListedBytes ||
            byteCountValue > limits.maxListedBytes - listedBytes) {
            addError(diagnostics, "cxc.budget.exceeded", "CXC listed byte total exceeds the limit",
                     std::string{byteCountReader->fieldPath()});
        } else {
            listedBytes += byteCountValue;
        }
        result.push_back(
            CxcManifestEntry{pathText, byteCountValue, shaText, std::string{item->fieldPath()}});
    }
    if (!hasProject) {
        addError(diagnostics, "cxc.entry.missing", "CXC manifest does not list cuexis.project.json",
                 std::string{reader.fieldPath()});
    }
    return result;
}

} // namespace

auto CxcManifestLoader::load(std::string_view jsonText, const CxcManifestLimits& limits)
    -> CxcManifestResult {
    auto diagnostics = makeDiagnostics(limits);
    if (limits.maxDiagnostics == 0) {
        diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.budget.exceeded",
                                         "CXC diagnostic limit must be greater than zero",
                                         "$/limits/maxDiagnostics"});
        diagnostics.sortDeterministically();
        return CxcManifestResult{std::nullopt, std::move(diagnostics)};
    }
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxManifestBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addParseError(diagnostics, parsed.error());
        diagnostics.sortDeterministically();
        return CxcManifestResult{std::nullopt, std::move(diagnostics)};
    }

    json::Reader root{*parsed, diagnostics};
    constexpr std::array fields{std::string_view{"format"},
                                std::string_view{"version"},
                                std::string_view{"project"},
                                std::string_view{"entries"},
                                std::string_view{"requiredExtensions"},
                                std::string_view{"extensions"}};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return CxcManifestResult{std::nullopt, std::move(diagnostics)};
    }
    root.rejectUnknownFields(fields);
    const auto formatReader = root.requiredField("format");
    const auto versionReader = root.requiredField("version");
    const auto projectReader = root.requiredField("project");
    const auto entriesReader = root.requiredField("entries");
    const auto requiredExtensionsReader = root.requiredField("requiredExtensions");
    const auto extensionsReader = root.requiredField("extensions");
    const auto format = formatReader ? formatReader->readString() : std::nullopt;
    auto version = std::optional<std::int64_t>{};
    const auto project = projectReader ? projectReader->readString() : std::nullopt;
    if (versionReader) {
        version = versionReader->readInt64();
    }
    if (format && *format != "cuexis.cxc") {
        addError(diagnostics, "cxc.format.unsupported", "CXC manifest format is unsupported",
                 std::string{formatReader->fieldPath()});
    }
    if (version && *version != 1) {
        addError(diagnostics, "cxc.version.unsupported", "CXC manifest version is unsupported",
                 std::string{versionReader->fieldPath()});
    }
    if (project && *project != "cuexis.project.json") {
        addError(diagnostics, "cxc.project.invalid",
                 "CXC v1 project path must be cuexis.project.json",
                 std::string{projectReader->fieldPath()});
    }

    auto entries = entriesReader ? readEntries(*entriesReader, limits, diagnostics)
                                 : std::vector<CxcManifestEntry>{};
    auto requiredExtensions =
        requiredExtensionsReader
            ? readRequiredExtensions(*requiredExtensionsReader, limits, diagnostics)
            : std::vector<CxcRequiredExtension>{};
    std::string extensionsJson;
    if (extensionsReader) {
        const auto* object = extensionsReader->readObject();
        if (object != nullptr && object->size() > limits.maxExtensions) {
            addError(diagnostics, "cxc.budget.exceeded",
                     "CXC extension member count exceeds the limit",
                     std::string{extensionsReader->fieldPath()});
        }
        if (auto serialized = json::serialize(extensionsReader->value())) {
            extensionsJson = std::move(*serialized);
        } else {
            addParseError(diagnostics, serialized.error());
        }
    }
    std::string canonicalSource;
    if (auto serialized = json::serialize(*parsed)) {
        canonicalSource = std::move(*serialized);
    } else {
        addParseError(diagnostics, serialized.error());
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors() || !format || *format != "cuexis.cxc" || !version ||
        *version != 1 || !project || *project != "cuexis.project.json" || !entriesReader ||
        !requiredExtensionsReader || !extensionsReader) {
        return CxcManifestResult{std::nullopt, std::move(diagnostics)};
    }
    return CxcManifestResult{CxcManifestDocument{std::string{*project}, std::move(entries),
                                                 std::move(requiredExtensions),
                                                 std::move(extensionsJson),
                                                 std::move(canonicalSource)},
                             std::move(diagnostics)};
}

} // namespace cuexis::cxc
