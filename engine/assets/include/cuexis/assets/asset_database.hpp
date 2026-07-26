#pragma once

//  AssetDatabase — 不可变资产索引
//  从 AssetDatabaseInput（各资产根及其 cuexis.asset-index.json）构建
//  AssetId 到类型/来源/依赖的映射；目录枚举不用于发现 AssetId
//  v1 类型只包含 Mesh、Material、Texture 的 CPU blob

#include <cuexis/assets/asset_id.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::assets {

// The v1 index deliberately contains only types whose CPU representation is a
// bounded opaque blob.  Concrete mesh/material/texture formats are introduced
// by later stages and must not leak into the index contract.
enum class AssetType {
    Mesh,
    Material,
    Texture,
};

using AssetKind = AssetType;

[[nodiscard]] std::string_view assetTypeName(AssetType type) noexcept;

struct AssetRoot final {
    std::string id;
    std::filesystem::path path;

    friend bool operator==(const AssetRoot&, const AssetRoot&) = default;
};

struct AssetRecord final {
    AssetId id;
    AssetType type{AssetType::Mesh};
    // Portable, project-relative path using '/' separators.  It is never an
    // AssetId and is resolved only by AssetDatabase.
    std::string source;
    std::vector<AssetId> dependencies;

    friend bool operator==(const AssetRecord&, const AssetRecord&) = default;
};

struct AssetIndex final {
    std::string format{"cuexis.asset-index"};
    std::uint32_t version{1};
    std::vector<AssetRecord> assets;

    friend bool operator==(const AssetIndex&, const AssetIndex&) = default;
};

// A root and its independently loaded cuexis.asset-index.json.  Keeping this
// association explicit prevents root ordering from becoming an override rule.
struct AssetRootIndex final {
    AssetRoot root;
    AssetIndex index;
};

enum class AssetSourceMode {
    Filesystem,
    Logical,
};

struct AssetDatabaseInput final {
    std::vector<AssetRootIndex> roots;
    // Filesystem mode preserves the stage-1B physical validation and supplies a default
    // FilesystemContentProvider. Logical mode validates only root/source identities and requires
    // callers to inject a provider into ResourceManager or PlaybackSession.
    AssetSourceMode sourceMode{AssetSourceMode::Filesystem};
};

struct AssetDatabaseLimits final {
    std::size_t maxRoots{16};
    std::size_t maxAssets{100'000};
    std::size_t maxDependenciesPerAsset{256};
    std::size_t maxDependencyDepth{64};
    std::size_t maxAssetIdBytes{256};
    std::size_t maxSourcePathBytes{4096};
    std::size_t maxDiagnostics{1024};
};

struct AssetBlobLimits final {
    // A deliberately conservative default for the stage-1 CPU blob loader.
    std::size_t maxBytes{64U * 1024U * 1024U};
};

struct AssetBlob final {
    std::vector<std::byte> bytes;
    std::string rootId;
    std::string source;
    std::uint64_t providerRevision{};

    [[nodiscard]] std::span<const std::byte> span() const noexcept {
        return bytes;
    }
};

struct AssetDatabaseBuildResult;

class AssetDatabase final {
  public:
    AssetDatabase() = default;

    [[nodiscard]] static auto build(const AssetDatabaseInput& input,
                                    const AssetDatabaseLimits& limits = {})
        -> AssetDatabaseBuildResult;

    // Error-returning convenience for callers that do not need to retain the
    // complete validation diagnostic set.
    [[nodiscard]] static auto create(const AssetDatabaseInput& input,
                                     const AssetDatabaseLimits& limits = {})
        -> core::Result<AssetDatabase>;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t rootCount() const noexcept;

    [[nodiscard]] std::vector<AssetId> ids() const;
    [[nodiscard]] const AssetRecord* find(const AssetId& id) const noexcept;
    [[nodiscard]] const AssetRecord* find(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<AssetType> typeOf(const AssetId& id) const noexcept;
    [[nodiscard]] std::string_view rootIdOf(const AssetId& id) const noexcept;
    [[nodiscard]] std::filesystem::path sourcePath(const AssetId& id) const;
    [[nodiscard]] std::vector<AssetId> dependenciesOf(const AssetId& id) const;

    // Reads exactly one indexed source file.  Directory enumeration is never
    // used to discover AssetIds.  The returned bytes are owned by the caller.
    [[nodiscard]] auto readBlob(const AssetId& id, const AssetBlobLimits& limits = {}) const
        -> core::Result<AssetBlob>;

    [[nodiscard]] auto defaultContentProvider() const noexcept
        -> std::shared_ptr<content::IContentProvider>;

    [[nodiscard]] const std::vector<AssetRoot>& roots() const noexcept;

  private:
    struct Data;
    explicit AssetDatabase(std::shared_ptr<const Data> data) noexcept;

    std::shared_ptr<const Data> data_;
};

struct AssetDatabaseBuildResult final {
    std::optional<AssetDatabase> database;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept {
        return database.has_value() && !diagnostics.hasErrors();
    }
};

} // namespace cuexis::assets
