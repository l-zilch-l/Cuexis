#include <cuexis/tools/cxc_tool.hpp>

#include "cxc_tool_internal.hpp"

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_package.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace cuexis::tools::detail {
namespace {

[[nodiscard]] auto safeContextKey(std::string_view key) noexcept -> bool {
    return key == "path" || key == "source" || key == "file" || key == "assetId" || key == "use" ||
           key == "actual_type" || key == "expected_type" || key == "root" ||
           key == "conflicting_root" || key == "import_id" || key == "template_id" ||
           key == "size_bytes" || key == "limit_bytes";
}

[[nodiscard]] auto diagnosticText(const core::Diagnostics& diagnostics,
                                  std::string_view defaultPath) -> std::string {
    std::ostringstream output;
    for (const auto& diagnostic : diagnostics.items()) {
        output << diagnostic.code();
        if (!diagnostic.fieldPath().empty()) {
            output << ' ' << diagnostic.fieldPath();
        }
        bool hasDocumentPath = false;
        std::vector<core::DiagnosticContext> contexts;
        for (const auto& context : diagnostic.context()) {
            if (!safeContextKey(context.key)) {
                continue;
            }
            hasDocumentPath = hasDocumentPath || context.key == "path" || context.key == "source";
            contexts.push_back(context);
        }
        if (!defaultPath.empty() && !hasDocumentPath) {
            contexts.push_back(core::DiagnosticContext{"path", std::string{defaultPath}});
        }
        std::ranges::sort(contexts, {}, &core::DiagnosticContext::key);
        for (const auto& context : contexts) {
            output << ' ' << context.key << '=' << context.value;
        }
        output << ": " << diagnostic.message() << '\n';
    }
    return output.str();
}

[[nodiscard]] auto isIoDiagnostic(std::string_view code) noexcept -> bool {
    return code.starts_with("cxc.file.");
}

[[nodiscard]] auto componentKey(const std::filesystem::path& component) -> std::string {
    auto result = component.generic_string();
#if defined(_WIN32)
    std::ranges::transform(result, result.begin(), [](char character) {
        return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a')
                                                    : character;
    });
#endif
    return result;
}

[[nodiscard]] bool hasReparsePoint(const std::filesystem::path& path) noexcept {
#if defined(_WIN32)
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    static_cast<void>(path);
    return false;
#endif
}

} // namespace

auto success(std::string summary) -> CxcToolResult {
    if (!summary.empty() && !summary.ends_with('\n')) {
        summary.push_back('\n');
    }
    return CxcToolResult{successExitCode, std::move(summary), {}};
}

auto failure(std::string code, std::string fieldPath, std::string message,
             std::string_view contextKey, std::string_view contextValue) -> CxcToolResult {
    std::ostringstream output;
    output << code;
    if (!fieldPath.empty()) {
        output << ' ' << fieldPath;
    }
    if (!contextKey.empty() && !contextValue.empty()) {
        output << ' ' << contextKey << '=' << contextValue;
    }
    output << ": " << message << '\n';
    return CxcToolResult{usageOrIoExitCode, {}, output.str()};
}

auto invalid(const core::Diagnostics& diagnostics, std::string_view defaultPath) -> CxcToolResult {
    return CxcToolResult{invalidContentExitCode, {}, diagnosticText(diagnostics, defaultPath)};
}

auto loadPackage(const std::filesystem::path& inputPath) -> LoadedPackage {
    if (inputPath.empty() || inputPath.extension().string() != ".cxc") {
        return {std::nullopt,
                failure("cxc.tool.input_invalid", "$/input", "Input must identify a .cxc file")};
    }
    auto loaded = cxc::CxcPackageLoader::loadFile(inputPath);
    if (loaded.hasValue()) {
        return {std::move(loaded.package), std::nullopt};
    }
    const bool ioFailure = std::ranges::any_of(
        loaded.diagnostics.items(), [](const auto& item) { return isIoDiagnostic(item.code()); });
    if (ioFailure) {
        return {std::nullopt,
                CxcToolResult{usageOrIoExitCode, {}, diagnosticText(loaded.diagnostics, {})}};
    }
    return {std::nullopt, invalid(loaded.diagnostics)};
}

auto normalizedPath(const std::filesystem::path& path) -> std::optional<std::filesystem::path> {
    if (path.empty()) {
        return std::nullopt;
    }
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return normalized.lexically_normal();
    }
    error.clear();
    normalized = std::filesystem::absolute(path, error);
    return error ? std::nullopt : std::optional{normalized.lexically_normal()};
}

