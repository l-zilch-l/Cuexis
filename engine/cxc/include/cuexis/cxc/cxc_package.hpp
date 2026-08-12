#pragma once

#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/cxc/cxc_manifest.hpp>
#include <cuexis/project/asset_index_reader.hpp>
#include <cuexis/project/project_config.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cuexis::cxc {

struct CxcPackageLimits final {
    std::size_t maxPackageBytes{512U * 1024U * 1024U};
    std::size_t maxEntryBytes{64U * 1024U * 1024U};
    std::size_t maxManifestBytes{1024U * 1024U};
    std::size_t maxEntries{65534};
    std::size_t maxPathBytes{4096};
    std::size_t maxPathDepth{64};
    std::size_t maxDependencyDepth{64};
    std::size_t maxDiagnostics{1024};
    CxcManifestLimits manifest{};
    project::ProjectLimits project{};
    project::AssetIndexLimits assetIndex{};
    chart::ChartLimits chart{};
};

struct CxcPackageIdentity final {
    std::array<std::uint8_t, 32> sha256{};

    [[nodiscard]] auto hex() const -> std::string;
    auto operator<=>(const CxcPackageIdentity&) const = default;
};

struct CxcArchiveEntry final {
    std::string path;
    std::uint64_t byteCount{};
    std::uint32_t crc32{};
    std::string sha256;

    auto operator<=>(const CxcArchiveEntry&) const = default;
};

struct CxcAssetIndex final {
    std::string rootId;
    project::AssetIndexDocument document;
};

namespace detail {
struct CxcPackageData;
}

class CxcContentProvider final : public content::IContentProvider {
  public:
    ~CxcContentProvider() override;

    [[nodiscard]] auto readBlob(const content::ContentRequest& request)
        -> core::Result<content::ContentBlob> override;

  private:
    friend class CxcPackage;
    explicit CxcContentProvider(std::shared_ptr<const detail::CxcPackageData> data) noexcept;

    std::shared_ptr<const detail::CxcPackageData> data_;
};

class CxcPackage final {
  public:
    explicit CxcPackage(std::shared_ptr<const detail::CxcPackageData> data) noexcept;
    CxcPackage(const CxcPackage&) noexcept = default;
    CxcPackage(CxcPackage&&) noexcept = default;
    auto operator=(const CxcPackage&) noexcept -> CxcPackage& = default;
    auto operator=(CxcPackage&&) noexcept -> CxcPackage& = default;
    ~CxcPackage() = default;

    [[nodiscard]] auto identity() const noexcept -> const CxcPackageIdentity&;
    [[nodiscard]] auto manifest() const noexcept -> const CxcManifestDocument&;
    [[nodiscard]] auto project() const noexcept -> const project::ProjectConfig&;
    [[nodiscard]] auto assetIndexes() const noexcept -> std::span<const CxcAssetIndex>;
    [[nodiscard]] auto projectDocuments() const noexcept -> std::span<const chart::ProjectDocument>;
    [[nodiscard]] auto entries() const noexcept -> std::span<const CxcArchiveEntry>;
    [[nodiscard]] auto bytes() const noexcept -> std::span<const std::byte>;
    [[nodiscard]] auto contentProvider() const -> std::shared_ptr<CxcContentProvider>;

  private:
    std::shared_ptr<const detail::CxcPackageData> data_;
};

struct CxcPackageResult final {
    std::optional<CxcPackage> package;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return package.has_value() && !diagnostics.hasErrors();
    }
};

class CxcPackageLoader final {
  public:
    [[nodiscard]] static auto loadMemory(std::span<const std::byte> bytes,
                                         const CxcPackageLimits& limits = {}) -> CxcPackageResult;
    [[nodiscard]] static auto loadMemory(std::vector<std::byte> bytes,
                                         const CxcPackageLimits& limits = {}) -> CxcPackageResult;
    [[nodiscard]] static auto loadFile(const std::filesystem::path& path,
                                       const CxcPackageLimits& limits = {}) -> CxcPackageResult;
};

} // namespace cuexis::cxc
