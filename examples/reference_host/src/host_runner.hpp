#pragma once

// The reference host command loop. It owns the main loop, the host clock, the
// content provider and the frame consumption; the SDK owns only the session.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "host_content.hpp"
#include "host_report.hpp"

namespace cuexis_reference_host {

struct HostOptions final {
    std::filesystem::path contentDirectory;
    std::optional<std::filesystem::path> packageFile;
    std::size_t advanceFrames{4};
    std::optional<std::string> expectedIdentity;
    std::vector<std::pair<std::size_t, std::uint64_t>> expectedDigests;
};

// Runs the full host sequence: start, load, commit, frames, seek, reload,
// rejected reload, package load and destroy. Returns the process exit code.
[[nodiscard]] auto runHost(const HostOptions& options, HostReport& report) -> int;

} // namespace cuexis_reference_host
