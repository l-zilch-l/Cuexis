// Internal project loader helpers.
#pragma once

//  ProjectLoader 实现 — 项目配置加载与原子保存
//  固定文件：<project-root>/cuexis.project.json，format: "cuexis.project"
//  load()：定位文件 → 物理 containment 校验 → typed-read ProjectConfig → 解析资产根
//  saveAtomic()：序列化 → 校验 → 独占临时文件写入 → 重新加载验证 → 原子替换
//  便携路径规则：相对路径、正斜杠、禁止 dot segment/Windows 保留名/非法字符

#include <cuexis/project/project_loader.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/uuid.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>
#include <cuexis_internal/portable_path.hpp>

#include "project_validation.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace cuexis::project::project_internal {

// Exclusive temporary creation and atomic replacement for saveAtomic.
[[nodiscard]] inline core::Result<fs::path> saveTarget(const fs::path& locator) {
    if (locator.empty()) {
        return core::unexpected(
            core::Error{"project.locator.empty", "Project locator cannot be empty"});
    }
    std::error_code error;
    const auto status = fs::status(locator, error);
    fs::path target;
    if (!error && fs::is_directory(status)) {
        target = locator / std::string{projectFileName};
    } else if (locator.filename() == fs::path{projectFileName}) {
        target = locator;
    } else {
        return core::unexpected(core::Error{
            "project.locator.invalid",
            "Project locator must be a directory or the exact cuexis.project.json file"});
    }
    const auto parentStatus = fs::status(target.parent_path(), error);
    if (error || !fs::is_directory(parentStatus)) {
        return core::unexpected(
            core::Error{"project.root.invalid", "Project root must be an existing directory"});
    }
    return target;
}

[[nodiscard]] inline fs::path temporarySibling(const fs::path& target, std::uint64_t attempt) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto ticks =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto token = ticks ^ sequence.fetch_add(1, std::memory_order_relaxed) ^ attempt;
    return target.parent_path() /
           (target.filename().native() + fs::path{".tmp." + std::to_string(token)}.native());
}

#if defined(_WIN32)
[[nodiscard]] inline core::Result<void> writeExclusive(const fs::path& file,
                                                       std::string_view text) {
    const HANDLE handle = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::unexpected(
            core::Error{"project.save.temp_create_failed", "Temporary project file creation failed"}
                .withContext("platform_error", std::to_string(GetLastError())));
    }

    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto remaining = std::min<std::size_t>(text.size() - offset, 0x7FFFFFFFU);
        DWORD written = 0;
        if (WriteFile(handle, text.data() + offset, static_cast<DWORD>(remaining), &written,
                      nullptr) == 0 ||
            written == 0) {
            succeeded = false;
            break;
        }
        offset += written;
    }
    if (succeeded && FlushFileBuffers(handle) == 0) {
        succeeded = false;
    }
    const bool closed = CloseHandle(handle) != 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            core::Error{"project.save.temp_write_failed", "Temporary project file write failed"}
                .withContext("platform_error", std::to_string(GetLastError())));
    }
    return {};
}

[[nodiscard]] inline core::Result<void> replaceAtomically(const fs::path& source,
                                                          const fs::path& target) {
    if (MoveFileExW(source.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        return core::unexpected(
            core::Error{"project.save.replace_failed", "Project file atomic replacement failed"}
                .withContext("platform_error", std::to_string(GetLastError())));
    }
    return {};
}
#else
[[nodiscard]] inline core::Result<void> writeExclusive(const fs::path& file,
                                                       std::string_view text) {
    const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) {
        return core::unexpected(
            core::Error{"project.save.temp_create_failed", "Temporary project file creation failed"}
                .withContext("platform_error", std::to_string(errno)));
    }

    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto written = ::write(descriptor, text.data() + offset, text.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            succeeded = false;
            break;
        }
        offset += static_cast<std::size_t>(written);
    }
    if (succeeded && ::fsync(descriptor) != 0) {
        succeeded = false;
    }
    const bool closed = ::close(descriptor) == 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            core::Error{"project.save.temp_write_failed", "Temporary project file write failed"}
                .withContext("platform_error", std::to_string(errno)));
    }
    return {};
}

[[nodiscard]] inline core::Result<void> replaceAtomically(const fs::path& source,
                                                          const fs::path& target) {
    if (::rename(source.c_str(), target.c_str()) != 0) {
        return core::unexpected(
            core::Error{"project.save.replace_failed", "Project file atomic replacement failed"}
                .withContext("platform_error", std::to_string(errno)));
    }
    return {};
}
#endif

} // namespace cuexis::project::project_internal
