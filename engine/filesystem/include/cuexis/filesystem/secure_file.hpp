#pragma once

// Bounded file reads with handle-relative containment and no path re-open.

#include <cuexis/core/result.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace cuexis::filesystem {

struct ReadFileErrorCodes final {
    std::string rootUnavailable{"filesystem.file.root_unavailable"};
    std::string openFailed{"filesystem.file.open_failed"};
    std::string outsideRoot{"filesystem.file.outside_root"};
    std::string notRegular{"filesystem.file.not_regular"};
    std::string tooLarge{"filesystem.file.too_large"};
    std::string readFailed{"filesystem.file.read_failed"};
    std::string changedDuringRead{"filesystem.file.changed_during_read"};
};

struct ReadFileOptions final {
    std::filesystem::path root;
    std::size_t maxBytes{};
    ReadFileErrorCodes errors{};
};

struct FileContents final {
    std::vector<std::byte> bytes;
    std::filesystem::path resolvedPath;
    std::filesystem::path resolvedRoot;
};

struct TextFileContents final {
    std::string text;
    std::filesystem::path resolvedPath;
    std::filesystem::path resolvedRoot;
};

[[nodiscard]] auto readBoundedFile(const std::filesystem::path& path,
                                   const ReadFileOptions& options) -> core::Result<FileContents>;

[[nodiscard]] auto readBoundedTextFile(const std::filesystem::path& path,
                                       const ReadFileOptions& options)
    -> core::Result<TextFileContents>;

} // namespace cuexis::filesystem
