#pragma once

// ProjectConfigReader reads typed ProjectConfig data from JSON without filesystem access.
// ProjectLoader locates the fixed file, checks containment, and produces PreparedProject paths.
// saveAtomic writes through a temporary file and preserves the prior file on failure.

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
    // locator must be a project directory or the exact cuexis.project.json path.
    [[nodiscard]] static ProjectLoadResult load(const std::filesystem::path& locator,
                                                const ProjectLimits& limits = {});

    // Loads in-memory JSON while applying the same physical path checks as load().
    [[nodiscard]] static ProjectLoadResult loadText(std::string_view jsonText,
                                                    const std::filesystem::path& projectRoot,
                                                    const ProjectLimits& limits = {});

    [[nodiscard]] static core::Result<std::filesystem::path>
    locateProjectFile(const std::filesystem::path& locator);

    // Writes through an exclusively created temporary file, then atomically replaces the target.
    [[nodiscard]] static core::Result<void> saveAtomic(const ProjectConfig& config,
                                                       const std::filesystem::path& locator,
                                                       const ProjectLimits& limits = {});
};

} // namespace cuexis::project
