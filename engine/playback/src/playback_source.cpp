#include <cuexis/playback/playback_source.hpp>

#include "playback_source_state.hpp"

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/detail/chart_dispatch_internal.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/project/asset_index_reader.hpp>
#include <cuexis/project/project_loader.hpp>
#include <cuexis_internal/portable_path.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::playback {
namespace {

constexpr std::string_view standaloneChartPath = "chart.cuexis.chart.json";
constexpr std::size_t maxProjectDocumentCount = 10001;
constexpr std::size_t maxProjectDocumentBytes = 16U * 1024U * 1024U;
constexpr std::size_t maxProjectDocumentTotalBytes = 512U * 1024U * 1024U;
constexpr std::size_t maxProjectPathBytes = 4096;
constexpr std::size_t maxProjectPathDepth = 64;
constexpr std::size_t maxSourceIdBytes = 256;

struct CxcSourceData final {
    std::string sourceId;
    std::string entryChartPath;
    std::vector<PlaybackProjectDocument> projectDocuments;
    std::optional<assets::AssetDatabase> database;
    std::shared_ptr<content::IContentProvider> provider;
    std::array<std::uint8_t, 32> packageIdentity{};
};

[[nodiscard]] auto firstDiagnosticError(std::string code, std::string message,
                                        const core::Diagnostics& diagnostics) -> core::Error {
    auto error = core::Error{std::move(code), std::move(message)};
    if (!diagnostics.empty()) {
        const auto& first = diagnostics.items().front();
        error.withContext("diagnostic_code", std::string{first.code()})
            .withContext("field_path", std::string{first.fieldPath()});
        for (const auto& item : first.context()) {
            error.withContext(item.key, item.value);
        }
    }
    return error;
}

[[nodiscard]] auto translatePresentationAssetError(core::Error error) -> core::Error {
    if (error.code() != "assets.database.dependency_cycle") {
        return error;
    }

    auto translated = core::Error{"playback.presentation.dependency.cycle",
                                  "Portable dependency closure contains a cycle"};
    for (const auto& context : error.context()) {
        translated.withContext(context.key, context.value);
    }
    translated.withCause(std::move(error));
    return translated;
}

[[nodiscard]] auto sourceFailure(std::string_view operation,
                                 const std::exception* exception = nullptr) -> core::Error {
    auto error = core::Error{"playback.source.create_failed", "PlaybackSource creation failed"};
    error.withContext("operation", std::string{operation});
    if (exception != nullptr) {
        error.withContext("exception", exception->what());
    }
    return error;
}

[[nodiscard]] auto sourceAllocationFailure(std::string_view operation) -> core::Error {
    return core::Error{"playback.source.budget_exceeded",
                       "PlaybackSource allocation could not be satisfied"}
        .withContext("operation", std::string{operation});
}

[[nodiscard]] auto readTextFile(const std::filesystem::path& path,
                                const std::filesystem::path& root, std::size_t maxBytes,
                                std::string_view prefix, std::string_view description)
    -> core::Result<std::string> {
    const auto name = std::string{prefix};
    auto result = filesystem::readBoundedTextFile(
        path, {.root = root,
               .maxBytes = maxBytes,
               .errors = {.rootUnavailable = name + ".root_unavailable",
                          .rootChanged = name + ".root_changed",
                          .openFailed = name + ".open_failed",
                          .outsideRoot = name + ".outside_root",
                          .notRegular = name + ".not_regular",
                          .tooLarge = name + ".too_large",
                          .readFailed = name + ".read_failed",
                          .changedDuringRead = name + ".changed_during_read"}});
    if (!result) {
        return core::unexpected(
            std::move(result.error()).withContext("description", std::string{description}));
    }
    return std::move(result->text);
}

[[nodiscard]] auto isAsciiAlphaNumeric(char character) noexcept -> bool {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}

using cuexis::core::detail::foldAscii;
using cuexis::core::detail::isWindowsReservedSegment;

[[nodiscard]] auto isPortableProjectPath(std::string_view path) -> bool {
    if (path.empty() || path.size() > maxProjectPathBytes || path.front() == '/' ||
        path.back() == '/' || path.find("//") != std::string_view::npos) {
        return false;
    }

    std::size_t depth = 0;
    std::size_t segmentStart = 0;
    while (segmentStart < path.size()) {
        const auto separator = path.find('/', segmentStart);
        const auto segment = path.substr(segmentStart, separator == std::string_view::npos
                                                           ? path.size() - segmentStart
                                                           : separator - segmentStart);
        ++depth;
        if (depth > maxProjectPathDepth || segment.empty() || segment == "." || segment == ".." ||
            segment.back() == '.' || segment.back() == ' ' || isWindowsReservedSegment(segment)) {
            return false;
        }
        for (const char character : segment) {
            if (!isAsciiAlphaNumeric(character) && character != '.' && character != '_' &&
                character != '-') {
                return false;
            }
        }
        if (separator == std::string_view::npos) {
            break;
        }
        segmentStart = separator + 1U;
    }
    return true;
}

[[nodiscard]] auto joinPortableProjectPath(std::string_view base, std::string_view relative)
    -> std::optional<std::string> {
    if (!isPortableProjectPath(base) || !isPortableProjectPath(relative) ||
        base.size() >= maxProjectPathBytes ||
        relative.size() > maxProjectPathBytes - base.size() - 1U) {
        return std::nullopt;
    }
    std::string result;
    result.reserve(base.size() + relative.size() + 1U);
    result.append(base);
    result.push_back('/');
    result.append(relative);
    return isPortableProjectPath(result) ? std::optional<std::string>{std::move(result)}
                                         : std::nullopt;
}

[[nodiscard]] auto insertUniqueProjectPath(std::set<std::string, std::less<>>& foldedPaths,
                                           std::string_view path) -> bool {
    auto folded = foldAscii(path);
    if (foldedPaths.contains(folded)) {
        return false;
    }

    auto separator = folded.find('/');
    while (separator != std::string::npos) {
        if (foldedPaths.contains(folded.substr(0, separator))) {
            return false;
        }
        separator = folded.find('/', separator + 1U);
    }

    auto descendantPrefix = folded;
    descendantPrefix.push_back('/');
    const auto descendant = foldedPaths.lower_bound(descendantPrefix);
    if (descendant != foldedPaths.end() && descendant->starts_with(descendantPrefix)) {
        return false;
    }
    foldedPaths.emplace(std::move(folded));
    return true;
}

[[nodiscard]] auto isPortableSourceId(std::string_view value) -> bool {
    if (value.empty()) {
        return true;
    }
    if (value.size() > maxSourceIdBytes || !isAsciiAlphaNumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return isAsciiAlphaNumeric(character) || character == '.' || character == '_' ||
               character == '-' || character == '/';
    });
}

