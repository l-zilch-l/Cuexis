#pragma once

// Publication helpers shared by the asset publish units. The marker, lock, generation and
// package units all name the same files, so the names live here once.

#include "publish_fs_internal.hpp"

#include <cuexis/tools/asset_publish.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::tools {

namespace fs = std::filesystem;

using detail::publishBusyCode;
using detail::publishConflictCode;
using detail::publishError;
using detail::publishInvalidCode;
using detail::publishIoCode;
using detail::publishReplaceCode;
using detail::publishStagingCode;
using detail::publishValidationCode;

inline constexpr std::string_view generationDomain = "cuexis.generation.closure.v1";
inline constexpr std::string_view generationStagingDirectory = "staging";
inline constexpr std::string_view generationPublishedDirectory = "generations";
inline constexpr std::string_view publicationRole = "cuexis-publish";
inline constexpr std::string_view publicationBackupRole = "cuexis-backup";
inline constexpr std::string_view temporaryInfix = ".tmp.";
inline constexpr std::size_t maxGenerationIdBytes = 64;
inline constexpr std::size_t maxEntries = 65534;

struct ListedEntry final {
    std::string path;
    std::uint64_t byteCount{};
    std::string sha256;
};

struct MarkerDocument final {
    std::string label;
    std::string identity;
    std::vector<ListedEntry> closure;
    std::vector<ListedEntry> provenance;
};

[[nodiscard]] auto diagnosticsMessage(const core::Diagnostics& diagnostics) -> std::string;

[[nodiscard]] auto pathOf(std::string_view name) -> fs::path;

[[nodiscard]] auto isPortableToken(std::string_view value, std::size_t maxBytes) noexcept -> bool;

[[nodiscard]] auto closureByteTotal(const std::vector<ListedEntry>& closure) -> std::uint64_t;

[[nodiscard]] auto listEntries(const std::vector<PublishEntry>& entries)
    -> core::Result<std::vector<ListedEntry>>;

[[nodiscard]] auto generationIdentityOf(std::string_view label,
                                        const std::vector<ListedEntry>& closure,
                                        const std::vector<ListedEntry>& provenance) -> std::string;

[[nodiscard]] auto encodeMarker(const MarkerDocument& marker) -> std::string;

[[nodiscard]] auto verifyGenerationTree(const fs::path& generationPath,
                                        const MarkerDocument& marker) -> core::Result<void>;

[[nodiscard]] auto readMarker(const fs::path& generationPath) -> core::Result<MarkerDocument>;

[[nodiscard]] auto markersEqual(const MarkerDocument& left, const MarkerDocument& right) noexcept
    -> bool;

[[nodiscard]] auto writeEntryTree(const fs::path& base, const std::vector<PublishEntry>& entries,
                                  const fs::path& relativeBase) -> core::Result<void>;

[[nodiscard]] auto publishGenerationLocked(const GenerationPublishRequest& request,
                                           bool acquireLock)
    -> core::Result<GenerationPublishResult>;

[[nodiscard]] auto adoptGenerationLocked(const fs::path& root, std::string_view generationId,
                                         bool acquireLock, bool* commitVisible = nullptr)
    -> core::Result<void>;

} // namespace cuexis::tools
