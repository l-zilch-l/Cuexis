#include <cuexis/playback/playback_source.hpp>

#include "playback_source_state.hpp"

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/project/asset_index_reader.hpp>
#include <cuexis/project/project_loader.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::playback {
namespace {

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
    }
    return assets::AssetType::Mesh;
}

[[nodiscard]] auto convertTypedProject(const TypedPlaybackProject& project)
    -> core::Result<assets::AssetDatabase> {
    assets::AssetDatabaseInput input;
    input.sourceMode = assets::AssetSourceMode::Logical;
    std::map<std::string, std::size_t, std::less<>> rootIndexes;
    bool hasAudio = false;
    for (const auto& descriptor : project.assets) {
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
        index.version = descriptor.type == PlaybackAssetType::Audio ? 2 : index.version;
        hasAudio = hasAudio || descriptor.type == PlaybackAssetType::Audio;
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
    if (hasAudio) {
        for (auto& root : input.roots) {
            root.index.version = 2;
        }
    }
    if (input.roots.empty()) {
        return core::unexpected(core::Error{"playback.source.assets_empty",
                                            "TypedPlaybackProject must contain an asset root"});
    }
    return assets::AssetDatabase::create(input);
}

} // namespace

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
    if (chartJson.empty()) {
        return core::unexpected(
            core::Error{"playback.source.chart_empty", "Playback chart text must not be empty"});
    }
    auto state = std::make_unique<State>();
    state->chartJson = std::move(chartJson);
    return PlaybackSource{std::move(state)};
}

auto PlaybackSource::fromTypedProject(TypedPlaybackProject project,
                                      std::shared_ptr<content::IContentProvider> provider)
    -> core::Result<PlaybackSource> {
    if (project.chartJson.empty()) {
        return core::unexpected(
            core::Error{"playback.source.chart_empty", "Playback chart text must not be empty"});
    }
    if (!provider) {
        return core::unexpected(core::Error{"playback.source.provider_missing",
                                            "TypedPlaybackProject requires a content provider"});
    }
    auto state = std::make_unique<State>();
    state->chartJson = std::move(project.chartJson);
    state->sourceId = std::move(project.sourceId);
    if (!project.assets.empty()) {
        auto database = convertTypedProject(project);
        if (!database) {
            return core::unexpected(translatePresentationAssetError(std::move(database.error())));
        }
        state->database.emplace(std::move(*database));
        state->provider = std::move(provider);
    }
    return PlaybackSource{std::move(state)};
}

auto PlaybackSource::fromFilesystemProject(const std::filesystem::path& locator)
    -> core::Result<PlaybackSource> {
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
    auto chartText = readTextFile(project.chartFile, chartRoot->absolutePath, 16U * 1024U * 1024U,
                                  "playback.chart", "chart");
    if (!chartText) {
        return core::unexpected(std::move(chartText.error()));
    }
    auto provider = database->defaultContentProvider();
    if (!provider) {
        return core::unexpected(core::Error{"playback.source.provider_missing",
                                            "Filesystem project has no content provider"});
    }
    auto state = std::make_unique<State>();
    state->chartJson = std::move(*chartText);
    state->database.emplace(std::move(*database));
    state->provider = std::move(provider);
    state->sourceId = project.config.projectId;
    return PlaybackSource{std::move(state)};
}

} // namespace cuexis::playback