[[nodiscard]] auto isUtf8Continuation(unsigned char value) noexcept -> bool {
    return value >= 0x80U && value <= 0xBFU;
}

[[nodiscard]] auto isValidUtf8(std::string_view text) noexcept -> bool {
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = bytes[index];
        if (first <= 0x7FU) {
            ++index;
            continue;
        }
        if (first >= 0xC2U && first <= 0xDFU) {
            if (index + 1U >= text.size() || !isUtf8Continuation(bytes[index + 1U])) {
                return false;
            }
            index += 2U;
            continue;
        }
        if (first == 0xE0U) {
            if (index + 2U >= text.size() || bytes[index + 1U] < 0xA0U ||
                bytes[index + 1U] > 0xBFU || !isUtf8Continuation(bytes[index + 2U])) {
                return false;
            }
            index += 3U;
            continue;
        }
        if ((first >= 0xE1U && first <= 0xECU) || (first >= 0xEEU && first <= 0xEFU)) {
            if (index + 2U >= text.size() || !isUtf8Continuation(bytes[index + 1U]) ||
                !isUtf8Continuation(bytes[index + 2U])) {
                return false;
            }
            index += 3U;
            continue;
        }
        if (first == 0xEDU) {
            if (index + 2U >= text.size() || bytes[index + 1U] < 0x80U ||
                bytes[index + 1U] > 0x9FU || !isUtf8Continuation(bytes[index + 2U])) {
                return false;
            }
            index += 3U;
            continue;
        }
        if (first == 0xF0U) {
            if (index + 3U >= text.size() || bytes[index + 1U] < 0x90U ||
                bytes[index + 1U] > 0xBFU || !isUtf8Continuation(bytes[index + 2U]) ||
                !isUtf8Continuation(bytes[index + 3U])) {
                return false;
            }
            index += 4U;
            continue;
        }
        if (first >= 0xF1U && first <= 0xF3U) {
            if (index + 3U >= text.size() || !isUtf8Continuation(bytes[index + 1U]) ||
                !isUtf8Continuation(bytes[index + 2U]) || !isUtf8Continuation(bytes[index + 3U])) {
                return false;
            }
            index += 4U;
            continue;
        }
        if (first == 0xF4U) {
            if (index + 3U >= text.size() || bytes[index + 1U] < 0x80U ||
                bytes[index + 1U] > 0x8FU || !isUtf8Continuation(bytes[index + 2U]) ||
                !isUtf8Continuation(bytes[index + 3U])) {
                return false;
            }
            index += 4U;
            continue;
        }
        return false;
    }
    return true;
}

