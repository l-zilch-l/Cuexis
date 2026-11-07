#include <cuexis/tools/cxc_tool.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

[[nodiscard]] auto parseInput(int argc, char** argv) -> std::optional<std::filesystem::path> {
    if (argc != 3 || std::string_view{argv[1]} != "--input" || std::string_view{argv[2]}.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path{argv[2]};
}

} // namespace

int main(int argc, char** argv) {
    const auto input = parseInput(argc, argv);
    if (!input) {
        std::cerr << "Usage: cuexis_cxc_validate --input <package.cxc>\n";
        return 2;
    }
    const auto result = cuexis::tools::validateCxc(*input);
    cuexis::tools::emitCxcToolResult(result);
    return result.exitCode;
}
