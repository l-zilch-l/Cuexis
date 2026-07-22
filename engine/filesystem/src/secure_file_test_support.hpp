#pragma once

// Internal deterministic race hooks for cuexis_filesystem_tests.

#include <cuexis/filesystem/secure_file.hpp>

namespace cuexis::filesystem::detail {

struct ReadFileTestHooks final {
    using Hook = void (*)(void*) noexcept;

    Hook afterRootValidated{};
    Hook afterFileOpened{};
    void* context{};
};

[[nodiscard]] auto readBoundedFileWithHooks(const std::filesystem::path& path,
                                            const ReadFileOptions& options,
                                            const ReadFileTestHooks& hooks)
    -> core::Result<FileContents>;

} // namespace cuexis::filesystem::detail