[[nodiscard]] auto validateProjectDocuments(std::string_view entryChartPath,
                                            std::vector<PlaybackProjectDocument> documents)
    -> core::Result<std::vector<PlaybackProjectDocument>> {
    if (!isPortableProjectPath(entryChartPath)) {
        return core::unexpected(core::Error{"playback.source.entry_path_invalid",
                                            "Playback entry Chart path is not portable"}
                                    .withContext("path", std::string{entryChartPath}));
    }
    if (documents.size() > maxProjectDocumentCount) {
        return core::unexpected(core::Error{"playback.source.document_count_exceeded",
                                            "Playback project-document count exceeds the limit"}
                                    .withContext("limit", std::to_string(maxProjectDocumentCount)));
    }

    std::size_t totalBytes = 0;
    std::set<std::string, std::less<>> foldedPaths;
    for (const auto& document : documents) {
        if (!isPortableProjectPath(document.path)) {
            return core::unexpected(core::Error{"playback.source.document_path_invalid",
                                                "Playback project-document path is not portable"}
                                        .withContext("path", document.path));
        }
        if (!insertUniqueProjectPath(foldedPaths, document.path)) {
            return core::unexpected(core::Error{"playback.source.document_path_conflict",
                                                "Playback project-document paths conflict"}
                                        .withContext("path", document.path));
        }
        if (document.utf8Text.size() > maxProjectDocumentBytes) {
            return core::unexpected(
                core::Error{"playback.source.document_too_large",
                            "Playback project-document exceeds the byte limit"}
                    .withContext("path", document.path)
                    .withContext("limit", std::to_string(maxProjectDocumentBytes)));
        }
        if (document.utf8Text.size() > maxProjectDocumentTotalBytes - totalBytes) {
            return core::unexpected(
                core::Error{"playback.source.document_total_too_large",
                            "Playback project-document table exceeds the aggregate byte limit"}
                    .withContext("limit", std::to_string(maxProjectDocumentTotalBytes)));
        }
        if (!isValidUtf8(document.utf8Text)) {
            return core::unexpected(core::Error{"playback.source.document_utf8_invalid",
                                                "Playback project-document is not valid UTF-8"}
                                        .withContext("path", document.path));
        }
        totalBytes += document.utf8Text.size();
    }

    std::sort(documents.begin(), documents.end(),
              [](const auto& left, const auto& right) { return left.path < right.path; });
    const auto entry = std::lower_bound(documents.begin(), documents.end(), entryChartPath,
                                        [](const PlaybackProjectDocument& document,
                                           std::string_view path) { return document.path < path; });
    if (entry == documents.end() || entry->path != entryChartPath) {
        return core::unexpected(
            core::Error{"playback.source.entry_document_missing",
                        "Playback entry Chart is missing from the document table"}
                .withContext("path", std::string{entryChartPath}));
    }
    if (entry->utf8Text.empty()) {
        return core::unexpected(
            core::Error{"playback.source.chart_empty", "Playback chart text must not be empty"});
    }
    return documents;
}

