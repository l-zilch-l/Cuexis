#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

void printUsage() {
    std::cerr << "Usage: cuexis_chart_validator --input <chart-path>\n";
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

[[nodiscard]] auto parseInputPath(int argc, char** argv) -> std::optional<std::filesystem::path> {
    if (argc != 3 || std::string_view{argv[1]} != "--input" || std::string_view{argv[2]}.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path{argv[2]};
}

[[nodiscard]] auto readInput(const std::filesystem::path& path, std::size_t maxBytes)
    -> std::optional<std::string> {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > maxBytes) {
        std::cerr << "chart.tool.input_unreadable: input is missing, unreadable, or exceeds "
                  << maxBytes << " bytes\n";
        return std::nullopt;
    }

    std::ifstream input{path, std::ios::binary};
    if (!input) {
        std::cerr << "chart.tool.input_unreadable: could not open input\n";
        return std::nullopt;
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(contents.size())) {
        std::cerr << "chart.tool.input_unreadable: could not read complete input\n";
        return std::nullopt;
    }
    return contents;
}

} // namespace

int main(int argc, char** argv) {
    const auto inputPath = parseInputPath(argc, argv);
    if (!inputPath) {
        printUsage();
        return 2;
    }

    const cuexis::chart::ChartLimits limits;
    const auto contents = readInput(*inputPath, limits.maxInputBytes);
    if (!contents) {
        return 2;
    }

    auto loaded = cuexis::chart::ChartLoader::load(*contents, limits);
    printDiagnostics(loaded.diagnostics);
    if (!loaded.hasValue()) {
        return 1;
    }

    auto compiled = cuexis::chart::ChartCompiler::compile(*loaded.document, limits);
    printDiagnostics(compiled.diagnostics);
    if (!compiled.hasValue()) {
        return 1;
    }

    std::cout << "Valid Cuexis Chart v" << loaded.document->version << ": " << inputPath->string()
              << '\n';
    return 0;
}
