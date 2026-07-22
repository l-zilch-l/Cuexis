#pragma once

//  ProjectLoader / ProjectConfigReader — 项目配置加载器
//  ProjectConfigReader：从 JSON 文本 typed-read ProjectConfig（不读取文件系统）
//  ProjectLoader：定位固定文件名、校验物理 containment、规范化路径、生成 PreparedProject
//  saveAtomic：通过临时文件写入并原子替换，失败保留上一有效文件

#include <cuexis/core/result.hpp>
#include <cuexis/project/project_config.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace cuexis::project {

class ProjectConfigReader final {
  public:
    [[nodiscard]] static ProjectConfigResult read(std::string_view jsonText,
                                                  const ProjectLimits& limits = {});
};

class ProjectLoader final {
  public:
    // locator 必须是项目目录或确切的 cuexis.project.json 文件路径
    [[nodiscard]] static ProjectLoadResult load(const std::filesystem::path& locator,
                                                const ProjectLimits& limits = {});

    // 加载内存中的 JSON 文本，同时执行与 load() 相同的物理路径检查
    [[nodiscard]] static ProjectLoadResult loadText(std::string_view jsonText,
                                                    const std::filesystem::path& projectRoot,
                                                    const ProjectLimits& limits = {});

    [[nodiscard]] static core::Result<std::filesystem::path>
    locateProjectFile(const std::filesystem::path& locator);

    // 通过独占创建的临时文件写入，然后原子替换目标文件
    [[nodiscard]] static core::Result<void> saveAtomic(const ProjectConfig& config,
                                                       const std::filesystem::path& locator,
                                                       const ProjectLimits& limits = {});
};

} // namespace cuexis::project
