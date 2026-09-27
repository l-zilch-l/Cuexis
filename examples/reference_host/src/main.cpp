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

namespace {

using cuexis_reference_host::HostOptions;
using cuexis_reference_host::HostReport;

void printUsage(std::ostream& out) {
    out << "usage: cuexis_reference_host --content <project-directory> [options]\n"
           "  --content <dir>         host content root (a project directory)\n"
           "  --package <file.cxc>    also load a published Cuexis package\n"
           "  --advance <n>           extra host-clock advance frames (default 4)\n"
           "  --expect-identity <hex> require the reference content identity\n"
           "  --expect-digest <i>=<v> require a frame digest (repeatable)\n"
           "  --report <file>         write the run record to a file as well\n"
           "  --help                  print this usage text\n";
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
    HostOptions options;
    std::optional<std::filesystem::path> reportPath;
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
            printUsage(std::cout);
            return 0;
        }
        if (argument == "--content") {
            const auto value = next();
            if (!value) {
                printUsage(std::cerr);
                return 1;
            }
            options.contentDirectory = std::filesystem::path{*value};
            continue;
        }
        if (argument == "--package") {
            const auto value = next();
            if (!value) {
                printUsage(std::cerr);
                return 1;
            }
            options.packageFile = std::filesystem::path{*value};
            continue;
        }
        if (argument == "--advance") {
            const auto value = next();
            std::size_t parsed = 0;
            if (!value || !parseUnsigned(*value, parsed)) {
                printUsage(std::cerr);
                return 1;
            }
            options.advanceFrames = parsed;
            continue;
        }
        if (argument == "--expect-identity") {
            const auto value = next();
            if (!value) {
                printUsage(std::cerr);
                return 1;
            }
            options.expectedIdentity = std::string{*value};
            continue;
        }
        if (argument == "--expect-digest") {
            const auto value = next();
            std::size_t digestIndex = 0;
            std::uint64_t digestValue = 0;
            if (!value || !parseDigest(*value, digestIndex, digestValue)) {
                printUsage(std::cerr);
                return 1;
            }
            options.expectedDigests.emplace_back(digestIndex, digestValue);
            continue;
        }
        if (argument == "--report") {
            const auto value = next();
            if (!value) {
                printUsage(std::cerr);
                return 1;
            }
            reportPath = std::filesystem::path{*value};
            continue;
        }
        std::cerr << "unknown argument: " << argument << '\n';
        printUsage(std::cerr);
        return 1;
    }
    if (options.contentDirectory.empty()) {
        printUsage(std::cerr);
        return 1;
    }

    std::ofstream reportFile;
    std::unique_ptr<HostReport> report;
    if (reportPath) {
        reportFile.open(*reportPath, std::ios::binary | std::ios::trunc);
        if (!reportFile) {
            std::cerr << "could not open report file: " << reportPath->string() << '\n';
            return 1;
        }
        report = std::make_unique<HostReport>(reportFile);
    } else {
        report = std::make_unique<HostReport>(std::cout);
    }

    const auto code = cuexis_reference_host::runHost(options, *report);
    report->summary();
    reportFile.flush();
    reportFile.close();
    if (reportPath) {
        echoFile(*reportPath, std::cout);
    }
    return code;
}
