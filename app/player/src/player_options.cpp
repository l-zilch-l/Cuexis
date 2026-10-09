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
        if (argument == "--mode") {
            if (options.clock.has_value()) {
                return core::unexpected(core::Error{"player.arguments.duplicate_mode",
                                                    "The mode option may only be provided once"});
            }
            if (++index >= argumentCount || std::string_view{arguments[index]}.empty() ||
                std::string_view{arguments[index]}.starts_with("--")) {
                return core::unexpected(core::Error{"player.arguments.mode_value_missing",
                                                    "The mode option requires a value"});
            }
            const std::string_view value{arguments[index]};
            if (value == "chart") {
                options.clock = PlayerClockOption::Chart;
            } else if (value == "host") {
                options.clock = PlayerClockOption::Host;
            } else if (value == "audio") {
                options.clock = PlayerClockOption::Audio;
            } else {
                return core::unexpected(core::Error{"player.arguments.mode_unknown",
                                                    "The mode option accepts chart, host, or audio"}
                                            .withContext("value", std::string{value}));
            }
            continue;
        }
        if (argument == "--gameplay-configuration" || argument == "--gameplay-config-budget" ||
            argument == "--gameplay-h-step" || argument == "--gameplay-presentation-step" ||
            argument == "--gameplay-key" || argument == "--gameplay-guide") {
#if !defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
            return core::unexpected(core::Error{"player.candidate.disabled",
                                                "Gameplay requires the candidate SDK flavor"});
#else
            auto value =
                requirePathArgument(index, argumentCount, arguments, "player.arguments.unknown",
                                    "Gameplay option requires a value");
            if (!value)
                return core::unexpected(std::move(value.error()));
            const auto text = value->generic_string();
            if (argument == "--gameplay-key") {
                options.gameplayKeys.push_back(text);
            } else if (argument == "--gameplay-configuration") {
                if (options.gameplayConfiguration)
                    return core::unexpected(
                        core::Error{"player.arguments.unknown", "Duplicate Gameplay option"});
                options.gameplayConfiguration = *value;
            } else if (argument == "--gameplay-guide") {
                if (options.gameplayGuide)
                    return core::unexpected(
                        core::Error{"player.arguments.unknown", "Duplicate Gameplay guide"});
                options.gameplayGuide = *value;
            } else {
                auto& field = argument == "--gameplay-config-budget" ? options.gameplayBudget
                              : argument == "--gameplay-h-step"      ? options.gameplayHStep
                                                                     : options.gameplayTStep;
                if (field)
                    return core::unexpected(
                        core::Error{"player.arguments.unknown", "Duplicate Gameplay option"});
                field = text;
            }
            continue;
#endif
        }
        if (argument == "--candidate-entry") {
#ifndef CUEXIS_EXPERIMENTAL_BUILD
            return core::unexpected(core::Error{"player.candidate.disabled",
                                                "Candidate entry requires an experimental build"});
#else
            if (options.candidateEntry) {
                return core::unexpected(core::Error{"player.arguments.duplicate_candidate_entry",
                                                    "Candidate entry may only be provided once"});
            }
            auto path = requirePathArgument(index, argumentCount, arguments,
                                            "player.arguments.candidate_entry_missing",
                                            "Candidate entry requires a relative entry path");
            if (!path) {
                return core::unexpected(std::move(path.error()));
            }
            options.candidateEntry = path->generic_string();
            continue;
#endif
        }
        if (argument == "--cxc") {
            if (options.cxcPath) {
                return core::unexpected(core::Error{"player.arguments.duplicate_cxc",
                                                    "CXC locator may only be provided once"});
            }
            auto path = requirePathArgument(index, argumentCount, arguments,
                                            "player.arguments.cxc_path_missing",
                                            "CXC requires a package path");
            if (!path) {
                return core::unexpected(std::move(path.error()));
            }
            options.cxcPath = std::move(*path);
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
    if (options.cxcPath && (options.chartPath || options.projectPath)) {
        return core::unexpected(core::Error{"player.arguments.source_conflict",
                                            "CXC, project and chart locators are exclusive"});
    }
    if (options.candidateEntry && (!options.projectPath && !options.cxcPath)) {
        return core::unexpected(core::Error{"player.arguments.candidate_source_required",
                                            "Candidate entry requires project or CXC locator"});
    }
    const bool gameplay = options.gameplayGuide || options.gameplayConfiguration ||
                          options.gameplayBudget || options.gameplayHStep ||
                          options.gameplayTStep || !options.gameplayKeys.empty();
    if (gameplay && (!options.candidateEntry || !options.gameplayConfiguration ||
                     !options.gameplayBudget || !options.gameplayHStep || !options.gameplayTStep))
        return core::unexpected(
            core::Error{"player.arguments.unknown",
                        "Gameplay requires explicit entry, configuration, budget and H/T steps"});
    if (gameplay && (options.smokeTest || options.audioSmokeTest))
        return core::unexpected(core::Error{"player.arguments.unknown",
                                            "Legacy smoke clock is unavailable for Gameplay"});
    if (options.smokeTest && options.audioSmokeTest) {
        return core::unexpected(core::Error{"player.arguments.smoke_test_conflict",
                                            "Smoke test modes are mutually exclusive"});
    }
    return options;
}

} // namespace cuexis::player
