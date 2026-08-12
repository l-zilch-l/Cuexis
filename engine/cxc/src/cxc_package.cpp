#include <cuexis/cxc/cxc_package.hpp>

#include <cuexis/cxc/cxc_manifest_loader.hpp>

#include <cuexis/chart/animation_template_loader.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/project/project_loader.hpp>

#include "cxc_hash_internal.hpp"
#include "cxc_path_internal.hpp"
#include "zip32_envelope_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::cxc {
namespace detail {

struct CxcPackageData final {
    std::vector<std::byte> archiveBytes;
    CxcPackageIdentity identity;
    CxcManifestDocument manifest;
    project::ProjectConfig project;
    std::vector<CxcAssetIndex> assetIndexes;
    std::vector<chart::ProjectDocument> projectDocuments;
    std::vector<CxcArchiveEntry> entries;
    std::vector<Zip32Entry> zipEntries;
    std::map<std::string, std::size_t, std::less<>> entryIndexes;
    std::map<std::string, std::string, std::less<>> contentPaths;
};

} // namespace detail

namespace {

struct AssetDeclaration final {
    project::AssetType type{project::AssetType::Mesh};
    std::string rootId;
    std::string source;
    std::vector<std::string> dependencies;
};

[[nodiscard]] auto makeDiagnostics(const CxcPackageLimits& limits) -> core::Diagnostics {
    return core::Diagnostics{limits.maxDiagnostics,
                             core::Diagnostic{core::DiagnosticSeverity::Error,
                                              "cxc.budget.exceeded",
                                              "CXC diagnostics reached the configured limit", "$"}};
}

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string fieldPath, std::string_view path = {}) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                       std::move(message), std::move(fieldPath)};
    if (!path.empty()) {
        diagnostic.withContext("path", std::string{path});
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

void addError(core::Diagnostics& diagnostics, const core::Error& error, std::string fieldPath) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(fieldPath)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

void appendDiagnostics(core::Diagnostics& diagnostics, core::Diagnostics source) {
    static_cast<void>(diagnostics.append(std::move(source)));
}

[[nodiscard]] auto textFromBytes(std::span<const std::byte> bytes) -> std::string {
    std::string result;
    result.resize(bytes.size());
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index] = static_cast<char>(std::to_integer<unsigned char>(bytes[index]));
    }
    return result;
}

[[nodiscard]] auto contentKey(std::string_view rootId, std::string_view source) -> std::string {
    std::string result;
    result.reserve(rootId.size() + source.size() + 1U);
    result.append(rootId);
    result.push_back('\0');
    result.append(source);
    return result;
}

[[nodiscard]] auto isPathPrefix(std::string_view prefix, std::string_view path) noexcept -> bool {
    return path == prefix ||
           (path.size() > prefix.size() && path.starts_with(prefix) && path[prefix.size()] == '/');
}

[[nodiscard]] auto isV4Chart(std::string_view text, const chart::ChartLimits& limits) -> bool {
    auto parsed =
        json::parse(text, {limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes});
    if (!parsed) {
        return false;
    }
    const auto* version = parsed->find("version");
    if (version == nullptr) {
        return false;
    }
    if (const auto* signedValue = version->signedInteger()) {
        return *signedValue == 4;
    }
    if (const auto* unsignedValue = version->unsignedInteger()) {
        return *unsignedValue == 4;
    }
    return false;
}

[[nodiscard]] auto findZipEntry(const detail::CxcPackageData& data, std::string_view path) noexcept
    -> const detail::Zip32Entry* {
    const auto found = data.entryIndexes.find(path);
    return found == data.entryIndexes.end() ? nullptr : &data.zipEntries[found->second];
}

[[nodiscard]] auto archiveEntryBytes(const detail::CxcPackageData& data,
                                     const detail::Zip32Entry& entry) noexcept
    -> std::span<const std::byte> {
    return std::span<const std::byte>{data.archiveBytes}.subspan(
        entry.dataOffset, static_cast<std::size_t>(entry.metadata.byteCount));
}

[[nodiscard]] auto entryText(const detail::CxcPackageData& data, std::string_view path)
    -> std::optional<std::string> {
    const auto* entry = findZipEntry(data, path);
    return entry == nullptr
               ? std::nullopt
               : std::optional<std::string>{textFromBytes(archiveEntryBytes(data, *entry))};
}

