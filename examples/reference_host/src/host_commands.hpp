#pragma once

// The command file: its syntax, its limits and the typed program it parses into.
//
// R9 section 3.2 requires the whole file to be accepted before any SDK load, so
// parsing is a separate, complete step that either produces a full program or a
// single failure. Nothing downstream ever sees a partial program.

#include "host_clock.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cuexis_reference_host {

// R9 section 3.2. Endpoints are inclusive.
inline constexpr std::size_t maxCommandFileBytes = 1'048'576;
inline constexpr std::size_t maxCommands = 10'000;
inline constexpr std::uint64_t maxTickAttempts = 100'000;

// R9 section 3.5. The stable host diagnostic suffixes, spelled once so the
// parser, the executor and the fixtures cannot drift apart. A host code never
// replaces an SDK error: an SDK failure keeps its own code and names the command
// it came from.
namespace diagnostic {

inline constexpr std::string_view fileRead = "host.command.file_read";
inline constexpr std::string_view syntax = "host.command.syntax";
inline constexpr std::string_view unknownVerb = "host.command.unknown_verb";
inline constexpr std::string_view invalidNumber = "host.command.invalid_number";
inline constexpr std::string_view limitExceeded = "host.command.limit_exceeded";
inline constexpr std::string_view missingQuit = "host.command.missing_quit";
inline constexpr std::string_view afterQuit = "host.command.after_quit";
inline constexpr std::string_view flagConflict = "host.command.flag_conflict";
inline constexpr std::string_view notOpen = "host.command.not_open";
inline constexpr std::string_view alreadyOpen = "host.command.already_open";
inline constexpr std::string_view noSample = "host.command.no_sample";
inline constexpr std::string_view clockOverflow = "host.command.clock_overflow";
inline constexpr std::string_view stateMismatch = "host.command.state_mismatch";
inline constexpr std::string_view expectationFailed = "host.command.expectation_failed";

} // namespace diagnostic

enum class Verb { Open, Play, Pause, Tick, Seek, Reload, Quit };

[[nodiscard]] auto verbName(Verb verb) -> std::string_view;

// One accepted command. Runtime preconditions (a second open, a reload with no
// sample) are deliberately NOT modelled here: section 3.2 keeps them out of the
// syntax so that they are refused when the command executes, which preserves the
// lifecycle evidence.
struct Command final {
    Verb verb{Verb::Quit};
    // 1-based physical source line, counting comment and blank lines, because a
    // diagnostic has to point at the real line a reader can open.
    std::size_t line{0};
    // tick count or seek target; zero for every other verb.
    std::uint64_t number{0};
    // open's decoded path, already resolved against the command file's directory
    // because section 3.2 makes in-file paths relative to the containing file
    // rather than to the process working directory.
    std::filesystem::path openPath;
};

struct Program final {
    std::vector<Command> commands;
    // Cumulative tick requests across the whole file, checked against
    // maxTickAttempts. Requests made while paused count too: the budget bounds
    // work asked for, not work that ended up advancing the clock.
    std::uint64_t totalTickAttempts{0};
};

struct ParseFailure final {
    std::string_view code;
    // Carries the source line and the offending text so the record explains
    // itself without a debugger.
    std::string detail;
};

// Parses a complete command file.
[[nodiscard]] auto parseCommandFile(const std::filesystem::path& path)
    -> std::variant<Program, ParseFailure>;

} // namespace cuexis_reference_host