[[nodiscard]] auto toAssetType(PlaybackAssetType type) noexcept -> assets::AssetType {
    switch (type) {
    case PlaybackAssetType::Mesh:
        return assets::AssetType::Mesh;
    case PlaybackAssetType::Material:
        return assets::AssetType::Material;
    case PlaybackAssetType::Texture:
        return assets::AssetType::Texture;
    case PlaybackAssetType::Audio:
        return assets::AssetType::Audio;
    case PlaybackAssetType::Shader:
        return assets::AssetType::Shader;
    }
    return assets::AssetType::Mesh;
}

[[nodiscard]] auto toAssetType(project::AssetType type) noexcept -> assets::AssetType {
    switch (type) {
    case project::AssetType::Mesh:
        return assets::AssetType::Mesh;
    case project::AssetType::Material:
        return assets::AssetType::Material;
    case project::AssetType::Texture:
        return assets::AssetType::Texture;
    case project::AssetType::Audio:
        return assets::AssetType::Audio;
    case project::AssetType::Shader:
        return assets::AssetType::Shader;
    }
    return assets::AssetType::Mesh;
}

[[nodiscard]] auto convertTypedAssets(const std::vector<PlaybackAssetDescriptor>& descriptors)
    -> core::Result<assets::AssetDatabase> {
    assets::AssetDatabaseInput input;
    input.sourceMode = assets::AssetSourceMode::Logical;
    std::map<std::string, std::size_t, std::less<>> rootIndexes;
    std::uint32_t requiredVersion = 1;
    for (const auto& descriptor : descriptors) {
        if (descriptor.rootId.empty()) {
            return core::unexpected(core::Error{"playback.source.root_missing",
                                                "Playback asset descriptor requires a root ID"});
        }
        auto [rootIt, inserted] = rootIndexes.emplace(descriptor.rootId, input.roots.size());
        if (inserted) {
            input.roots.push_back(
                assets::AssetRootIndex{.root = {.id = descriptor.rootId, .path = {}}, .index = {}});
        }
        auto& index = input.roots[rootIt->second].index;
        if (descriptor.type == PlaybackAssetType::Audio) {
            requiredVersion = std::max(requiredVersion, 2U);
        } else if (descriptor.type == PlaybackAssetType::Shader) {
            requiredVersion = 3;
        }
        assets::AssetRecord record{.id = assets::AssetId{descriptor.id},
                                   .type = toAssetType(descriptor.type),
                                   .source = descriptor.logicalSource,
                                   .dependencies = {}};
        record.dependencies.reserve(descriptor.dependencies.size());
        for (const auto& dependency : descriptor.dependencies) {
            record.dependencies.push_back(assets::AssetId{dependency});
        }
        index.assets.push_back(std::move(record));
    }
    if (requiredVersion > 1) {
        for (auto& root : input.roots) {
            root.index.version = requiredVersion;
        }
    }
    return assets::AssetDatabase::create(input);
}

[[nodiscard]] auto convertCxcAssets(const cxc::CxcPackage& package)
    -> core::Result<assets::AssetDatabase> {
    assets::AssetDatabaseInput input;
    input.sourceMode = assets::AssetSourceMode::Logical;
    input.roots.reserve(package.assetIndexes().size());
    for (const auto& packageIndex : package.assetIndexes()) {
        assets::AssetRootIndex converted;
        converted.root = {.id = packageIndex.rootId, .path = {}};
        converted.index.format = packageIndex.document.format;
        converted.index.version = packageIndex.document.version;
        converted.index.assets.reserve(packageIndex.document.assets.size());
        for (const auto& record : packageIndex.document.assets) {
            assets::AssetRecord asset{.id = assets::AssetId{record.id},
                                      .type = toAssetType(record.type),
                                      .source = record.source,
                                      .dependencies = {}};
            asset.dependencies.reserve(record.dependencies.size());
            for (const auto& dependency : record.dependencies) {
                asset.dependencies.push_back(assets::AssetId{dependency});
            }
            converted.index.assets.push_back(std::move(asset));
        }
        input.roots.push_back(std::move(converted));
    }
    return assets::AssetDatabase::create(input);
}

