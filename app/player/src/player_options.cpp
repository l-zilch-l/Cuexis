#include "player_options.hpp"

#include <string>
#include <string_view>

namespace cuexis::player {
namespace {

[[nodiscard]] auto requirePathArgument(int& index, int argumentCount, char** arguments,
                                       std::string_view code, std::string_view message)
    -> core::Result<std::filesystem::path> {
    if (++index >= argumentCount || std::string_view{arguments[index]}.empty() ||
        std::string_view{arguments[index]}.starts_with("--")) {
        return core::unexpected(core::Error{std::string{code}, std::string{message}});
    }
    return std::filesystem::path{arguments[index]};
}

} // namespace

auto parsePlayerOptions(int argumentCount, char** arguments) -> core::Result<PlayerOptions> {
    PlayerOptions options;
    for (int index = 1; index < argumentCount; ++index) {
        const std::string_view argument{arguments[index]};
        if (argument == "--smoke-test") {
            options.smokeTest = true;
            continue;
        }
        if (argument == "--audio-smoke-test") {
            options.audioSmokeTest = true;
            continue;
        }
        if (argument == "--chart") {
            if (options.chartPath.has_value()) {
                return core::unexpected(core::Error{"player.arguments.duplicate_chart",
                                                    "The chart option may only be provided once"});
            }
            auto path = requirePathArgument(index, argumentCount, arguments,
                                            "player.arguments.chart_path_missing",
                                            "The chart option requires a path");
            if (!path) {
                return core::unexpected(std::move(path.error()));
            }
            options.chartPath = std::move(*path);
            continue;
        }
        if (argument == "--project") {
            if (options.projectPath.has_value()) {
                return core::unexpected(
                    core::Error{"player.arguments.duplicate_project",
                                "The project option may only be provided once"});
            }
            auto path = requirePathArgument(index, argumentCount, arguments,
                                            "player.arguments.project_path_missing",
                                            "The project option requires a directory or "
                                            "cuexis.project.json path");
            if (!path) {
                return core::unexpected(std::move(path.error()));
            }
            options.projectPath = std::move(*path);
            continue;
        }
        if (argument == "--shader-cache-dir") {
            if (options.shaderCacheDirectory.has_value()) {
                return core::unexpected(
                    core::Error{"player.arguments.duplicate_shader_cache_dir",
                                "The shader cache directory option may only be provided once"});
            }
            auto path = requirePathArgument(index, argumentCount, arguments,
                                            "player.arguments.shader_cache_dir_missing",
                                            "The shader cache directory option requires a path");
            if (!path) {
                return core::unexpected(std::move(path.error()));
            }
            options.shaderCacheDirectory = std::move(*path);
            continue;
        }
        if (argument == "--frame-stats") {
            if (options.frameStatsPrefix.has_value()) {
                return core::unexpected(
                    core::Error{"player.arguments.duplicate_frame_stats",
                                "The frame stats option may only be provided once"});
            }
            auto path = requirePathArgument(
                index, argumentCount, arguments, "player.arguments.frame_stats_path_missing",
                "The frame stats option requires an artifact path prefix");
            if (!path) {
                return core::unexpected(std::move(path.error()));
            }
            options.frameStatsPrefix = std::move(*path);
            continue;
        }

        return core::unexpected(
            core::Error{"player.arguments.unknown", "Unknown command-line argument"}.withContext(
                "argument", std::string{argument}));
    }
    if (options.chartPath.has_value() && options.projectPath.has_value()) {
        return core::unexpected(
            core::Error{"player.arguments.project_chart_conflict",
                        "The project and chart options are mutually exclusive"});
    }
    if (options.smokeTest && options.audioSmokeTest) {
        return core::unexpected(core::Error{"player.arguments.smoke_test_conflict",
                                            "Smoke test modes are mutually exclusive"});
    }
    return options;
}

} // namespace cuexis::player
