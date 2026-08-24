#include <cuexis/chart/chart_migrator.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace {

struct Arguments final {
    std::filesystem::path input;
    std::filesystem::path output;
    std::filesystem::path report;
    std::uint32_t targetVersion{3};
};

void printUsage() {
    std::cerr << "Usage: cuexis_chart_migrator --input <chart> "
                 "--output <chart> --report <report-json> [--target 3|4]\n";
}

void printDiagnostics(const cuexis::core::Diagnostics& diagnostics) {
    for (const auto& diagnostic : diagnostics.items()) {
        std::cerr << diagnostic.code();
        if (!diagnostic.fieldPath().empty()) {
            std::cerr << " " << diagnostic.fieldPath();
        }
        std::cerr << ": " << diagnostic.message() << '\n';
    }
}

[[nodiscard]] auto parseArguments(int argc, char** argv) -> std::optional<Arguments> {
    Arguments result;
    bool hasInput = false;
    bool hasOutput = false;
    bool hasReport = false;
    bool hasTarget = false;
    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc || std::string_view{argv[index + 1]}.empty()) {
            return std::nullopt;
        }
        const std::string_view option{argv[index]};
        if (option == "--input" && !hasInput) {
            result.input = argv[index + 1];
            hasInput = true;
        } else if (option == "--output" && !hasOutput) {
            result.output = argv[index + 1];
            hasOutput = true;
        } else if (option == "--report" && !hasReport) {
            result.report = argv[index + 1];
            hasReport = true;
        } else if (option == "--target" && !hasTarget) {
            const std::string_view target{argv[index + 1]};
            if (target == "3") {
                result.targetVersion = 3;
            } else if (target == "4") {
                result.targetVersion = 4;
            } else {
                return std::nullopt;
            }
            hasTarget = true;
        } else {
            return std::nullopt;
        }
    }
    if (!hasInput || !hasOutput || !hasReport) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] auto normalizedPath(const std::filesystem::path& path)
    -> std::optional<std::filesystem::path> {
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    if (error) {
        return std::nullopt;
    }
    return absolute.lexically_normal();
}

[[nodiscard]] bool samePath(const std::filesystem::path& left, const std::filesystem::path& right) {
    const auto normalizedLeft = normalizedPath(left);
    const auto normalizedRight = normalizedPath(right);
    if (!normalizedLeft || !normalizedRight) {
        return false;
    }
    if (*normalizedLeft == *normalizedRight) {
        return true;
    }
    std::error_code error;
    const bool equivalent = std::filesystem::equivalent(*normalizedLeft, *normalizedRight, error);
    return !error && equivalent;
}

[[nodiscard]] auto readInput(const std::filesystem::path& path, std::size_t maxBytes)
    -> std::optional<std::string> {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > maxBytes) {
        std::cerr << "chart.migration.input_unreadable: input is missing, unreadable, or exceeds "
                  << maxBytes << " bytes\n";
        return std::nullopt;
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        std::cerr << "chart.migration.input_unreadable: could not open input\n";
        return std::nullopt;
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(contents.size())) {
        std::cerr << "chart.migration.input_unreadable: could not read complete input\n";
        return std::nullopt;
    }
    return contents;
}

[[nodiscard]] auto temporaryPath(const std::filesystem::path& target, std::string_view role)
    -> std::filesystem::path {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return target.parent_path() / (target.filename().string() + ".cuexis-" + std::string{role} +
                                   "-" + std::to_string(stamp));
}

[[nodiscard]] bool writeTemporary(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    return output.good();
}

void removeIfPresent(const std::filesystem::path& path) noexcept {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

void restoreBackup(const std::filesystem::path& backup,
                   const std::filesystem::path& target) noexcept {
    std::error_code ignored;
    if (!std::filesystem::exists(backup, ignored)) {
        return;
    }
    std::filesystem::remove(target, ignored);
    ignored.clear();
    std::filesystem::rename(backup, target, ignored);
}

[[nodiscard]] bool commitOutputs(const std::filesystem::path& outputPath,
                                 std::string_view outputContents,
                                 const std::filesystem::path& reportPath,
                                 std::string_view reportContents) {
    const auto outputTemp = temporaryPath(outputPath, "output-tmp");
    const auto reportTemp = temporaryPath(reportPath, "report-tmp");
    const auto outputBackup = temporaryPath(outputPath, "output-backup");
    const auto reportBackup = temporaryPath(reportPath, "report-backup");
    removeIfPresent(outputTemp);
    removeIfPresent(reportTemp);
    removeIfPresent(outputBackup);
    removeIfPresent(reportBackup);

    if (!writeTemporary(outputTemp, outputContents) ||
        !writeTemporary(reportTemp, reportContents)) {
        removeIfPresent(outputTemp);
        removeIfPresent(reportTemp);
        return false;
    }

    std::error_code error;
    const bool hadOutput = std::filesystem::exists(outputPath, error) && !error;
    error.clear();
    const bool hadReport = std::filesystem::exists(reportPath, error) && !error;
    if (error) {
        removeIfPresent(outputTemp);
        removeIfPresent(reportTemp);
        return false;
    }

    if (hadOutput) {
        std::filesystem::rename(outputPath, outputBackup, error);
        if (error) {
            removeIfPresent(outputTemp);
            removeIfPresent(reportTemp);
            return false;
        }
    }
    if (hadReport) {
        std::filesystem::rename(reportPath, reportBackup, error);
        if (error) {
            restoreBackup(outputBackup, outputPath);
            removeIfPresent(outputTemp);
            removeIfPresent(reportTemp);
            return false;
        }
    }

    std::filesystem::rename(outputTemp, outputPath, error);
    if (error) {
        restoreBackup(outputBackup, outputPath);
        restoreBackup(reportBackup, reportPath);
        removeIfPresent(outputTemp);
        removeIfPresent(reportTemp);
        return false;
    }
    std::filesystem::rename(reportTemp, reportPath, error);
    if (error) {
        removeIfPresent(outputPath);
        restoreBackup(outputBackup, outputPath);
        restoreBackup(reportBackup, reportPath);
        removeIfPresent(reportTemp);
        return false;
    }

    removeIfPresent(outputBackup);
    removeIfPresent(reportBackup);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const auto arguments = parseArguments(argc, argv);
    if (!arguments) {
        printUsage();
        return 2;
    }
    if (samePath(arguments->input, arguments->output) ||
        samePath(arguments->input, arguments->report) ||
        samePath(arguments->output, arguments->report)) {
        std::cerr << "chart.migration.path_conflict: input, output, and report paths must be "
                     "distinct\n";
        return 2;
    }

    const cuexis::chart::ChartLimits limits;
    const auto source = readInput(arguments->input, limits.maxInputBytes);
    if (!source) {
        return 2;
    }

    auto migrated = arguments->targetVersion == 4
                        ? cuexis::chart::ChartMigrator::migrateToV4(*source, limits)
                        : cuexis::chart::ChartMigrator::migrateToV3(*source, limits);
    printDiagnostics(migrated.diagnostics);
    if (!migrated.hasValue()) {
        return 1;
    }

    if (!commitOutputs(arguments->output, migrated.artifact->chartJson, arguments->report,
                       migrated.artifact->reportJson)) {
        std::cerr << "chart.migration.output_commit_failed: neither target could be committed\n";
        return 2;
    }

    std::cout << "Migrated Cuexis Chart v" << migrated.artifact->report.sourceVersion << " to v"
              << migrated.artifact->report.targetVersion << '\n';
    return 0;
}
