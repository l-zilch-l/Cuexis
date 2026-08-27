//  AssetDatabase 实现 — 从 AssetDatabaseInput 构建不可变资产索引
//  build(): 校验全局 AssetId 唯一性、依赖存在性、循环依赖和来源 containment
//  v1 provides bounded render blobs; v2 adds bounded leaf Audio source blobs.

#include <cuexis/assets/asset_database.hpp>

#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis_internal/portable_path.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

namespace cuexis::assets {
namespace {

constexpr std::string_view assetIndexFormat = "cuexis.asset-index";
constexpr std::uint32_t assetIndexVersion1 = 1;
constexpr std::uint32_t assetIndexVersion2 = 2;

struct PendingRecord final {
    AssetRecord record;
    std::size_t rootIndex{};
    std::filesystem::path canonicalSource;
};

auto makeDiagnostics(std::size_t capacity) -> core::Diagnostics {
    return core::Diagnostics{capacity,
                             core::Diagnostic{core::DiagnosticSeverity::Error,
                                              "assets.database.diagnostic_limit",
                                              "AssetDatabase diagnostic limit was reached", "$"}};
}

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string fieldPath) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(fieldPath)});
}

auto rootFieldPath(std::size_t rootIndex) -> std::string {
    return "$.roots[" + std::to_string(rootIndex) + "]";
}

auto assetFieldPath(std::size_t rootIndex, std::size_t assetIndex) -> std::string {
    return rootFieldPath(rootIndex) + ".index.assets[" + std::to_string(assetIndex) + "]";
}

auto isPortableIdentifier(std::string_view value, std::size_t maxBytes) -> bool {
    if (value.empty() || value.size() > maxBytes ||
        !std::isalnum(static_cast<unsigned char>(value.front()))) {
        return false;
    }
    for (const unsigned char character : value.substr(1)) {
        if (!std::isalnum(character) && character != '.' && character != '_' && character != '-' &&
            character != '/') {
            return false;
        }
    }
    return true;
}

auto isKnownAssetType(AssetType type, std::uint32_t version) noexcept -> bool {
    if (type == AssetType::Mesh || type == AssetType::Material || type == AssetType::Texture) {
        return true;
    }
    return version == assetIndexVersion2 && type == AssetType::Audio;
}

auto isRootId(std::string_view value) -> bool {
    if (value.empty() || value.size() > 64 || value.front() < 'a' || value.front() > 'z') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
               character == '.' || character == '_' || character == '-';
    });
}

#if defined(_WIN32)
auto uppercase(std::string_view value) -> std::string {
    std::string result{value};
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return result;
}
#endif

auto isWindowsReservedSegment(std::string_view segment) -> bool {
    return core::detail::isWindowsReservedSegment(segment);
}

auto isPortableRelativePath(std::string_view value, std::size_t maxBytes) -> bool {
    if (value.empty() || value.size() > maxBytes || value.front() == '/' ||
        value.find('\\') != std::string_view::npos || value.find(':') != std::string_view::npos) {
        return false;
    }

    std::size_t start = 0;
    while (start <= value.size()) {
        const auto separator = value.find('/', start);
        const auto end = separator == std::string_view::npos ? value.size() : separator;
        const auto segment = value.substr(start, end - start);
        if (segment.empty() || segment == "." || segment == ".." || segment.back() == '.' ||
            segment.back() == ' ' || isWindowsReservedSegment(segment)) {
            return false;
        }
        for (const unsigned char character : segment) {
            if (character < 0x21U || character > 0x7EU) {
                return false;
            }
        }
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }
    return true;
}

auto pathComponentEqual(const std::filesystem::path& left, const std::filesystem::path& right)
    -> bool {
#if defined(_WIN32)
    auto leftText = uppercase(left.string());
    auto rightText = uppercase(right.string());
    return leftText == rightText;
#else
    return left == right;
#endif
}

auto isPathPrefix(const std::filesystem::path& prefix, const std::filesystem::path& path) -> bool {
    auto prefixPart = prefix.begin();
    auto pathPart = path.begin();
    while (prefixPart != prefix.end()) {
        if (pathPart == path.end() || !pathComponentEqual(*prefixPart, *pathPart)) {
            return false;
        }
        ++prefixPart;
        ++pathPart;
    }
    return true;
}

