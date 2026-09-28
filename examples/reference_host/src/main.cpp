#include "host_commands.hpp"
#include "host_report.hpp"
#include "host_runner.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>

namespace {

using cuexis_reference_host::HostOptions;
using cuexis_reference_host::HostReport;
using cuexis_reference_host::ParseFailure;
using cuexis_reference_host::Program;

void printUsage(std::ostream& out) {
    out << "usage: cuexis_reference_host --content <project-directory> [options]\n"
           "  --content <dir>            host content root (a project directory)\n"
           "  --package <file.cxc>       also load a published Cuexis package\n"
           "  --advance <n>              extra host-clock advance frames (default 4)\n"
           "  --expect-identity <hex>    require the reference content identity\n"
           "  --expect-digest <i>=<v>    require a frame digest (repeatable)\n"
           "  --report <file>            write the run record to a file as well\n"
           "  --command-file <file>      run a command program instead of the\n"
           "                             built-in self-check sequence\n"
           "  --help                     print this usage text\n"
           "\n"
           "In command mode --content is an optional default root that only an\n"
           "argument-less open depends on, and --package, --advance and\n"
           "--expect-digest are refused rather than reinterpreted.\n";
}

[[nodiscard]] auto parseUnsigned(std::string_view text, std::size_t& value) -> bool {
    if (text.empty()) {
        return false;
    }
    std::size_t parsed = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') {
            return false;
        }
        parsed = (parsed * 10U) + static_cast<std::size_t>(digit - '0');
    }
    value = parsed;
    return true;
}

[[nodiscard]] auto parseDigest(std::string_view text, std::size_t& index, std::uint64_t& value)
    -> bool {
    const auto separator = text.find('=');
    if (separator == std::string_view::npos) {
        return false;
    }
    if (!parseUnsigned(text.substr(0, separator), index)) {
        return false;
    }
    const auto digits = text.substr(separator + 1U);
    if (digits.empty()) {
        return false;
    }
    std::uint64_t parsed = 0;
    for (const char digit : digits) {
        if (digit < '0' || digit > '9') {
            return false;
        }
        parsed = (parsed * 10U) + static_cast<std::uint64_t>(digit - '0');
    }
    value = parsed;
    return true;
}

// The whole invocation, parsed but not yet acted on. Parsing collects the first
// problem instead of exiting, so the run record can be opened before the problem
// is reported: section 3.6.1 requires a flags, parse or file rejection to leave
// a record, and a record cannot be created by the code that already returned.
struct Invocation final {
    HostOptions options;
    std::optional<std::filesystem::path> reportPath;
    std::optional<std::filesystem::path> commandFile;
    // Whether a flag actually appeared. The command mode conflicts are about
    // explicit presence, so a default value must not be able to trigger one.
    bool packageSeen{false};
    bool advanceSeen{false};
    bool expectDigestSeen{false};
    std::size_t commandFileCount{0};
    bool helpRequested{false};
    bool parseFailed{false};
    std::string parseDetail;
};

void recordParseFailure(Invocation& invocation, std::string_view detail) {
    if (!invocation.parseFailed) {
        invocation.parseFailed = true;
        invocation.parseDetail = std::string{detail};
    }
}

void parseInvocation(int argc, char** argv, Invocation& invocation) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto next = [&]() -> std::optional<std::string_view> {
            if (index + 1 >= argc) {
                return std::nullopt;
            }
            ++index;
            return std::string_view{argv[index]};
        };
        if (argument == "--help" || argument == "-h") {
            invocation.helpRequested = true;
            continue;
        }
        if (argument == "--content") {
            const auto value = next();
            if (!value) {
                recordParseFailure(invocation, "missing value for --content");
                continue;
            }
            invocation.options.contentDirectory = std::filesystem::path{*value};
            continue;
        }
        if (argument == "--package") {
            const auto value = next();
            if (!value) {
                recordParseFailure(invocation, "missing value for --package");
                continue;
            }
            invocation.options.packageFile = std::filesystem::path{*value};
            invocation.packageSeen = true;
            continue;
        }
        if (argument == "--advance") {
            const auto value = next();
            std::size_t parsed = 0;
            if (!value || !parseUnsigned(*value, parsed)) {
                recordParseFailure(invocation, "invalid value for --advance");
                continue;
            }
            invocation.options.advanceFrames = parsed;
            invocation.advanceSeen = true;
            continue;
        }
        if (argument == "--expect-identity") {
            const auto value = next();
            if (!value) {
                recordParseFailure(invocation, "missing value for --expect-identity");
                continue;
            }
            invocation.options.expectedIdentity = std::string{*value};
            continue;
        }
        if (argument == "--expect-digest") {
            const auto value = next();
            std::size_t digestIndex = 0;
            std::uint64_t digestValue = 0;
            if (!value || !parseDigest(*value, digestIndex, digestValue)) {
                recordParseFailure(invocation, "invalid value for --expect-digest");
                continue;
            }
            invocation.options.expectedDigests.emplace_back(digestIndex, digestValue);
            invocation.expectDigestSeen = true;
            continue;
        }
        if (argument == "--report") {
            const auto value = next();
            if (!value) {
                recordParseFailure(invocation, "missing value for --report");
                continue;
            }
            invocation.reportPath = std::filesystem::path{*value};
            continue;
        }
        if (argument == "--command-file") {
            const auto value = next();
            if (!value) {
                recordParseFailure(invocation, "missing value for --command-file");
                continue;
            }
            ++invocation.commandFileCount;
            invocation.commandFile = std::filesystem::path{*value};
            continue;
        }
        recordParseFailure(invocation, "unknown argument: " + std::string{argument});
    }
}

