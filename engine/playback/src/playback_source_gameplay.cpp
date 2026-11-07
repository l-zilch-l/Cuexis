#include "playback_source_internal.hpp"

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

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include "gameplay_entry_internal.hpp"
#include "gameplay_internal.hpp"
#include <cuexis/json/parse.hpp>
#include <cuexis_internal/sha256.hpp>
#endif

namespace cuexis::playback {

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
namespace {
auto selectGameplayMetadata(std::string_view extensions, std::string_view entryPath,
                            GameplayConfigurationDecodeBudget budget) -> core::Result<std::string> {
    if (!budget.testOnly || !budget.maxBytes || !budget.maxDepth || !budget.maxStringBytes ||
        !budget.maxValues || !budget.maxContainerElements)
        return core::unexpected(core::Error{"capability.budget_insufficient",
                                            "Explicit extension metadata budgets required"}
                                    .withContext("category", "budget_exceeded")
                                    .withContext("severity", "error")
                                    .withContext("faulted", "false"));
    auto parsed =
        json::parseBounded(extensions, {{budget.maxBytes, budget.maxDepth, budget.maxStringBytes},
                                        budget.maxValues,
                                        budget.maxContainerElements});
    if (!parsed) {
        const auto code = parsed.error().code();
        const bool exceeded = code == "json.parse.size_limit" || code == "json.parse.depth_limit" ||
                              code == "json.parse.string_limit" ||
                              code == "json.parse.value_limit" ||
                              code == "json.parse.element_limit";
        return core::unexpected(
            core::Error{exceeded ? "capability.budget_insufficient" : "playback.gameplay.invalid",
                        "Gameplay extension JSON invalid"}
                .withContext("sourceCode", std::string{code})
                .withContext("category", exceeded ? "budget_exceeded" : "invalid_relation")
                .withContext("severity", "error")
                .withContext("faulted", "false"));
    }
    const auto* extension = parsed->find("cuexis.gameplay-entry.v1");
    if (!extension)
        return core::unexpected(
            core::Error{"playback.gameplay.invalid", "Gameplay extension missing"});
    const auto* entries = extension->find("entries");
    if (!extension->object() || extension->object()->size() != 1 || !entries || !entries->array() ||
        entries->array()->empty())
        return core::unexpected(
            core::Error{"playback.gameplay.invalid", "Gameplay entry directory invalid"});
    const json::Value* selected = nullptr;
    std::set<std::string> paths;
    for (const auto& entry : *entries->array()) {
        const auto* path = entry.find("path");
        if (!path || !path->string() || !paths.insert(*path->string()).second)
            return core::unexpected(core::Error{"playback.gameplay.invalid",
                                                "Gameplay entry path missing or duplicated"});
        auto text = json::serialize(entry);
        if (!text)
            return core::unexpected(std::move(text.error()));
        auto validated = detail::parseGameplayEntryMetadata(*text, budget);
        if (!validated)
            return core::unexpected(std::move(validated.error()));
        if (*path->string() == entryPath)
            selected = &entry;
    }
    if (!selected)
        return core::unexpected(
            core::Error{"playback.gameplay.invalid", "Requested Gameplay entry path mismatch"});
    return json::serialize(*selected);
}
auto descriptors(const assets::AssetDatabase* database) -> std::vector<PlaybackAssetDescriptor> {
    std::vector<PlaybackAssetDescriptor> result;
    if (!database)
        return result;
    for (const auto& id : database->ids()) {
        const auto* record = database->find(id);
        PlaybackAssetType type;
        switch (record->type) {
        case assets::AssetType::Mesh:
            type = PlaybackAssetType::Mesh;
            break;
        case assets::AssetType::Material:
            type = PlaybackAssetType::Material;
            break;
        case assets::AssetType::Texture:
            type = PlaybackAssetType::Texture;
            break;
        case assets::AssetType::Audio:
            type = PlaybackAssetType::Audio;
            break;
        case assets::AssetType::Shader:
            type = PlaybackAssetType::Shader;
            break;
        default:
            throw std::invalid_argument{"Unknown asset type"};
        }
        PlaybackAssetDescriptor value{
            record->id.value, type, std::string{database->rootIdOf(id)}, record->source, {}};
        for (const auto& dependency : record->dependencies)
            value.dependencies.push_back(dependency.value);
        result.push_back(std::move(value));
    }
    return result;
}
auto cxcGameplaySource(const cxc::CxcPackage& package, std::string_view entryPath,
                       const GameplayConfiguration& configuration,
                       GameplayConfigurationDecodeBudget metadataBudget,
                       GameplayGraphDecodeBudget graphBudget, GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource> {
    auto metadata = selectGameplayMetadata(package.project().extensions.canonicalText, entryPath,
                                           metadataBudget);
    if (!metadata)
        return core::unexpected(std::move(metadata.error()));
    auto bytes = package.entryBytes(entryPath);
    if (!bytes)
        return core::unexpected(
            core::Error{"playback.gameplay.invalid", "Gameplay entry bytes missing"});
    auto database = convertCxcAssets(package);
    if (!database)
        return core::unexpected(std::move(database.error()));
    return PlaybackSource::fromGameplayEntry(package.project().projectId, std::move(*metadata),
                                             {bytes->begin(), bytes->end()}, configuration,
                                             metadataBudget, graphBudget, descriptors(&*database),
                                             package.contentProvider(), intent);
}
} // namespace

auto PlaybackSource::fromCxcFileGameplayEntry(const std::filesystem::path& locator,
                                              std::string entryPath,
                                              const GameplayConfiguration& configuration,
                                              GameplayConfigurationDecodeBudget metadataBudget,
                                              GameplayGraphDecodeBudget graphBudget,
                                              GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource> try {
    auto loaded = cxc::CxcPackageLoader::loadFile(locator);
    if (!loaded.hasValue())
        return core::unexpected(firstDiagnosticError("playback.source.cxc_invalid",
                                                     "CXC package loading produced errors",
                                                     loaded.diagnostics));
    auto source = cxcGameplaySource(*loaded.package, entryPath, configuration, metadataBudget,
                                    graphBudget, intent);
    if (source)
        source->state_->cxcPackageIdentity = loaded.package->identity().sha256;
    return source;
} catch (...) {
    return core::unexpected(sourceAllocationFailure("cxc_gameplay_file"));
}

auto PlaybackSource::fromCxcMemoryGameplayEntry(std::vector<std::byte> bytes, std::string entryPath,
                                                const GameplayConfiguration& configuration,
                                                GameplayConfigurationDecodeBudget metadataBudget,
                                                GameplayGraphDecodeBudget graphBudget,
                                                GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource> try {
    auto loaded = cxc::CxcPackageLoader::loadMemory(std::move(bytes));
    if (!loaded.hasValue())
        return core::unexpected(firstDiagnosticError("playback.source.cxc_invalid",
                                                     "CXC package loading produced errors",
                                                     loaded.diagnostics));
    auto source = cxcGameplaySource(*loaded.package, entryPath, configuration, metadataBudget,
                                    graphBudget, intent);
    if (source)
        source->state_->cxcPackageIdentity = loaded.package->identity().sha256;
    return source;
} catch (...) {
    return core::unexpected(sourceAllocationFailure("cxc_gameplay_memory"));
}

auto PlaybackSource::fromFilesystemGameplayEntry(const std::filesystem::path& locator,
                                                 std::string entryPath,
                                                 const GameplayConfiguration& configuration,
                                                 GameplayConfigurationDecodeBudget metadataBudget,
                                                 GameplayGraphDecodeBudget graphBudget,
                                                 GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource> try {
    auto project = project::ProjectLoader::load(locator);
    if (!project.hasValue())
        return core::unexpected(firstDiagnosticError("playback.source.project_invalid",
                                                     "Project loading produced errors",
                                                     project.diagnostics));
    auto metadata = selectGameplayMetadata(project.project->config.extensions.canonicalText,
                                           entryPath, metadataBudget);
    if (!metadata)
        return core::unexpected(std::move(metadata.error()));
    if (!isPortableProjectPath(entryPath))
        return core::unexpected(
            core::Error{"playback.gameplay.invalid", "Gameplay entry path invalid"});
    auto base = fromFilesystemProject(locator);
    if (!base)
        return core::unexpected(std::move(base.error()));
    auto bytes = filesystem::readBoundedFile(
        project.project->projectRoot / entryPath,
        {.root = project.project->projectRoot,
         .maxBytes = maxCandidateEntryBytes,
         .errors = {.rootUnavailable = "playback.candidate.root_unavailable",
                    .rootChanged = "playback.candidate.root_changed",
                    .openFailed = "playback.candidate.open_failed",
                    .outsideRoot = "playback.candidate.outside_root",
                    .notRegular = "playback.candidate.not_regular",
                    .tooLarge = "playback.candidate.too_large",
                    .readFailed = "playback.candidate.read_failed",
                    .changedDuringRead = "playback.candidate.changed_during_read"}});
    if (!bytes)
        return core::unexpected(std::move(bytes.error()));
    return fromGameplayEntry(
        base->state_->sourceId, std::move(*metadata), std::move(bytes->bytes), configuration,
        metadataBudget, graphBudget,
        descriptors(base->state_->database ? &*base->state_->database : nullptr),
        base->state_->provider, intent);
} catch (...) {
    return core::unexpected(sourceAllocationFailure("filesystem_gameplay_entry"));
}

auto PlaybackSource::fromGameplayEntry(
    std::string sourceId, std::string metadataJson, std::vector<std::byte> entryBytes,
    const GameplayConfiguration& configuration, GameplayConfigurationDecodeBudget metadataBudget,
    GameplayGraphDecodeBudget graphBudget, std::vector<PlaybackAssetDescriptor> assets,
    std::shared_ptr<content::IContentProvider> provider, GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource> try {
    auto metadata = detail::parseGameplayEntryMetadata(metadataJson, metadataBudget);
    if (!metadata)
        return core::unexpected(std::move(metadata.error()));
    const auto* path = metadata->find("path")->string();
    if (!path || !isPortableProjectPath(*path))
        return core::unexpected(
            core::Error{"playback.gameplay.invalid", "Invalid Gameplay entry path"}
                .withContext("category", "invalid_relation")
                .withContext("severity", "error")
                .withContext("faulted", "false"));
    if (*metadata->find("artifactIdentity")->string() != core::detail::sha256Hex(entryBytes))
        return core::unexpected(
            core::Error{"playback.gameplay.invalid", "Gameplay artifact hash mismatch"}
                .withContext("category", "identity_closure_incomplete")
                .withContext("severity", "error")
                .withContext("faulted", "false"));
    const auto& kind = *metadata->find("entryKind")->string();
    const auto& encoding = *metadata->find("encoding")->string();
    const auto revision = detail::packedGameplayRevision(entryBytes);
    std::optional<std::string_view> expectedCompiledSemanticIdentity;
    if (kind == "packed-chart" && (revision == 4 || encoding == "capsule.4")) {
        if (revision != 4 || encoding != "capsule.4")
            return core::unexpected(core::Error{"playback.gameplay.invalid",
                                                "Gameplay metadata encoding/Header mismatch"}
                                        .withContext("category", "identity_closure_incomplete")
                                        .withContext("severity", "error")
                                        .withContext("faulted", "false")
                                        .withContext("fieldPath", "metadata.encoding"));
        expectedCompiledSemanticIdentity = *metadata->find("compiledSemanticIdentity")->string();
    }
    auto source = fromGameplayEncoded(
        std::move(sourceId), *path, entryBytes, configuration, assets, std::move(provider), intent,
        kind == "gameplay-graph" ? std::optional{graphBudget} : std::nullopt,
        expectedCompiledSemanticIdentity);
    if (!source)
        return core::unexpected(std::move(source.error()));
    auto expected = detail::gameplayEntryMetadata(source->state_->gameplayContent, configuration,
                                                  *path, kind, entryBytes, assets);
    if (!expected)
        return core::unexpected(std::move(expected.error()));
    if (*metadata != *expected)
        return core::unexpected(
            core::Error{"playback.gameplay.invalid", "Gameplay metadata closure mismatch"}
                .withContext("category", "identity_closure_incomplete")
                .withContext("severity", "error")
                .withContext("faulted", "false"));
    return source;
} catch (...) {
    return core::unexpected(sourceAllocationFailure("gameplay_entry"));
}

auto PlaybackSource::fromGameplayEncoded(
    std::string sourceId, std::string entryPath, std::vector<std::byte> bytes,
    const GameplayConfiguration& configuration, std::vector<PlaybackAssetDescriptor> assets,
    std::shared_ptr<content::IContentProvider> provider, GameplayPrepareIntent intent,
    std::optional<GameplayGraphDecodeBudget> graphBudget,
    std::optional<std::string_view> expectedCompiledSemanticIdentity)
    -> core::Result<PlaybackSource> try {
    if (intent != GameplayPrepareIntent::Presentation &&
        intent != GameplayPrepareIntent::GameplayOnly)
        return core::unexpected(core::Error{"playback.gameplay.invalid", "Unknown prepare intent"});
    if (!isPortableSourceId(sourceId))
        return core::unexpected(core::Error{"playback.source.id_invalid", "Invalid source ID"});
    if (!isPortableProjectPath(entryPath))
        return core::unexpected(
            core::Error{"playback.source.entry_path_invalid", "Invalid Gameplay entry path"});
    if (!provider)
        return core::unexpected(
            core::Error{"playback.source.provider_missing", "Gameplay provider missing"});
    std::optional<assets::AssetDatabase> database;
    if (!assets.empty()) {
        auto converted = convertTypedAssets(assets);
        if (!converted)
            return core::unexpected(translatePresentationAssetError(std::move(converted.error())));
        database = std::move(*converted);
    }
    auto content = graphBudget ? GameplayContent::fromGraph(
                                     {reinterpret_cast<const char*>(bytes.data()), bytes.size()},
                                     configuration, *graphBudget)
                               : detail::gameplayContentFromPacked(
                                     bytes, configuration, expectedCompiledSemanticIdentity);
    if (!content)
        return core::unexpected(std::move(content.error()));
    const auto* typed = detail::GameplayAccess::content(*content);
    auto identity = chart::packed::semanticIdentity(typed->capsule.chart);
    if (!identity)
        return core::unexpected(std::move(identity.error()));
    auto state = std::make_unique<State>();
    state->sourceId = std::move(sourceId);
    state->entryChartPath = entryPath;
    state->database = std::move(database);
    state->provider = std::move(provider);
    cxc::CandidateChart candidate;
    candidate.entry.path = std::move(entryPath);
    candidate.entry.kind = "chart";
    candidate.entry.encoding = graphBudget ? "gameplay-graph" : "packed-chart";
    candidate.entry.playback = true;
    candidate.entry.compilerProfile = "gameplay.author.t4-k4.v1";
    candidate.bytes = std::move(bytes);
    candidate.semantic = typed->capsule.chart;
    candidate.semanticIdentity = *identity;
    state->candidate = std::move(candidate);
    state->gameplayContent = std::move(*content);
    state->gameplayIntent = intent;
    return PlaybackSource{std::move(state)};
} catch (...) {
    return core::unexpected(sourceAllocationFailure("gameplay_packed"));
}

auto PlaybackSource::fromGameplayPacked(std::string sourceId, std::string entryPath,
                                        std::vector<std::byte> bytes,
                                        const GameplayConfiguration& configuration,
                                        std::vector<PlaybackAssetDescriptor> assets,
                                        std::shared_ptr<content::IContentProvider> provider,
                                        GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource> {
    return fromGameplayEncoded(std::move(sourceId), std::move(entryPath), std::move(bytes),
                               configuration, std::move(assets), std::move(provider), intent,
                               std::nullopt);
}
auto PlaybackSource::fromGameplayGraph(std::string sourceId, std::string entryPath,
                                       std::vector<std::byte> bytes,
                                       const GameplayConfiguration& configuration,
                                       GameplayGraphDecodeBudget budget,
                                       std::vector<PlaybackAssetDescriptor> assets,
                                       std::shared_ptr<content::IContentProvider> provider,
                                       GameplayPrepareIntent intent)
    -> core::Result<PlaybackSource> {
    return fromGameplayEncoded(std::move(sourceId), std::move(entryPath), std::move(bytes),
                               configuration, std::move(assets), std::move(provider), intent,
                               budget);
}

#endif

} // namespace cuexis::playback