auto normalizedPhysicalKey(const std::filesystem::path& path) -> std::string {
    auto result = path.generic_string();
#if defined(_WIN32)
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
#endif
    return result;
}

auto dependencyCycleText(const std::vector<std::size_t>& stack, std::size_t repeated,
                         const std::vector<PendingRecord>& records) -> std::string {
    const auto first = std::find(stack.begin(), stack.end(), repeated);
    std::ostringstream stream;
    for (auto current = first; current != stack.end(); ++current) {
        if (current != first) {
            stream << " -> ";
        }
        stream << records[*current].record.id.value;
    }
    if (first != stack.end()) {
        stream << " -> " << records[repeated].record.id.value;
    }
    return stream.str();
}

} // namespace

struct AssetDatabase::Data final {
    struct StoredRecord final {
        AssetRecord record;
        std::size_t rootIndex{};
        std::filesystem::path canonicalSource;
    };

    std::vector<AssetRoot> roots;
    std::vector<std::filesystem::path> canonicalRoots;
    std::vector<StoredRecord> records;
    std::map<std::string, std::size_t, std::less<>> byId;
    std::shared_ptr<content::IContentProvider> defaultContentProvider;
};

std::string_view assetTypeName(AssetType type) noexcept {
    switch (type) {
    case AssetType::Mesh:
        return "mesh";
    case AssetType::Material:
        return "material";
    case AssetType::Texture:
        return "texture";
    case AssetType::Audio:
        return "audio";
    }
    return "unknown";
}

AssetDatabase::AssetDatabase(std::shared_ptr<const Data> data) noexcept : data_(std::move(data)) {}

