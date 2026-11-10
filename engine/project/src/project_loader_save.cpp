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

#include "project_loader_read_helpers.hpp"

#include "project_loader_write_helpers.hpp"

namespace cuexis::project {

using namespace project_internal;

core::Result<void> ProjectLoader::saveAtomic(const ProjectConfig& config, const fs::path& locator,
                                             const ProjectLimits& limits) {
    auto serialized = serializeProjectConfig(config, limits);
    if (!serialized) {
        return core::unexpected(serialized.error());
    }
    auto validated = ProjectConfigReader::read(*serialized, limits);
    if (!validated.hasValue()) {
        return core::unexpected(invalidConfigError(validated.diagnostics,
                                                   "project.save.config_invalid",
                                                   "ProjectConfig is invalid and was not saved"));
    }
    serialized = serializeProjectConfig(*validated.config, limits);
    if (!serialized) {
        return core::unexpected(serialized.error());
    }

    const auto target = saveTarget(locator);
    if (!target) {
        return core::unexpected(target.error());
    }

    fs::path temporary;
    core::Result<void> written = core::unexpected(
        core::Error{"project.save.temp_create_failed", "Temporary project file creation failed"});
    constexpr std::uint64_t maxAttempts = 128;
    for (std::uint64_t attempt = 0; attempt < maxAttempts; ++attempt) {
        temporary = temporarySibling(*target, attempt);
        written = writeExclusive(temporary, *serialized);
        if (written) {
            break;
        }
        std::error_code ignored;
        fs::remove(temporary, ignored);
    }
    if (!written) {
        return core::unexpected(written.error());
    }

    const auto cleanup = [&temporary]() {
        std::error_code ignored;
        fs::remove(temporary, ignored);
    };
    const auto reloadedText = readBoundedFile(temporary, limits.maxInputBytes);
    if (!reloadedText) {
        cleanup();
        return core::unexpected(reloadedText.error());
    }
    auto reloaded = ProjectConfigReader::read(*reloadedText, limits);
    if (!reloaded.hasValue() || *reloaded.config != *validated.config) {
        const auto error =
            invalidConfigError(reloaded.diagnostics, "project.save.reload_failed",
                               "Temporary ProjectConfig failed full reload validation");
        cleanup();
        return core::unexpected(error);
    }
    auto prepared = prepareProject(*reloaded.config, *target, target->parent_path(),
                                   std::move(reloaded.diagnostics), limits);
    if (!prepared.hasValue()) {
        const auto error =
            invalidConfigError(prepared.diagnostics, "project.save.reload_failed",
                               "Temporary ProjectConfig failed full reload validation");
        cleanup();
        return core::unexpected(error);
    }

    auto replaced = replaceAtomically(temporary, *target);
    if (!replaced) {
        cleanup();
        return core::unexpected(replaced.error());
    }
    return {};
}

} // namespace cuexis::project
