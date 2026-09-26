#pragma once

// File and publication helpers for the media importer CLI. Internal to the tool.

#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::media_importer {

// Reads a whole file, refusing anything above the encoded-source limit before it allocates.
[[nodiscard]] auto readFileBounded(const std::filesystem::path& path, std::uint64_t limit)
    -> core::Result<std::vector<std::byte>>;

// Writes bytes to a unique temporary file next to the target, then renames it into place. An
// existing target with identical bytes is left untouched; an existing target with different bytes
// is an immutability conflict and no rename happens.
[[nodiscard]] auto publishImmutable(const std::filesystem::path& target,
                                    std::span<const std::byte> bytes) -> core::Result<void>;

// Streaming SHA-256 of a file, lowercase hexadecimal.
[[nodiscard]] auto hashFile(const std::filesystem::path& path) -> core::Result<std::string>;

// Renames an already published temporary file into its content-identity addressed target. The
// target name is the identity of the temporary file: an existing target that still hashes to that
// identity is left untouched and the temporary copy is dropped, while an existing target holding
// anything else is an immutability conflict.
[[nodiscard]] auto promoteTemporary(const std::filesystem::path& temporary,
                                    const std::filesystem::path& target, std::string_view identity)
    -> core::Result<void>;

[[nodiscard]] auto currentProcessId() -> long;

} // namespace cuexis::media_importer