auto AssetDatabase::build(const AssetDatabaseInput& input, const AssetDatabaseLimits& limits)
    -> AssetDatabaseBuildResult {
    auto diagnostics = makeDiagnostics(limits.maxDiagnostics);
    if (limits.maxRoots == 0 || limits.maxAssets == 0 || limits.maxDependenciesPerAsset == 0 ||
        limits.maxDependencyDepth == 0 || limits.maxAssetIdBytes == 0 ||
        limits.maxSourcePathBytes == 0 || limits.maxDiagnostics == 0) {
        addError(diagnostics, "assets.database.invalid_limits",
                 "AssetDatabase limits must all be non-zero", "$limits");
        return {std::nullopt, std::move(diagnostics)};
    }
    if (input.roots.empty()) {
        addError(diagnostics, "assets.database.roots_empty",
                 "AssetDatabase requires at least one asset root", "$.roots");
        return {std::nullopt, std::move(diagnostics)};
    }
    if (input.roots.size() > limits.maxRoots) {
        addError(diagnostics, "assets.database.root_limit",
                 "AssetDatabase asset root limit was exceeded", "$.roots");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    auto data = std::make_shared<Data>();
    data->roots.reserve(std::min(input.roots.size(), limits.maxRoots));
    data->canonicalRoots.reserve(std::min(input.roots.size(), limits.maxRoots));
    std::vector<bool> validRoots(input.roots.size(), false);
    std::set<std::string, std::less<>> rootIds;

    for (std::size_t rootIndex = 0; rootIndex < input.roots.size(); ++rootIndex) {
        const auto& rootInput = input.roots[rootIndex];
        const auto fieldPath = rootFieldPath(rootIndex);
        bool valid = true;
        if (!isRootId(rootInput.root.id)) {
            addError(diagnostics, "assets.database.root_id_invalid",
                     "Asset root ID is not a portable root identifier", fieldPath + ".root.id");
            valid = false;
        } else if (!rootIds.insert(rootInput.root.id).second) {
            addError(diagnostics, "assets.database.root_id_duplicate",
                     "Asset root ID is duplicated", fieldPath + ".root.id");
            valid = false;
        }
        if (rootInput.index.format != assetIndexFormat) {
            addError(diagnostics, "assets.database.index_format",
                     "Asset index format is unsupported", fieldPath + ".index.format");
            valid = false;
        }
        if (rootInput.index.version != assetIndexVersion1 &&
            rootInput.index.version != assetIndexVersion2) {
            addError(diagnostics, "assets.database.index_version",
                     "Asset index version is unsupported", fieldPath + ".index.version");
            valid = false;
        }

        std::filesystem::path canonicalRoot;
        if (input.sourceMode == AssetSourceMode::Filesystem) {
            std::error_code error;
            canonicalRoot = std::filesystem::weakly_canonical(rootInput.root.path, error);
            if (error || canonicalRoot.empty() ||
                !std::filesystem::is_directory(canonicalRoot, error) || error) {
                addError(diagnostics, "assets.database.root_unavailable",
                         "Asset root does not name an accessible directory",
                         fieldPath + ".root.path");
                valid = false;
            }
        }
        if (valid && input.sourceMode == AssetSourceMode::Filesystem) {
            for (std::size_t previous = 0; previous < data->canonicalRoots.size(); ++previous) {
                if (data->canonicalRoots[previous].empty()) {
                    continue;
                }
                if (isPathPrefix(data->canonicalRoots[previous], canonicalRoot) ||
                    isPathPrefix(canonicalRoot, data->canonicalRoots[previous])) {
                    addError(diagnostics, "assets.database.root_overlap",
                             "Asset roots must not be aliases or overlap",
                             fieldPath + ".root.path");
                    valid = false;
                    break;
                }
            }
        }
        if (valid) {
            validRoots[rootIndex] = true;
            data->roots.push_back(rootInput.root);
            data->canonicalRoots.push_back(canonicalRoot);
        } else {
            // Keep positional alignment for records while still refusing to
            // publish the partially valid database below.
            data->roots.push_back(rootInput.root);
            data->canonicalRoots.push_back({});
        }
    }

    std::size_t totalAssets = 0;
    for (const auto& root : input.roots) {
        if (root.index.assets.size() > limits.maxAssets - std::min(totalAssets, limits.maxAssets)) {
            totalAssets = limits.maxAssets + 1;
            break;
        }
        totalAssets += root.index.assets.size();
    }
    if (totalAssets > limits.maxAssets) {
        addError(diagnostics, "assets.database.asset_limit",
                 "AssetDatabase asset count limit was exceeded", "$.roots");
    }

    std::vector<PendingRecord> pending;
    pending.reserve(std::min(totalAssets, limits.maxAssets));
    std::map<std::string, std::size_t, std::less<>> byId;
    std::map<std::string, std::string, std::less<>> byResolvedSource;

    for (std::size_t rootIndex = 0; rootIndex < input.roots.size(); ++rootIndex) {
        const auto& rootInput = input.roots[rootIndex];
        for (std::size_t assetIndex = 0; assetIndex < rootInput.index.assets.size(); ++assetIndex) {
            if (pending.size() >= limits.maxAssets) {
                break;
            }
            auto record = rootInput.index.assets[assetIndex];
            const auto fieldPath = assetFieldPath(rootIndex, assetIndex);
            bool valid = validRoots[rootIndex];
            if (!isPortableIdentifier(record.id.value, limits.maxAssetIdBytes)) {
                addError(diagnostics, "assets.database.asset_id_invalid",
                         "AssetId is empty, too long, or not portable", fieldPath + ".id");
                valid = false;
            } else if (byId.contains(record.id.value)) {
                addError(diagnostics, "assets.database.asset_id_duplicate",
                         "AssetId is duplicated across the database", fieldPath + ".id");
                valid = false;
            }
            if (!isKnownAssetType(record.type, rootInput.index.version)) {
                addError(diagnostics, "assets.database.asset_type_invalid",
                         "Asset type is not supported by this database version",
                         fieldPath + ".type");
                valid = false;
            }
            if (!isPortableRelativePath(record.source, limits.maxSourcePathBytes)) {
                addError(diagnostics, "assets.database.source_path_invalid",
                         "Asset source is not a portable relative path", fieldPath + ".source");
                valid = false;
            }
            if (record.source == "cuexis.asset-index.json") {
                addError(diagnostics, "assets.database.source_is_index",
                         "An asset source cannot name the asset index itself",
                         fieldPath + ".source");
                valid = false;
            }
            if (record.dependencies.size() > limits.maxDependenciesPerAsset) {
                addError(diagnostics, "assets.database.dependency_limit",
                         "Asset dependency count limit was exceeded", fieldPath + ".dependencies");
                valid = false;
            }

            std::sort(record.dependencies.begin(), record.dependencies.end());
            if (std::adjacent_find(record.dependencies.begin(), record.dependencies.end()) !=
                record.dependencies.end()) {
                addError(diagnostics, "assets.database.dependency_duplicate",
                         "Asset dependency list contains a duplicate", fieldPath + ".dependencies");
                valid = false;
            }
            for (std::size_t dependency = 0; dependency < record.dependencies.size();
                 ++dependency) {
                if (!isPortableIdentifier(record.dependencies[dependency].value,
                                          limits.maxAssetIdBytes)) {
                    addError(diagnostics, "assets.database.dependency_id_invalid",
                             "Asset dependency ID is not portable",
                             fieldPath + ".dependencies[" + std::to_string(dependency) + "]");
                    valid = false;
                }
            }

            std::filesystem::path canonicalSource;
            if (valid && input.sourceMode == AssetSourceMode::Filesystem) {
                std::error_code error;
                const auto joined =
                    data->canonicalRoots[rootIndex] / std::filesystem::path{record.source};
                canonicalSource = std::filesystem::weakly_canonical(joined, error);
                if (error || !isPathPrefix(data->canonicalRoots[rootIndex], canonicalSource)) {
                    addError(diagnostics, "assets.database.source_outside_root",
                             "Asset source escapes its declared root", fieldPath + ".source");
                    valid = false;
                } else if (!std::filesystem::is_regular_file(canonicalSource, error) || error) {
                    addError(diagnostics, "assets.database.source_unavailable",
                             "Asset source does not name an accessible regular file",
                             fieldPath + ".source");
                    valid = false;
                }
            }
            if (valid) {
                const auto sourceKey = input.sourceMode == AssetSourceMode::Filesystem
                                           ? normalizedPhysicalKey(canonicalSource)
                                           : rootInput.root.id + "\n" + record.source;
                const auto existingSource = byResolvedSource.find(sourceKey);
                if (existingSource != byResolvedSource.end()) {
                    auto diagnostic = core::Diagnostic{
                        core::DiagnosticSeverity::Error, "assets.database.source_duplicate",
                        "Two AssetIds resolve to the same content source", fieldPath + ".source"};
                    diagnostic.withContext("otherAssetId", existingSource->second);
                    diagnostics.add(std::move(diagnostic));
                    valid = false;
                } else {
                    byResolvedSource.emplace(sourceKey, record.id.value);
                }
            }
            if (valid) {
                byId.emplace(record.id.value, pending.size());
                pending.push_back(
                    PendingRecord{std::move(record), rootIndex, std::move(canonicalSource)});
            }
        }
    }

    for (std::size_t recordIndex = 0; recordIndex < pending.size(); ++recordIndex) {
        const auto& record = pending[recordIndex].record;
        if (record.type == AssetType::Audio && !record.dependencies.empty()) {
            auto diagnostic = core::Diagnostic{
                core::DiagnosticSeverity::Error, "assets.database.audio_dependencies_not_empty",
                "Audio assets must be dependency leaves", "$.dependencies"};
            diagnostic.withContext("assetId", record.id.value);
            diagnostics.add(std::move(diagnostic));
        }
        for (const auto& dependency : record.dependencies) {
            const auto dependencyRecord = byId.find(dependency.value);
            if (dependencyRecord == byId.end()) {
                auto diagnostic = core::Diagnostic{
                    core::DiagnosticSeverity::Error, "assets.database.dependency_missing",
                    "Asset dependency does not exist in the database", "$.dependencies"};
                diagnostic.withContext("assetId", record.id.value)
                    .withContext("dependency", dependency.value);
                diagnostics.add(std::move(diagnostic));
            } else if (record.type != AssetType::Audio &&
                       pending[dependencyRecord->second].record.type == AssetType::Audio) {
                auto diagnostic = core::Diagnostic{
                    core::DiagnosticSeverity::Error, "assets.database.audio_dependency_forbidden",
                    "Non-audio assets must not depend on Audio assets", "$.dependencies"};
                diagnostic.withContext("assetId", record.id.value)
                    .withContext("dependency", dependency.value);
                diagnostics.add(std::move(diagnostic));
            }
        }
    }

    std::vector<unsigned char> colors(pending.size(), 0);
    std::vector<std::size_t> stack;
    const auto visit = [&](auto&& self, std::size_t recordIndex) -> void {
        if (colors[recordIndex] == 2 || diagnostics.limitReached()) {
            return;
        }
        colors[recordIndex] = 1;
        stack.push_back(recordIndex);
        if (stack.size() > limits.maxDependencyDepth) {
            auto diagnostic = core::Diagnostic{
                core::DiagnosticSeverity::Error, "assets.database.dependency_depth",
                "Asset dependency depth limit was exceeded", "$.dependencies"};
            diagnostic.withContext("assetId", pending[recordIndex].record.id.value);
            diagnostics.add(std::move(diagnostic));
        } else {
            for (const auto& dependency : pending[recordIndex].record.dependencies) {
                const auto found = byId.find(dependency.value);
                if (found == byId.end()) {
                    continue;
                }
                const auto dependencyIndex = found->second;
                if (colors[dependencyIndex] == 1) {
                    auto diagnostic = core::Diagnostic{
                        core::DiagnosticSeverity::Error, "assets.database.dependency_cycle",
                        "Asset dependency graph contains a cycle", "$.dependencies"};
                    diagnostic.withContext("cycle",
                                           dependencyCycleText(stack, dependencyIndex, pending));
                    diagnostics.add(std::move(diagnostic));
                    continue;
                }
                self(self, dependencyIndex);
            }
        }
        stack.pop_back();
        colors[recordIndex] = 2;
    };
    for (std::size_t index = 0; index < pending.size(); ++index) {
        if (colors[index] == 0) {
            visit(visit, index);
        }
    }

    if (!diagnostics.hasErrors() && input.sourceMode == AssetSourceMode::Filesystem) {
        std::vector<content::FilesystemContentRoot> roots;
        roots.reserve(data->roots.size());
        for (const auto& root : data->roots) {
            roots.push_back(content::FilesystemContentRoot{.id = root.id, .path = root.path});
        }
        auto provider = content::FilesystemContentProvider::create(std::move(roots));
        if (!provider) {
            addError(diagnostics, "assets.database.content_provider_failed",
                     "Filesystem content provider could not be created", "$.roots");
        } else {
            data->defaultContentProvider = std::move(*provider);
        }
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        return {std::nullopt, std::move(diagnostics)};
    }

    std::sort(pending.begin(), pending.end(),
              [](const PendingRecord& left, const PendingRecord& right) {
                  return left.record.id < right.record.id;
              });
    data->records.reserve(pending.size());
    for (auto& record : pending) {
        const auto index = data->records.size();
        data->byId.emplace(record.record.id.value, index);
        data->records.push_back(Data::StoredRecord{std::move(record.record), record.rootIndex,
                                                   std::move(record.canonicalSource)});
    }
    return {AssetDatabase{std::move(data)}, std::move(diagnostics)};
}

auto AssetDatabase::create(const AssetDatabaseInput& input, const AssetDatabaseLimits& limits)
    -> core::Result<AssetDatabase> {
    auto built = build(input, limits);
    if (built.hasValue()) {
        return std::move(*built.database);
    }
    if (!built.diagnostics.empty()) {
        const auto& first = built.diagnostics.items().front();
        auto error =
            core::Error{std::string{first.code()}, std::string{first.message()}}.withContext(
                "fieldPath", std::string{first.fieldPath()});
        for (const auto& context : first.context()) {
            error.withContext(context.key, context.value);
        }
        return core::unexpected(std::move(error));
    }
    return core::unexpected(
        core::Error{"assets.database.build_failed", "AssetDatabase could not be built"});
}

bool AssetDatabase::empty() const noexcept {
    return !data_ || data_->records.empty();
}

std::size_t AssetDatabase::size() const noexcept {
    return data_ ? data_->records.size() : 0;
}

std::size_t AssetDatabase::rootCount() const noexcept {
    return data_ ? data_->roots.size() : 0;
}

std::vector<AssetId> AssetDatabase::ids() const {
    std::vector<AssetId> result;
    if (!data_) {
        return result;
    }
    result.reserve(data_->records.size());
    for (const auto& record : data_->records) {
        result.push_back(record.record.id);
    }
    return result;
}

const AssetRecord* AssetDatabase::find(const AssetId& id) const noexcept {
    return find(id.value);
}

const AssetRecord* AssetDatabase::find(std::string_view id) const noexcept {
    if (!data_) {
        return nullptr;
    }
    const auto found = data_->byId.find(id);
    return found == data_->byId.end() ? nullptr : &data_->records[found->second].record;
}

std::optional<AssetType> AssetDatabase::typeOf(const AssetId& id) const noexcept {
    const auto* record = find(id);
    return record == nullptr ? std::nullopt : std::optional{record->type};
}

std::string_view AssetDatabase::rootIdOf(const AssetId& id) const noexcept {
    if (!data_) {
        return {};
    }
    const auto found = data_->byId.find(id.value);
    if (found == data_->byId.end()) {
        return {};
    }
    return data_->roots[data_->records[found->second].rootIndex].id;
}

std::filesystem::path AssetDatabase::sourcePath(const AssetId& id) const {
    if (!data_) {
        return {};
    }
    const auto found = data_->byId.find(id.value);
    return found == data_->byId.end() ? std::filesystem::path{}
                                      : data_->records[found->second].canonicalSource;
}

std::vector<AssetId> AssetDatabase::dependenciesOf(const AssetId& id) const {
    const auto* record = find(id);
    return record == nullptr ? std::vector<AssetId>{} : record->dependencies;
}

auto AssetDatabase::readBlob(const AssetId& id, const AssetBlobLimits& limits) const
    -> core::Result<AssetBlob> {
    if (limits.maxBytes == 0) {
        return core::unexpected(
            core::Error{"assets.blob.invalid_limit", "Asset blob byte limit must be non-zero"});
    }
    if (!data_) {
        return core::unexpected(
            core::Error{"assets.database.empty", "AssetDatabase has not been initialized"});
    }
    const auto found = data_->byId.find(id.value);
    if (found == data_->byId.end()) {
        return core::unexpected(
            core::Error{"assets.asset.not_found", "AssetId does not exist in AssetDatabase"}
                .withContext("assetId", id.value));
    }
    const auto& stored = data_->records[found->second];
    if (!data_->defaultContentProvider) {
        return core::unexpected(core::Error{"assets.blob.provider_missing",
                                            "AssetDatabase has no default content provider"}
                                    .withContext("assetId", id.value));
    }
    auto contents =
        data_->defaultContentProvider->readBlob({.rootId = data_->roots[stored.rootIndex].id,
                                                 .source = stored.record.source,
                                                 .maxBytes = limits.maxBytes});
    if (!contents) {
        const auto providerCode = std::string{contents.error().code()};
        auto code = std::string{"assets.blob.source_unavailable"};
        if (providerCode == "content.filesystem.too_large" ||
            providerCode == "content.provider.too_large") {
            code = "assets.blob.too_large";
        } else if (providerCode == "content.filesystem.read_failed" ||
                   providerCode == "content.filesystem.changed_during_read") {
            code = "assets.blob.read_failed";
        }
        auto error = core::Error{std::move(code), "Asset content could not be read"}.withCause(
            std::move(contents.error()));
        error.withContext("assetId", id.value)
            .withContext("rootId", data_->roots[stored.rootIndex].id)
            .withContext("source", stored.record.source);
        return core::unexpected(std::move(error));
    }

    AssetBlob blob;
    blob.rootId = data_->roots[stored.rootIndex].id;
    blob.source = stored.record.source;
    blob.providerRevision = contents->revision;
    blob.bytes = std::move(contents->bytes);
    return blob;
}

auto AssetDatabase::defaultContentProvider() const noexcept
    -> std::shared_ptr<content::IContentProvider> {
    return data_ ? data_->defaultContentProvider : nullptr;
}

const std::vector<AssetRoot>& AssetDatabase::roots() const noexcept {
    static const std::vector<AssetRoot> emptyRoots;
    return data_ ? data_->roots : emptyRoots;
}

} // namespace cuexis::assets