bool samePath(const std::filesystem::path& left, const std::filesystem::path& right) {
    const auto normalizedLeft = normalizedPath(left);
    const auto normalizedRight = normalizedPath(right);
    if (!normalizedLeft || !normalizedRight) {
        return false;
    }
    auto leftPart = normalizedLeft->begin();
    auto rightPart = normalizedRight->begin();
    for (; leftPart != normalizedLeft->end() && rightPart != normalizedRight->end();
         ++leftPart, ++rightPart) {
        if (componentKey(*leftPart) != componentKey(*rightPart)) {
            return false;
        }
    }
    return leftPart == normalizedLeft->end() && rightPart == normalizedRight->end();
}

bool containsPath(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    const auto normalizedRoot = normalizedPath(root);
    const auto normalizedCandidate = normalizedPath(candidate);
    if (!normalizedRoot || !normalizedCandidate) {
        return false;
    }
    auto rootPart = normalizedRoot->begin();
    auto candidatePart = normalizedCandidate->begin();
    for (; rootPart != normalizedRoot->end(); ++rootPart, ++candidatePart) {
        if (candidatePart == normalizedCandidate->end() ||
            componentKey(*rootPart) != componentKey(*candidatePart)) {
            return false;
        }
    }
    return true;
}

bool isSafeExistingPath(const std::filesystem::path& path, bool requireDirectory) {
    if (path.empty()) {
        return false;
    }
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    if (error || absolute.root_path().empty()) {
        return false;
    }

    std::filesystem::path current = absolute.root_path();
    auto status = std::filesystem::symlink_status(current, error);
    if (error || !std::filesystem::exists(status) || std::filesystem::is_symlink(status) ||
        hasReparsePoint(current)) {
        return false;
    }
    for (const auto& component : absolute.relative_path()) {
        if (component == ".") {
            continue;
        }
        if (component == "..") {
            return false;
        }
        current /= component;
        status = std::filesystem::symlink_status(current, error);
        if (error || !std::filesystem::exists(status) || std::filesystem::is_symlink(status) ||
            hasReparsePoint(current)) {
            return false;
        }
    }
    return requireDirectory ? std::filesystem::is_directory(status)
                            : std::filesystem::is_regular_file(status);
}

auto uniqueSibling(const std::filesystem::path& target, std::string_view role,
                   std::string_view suffix) -> std::filesystem::path {
    static std::atomic<std::uint64_t> next{1};
    const auto parent =
        target.parent_path().empty() ? std::filesystem::path{"."} : target.parent_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (std::uint64_t attempt = 0; attempt < 64; ++attempt) {
        const auto name = target.filename().string() + ".cuexis-" + std::string{role} + "-" +
                          std::to_string(stamp) + "-" + std::to_string(next.fetch_add(1)) +
                          std::string{suffix};
        const auto candidate = parent / name;
        std::error_code error;
        if (!std::filesystem::exists(candidate, error) && !error) {
            return candidate;
        }
    }
    return {};
}

bool writeBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        return false;
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    output.close();
    return output.good();
}

void removeFile(const std::filesystem::path& path) noexcept {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

void removeTree(const std::filesystem::path& path) noexcept {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
}

bool testFailureEnabled(std::string_view name) noexcept {
#if defined(CUEXIS_CXC_TOOL_TESTING)
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t length = 0;
    const auto status = _dupenv_s(&value, &length, std::string{name}.c_str());
    const bool enabled = status == 0 && value != nullptr && std::string_view{value} == "1";
    std::free(value);
    return enabled;
#else
    const auto* value = std::getenv(std::string{name}.c_str());
    return value != nullptr && std::string_view{value} == "1";
#endif
#else
    static_cast<void>(name);
    return false;
#endif
}

} // namespace cuexis::tools::detail

namespace cuexis::tools {

auto validateCxc(const std::filesystem::path& inputPath) -> CxcToolResult {
    auto loaded = detail::loadPackage(inputPath);
    if (loaded.failure) {
        return std::move(*loaded.failure);
    }
    const auto& package = *loaded.package;
    return detail::success(
        "Valid CXC: entries=" + std::to_string(package.manifest().entries.size()) + " bytes=" +
        std::to_string(package.bytes().size()) + " identity=" + package.identity().hex());
}

void emitCxcToolResult(const CxcToolResult& result) {
    if (!result.standardOutput.empty()) {
        std::cout << result.standardOutput;
    }
    if (!result.standardError.empty()) {
        std::cerr << result.standardError;
    }
}

} // namespace cuexis::tools
