#pragma once

#include <chrono>
#include <filesystem>
#include <system_error>
#include <thread>

namespace cuexis::tools::detail {

template <typename Rename, typename Wait>
void renameUnpackDirectoryWithRetry(const std::filesystem::path& source,
                                    const std::filesystem::path& target, std::error_code& error,
                                    Rename&& rename, Wait&& wait) {
    // Windows readers without FILE_SHARE_DELETE can briefly prevent directory renames after
    // our own handles are closed. Retry only these native errors, preserving atomic rename
    // semantics with at most seven requested 10 ms waits, not a wall-clock deadline.
    // Persistent failures still reach rollback.
    constexpr int maxAttempts = 8;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        rename(source, target, error);
#if defined(_WIN32)
        // MSVC reports the sharing failures natively. libstdc++ on MinGW forwards the CRT
        // errno instead, so the same held-directory condition arrives as EBUSY.
        const bool retryable =
            (error.category() == std::system_category() &&
             (error.value() == 5 || error.value() == 32 || error.value() == 33)) ||
            error == std::make_error_code(std::errc::device_or_resource_busy);
#else
        constexpr bool retryable = false;
#endif
        if (!error || !retryable || attempt + 1 == maxAttempts) {
            return;
        }
        wait();
    }
}

inline void renameUnpackDirectory(const std::filesystem::path& source,
                                  const std::filesystem::path& target, std::error_code& error) {
    renameUnpackDirectoryWithRetry(
        source, target, error,
        [](const auto& from, const auto& to, auto& result) {
            std::filesystem::rename(from, to, result);
        },
        [] { std::this_thread::sleep_for(std::chrono::milliseconds{10}); });
}

} // namespace cuexis::tools::detail
