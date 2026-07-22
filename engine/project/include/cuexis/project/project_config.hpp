#pragma once

//  ProjectConfig v1 — 固定文件 cuexis.project.json，format: "cuexis.project"
//  包含项目身份（UUIDv7）、命名的资产根、入口谱面和 opaque extensions
//  ProjectConfig 不包含窗口位置、音频设备、用户目录、设备预算或 profile 引用
//  BootstrapLocator 使用 {root, path} 定位入口谱面，不把路径当作 AssetId

#include <cuexis/core/diagnostic.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::project {

inline constexpr std::string_view projectFormat = "cuexis.project";
inline constexpr std::string_view projectFileName = "cuexis.project.json";
inline constexpr std::string_view assetIndexFileName = "cuexis.asset-index.json";
inline constexpr std::uint32_t projectFormatVersion = 1;

struct ProjectLimits final {
    std::size_t maxInputBytes{1024U * 1024U};
    std::size_t maxNestingDepth{32};
    std::size_t maxStringBytes{16U * 1024U};
    std::size_t maxPortablePathBytes{4096};
    std::size_t maxDiagnostics{1024};
    std::size_t maxAssetRoots{16};
    std::size_t maxExtensions{256};
};

struct BootstrapLocator final {
    std::string root;
    std::string path;

    auto operator<=>(const BootstrapLocator&) const = default;
};

struct AssetRoot final {
    std::string id;
    std::string path;

    auto operator<=>(const AssetRoot&) const = default;
};

// Opaque extension data is normalized JSON text and cannot expose a JSON DOM.
struct OpaqueJson final {
    std::string canonicalText{"{}"};

    auto operator<=>(const OpaqueJson&) const = default;
};

struct ProjectEntry final {
    BootstrapLocator chart;

    auto operator<=>(const ProjectEntry&) const = default;
};

struct ProjectConfig final {
    std::string format{projectFormat};
    std::uint32_t version{projectFormatVersion};
    std::string projectId;
    std::vector<AssetRoot> assetRoots;
    ProjectEntry entry;
    OpaqueJson extensions;

    auto operator<=>(const ProjectConfig&) const = default;
};

struct ProjectConfigResult final {
    std::optional<ProjectConfig> config;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept {
        return config.has_value() && !diagnostics.hasErrors();
    }
};

struct PreparedAssetRoot final {
    AssetRoot declaration;
    std::filesystem::path absolutePath;
    std::filesystem::path assetIndexFile;
};

struct PreparedProject final {
    ProjectConfig config;
    std::filesystem::path projectFile;
    std::filesystem::path projectRoot;
    std::vector<PreparedAssetRoot> assetRoots;
    std::filesystem::path chartFile;

    [[nodiscard]] const PreparedAssetRoot* findAssetRoot(std::string_view id) const noexcept;
};

struct ProjectLoadResult final {
    std::optional<PreparedProject> project;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept {
        return project.has_value() && !diagnostics.hasErrors();
    }
};

} // namespace cuexis::project