[[nodiscard]] auto cxcEntryChartPath(const cxc::CxcPackage& package) -> core::Result<std::string> {
    const auto& project = package.project();
    const auto root = std::find_if(project.assetRoots.begin(), project.assetRoots.end(),
                                   [&](const project::AssetRoot& candidate) {
                                       return candidate.id == project.entry.chart.root;
                                   });
    if (root == project.assetRoots.end()) {
        return core::unexpected(core::Error{"playback.source.chart_root_missing",
                                            "CXC entry Chart root is unavailable"});
    }
    auto path = joinPortableProjectPath(root->path, project.entry.chart.path);
    if (!path) {
        return core::unexpected(core::Error{"playback.source.entry_path_invalid",
                                            "CXC entry Chart path is not portable"});
    }
    return std::move(*path);
}

[[nodiscard]] auto convertCxcPackage(cxc::CxcPackage package) -> core::Result<CxcSourceData> {
    auto entryPath = cxcEntryChartPath(package);
    if (!entryPath) {
        return core::unexpected(std::move(entryPath.error()));
    }

    std::vector<PlaybackProjectDocument> documents;
    documents.reserve(package.projectDocuments().size());
    for (const auto& document : package.projectDocuments()) {
        documents.push_back({document.path, document.utf8Text});
    }
    auto validatedDocuments = validateProjectDocuments(*entryPath, std::move(documents));
    if (!validatedDocuments) {
        return core::unexpected(std::move(validatedDocuments.error()));
    }

    std::optional<assets::AssetDatabase> database;
    if (!package.assetIndexes().empty()) {
        auto converted = convertCxcAssets(package);
        if (!converted) {
            return core::unexpected(translatePresentationAssetError(std::move(converted.error())));
        }
        database.emplace(std::move(*converted));
    }

    auto provider = package.contentProvider();
    if (!provider) {
        return core::unexpected(
            core::Error{"playback.source.provider_missing", "CXC package has no content provider"});
    }

    CxcSourceData result;
    result.sourceId = package.project().projectId;
    result.entryChartPath = std::move(*entryPath);
    result.projectDocuments = std::move(*validatedDocuments);
    result.database = std::move(database);
    result.provider = std::move(provider);
    result.packageIdentity = package.identity().sha256;
    return result;
}

} // namespace

auto PlaybackSource::State::entryChart() const noexcept -> const PlaybackProjectDocument* {
    const auto found =
        std::lower_bound(projectDocuments.begin(), projectDocuments.end(), entryChartPath,
                         [](const PlaybackProjectDocument& document, std::string_view path) {
                             return document.path < path;
                         });
    return found != projectDocuments.end() && found->path == entryChartPath ? &*found : nullptr;
}