// Section 3.1: in command mode these three flags are refused rather than
// reinterpreted, and a repeated --command-file is refused. --expect-identity and
// --report keep their meaning.
[[nodiscard]] auto commandModeConflict(const Invocation& invocation) -> std::optional<std::string> {
    if (invocation.commandFileCount > 1U) {
        return std::string{"--command-file was given more than once"};
    }
    if (invocation.packageSeen) {
        return std::string{"--package cannot be combined with --command-file"};
    }
    if (invocation.advanceSeen) {
        return std::string{"--advance cannot be combined with --command-file"};
    }
    if (invocation.expectDigestSeen) {
        return std::string{"--expect-digest cannot be combined with --command-file"};
    }
    return std::nullopt;
}

void echoFile(const std::filesystem::path& path, std::ostream& out) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return;
    }
    out << stream.rdbuf();
    out.flush();
}

} // namespace

int main(int argc, char** argv) {
    Invocation invocation;
    parseInvocation(argc, argv, invocation);
    const bool commandMode = invocation.commandFileCount > 0U;

    // The legacy path keeps its exact previous behaviour: an unusable command
    // line prints usage and returns without creating a report.
    if (invocation.parseFailed && !commandMode) {
        std::cerr << invocation.parseDetail << '\n';
        printUsage(std::cerr);
        return 1;
    }
    if (!commandMode && !invocation.helpRequested && invocation.options.contentDirectory.empty()) {
        printUsage(std::cerr);
        return 1;
    }

    std::ofstream reportFile;
    if (invocation.reportPath) {
        reportFile.open(*invocation.reportPath, std::ios::binary | std::ios::trunc);
        if (!reportFile) {
            std::cerr << "could not open report file: " << invocation.reportPath->string() << '\n';
            return 1;
        }
    }

    int exitCode = 0;
    {
        std::unique_ptr<HostReport> report;
        if (invocation.reportPath) {
            report = std::make_unique<HostReport>(reportFile);
        } else {
            report = std::make_unique<HostReport>(std::cout);
        }

        // Only command mode has conflict rules. Computing this unconditionally
        // would make the legacy invocation's own --package a conflict.
        const auto conflict =
            commandMode ? commandModeConflict(invocation) : std::optional<std::string>{};

        if (invocation.helpRequested) {
            printUsage(std::cout);
            report->summary();
        } else if (invocation.parseFailed) {
            report->diagnostic("flags", cuexis_reference_host::diagnostic::flagConflict,
                               invocation.parseDetail);
            report->summary();
            exitCode = 1;
        } else if (conflict) {
            report->diagnostic("flags", cuexis_reference_host::diagnostic::flagConflict, *conflict);
            report->summary();
            exitCode = 1;
        } else if (commandMode) {
            auto parsed = cuexis_reference_host::parseCommandFile(*invocation.commandFile);
            if (auto* failure = std::get_if<ParseFailure>(&parsed)) {
                // An unreadable file is a file problem; every other parse
                // rejection is a problem with the file's contents.
                const auto step = failure->code == cuexis_reference_host::diagnostic::fileRead
                                      ? std::string_view{"file"}
                                      : std::string_view{"parse"};
                report->diagnostic(step, failure->code, failure->detail);
                report->summary();
                exitCode = 1;
            } else {
                invocation.options.commandProgram = std::move(std::get<Program>(parsed));
                exitCode = cuexis_reference_host::runHost(invocation.options, *report);
                report->summary();
            }
        } else {
            exitCode = cuexis_reference_host::runHost(invocation.options, *report);
            report->summary();
        }
        reportFile.flush();
    }
    reportFile.close();
    if (invocation.reportPath) {
        echoFile(*invocation.reportPath, std::cout);
    }
    return exitCode;
}
