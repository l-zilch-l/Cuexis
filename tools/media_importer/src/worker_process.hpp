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

// Runs the importer again as a child process whose memory is capped by the operating system: a job
// object process-memory limit on Windows and RLIMIT_AS on POSIX. The cap is a backstop for decoder
// allocations that cannot be measured; the measured conversion budget remains the primary gate.
[[nodiscard]] auto runBoundedWorker(const WorkerRequest& request) -> core::Result<WorkerOutcome>;

} // namespace cuexis::media_importer