PlaybackSource::PlaybackSource() noexcept = default;
PlaybackSource::PlaybackSource(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {}
PlaybackSource::~PlaybackSource() = default;

PlaybackSource::PlaybackSource(PlaybackSource&& other) noexcept : state_(std::move(other.state_)) {}

auto PlaybackSource::operator=(PlaybackSource&& other) noexcept -> PlaybackSource& {
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

auto PlaybackSource::fromChartText(std::string chartJson) -> core::Result<PlaybackSource> {
    try {
        std::vector<PlaybackProjectDocument> documents;
        documents.push_back({std::string{standaloneChartPath}, std::move(chartJson)});
        auto validated = validateProjectDocuments(standaloneChartPath, std::move(documents));
        if (!validated) {
            return core::unexpected(std::move(validated.error()));
        }
        auto state = std::make_unique<State>();
        state->entryChartPath = standaloneChartPath;
        state->projectDocuments = std::move(*validated);
        return PlaybackSource{std::move(state)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("chart_text"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("chart_text", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("chart_text"));
    }
}

auto PlaybackSource::fromTypedProject(TypedPlaybackProject project,
                                      std::shared_ptr<content::IContentProvider> provider)
    -> core::Result<PlaybackSource> {
    try {
        if (project.chartJson.empty()) {
            return core::unexpected(core::Error{"playback.source.chart_empty",
                                                "Playback chart text must not be empty"});
        }
        TypedPlaybackProjectSource source;
        source.sourceId = std::move(project.sourceId);
        source.entryChartPath = standaloneChartPath;
        source.projectDocuments.push_back(
            {std::string{standaloneChartPath}, std::move(project.chartJson)});
        source.assets = std::move(project.assets);
        return fromTypedProjectSource(std::move(source), std::move(provider));
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("typed_project_legacy"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("typed_project_legacy", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("typed_project_legacy"));
    }
}

auto PlaybackSource::fromTypedProjectSource(TypedPlaybackProjectSource project,
                                            std::shared_ptr<content::IContentProvider> provider)
    -> core::Result<PlaybackSource> {
    try {
        if (!isPortableSourceId(project.sourceId)) {
            return core::unexpected(
                core::Error{"playback.source.id_invalid", "Playback source ID is not portable"});
        }
        auto documents =
            validateProjectDocuments(project.entryChartPath, std::move(project.projectDocuments));
        if (!documents) {
            return core::unexpected(std::move(documents.error()));
        }
        if (!provider) {
            return core::unexpected(
                core::Error{"playback.source.provider_missing",
                            "TypedPlaybackProject requires a content provider"});
        }

        std::optional<assets::AssetDatabase> database;
        if (!project.assets.empty()) {
            auto converted = convertTypedAssets(project.assets);
            if (!converted) {
                return core::unexpected(
                    translatePresentationAssetError(std::move(converted.error())));
            }
            database.emplace(std::move(*converted));
        }

        auto state = std::make_unique<State>();
        state->sourceId = std::move(project.sourceId);
        state->entryChartPath = std::move(project.entryChartPath);
        state->projectDocuments = std::move(*documents);
        state->database = std::move(database);
        state->provider = std::move(provider);
        return PlaybackSource{std::move(state)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("typed_project"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("typed_project", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("typed_project"));
    }
}

auto PlaybackSource::fromFilesystemProject(const std::filesystem::path& locator)
    -> core::Result<PlaybackSource> {
    try {
        auto loaded = project::ProjectLoader::load(locator);
        if (!loaded.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.source.project_invalid",
                                                         "Project loading produced errors",
                                                         loaded.diagnostics));
        }
        const auto& project = *loaded.project;
        assets::AssetDatabaseInput input;
        input.sourceMode = assets::AssetSourceMode::Filesystem;
        input.roots.reserve(project.assetRoots.size());
        for (const auto& root : project.assetRoots) {
            auto indexText = readTextFile(root.assetIndexFile, root.absolutePath,
                                          project::AssetIndexLimits{}.maxInputBytes,
                                          "playback.asset_index", "asset index");
            if (!indexText) {
                return core::unexpected(std::move(indexText.error()));
            }
            auto parsed = project::AssetIndexReader::read(*indexText);
            if (!parsed.hasValue()) {
                return core::unexpected(firstDiagnosticError("playback.source.asset_index_invalid",
                                                             "Asset index loading produced errors",
                                                             parsed.diagnostics));
            }
            assets::AssetRootIndex converted;
            converted.root = {.id = root.declaration.id, .path = root.absolutePath};
            converted.index.format = parsed.document->format;
            converted.index.version = parsed.document->version;
            converted.index.assets.reserve(parsed.document->assets.size());
            for (const auto& record : parsed.document->assets) {
                assets::AssetRecord asset{.id = assets::AssetId{record.id},
                                          .type = toAssetType(record.type),
                                          .source = record.source,
                                          .dependencies = {}};
                asset.dependencies.reserve(record.dependencies.size());
                for (const auto& dependency : record.dependencies) {
                    asset.dependencies.push_back(assets::AssetId{dependency});
                }
                converted.index.assets.push_back(std::move(asset));
            }
            input.roots.push_back(std::move(converted));
        }
        auto database = assets::AssetDatabase::create(input);
        if (!database) {
            return core::unexpected(translatePresentationAssetError(std::move(database.error())));
        }
        const auto* chartRoot = project.findAssetRoot(project.config.entry.chart.root);
        if (chartRoot == nullptr) {
            return core::unexpected(core::Error{"playback.source.chart_root_missing",
                                                "Project entry chart root is unavailable"});
        }
        auto entryPath =
            joinPortableProjectPath(chartRoot->declaration.path, project.config.entry.chart.path);
        if (!entryPath) {
            return core::unexpected(core::Error{"playback.source.entry_path_invalid",
                                                "Project entry Chart path is not portable"});
        }
        auto chartText = readTextFile(project.chartFile, chartRoot->absolutePath,
                                      maxProjectDocumentBytes, "playback.chart", "chart");
        if (!chartText) {
            return core::unexpected(std::move(chartText.error()));
        }
        std::vector<PlaybackProjectDocument> documents;
        if (auto loadedChart = chart::detail::loadV4IfPresent(*chartText)) {
            documents.reserve(loadedChart->animationTemplateImports.size() + 1U);
            for (const auto& import : loadedChart->animationTemplateImports) {
                auto cxtText =
                    readTextFile(project.projectRoot / import.source, project.projectRoot,
                                 maxProjectDocumentBytes, "playback.cxt", "animation template");
                if (cxtText) {
                    documents.push_back({import.source, std::move(*cxtText)});
                }
            }
        }
        documents.push_back({*entryPath, std::move(*chartText)});
        auto validated = validateProjectDocuments(*entryPath, std::move(documents));
        if (!validated) {
            return core::unexpected(std::move(validated.error()));
        }
        auto provider = database->defaultContentProvider();
        if (!provider) {
            return core::unexpected(core::Error{"playback.source.provider_missing",
                                                "Filesystem project has no content provider"});
        }
        auto state = std::make_unique<State>();
        state->sourceId = project.config.projectId;
        state->entryChartPath = std::move(*entryPath);
        state->projectDocuments = std::move(*validated);
        state->database.emplace(std::move(*database));
        state->provider = std::move(provider);
        return PlaybackSource{std::move(state)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("filesystem_project"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("filesystem_project", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("filesystem_project"));
    }
}

auto PlaybackSource::fromCxcFile(const std::filesystem::path& locator)
    -> core::Result<PlaybackSource> {
    try {
        auto loaded = cxc::CxcPackageLoader::loadFile(locator);
        if (!loaded.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.source.cxc_invalid",
                                                         "CXC package loading produced errors",
                                                         loaded.diagnostics));
        }
        auto converted = convertCxcPackage(std::move(*loaded.package));
        if (!converted) {
            return core::unexpected(std::move(converted.error()));
        }
        auto state = std::make_unique<State>();
        state->sourceId = std::move(converted->sourceId);
        state->entryChartPath = std::move(converted->entryChartPath);
        state->projectDocuments = std::move(converted->projectDocuments);
        state->database = std::move(converted->database);
        state->provider = std::move(converted->provider);
        state->cxcPackageIdentity = converted->packageIdentity;
        return PlaybackSource{std::move(state)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("cxc_file"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("cxc_file", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("cxc_file"));
    }
}

auto PlaybackSource::fromCxcMemory(std::vector<std::byte> packageBytes)
    -> core::Result<PlaybackSource> {
    try {
        auto loaded = cxc::CxcPackageLoader::loadMemory(std::move(packageBytes));
        if (!loaded.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.source.cxc_invalid",
                                                         "CXC package loading produced errors",
                                                         loaded.diagnostics));
        }
        auto converted = convertCxcPackage(std::move(*loaded.package));
        if (!converted) {
            return core::unexpected(std::move(converted.error()));
        }
        auto state = std::make_unique<State>();
        state->sourceId = std::move(converted->sourceId);
        state->entryChartPath = std::move(converted->entryChartPath);
        state->projectDocuments = std::move(converted->projectDocuments);
        state->database = std::move(converted->database);
        state->provider = std::move(converted->provider);
        state->cxcPackageIdentity = converted->packageIdentity;
        return PlaybackSource{std::move(state)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("cxc_memory"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("cxc_memory", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("cxc_memory"));
    }
}

} // namespace cuexis::playback
