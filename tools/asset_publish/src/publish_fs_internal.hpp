#pragma once

// Internal filesystem primitives for the atomic publication transaction.
//
// These mirror the two proven patterns in the engine: the exclusive-create + flush write and the
// replace-existing rename from engine/project/src/project_loader.cpp, and the exclusive-create lock
// handle from engine/player_support/src/user_preferences.cpp. They are re-declared here because
// neither implementation is exported, and publication must be fsync-backed rather than the
// flush-only commit used by the shader cache.

#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::tools::detail {

inline constexpr std::string_view publishBusyCode = "asset.publish.busy";
inline constexpr std::string_view publishStagingCode = "asset.publish.staging_failed";
inline constexpr std::string_view publishValidationCode = "asset.publish.validation_failed";
inline constexpr std::string_view publishReplaceCode = "asset.publish.replace_failed";
inline constexpr std::string_view publishConflictCode = "asset.publish.conflict";
inline constexpr std::string_view publishIoCode = "asset.publish.io_failed";
inline constexpr std::string_view publishInvalidCode = "asset.publish.invalid_request";
inline constexpr std::string_view publishRecoveryCode = "asset.publish.recovery_failed";

inline constexpr std::size_t publishMaxPathBytes = 4096;
inline constexpr std::size_t publishMaxEntryBytes = 512ULL * 1024ULL * 1024ULL;

[[nodiscard]] auto publishError(std::string_view code, std::string message) -> core::Error;

// Creates the file exclusively, writes every byte, flushes it to stable storage and closes it. An
// existing file is never truncated or overwritten.
[[nodiscard]] auto writeFileExclusive(const std::filesystem::path& file,
                                      std::span<const std::byte> bytes) -> core::Result<void>;

// Replaces `target` with `source` in one step. On POSIX this is rename(2); on Windows it is
// MoveFileEx with REPLACE_EXISTING and WRITE_THROUGH.
[[nodiscard]] auto replaceAtomically(const std::filesystem::path& source,
                                     const std::filesystem::path& target) -> core::Result<void>;

// Flushes a directory entry so a rename survives a crash. POSIX only; a no-op elsewhere.
[[nodiscard]] auto syncDirectory(const std::filesystem::path& directory) -> core::Result<void>;

[[nodiscard]] auto readFileBytes(const std::filesystem::path& file, std::size_t maxBytes)
    -> core::Result<std::vector<std::byte>>;

[[nodiscard]] auto isRegularFile(const std::filesystem::path& path) noexcept -> bool;
[[nodiscard]] auto isDirectory(const std::filesystem::path& path) noexcept -> bool;

// Best-effort recursive removal. Used for staging cleanup, where a leftover must never mask the
// original failure.
void removeTreeQuiet(const std::filesystem::path& path) noexcept;

// A unique sibling of `target`, so two writers never share a staging path.
[[nodiscard]] auto uniqueSibling(const std::filesystem::path& target, std::string_view role)
    -> std::filesystem::path;

// Portable relative path rules: forward slashes, no dot segments, no absolute or drive prefix,
// every segment non-empty, bounded length.
[[nodiscard]] auto isPortableRelativePath(std::string_view path) noexcept -> bool;

[[nodiscard]] auto isLowerHex(std::string_view value, std::size_t length) noexcept -> bool;

[[nodiscard]] auto toHex(std::span<const std::byte> bytes) -> std::string;
[[nodiscard]] auto sha256Hex(std::span<const std::byte> bytes) -> std::string;

// Test-only failure injection, compiled out of production builds.
[[nodiscard]] auto testFailureEnabled(std::string_view name) noexcept -> bool;
// Test-only: true when the named variable holds exactly `fileName`, so one target of a multi-target
// publication can be failed on purpose.
[[nodiscard]] auto testFailureTargetMatches(std::string_view name,
                                            std::string_view fileName) noexcept -> bool;

} // namespace cuexis::tools::detail
