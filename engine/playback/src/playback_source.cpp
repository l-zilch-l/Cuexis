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

namespace cuexis::playback {

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

auto PlaybackSource::fromFilesystemProjectEntry(const std::filesystem::path& locator,
                                                std::string entryPath)
    -> core::Result<PlaybackSource> {
#if !defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    static_cast<void>(locator);
    static_cast<void>(entryPath);
    return core::unexpected(candidateDisabledError());
#else
    try {
        auto projectResult = project::ProjectLoader::load(locator);
        if (!projectResult.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.source.project_invalid",
                                                         "Project loading produced errors",
                                                         projectResult.diagnostics));
        }
        auto sourceResult = fromFilesystemProject(locator);
        if (!sourceResult) {
            return core::unexpected(std::move(sourceResult.error()));
        }
        auto selected = cxc::parseCandidateChartEntryExtension(
            projectResult.project->config.extensions.canonicalText, entryPath);
        if (!selected.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.candidate.entry_invalid",
                                                         "Candidate entry metadata is invalid",
                                                         selected.diagnostics));
        }
        auto bytes = filesystem::readBoundedFile(
            projectResult.project->projectRoot / entryPath,
            {.root = projectResult.project->projectRoot,
             .maxBytes = maxCandidateEntryBytes,
             .errors = {.rootUnavailable = "playback.candidate.root_unavailable",
                        .rootChanged = "playback.candidate.root_changed",
                        .openFailed = "playback.candidate.open_failed",
                        .outsideRoot = "playback.candidate.outside_root",
                        .notRegular = "playback.candidate.not_regular",
                        .tooLarge = "playback.candidate.too_large",
                        .readFailed = "playback.candidate.read_failed",
                        .changedDuringRead = "playback.candidate.changed_during_read"}});
        if (!bytes) {
            return core::unexpected(std::move(bytes.error()));
        }
        auto candidate = cxc::validateCandidateChartBytes(*selected.entry, bytes->bytes);
        if (!candidate.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.candidate.invalid",
                                                         "Candidate Packed entry is invalid",
                                                         candidate.diagnostics));
        }
        auto source = std::move(*sourceResult);
        source.state_->candidate = std::move(*candidate.candidate);
        return source;
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("filesystem_project_entry"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("filesystem_project_entry", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("filesystem_project_entry"));
    }
#endif
}

auto PlaybackSource::fromCxcFileEntry(const std::filesystem::path& locator, std::string entryPath)
    -> core::Result<PlaybackSource> {
#if !defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    static_cast<void>(locator);
    static_cast<void>(entryPath);
    return core::unexpected(candidateDisabledError());
#else
    try {
        auto loaded = cxc::CxcPackageLoader::loadFile(locator);
        if (!loaded.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.source.cxc_invalid",
                                                         "CXC package loading produced errors",
                                                         loaded.diagnostics));
        }
        auto candidate = cxc::validateCandidateChartEntry(*loaded.package, entryPath);
        if (!candidate.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.candidate.invalid",
                                                         "Candidate Packed entry is invalid",
                                                         candidate.diagnostics));
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
        state->candidate = std::move(*candidate.candidate);
        return PlaybackSource{std::move(state)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("cxc_file_entry"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("cxc_file_entry", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("cxc_file_entry"));
    }
#endif
}

auto PlaybackSource::fromCxcMemoryEntry(std::vector<std::byte> packageBytes, std::string entryPath)
    -> core::Result<PlaybackSource> {
#if !defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    static_cast<void>(packageBytes);
    static_cast<void>(entryPath);
    return core::unexpected(candidateDisabledError());
#else
    try {
        auto loaded = cxc::CxcPackageLoader::loadMemory(std::move(packageBytes));
        if (!loaded.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.source.cxc_invalid",
                                                         "CXC package loading produced errors",
                                                         loaded.diagnostics));
        }
        auto candidate = cxc::validateCandidateChartEntry(*loaded.package, entryPath);
        if (!candidate.hasValue()) {
            return core::unexpected(firstDiagnosticError("playback.candidate.invalid",
                                                         "Candidate Packed entry is invalid",
                                                         candidate.diagnostics));
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
        state->candidate = std::move(*candidate.candidate);
        return PlaybackSource{std::move(state)};
    } catch (const std::bad_alloc&) {
        return core::unexpected(sourceAllocationFailure("cxc_memory_entry"));
    } catch (const std::exception& exception) {
        return core::unexpected(sourceFailure("cxc_memory_entry", &exception));
    } catch (...) {
        return core::unexpected(sourceFailure("cxc_memory_entry"));
    }
#endif
}

} // namespace cuexis::playback
