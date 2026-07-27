#pragma once

// Asset Index v1/v2 uses one cuexis.asset-index.json per asset root.
// It declares Mesh, Material, Texture, and Audio types with source paths and dependencies.
// Directory enumeration does not discover AssetIds; AssetDatabase reads only this index.

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/project/project_config.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::project {

inline constexpr std::string_view assetIndexFormat = "cuexis.asset-index";
inline constexpr std::uint32_t assetIndexFormatVersion = 1;
inline constexpr std::uint32_t assetIndexFormatVersion2 = 2;

enum class AssetType {
    Mesh,
    Material,
    Texture,
    Audio,
};

[[nodiscard]] std::string_view assetTypeName(AssetType type) noexcept;

struct AssetIndexLimits final {
    std::size_t maxInputBytes{64U * 1024U * 1024U};
    std::size_t maxNestingDepth{32};
    std::size_t maxStringBytes{16U * 1024U};
    std::size_t maxPortablePathBytes{4096};
    std::size_t maxAssetIdBytes{256};
    std::size_t maxDiagnostics{1024};
    std::size_t maxAssets{100000};
    std::size_t maxDependenciesPerAsset{256};
    std::size_t maxExtensions{256};
};

struct AssetIndexRecord final {
    std::string id;
    AssetType type{AssetType::Mesh};
    std::string source;
    std::vector<std::string> dependencies;
    OpaqueJson extensions;

    auto operator<=>(const AssetIndexRecord&) const = default;
};

struct AssetIndexDocument final {
    std::string format{assetIndexFormat};
    std::uint32_t version{assetIndexFormatVersion};
    std::vector<AssetIndexRecord> assets;
    OpaqueJson extensions;

    auto operator<=>(const AssetIndexDocument&) const = default;
};

struct AssetIndexResult final {
    std::optional<AssetIndexDocument> document;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept {
        return document.has_value() && !diagnostics.hasErrors();
    }
};

class AssetIndexReader final {
  public:
    [[nodiscard]] static AssetIndexResult read(std::string_view jsonText,
                                               const AssetIndexLimits& limits = {});
};

} // namespace cuexis::project
