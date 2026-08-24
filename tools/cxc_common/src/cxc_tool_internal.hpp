#pragma once

#include <cuexis/tools/cxc_tool.hpp>

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_package.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace cuexis::tools::detail {

inline constexpr int successExitCode = 0;
inline constexpr int invalidContentExitCode = 1;
inline constexpr int usageOrIoExitCode = 2;

struct LoadedPackage final {
    std::optional<cxc::CxcPackage> package;
    std::optional<CxcToolResult> failure;
};

[[nodiscard]] auto success(std::string summary) -> CxcToolResult;
[[nodiscard]] auto failure(std::string code, std::string fieldPath, std::string message,
                           std::string_view contextKey = {}, std::string_view contextValue = {})
    -> CxcToolResult;
[[nodiscard]] auto invalid(const core::Diagnostics& diagnostics, std::string_view defaultPath = {})
    -> CxcToolResult;
[[nodiscard]] auto loadPackage(const std::filesystem::path& inputPath) -> LoadedPackage;

[[nodiscard]] auto normalizedPath(const std::filesystem::path& path)
    -> std::optional<std::filesystem::path>;
[[nodiscard]] bool samePath(const std::filesystem::path& left, const std::filesystem::path& right);
[[nodiscard]] bool containsPath(const std::filesystem::path& root,
                                const std::filesystem::path& candidate);
[[nodiscard]] bool isSafeExistingPath(const std::filesystem::path& path, bool requireDirectory);
[[nodiscard]] auto uniqueSibling(const std::filesystem::path& target, std::string_view role,
                                 std::string_view suffix = {}) -> std::filesystem::path;

[[nodiscard]] bool writeBytes(const std::filesystem::path& path, std::span<const std::byte> bytes);
void removeFile(const std::filesystem::path& path) noexcept;
void removeTree(const std::filesystem::path& path) noexcept;
[[nodiscard]] bool testFailureEnabled(std::string_view name) noexcept;

} // namespace cuexis::tools::detail
