#pragma once

#include <filesystem>
#include <string>

namespace cuexis::tools {

struct CxcToolResult final {
    int exitCode{2};
    std::string standardOutput;
    std::string standardError;
};

[[nodiscard]] auto packCxc(const std::filesystem::path& sourceRoot,
                           const std::filesystem::path& outputPath) -> CxcToolResult;
[[nodiscard]] auto validateCxc(const std::filesystem::path& inputPath) -> CxcToolResult;
[[nodiscard]] auto unpackCxc(const std::filesystem::path& inputPath,
                             const std::filesystem::path& outputPath) -> CxcToolResult;

void emitCxcToolResult(const CxcToolResult& result);

} // namespace cuexis::tools
