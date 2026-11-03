// Internal CXC package data and parsing helpers.
#pragma once

#include <cuexis/cxc/cxc_package.hpp>

#include <cuexis/cxc/cxc_manifest_loader.hpp>

#include <cuexis/chart/animation_template_loader.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/detail/chart_dispatch_internal.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>
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

namespace package_internal {

struct AssetDeclaration final {
    project::AssetType type{project::AssetType::Mesh};
    std::string rootId;
    std::string source;
    std::vector<std::string> dependencies;
};

[[nodiscard]] inline auto makeDiagnostics(const CxcPackageLimits& limits) -> core::Diagnostics {
    return core::Diagnostics{limits.maxDiagnostics,
                             core::Diagnostic{core::DiagnosticSeverity::Error,
                                              "cxc.budget.exceeded",
                                              "CXC diagnostics reached the configured limit", "$"}};
}

inline void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
                     std::string fieldPath, std::string_view path = {}) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                       std::move(message), std::move(fieldPath)};
    if (!path.empty()) {
        diagnostic.withContext("path", std::string{path});
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

inline void addError(core::Diagnostics& diagnostics, const core::Error& error,
                     std::string fieldPath) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(fieldPath)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

inline void appendDiagnostics(core::Diagnostics& diagnostics, core::Diagnostics source) {
    static_cast<void>(diagnostics.append(std::move(source)));
}

[[nodiscard]] inline auto textFromBytes(std::span<const std::byte> bytes) -> std::string {
    std::string result;
    result.resize(bytes.size());
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index] = static_cast<char>(std::to_integer<unsigned char>(bytes[index]));
    }
    return result;
}

[[nodiscard]] inline auto contentKey(std::string_view rootId, std::string_view source)
    -> std::string {
    std::string result;
    result.reserve(rootId.size() + source.size() + 1U);
    result.append(rootId);
    result.push_back('\0');
    result.append(source);
    return result;
}

[[nodiscard]] inline auto isPathPrefix(std::string_view prefix, std::string_view path) noexcept
    -> bool {
    return path == prefix ||
           (path.size() > prefix.size() && path.starts_with(prefix) && path[prefix.size()] == '/');
}

[[nodiscard]] inline auto findZipEntry(const detail::CxcPackageData& data,
                                       std::string_view path) noexcept
    -> const detail::Zip32Entry* {
    const auto found = data.entryIndexes.find(path);
    return found == data.entryIndexes.end() ? nullptr : &data.zipEntries[found->second];
}

[[nodiscard]] inline auto archiveEntryBytes(const detail::CxcPackageData& data,
                                            const detail::Zip32Entry& entry) noexcept
    -> std::span<const std::byte> {
    return std::span<const std::byte>{data.archiveBytes}.subspan(
        entry.dataOffset, static_cast<std::size_t>(entry.metadata.byteCount));
}

[[nodiscard]] inline auto entryText(const detail::CxcPackageData& data, std::string_view path)
    -> std::optional<std::string> {
    const auto* entry = findZipEntry(data, path);
    return entry == nullptr
               ? std::nullopt
               : std::optional<std::string>{textFromBytes(archiveEntryBytes(data, *entry))};
}

inline void
validateAssetReference(core::Diagnostics& diagnostics,
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

inline void
validateLegacyResources(core::Diagnostics& diagnostics,
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

inline void
validateClipResources(core::Diagnostics& diagnostics,
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

[[nodiscard]] inline auto
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
        if (declaration.type == project::AssetType::Shader && !declaration.dependencies.empty()) {
            auto diagnostic =
                core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.project.invalid",
                                 "Shader assets must be dependency leaves", "$/project/assets"};
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
            } else if (declaration.type != project::AssetType::Material &&
                       found->second.type == project::AssetType::Shader) {
                auto diagnostic = core::Diagnostic{
                    core::DiagnosticSeverity::Error, "cxc.project.invalid",
                    "Only Material assets may depend on Shader assets", "$/project/assets"};
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

} // namespace package_internal
} // namespace cuexis::cxc