void validateAssetReference(core::Diagnostics& diagnostics,
                            const std::map<std::string, AssetDeclaration, std::less<>>& assets,
                            std::string_view id, project::AssetType expectedType,
                            std::string_view sourcePath, std::string_view use) {
    const auto found = assets.find(id);
    if (found == assets.end()) {
        auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.project.invalid",
                                           "Chart resource is missing from the Asset Index",
                                           "$/project/entry/chart"};
        diagnostic.withContext("path", std::string{sourcePath})
            .withContext("assetId", std::string{id})
            .withContext("use", std::string{use});
        static_cast<void>(diagnostics.add(std::move(diagnostic)));
        return;
    }
    if (found->second.type != expectedType) {
        auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.project.invalid",
                                           "Chart resource has an incompatible Asset Index type",
                                           "$/project/entry/chart"};
        diagnostic.withContext("path", std::string{sourcePath})
            .withContext("assetId", std::string{id})
            .withContext("use", std::string{use})
            .withContext("actual_type", std::string{project::assetTypeName(found->second.type)})
            .withContext("expected_type", std::string{project::assetTypeName(expectedType)});
        static_cast<void>(diagnostics.add(std::move(diagnostic)));
    }
}

void validateLegacyResources(core::Diagnostics& diagnostics,
                             const std::map<std::string, AssetDeclaration, std::less<>>& assets,
                             const chart::ChartDocument& document, std::string_view sourcePath) {
    if (document.audio) {
        validateAssetReference(diagnostics, assets, document.audio->mainMusic.value,
                               project::AssetType::Audio, sourcePath, "audio.mainMusic");
    }
    const auto validateComponents = [&](const chart::ObjectComponents& components) {
        if (components.renderable) {
            validateAssetReference(diagnostics, assets, components.renderable->mesh.value,
                                   project::AssetType::Mesh, sourcePath, "renderable.mesh");
            validateAssetReference(diagnostics, assets, components.renderable->material.value,
                                   project::AssetType::Material, sourcePath, "renderable.material");
        }
    };
    for (const auto& item : document.templates) {
        validateComponents(item.prototype);
    }
    for (const auto& item : document.objects) {
        validateComponents(item.components);
    }
    for (const auto& behavior : document.behaviors) {
        for (const auto& step : behavior.stepEvents) {
            if (const auto* asset = std::get_if<chart::AssetId>(&step.value)) {
                validateAssetReference(diagnostics, assets, asset->value,
                                       project::AssetType::Material, sourcePath,
                                       "behavior.render.material");
            }
        }
    }
}

void validateClipResources(core::Diagnostics& diagnostics,
                           const std::map<std::string, AssetDeclaration, std::less<>>& assets,
                           const chart::AnimationClip& clip, std::string_view sourcePath) {
    for (const auto& track : clip.stepTracks) {
        for (const auto& step : track.steps) {
            if (const auto* asset = std::get_if<chart::AssetId>(&step.value)) {
                validateAssetReference(diagnostics, assets, asset->value,
                                       project::AssetType::Material, sourcePath,
                                       "animation.render.material");
            }
        }
    }
}

