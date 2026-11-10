#include <cuexis/tools/cxc_tool.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

struct Arguments final {
    std::filesystem::path input;
    std::filesystem::path output;
};

[[nodiscard]] auto parseArguments(int argc, char** argv) -> std::optional<Arguments> {
    if (argc != 5) {
        return std::nullopt;
    }
    Arguments result;
    bool hasInput = false;
    bool hasOutput = false;
    for (int index = 1; index < argc; index += 2) {
        const std::string_view option{argv[index]};
        const std::string_view value{argv[index + 1]};
        if (value.empty()) {
            return std::nullopt;
        }
        if (option == "--input" && !hasInput) {
            result.input = value;
            hasInput = true;
        } else if (option == "--output" && !hasOutput) {
            result.output = value;
            hasOutput = true;
        } else {
            return std::nullopt;
        }
    }
    return hasInput && hasOutput ? std::optional{std::move(result)} : std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    const auto arguments = parseArguments(argc, argv);
    if (!arguments) {
        std::cerr
            << "Usage: cuexis_cxc_pack --input <source-project-root> --output <package.cxc>\n";
        return 2;
    }
    const auto result = cuexis::tools::packCxc(arguments->input, arguments->output);
    cuexis::tools::emitCxcToolResult(result);
    return result.exitCode;
}
