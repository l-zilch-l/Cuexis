#pragma once

// The reference host command loop. It owns the main loop, the host clock, the
// content provider and the frame consumption; the SDK owns only the session.

#include "host_commands.hpp"
#include "host_content.hpp"
#include "host_report.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cuexis_reference_host {

struct HostOptions final {
    std::filesystem::path contentDirectory;
    std::optional<std::filesystem::path> packageFile;
    std::optional<std::string> candidateEntry;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    std::optional<GameplayHost> gameplay;
#endif
    std::size_t advanceFrames{4};
    std::optional<std::string> expectedIdentity;
    std::vector<std::pair<std::size_t, std::uint64_t>> expectedDigests;

    // Command mode (R9 section 3.1). The program is parsed in main so that a
    // syntax or budget failure is refused before any SDK work, as section 3.2
    // requires; runHost only ever executes an already-accepted program. When this
    // is set, contentDirectory is an optional default root that only an
    // argument-less open depends on.
    std::optional<Program> commandProgram;
};

// Runs the host. In command mode it executes the parsed program; otherwise it
// runs the legacy self-check sequence over content, frames, reload, a rejected
// reload, an optional package and destroy. Returns the process exit code.
[[nodiscard]] auto runHost(const HostOptions& options, HostReport& report) -> int;

} // namespace cuexis_reference_host