[[nodiscard]] auto
validateDependencyGraph(core::Diagnostics& diagnostics,
                        const std::map<std::string, AssetDeclaration, std::less<>>& assets,
                        std::size_t maxDependencyDepth) -> bool {
    enum class VisitState {
        Visiting,
        Visited,
    };

    struct VisitFrame final {
        std::string_view id;
        const AssetDeclaration* declaration{};
        std::size_t nextDependency{};
        std::size_t maxChildDepth{};
    };

    std::map<std::string, VisitState, std::less<>> states;
    std::map<std::string, std::size_t, std::less<>> depths;
    bool valid = true;

    for (const auto& [id, declaration] : assets) {
        if (declaration.type == project::AssetType::Audio && !declaration.dependencies.empty()) {
            auto diagnostic =
                core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.project.invalid",
                                 "Audio assets must be dependency leaves", "$/project/assets"};
            diagnostic.withContext("assetId", id);
            static_cast<void>(diagnostics.add(std::move(diagnostic)));
            valid = false;
        }
        for (const auto& dependency : declaration.dependencies) {
            const auto found = assets.find(dependency);
            if (found == assets.end()) {
                auto diagnostic = core::Diagnostic{
                    core::DiagnosticSeverity::Error, "cxc.project.invalid",
                    "Asset dependency is missing from the declared Asset Index set",
                    "$/project/assets"};
                diagnostic.withContext("assetId", id).withContext("dependency", dependency);
                static_cast<void>(diagnostics.add(std::move(diagnostic)));
                valid = false;
            } else if (declaration.type != project::AssetType::Audio &&
                       found->second.type == project::AssetType::Audio) {
                auto diagnostic = core::Diagnostic{
                    core::DiagnosticSeverity::Error, "cxc.project.invalid",
                    "Non-audio assets must not depend on Audio assets", "$/project/assets"};
                diagnostic.withContext("assetId", id).withContext("dependency", dependency);
                static_cast<void>(diagnostics.add(std::move(diagnostic)));
                valid = false;
            }
        }
    }

    for (const auto& [rootId, rootDeclaration] : assets) {
        if (states.contains(rootId)) {
            continue;
        }

        std::vector<VisitFrame> stack;
        stack.push_back(VisitFrame{rootId, &rootDeclaration, 0, 0});
        states.emplace(rootId, VisitState::Visiting);
        while (!stack.empty()) {
            auto& frame = stack.back();
            if (frame.nextDependency >= frame.declaration->dependencies.size()) {
                const auto depth = frame.maxChildDepth + 1U;
                if (depth > maxDependencyDepth) {
                    auto diagnostic = core::Diagnostic{
                        core::DiagnosticSeverity::Error, "cxc.budget.exceeded",
                        "Asset dependency depth exceeds the configured limit", "$/project/assets"};
                    diagnostic.withContext("assetId", std::string{frame.id})
                        .withContext("limit", std::to_string(maxDependencyDepth));
                    static_cast<void>(diagnostics.add(std::move(diagnostic)));
                    valid = false;
                }
                depths.insert_or_assign(std::string{frame.id}, depth);
                states.insert_or_assign(std::string{frame.id}, VisitState::Visited);
                stack.pop_back();
                if (!stack.empty()) {
                    stack.back().maxChildDepth = std::max(stack.back().maxChildDepth, depth);
                }
                continue;
            }

            const auto& dependency = frame.declaration->dependencies[frame.nextDependency++];
            const auto declaration = assets.find(dependency);
            if (declaration == assets.end()) {
                continue;
            }
            const auto existing = states.find(dependency);
            if (existing != states.end()) {
                if (existing->second == VisitState::Visiting) {
                    auto diagnostic = core::Diagnostic{
                        core::DiagnosticSeverity::Error, "cxc.project.invalid",
                        "Asset dependency graph contains a cycle", "$/project/assets"};
                    diagnostic.withContext("assetId", std::string{frame.id})
                        .withContext("dependency", dependency);
                    static_cast<void>(diagnostics.add(std::move(diagnostic)));
                    valid = false;
                } else if (const auto depth = depths.find(dependency); depth != depths.end()) {
                    frame.maxChildDepth = std::max(frame.maxChildDepth, depth->second);
                }
                continue;
            }
            states.emplace(dependency, VisitState::Visiting);
            stack.push_back(VisitFrame{declaration->first, &declaration->second, 0, 0});
        }
    }
    return valid;
}

