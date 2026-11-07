#pragma once

// Source-construction helpers shared by the playback source units. The factories and the
// support unit validate the same project content, so the helpers are named once here.

#include <cuexis/playback/playback_source.hpp>

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/project/asset_index_reader.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::playback {

inline constexpr std::string_view standaloneChartPath = "chart.cuexis.chart.json";
inline constexpr std::size_t maxSourceIdBytes = 256;
inline constexpr std::size_t maxProjectDocumentBytes = 16U * 1024U * 1024U;
#if defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
inline constexpr std::size_t maxCandidateEntryBytes = 16U * 1024U * 1024U;
#endif

struct CxcSourceData final {
    std::string sourceId;
    std::string entryChartPath;
    std::vector<PlaybackProjectDocument> projectDocuments;
    std::optional<assets::AssetDatabase> database;
    std::shared_ptr<content::IContentProvider> provider;
    std::array<std::uint8_t, 32> packageIdentity{};
};

[[nodiscard]] auto firstDiagnosticError(std::string code, std::string message,
                                        const core::Diagnostics& diagnostics) -> core::Error;

[[nodiscard]] auto translatePresentationAssetError(core::Error error) -> core::Error;

[[nodiscard]] auto sourceFailure(std::string_view operation,
                                 const std::exception* exception = nullptr) -> core::Error;

[[nodiscard]] auto sourceAllocationFailure(std::string_view operation) -> core::Error;

#if !defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
[[nodiscard]] auto candidateDisabledError() -> core::Error;

#endif
[[nodiscard]] auto readTextFile(const std::filesystem::path& path,
                                const std::filesystem::path& root, std::size_t maxBytes,
                                std::string_view prefix, std::string_view description)
    -> core::Result<std::string>;

[[nodiscard]] auto isPortableProjectPath(std::string_view path) -> bool;

[[nodiscard]] auto joinPortableProjectPath(std::string_view base, std::string_view relative)
    -> std::optional<std::string>;

[[nodiscard]] auto isPortableSourceId(std::string_view value) -> bool;

[[nodiscard]] auto validateProjectDocuments(std::string_view entryChartPath,
                                            std::vector<PlaybackProjectDocument> documents)
    -> core::Result<std::vector<PlaybackProjectDocument>>;

[[nodiscard]] auto toAssetType(PlaybackAssetType type) noexcept -> assets::AssetType;

[[nodiscard]] auto toAssetType(project::AssetType type) noexcept -> assets::AssetType;

[[nodiscard]] auto convertTypedAssets(const std::vector<PlaybackAssetDescriptor>& descriptors)
    -> core::Result<assets::AssetDatabase>;

[[nodiscard]] auto convertCxcAssets(const cxc::CxcPackage& package)
    -> core::Result<assets::AssetDatabase>;

[[nodiscard]] auto convertCxcPackage(cxc::CxcPackage package) -> core::Result<CxcSourceData>;

} // namespace cuexis::playback
