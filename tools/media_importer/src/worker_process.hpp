#pragma once

// Bounded worker process for the media importer CLI. Internal to the tool.

#include <cuexis/core/result.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cuexis::media_importer {

struct WorkerRequest final {
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::uint64_t memoryLimitBytes{};
};

struct WorkerOutcome final {
    bool started{};
    int exitCode{};
    std::string output;
};

// AddressSanitizer reserves terabytes of shadow address space, so a POSIX address-space cap makes
// the sanitizer runtime abort before main. Sanitized builds therefore run the worker without the
// operating-system cap, and the CLI gate only asserts cap enforcement in a non-sanitized build.
// Windows job-object limits are per-process commit limits and stay active under ASan.
#if defined(__SANITIZE_ADDRESS__)
inline constexpr bool workerAddressSpaceCapSupported = false;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
inline constexpr bool workerAddressSpaceCapSupported = false;
#else
inline constexpr bool workerAddressSpaceCapSupported = true;
#endif
#else
inline constexpr bool workerAddressSpaceCapSupported = true;
#endif

// Runs the importer again as a child process whose memory is capped by the operating system: a job
// object process-memory limit on Windows and RLIMIT_AS on POSIX. The cap is a backstop for decoder
// allocations that cannot be measured; the measured conversion budget remains the primary gate.
[[nodiscard]] auto runBoundedWorker(const WorkerRequest& request) -> core::Result<WorkerOutcome>;

} // namespace cuexis::media_importer