[[nodiscard]] auto buildPackage(std::vector<std::byte> archiveBytes, const CxcPackageLimits& limits)
    -> CxcPackageResult {
    auto envelope = detail::validateZip32Envelope(archiveBytes, limits);
    if (!envelope.hasValue()) {
        return {std::nullopt, std::move(envelope.diagnostics)};
    }
    if (auto verified = detail::verifyWithMinizip(archiveBytes, envelope.entries); !verified) {
        addError(envelope.diagnostics, verified.error(), "$/archive");
        envelope.diagnostics.sortDeterministically();
        return {std::nullopt, std::move(envelope.diagnostics)};
    }

    auto diagnostics = std::move(envelope.diagnostics);
    auto data = std::make_shared<detail::CxcPackageData>();
    data->archiveBytes = std::move(archiveBytes);
    data->identity.sha256 = detail::sha256(
        std::span<const std::byte>{data->archiveBytes.data(), data->archiveBytes.size()});
    data->zipEntries = std::move(envelope.entries);
    data->entries.reserve(data->zipEntries.size());
    for (std::size_t index = 0; index < data->zipEntries.size(); ++index) {
        const auto& entry = data->zipEntries[index];
        data->entryIndexes.emplace(entry.metadata.path, index);
        data->entries.push_back(entry.metadata);
    }

    const auto* manifestEntry = findZipEntry(*data, "cuexis.cxc.json");
    if (manifestEntry == nullptr) {
        addError(diagnostics, "cxc.entry.missing", "CXC archive has no manifest", "$/archive",
                 "cuexis.cxc.json");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    if (manifestEntry->metadata.byteCount > limits.maxManifestBytes) {
        addError(diagnostics, "cxc.budget.exceeded", "CXC manifest exceeds the byte limit",
                 "$/archive", "cuexis.cxc.json");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    auto manifestLimits = limits.manifest;
    manifestLimits.maxManifestBytes =
        std::min(manifestLimits.maxManifestBytes, limits.maxManifestBytes);
    manifestLimits.maxEntries =
        std::min(manifestLimits.maxEntries, limits.maxEntries > 0 ? limits.maxEntries - 1U : 0U);
    manifestLimits.maxEntryBytes =
        std::min<std::uint64_t>(manifestLimits.maxEntryBytes, limits.maxEntryBytes);
    manifestLimits.maxListedBytes =
        std::min<std::uint64_t>(manifestLimits.maxListedBytes, limits.maxPackageBytes);
    manifestLimits.maxDiagnostics = std::min(manifestLimits.maxDiagnostics, limits.maxDiagnostics);
    auto manifestResult = CxcManifestLoader::load(
        textFromBytes(archiveEntryBytes(*data, *manifestEntry)), manifestLimits);
    appendDiagnostics(diagnostics, std::move(manifestResult.diagnostics));
    if (!manifestResult.document || diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    data->manifest = std::move(*manifestResult.document);

    std::set<std::string, std::less<>> declaredPaths;
    for (const auto& declared : data->manifest.entries) {
        declaredPaths.insert(declared.path);
        const auto* archived = findZipEntry(*data, declared.path);
        if (archived == nullptr) {
            addError(diagnostics, "cxc.entry.missing", "CXC manifest entry is missing",
                     declared.fieldPath, declared.path);
            continue;
        }
        if (archived->metadata.byteCount != declared.byteCount) {
            addError(diagnostics, "cxc.entry.size_mismatch",
                     "CXC entry size does not match the manifest", declared.fieldPath,
                     declared.path);
        }
        if (archived->metadata.sha256 != declared.sha256) {
            addError(diagnostics, "cxc.entry.hash_mismatch",
                     "CXC entry SHA-256 does not match the manifest", declared.fieldPath,
                     declared.path);
        }
    }
    for (const auto& archived : data->zipEntries) {
        if (archived.metadata.path != "cuexis.cxc.json" &&
            !declaredPaths.contains(archived.metadata.path)) {
            addError(diagnostics, "cxc.entry.unlisted",
                     "CXC archive entry is not listed by the manifest", "$/archive",
                     archived.metadata.path);
        }
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    const auto projectText = entryText(*data, data->manifest.projectPath);
    if (!projectText) {
        addError(diagnostics, "cxc.entry.missing", "CXC project document is missing", "$/project",
                 data->manifest.projectPath);
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    auto projectLimits = limits.project;
    projectLimits.maxInputBytes = std::min(projectLimits.maxInputBytes, limits.maxEntryBytes);
    projectLimits.maxPortablePathBytes =
        std::min(projectLimits.maxPortablePathBytes, limits.maxPathBytes);
    projectLimits.maxDiagnostics = std::min(projectLimits.maxDiagnostics, limits.maxDiagnostics);
    auto projectResult = project::ProjectConfigReader::read(*projectText, projectLimits);
    const auto projectInvalid = !projectResult.hasValue();
    appendDiagnostics(diagnostics, std::move(projectResult.diagnostics));
    if (projectInvalid || !projectResult.config) {
        addError(diagnostics, "cxc.project.invalid", "CXC ProjectConfig is invalid", "$/project",
                 data->manifest.projectPath);
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    data->project = std::move(*projectResult.config);

    std::set<std::string, std::less<>> reachable{data->manifest.projectPath};
    std::vector<std::string> foldedRootPaths;
    foldedRootPaths.reserve(data->project.assetRoots.size());
    for (std::size_t left = 0; left < data->project.assetRoots.size(); ++left) {
        const auto& root = data->project.assetRoots[left];
        const auto foldedRoot = detail::foldAscii(root.path);
        if (!detail::isPortablePath(root.path, limits.maxPathBytes, limits.maxPathDepth) ||
            std::ranges::find(foldedRootPaths, foldedRoot) != foldedRootPaths.end()) {
            addError(diagnostics, "cxc.project.invalid",
                     "CXC asset root path is invalid or conflicts by case",
                     "$/project/assetRoots/" + std::to_string(left), root.path);
        }
        for (std::size_t right = 0; right < left; ++right) {
            const auto& other = foldedRootPaths[right];
            if (isPathPrefix(foldedRoot, other) || isPathPrefix(other, foldedRoot)) {
                addError(diagnostics, "cxc.project.invalid",
                         "CXC asset root paths must not overlap",
                         "$/project/assetRoots/" + std::to_string(left), root.path);
            }
        }
        foldedRootPaths.push_back(foldedRoot);
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    std::map<std::string, AssetDeclaration, std::less<>> assets;
    std::map<std::string, std::string, std::less<>> contentOwners;
    std::size_t totalAssets = 0;
    auto assetIndexLimits = limits.assetIndex;
    assetIndexLimits.maxInputBytes = std::min(assetIndexLimits.maxInputBytes, limits.maxEntryBytes);
    assetIndexLimits.maxPortablePathBytes =
        std::min(assetIndexLimits.maxPortablePathBytes, limits.maxPathBytes);
    assetIndexLimits.maxDiagnostics =
        std::min(assetIndexLimits.maxDiagnostics, limits.maxDiagnostics);
    for (const auto& root : data->project.assetRoots) {
        const auto indexPath = detail::joinPortablePath(root.path, project::assetIndexFileName,
                                                        limits.maxPathBytes, limits.maxPathDepth);
        if (!indexPath || !findZipEntry(*data, *indexPath)) {
            addError(diagnostics, "cxc.entry.missing", "CXC Asset Index is missing",
                     "$/project/assetRoots", indexPath ? *indexPath : root.path);
            continue;
        }
        reachable.insert(*indexPath);
        const auto indexText = entryText(*data, *indexPath);
        auto indexResult = project::AssetIndexReader::read(*indexText, assetIndexLimits);
        const auto indexInvalid = !indexResult.hasValue();
        appendDiagnostics(diagnostics, std::move(indexResult.diagnostics));
        if (indexInvalid || !indexResult.document) {
            addError(diagnostics, "cxc.project.invalid", "CXC Asset Index is invalid",
                     "$/project/assetRoots", *indexPath);
            continue;
        }
        auto indexDocument = std::move(*indexResult.document);
        if (totalAssets > limits.assetIndex.maxAssets ||
            indexDocument.assets.size() > limits.assetIndex.maxAssets - totalAssets) {
            addError(diagnostics, "cxc.budget.exceeded",
                     "CXC aggregate Asset record count exceeds the configured limit",
                     "$/project/assets", *indexPath);
            continue;
        }
        totalAssets += indexDocument.assets.size();
        for (const auto& record : indexDocument.assets) {
            if (detail::foldAscii(record.source) ==
                detail::foldAscii(project::assetIndexFileName)) {
                addError(diagnostics, "cxc.project.invalid",
                         "CXC asset source must not alias the Asset Index document",
                         "$/project/assets", record.source);
                continue;
            }
            const auto sourcePath = detail::joinPortablePath(
                root.path, record.source, limits.maxPathBytes, limits.maxPathDepth);
            if (!sourcePath || findZipEntry(*data, *sourcePath) == nullptr) {
                addError(diagnostics, "cxc.entry.missing", "CXC asset source is missing",
                         "$/project/assets", sourcePath ? *sourcePath : record.source);
                continue;
            }
            reachable.insert(*sourcePath);
            const auto key = contentKey(root.id, record.source);
            if (!data->contentPaths.emplace(key, *sourcePath).second ||
                !contentOwners.emplace(*sourcePath, record.id).second) {
                addError(diagnostics, "cxc.project.invalid",
                         "CXC Asset Index contains a duplicate content source", "$/project/assets",
                         *sourcePath);
            }
            if (!assets
                     .emplace(record.id, AssetDeclaration{record.type, root.id, record.source,
                                                          record.dependencies})
                     .second) {
                addError(diagnostics, "cxc.project.invalid",
                         "CXC Asset ID is duplicated across Asset Index files", "$/project/assets",
                         record.id);
            }
        }
        data->assetIndexes.push_back(CxcAssetIndex{root.id, std::move(indexDocument)});
    }
    static_cast<void>(validateDependencyGraph(diagnostics, assets, limits.maxDependencyDepth));
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    const auto root = std::ranges::find(data->project.assetRoots, data->project.entry.chart.root,
                                        &project::AssetRoot::id);
    std::optional<std::string> chartPath;
    if (root != data->project.assetRoots.end()) {
        chartPath = detail::joinPortablePath(root->path, data->project.entry.chart.path,
                                             limits.maxPathBytes, limits.maxPathDepth);
    }
    if (!chartPath || findZipEntry(*data, *chartPath) == nullptr) {
        addError(diagnostics, "cxc.entry.missing", "CXC entry Chart is missing",
                 "$/project/entry/chart", chartPath ? *chartPath : data->project.entry.chart.path);
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    if (contentOwners.contains(*chartPath)) {
        addError(diagnostics, "cxc.project.invalid",
                 "CXC project documents must not also be Asset Index sources",
                 "$/project/entry/chart", *chartPath);
    }
    reachable.insert(*chartPath);
    const auto chartText = *entryText(*data, *chartPath);
    data->projectDocuments.push_back(chart::ProjectDocument{*chartPath, chartText});

    auto chartLimits = limits.chart;
    chartLimits.maxInputBytes = std::min(chartLimits.maxInputBytes, limits.maxEntryBytes);
    chartLimits.maxDiagnostics = std::min(chartLimits.maxDiagnostics, limits.maxDiagnostics);
    if (isV4Chart(chartText, chartLimits)) {
        auto chartResult = chart::ChartV4Loader::load(chartText, chartLimits);
        const auto chartInvalid = !chartResult.hasValue();
        appendDiagnostics(diagnostics, std::move(chartResult.diagnostics));
        if (!chartInvalid && chartResult.document) {
            validateLegacyResources(diagnostics, assets, chartResult.document->legacyProjection,
                                    *chartPath);
            for (const auto& clip : chartResult.document->animationClips) {
                validateClipResources(diagnostics, assets, clip, *chartPath);
            }
            std::set<std::string, std::less<>> documentPaths;
            for (const auto& import : chartResult.document->animationTemplateImports) {
                if (!documentPaths.emplace(import.source).second) {
                    addError(diagnostics, "cxc.project.invalid",
                             "CXC Chart imports the same project document more than once",
                             import.fieldPath, import.source);
                    continue;
                }
                const auto cxtText = entryText(*data, import.source);
                if (!cxtText) {
                    addError(diagnostics, "cxc.entry.missing", "CXC CXT import is missing",
                             import.fieldPath, import.source);
                    continue;
                }
                if (contentOwners.contains(import.source)) {
                    addError(diagnostics, "cxc.project.invalid",
                             "CXC project documents must not also be Asset Index sources",
                             import.fieldPath, import.source);
                    continue;
                }
                reachable.insert(import.source);
                auto templateResult = chart::AnimationTemplateLoader::load(*cxtText, chartLimits);
                const auto templateInvalid = !templateResult.hasValue();
                appendDiagnostics(diagnostics, std::move(templateResult.diagnostics));
                if (templateInvalid || !templateResult.document) {
                    addError(diagnostics, "cxc.project.invalid", "CXC CXT import is invalid",
                             import.fieldPath, import.source);
                    continue;
                }
                if (templateResult.document->templateId != import.id) {
                    auto diagnostic = core::Diagnostic{
                        core::DiagnosticSeverity::Error, "cxt.template.id_mismatch",
                        "CXT templateId does not match the Chart import ID", import.fieldPath};
                    diagnostic.withContext("source", import.source)
                        .withContext("import_id", import.id)
                        .withContext("template_id", templateResult.document->templateId);
                    static_cast<void>(diagnostics.add(std::move(diagnostic)));
                    continue;
                }
                validateClipResources(diagnostics, assets, templateResult.document->clip,
                                      import.source);
                data->projectDocuments.push_back(chart::ProjectDocument{import.source, *cxtText});
            }
        } else {
            addError(diagnostics, "cxc.project.invalid", "CXC entry Chart is invalid",
                     "$/project/entry/chart", *chartPath);
        }
    } else {
        auto chartResult = chart::ChartLoader::load(chartText, chartLimits);
        const auto chartInvalid = !chartResult.hasValue();
        appendDiagnostics(diagnostics, std::move(chartResult.diagnostics));
        if (!chartInvalid && chartResult.document) {
            validateLegacyResources(diagnostics, assets, *chartResult.document, *chartPath);
        } else {
            addError(diagnostics, "cxc.project.invalid", "CXC entry Chart is invalid",
                     "$/project/entry/chart", *chartPath);
        }
    }

    for (const auto& declared : data->manifest.entries) {
        if (!reachable.contains(declared.path)) {
            addError(diagnostics, "cxc.entry.unlisted",
                     "CXC entry is outside the project-declared closure", declared.fieldPath,
                     declared.path);
        }
    }
    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        return {std::nullopt, std::move(diagnostics)};
    }
    return {CxcPackage{std::move(data)}, std::move(diagnostics)};
}

[[nodiscard]] auto revisionFromSha256(std::string_view value) noexcept -> std::uint64_t {
    std::uint64_t result = 0;
    for (std::size_t index = 0; index < 16 && index < value.size(); ++index) {
        const char character = value[index];
        const auto digit = character >= '0' && character <= '9'
                               ? static_cast<std::uint64_t>(character - '0')
                               : static_cast<std::uint64_t>(character - 'a' + 10);
        result = (result << 4U) | digit;
    }
    return result == 0 ? 1 : result;
}

[[nodiscard]] auto internalFailure(const CxcPackageLimits& limits, std::string_view operation,
                                   const std::exception* exception = nullptr) -> CxcPackageResult {
    auto diagnostics = makeDiagnostics(limits);
    auto diagnostic =
        core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.internal.failure",
                         std::string{"CXC "} + std::string{operation} + " failed", "$"};
    if (exception != nullptr) {
        diagnostic.withContext("exception", exception->what());
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
    return {std::nullopt, std::move(diagnostics)};
}

} // namespace

auto CxcPackageIdentity::hex() const -> std::string {
    return detail::sha256Hex(sha256);
}

CxcContentProvider::CxcContentProvider(std::shared_ptr<const detail::CxcPackageData> data) noexcept
    : data_(std::move(data)) {}

CxcContentProvider::~CxcContentProvider() = default;

auto CxcContentProvider::readBlob(const content::ContentRequest& request)
    -> core::Result<content::ContentBlob> {
    try {
        if (request.rootId.empty() || request.source.empty()) {
            return core::unexpected(core::Error{"content.cxc.request_invalid",
                                                "CXC content root and source must not be empty"});
        }
        if (request.maxBytes == 0) {
            return core::unexpected(core::Error{"content.cxc.limit_invalid",
                                                "CXC content byte limit must be non-zero"});
        }
        if (!detail::isPortablePath(request.source)) {
            return core::unexpected(core::Error{"content.cxc.source_invalid",
                                                "CXC content source must be a portable path"});
        }
        const auto found = data_->contentPaths.find(contentKey(request.rootId, request.source));
        if (found == data_->contentPaths.end()) {
            return core::unexpected(
                core::Error{"content.cxc.source_not_found", "CXC content source was not declared"}
                    .withContext("rootId", std::string{request.rootId})
                    .withContext("source", std::string{request.source}));
        }
        const auto* entry = findZipEntry(*data_, found->second);
        if (entry == nullptr) {
            return core::unexpected(
                core::Error{"content.cxc.source_not_found", "CXC content source is unavailable"});
        }
        if (entry->metadata.byteCount > request.maxBytes) {
            return core::unexpected(
                core::Error{"content.provider.too_large", "CXC content exceeds the byte limit"}
                    .withContext("size_bytes", std::to_string(entry->metadata.byteCount))
                    .withContext("limit_bytes", std::to_string(request.maxBytes)));
        }
        const auto source = archiveEntryBytes(*data_, *entry);
        content::ContentBlob result;
        result.bytes.assign(source.begin(), source.end());
        result.revision = revisionFromSha256(entry->metadata.sha256);
        return result;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& exception) {
        return core::unexpected(
            core::Error{"content.cxc.read_failed", "CXC content read failed"}.withContext(
                "exception", exception.what()));
    } catch (...) {
        return core::unexpected(core::Error{"content.cxc.read_failed", "CXC content read failed"});
    }
}

CxcPackage::CxcPackage(std::shared_ptr<const detail::CxcPackageData> data) noexcept
    : data_(std::move(data)) {}

auto CxcPackage::identity() const noexcept -> const CxcPackageIdentity& {
    return data_->identity;
}

auto CxcPackage::manifest() const noexcept -> const CxcManifestDocument& {
    return data_->manifest;
}

auto CxcPackage::project() const noexcept -> const project::ProjectConfig& {
    return data_->project;
}

auto CxcPackage::assetIndexes() const noexcept -> std::span<const CxcAssetIndex> {
    return {data_->assetIndexes.data(), data_->assetIndexes.size()};
}

auto CxcPackage::projectDocuments() const noexcept -> std::span<const chart::ProjectDocument> {
    return {data_->projectDocuments.data(), data_->projectDocuments.size()};
}

auto CxcPackage::entries() const noexcept -> std::span<const CxcArchiveEntry> {
    return {data_->entries.data(), data_->entries.size()};
}

auto CxcPackage::bytes() const noexcept -> std::span<const std::byte> {
    return {data_->archiveBytes.data(), data_->archiveBytes.size()};
}

auto CxcPackage::contentProvider() const -> std::shared_ptr<CxcContentProvider> {
    return std::shared_ptr<CxcContentProvider>{new CxcContentProvider{data_}};
}

auto CxcPackageLoader::loadMemory(std::span<const std::byte> bytes, const CxcPackageLimits& limits)
    -> CxcPackageResult {
    try {
        if (limits.maxPackageBytes == 0 || bytes.size() > limits.maxPackageBytes) {
            auto diagnostics = makeDiagnostics(limits);
            addError(diagnostics, "cxc.budget.exceeded", "CXC package exceeds the byte limit",
                     "$/archive");
            diagnostics.sortDeterministically();
            return {std::nullopt, std::move(diagnostics)};
        }
        return loadMemory(std::vector<std::byte>{bytes.begin(), bytes.end()}, limits);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& exception) {
        return internalFailure(limits, "memory load", &exception);
    } catch (...) {
        return internalFailure(limits, "memory load");
    }
}

auto CxcPackageLoader::loadMemory(std::vector<std::byte> bytes, const CxcPackageLimits& limits)
    -> CxcPackageResult {
    try {
        return buildPackage(std::move(bytes), limits);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& exception) {
        return internalFailure(limits, "memory load", &exception);
    } catch (...) {
        return internalFailure(limits, "memory load");
    }
}

auto CxcPackageLoader::loadFile(const std::filesystem::path& path, const CxcPackageLimits& limits)
    -> CxcPackageResult {
    try {
        auto diagnostics = makeDiagnostics(limits);
        if (path.extension().string() != ".cxc") {
            addError(diagnostics, "cxc.archive.invalid", "CXC file locator must use .cxc",
                     "$/source");
            diagnostics.sortDeterministically();
            return {std::nullopt, std::move(diagnostics)};
        }
        const auto root =
            path.parent_path().empty() ? std::filesystem::path{"."} : path.parent_path();
        auto contents = filesystem::readBoundedFile(
            path, {.root = root,
                   .maxBytes = limits.maxPackageBytes,
                   .errors = {.rootUnavailable = "cxc.file.root_unavailable",
                              .rootChanged = "cxc.file.root_changed",
                              .openFailed = "cxc.file.open_failed",
                              .outsideRoot = "cxc.file.outside_root",
                              .notRegular = "cxc.file.not_regular",
                              .tooLarge = "cxc.budget.exceeded",
                              .readFailed = "cxc.file.read_failed",
                              .changedDuringRead = "cxc.file.changed_during_read"}});
        if (!contents) {
            addError(diagnostics, contents.error(), "$/source");
            diagnostics.sortDeterministically();
            return {std::nullopt, std::move(diagnostics)};
        }
        return loadMemory(std::move(contents->bytes), limits);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& exception) {
        return internalFailure(limits, "file load", &exception);
    } catch (...) {
        return internalFailure(limits, "file load");
    }
}

} // namespace cuexis::cxc
