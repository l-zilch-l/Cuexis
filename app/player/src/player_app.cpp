//  Cuexis Player 实现 — 应用组合层
//  PlaybackSession（cuexis_playback SDK 门面）负责 Chart 加载/编译和会话生命周期
//  宿主窗口/渲染后端由 Player 自行创建管理（SDL + OpenGL）
//  每帧只消费 PlaybackSession 的拥有型 FrameSnapshot，再转换为 RenderScene。
//  NullClock/NullInput/NullJudge 为阶段 1A/1B 占位

#include "player_app.hpp"

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/log.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/project/asset_index_reader.hpp>
#include <cuexis/project/project_loader.hpp>
#include <cuexis/render/render_backend.hpp>
#include <cuexis/render/render_scene.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>
#include <cuexis/version.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::player {
namespace {

constexpr std::uint32_t smokeTestFrameCount = 3;
constexpr std::size_t chartInputMaxBytes = 16U * 1024U * 1024U;
constexpr std::string_view defaultProjectDirectory = "stage1c_project";

struct PlayerOptions final {
    bool smokeTest{};
    std::optional<std::filesystem::path> chartPath;
    std::optional<std::filesystem::path> projectPath;
};

struct NullInputSource final {
    void poll() const noexcept {}
};

struct NullJudgeSystem final {
    void update(double) const noexcept {}
};

class PlayerClock final {
  public:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] auto nextFrame(bool deterministic, std::uint32_t frameIndex)
        -> playback::RuntimeFrame {
        if (deterministic) {
            constexpr double frameStepMs = 500.0;
            return playback::RuntimeFrame{.chartTimeMs = frameIndex * frameStepMs,
                                          .simulationDeltaTimeMs =
                                              frameIndex == 0 ? 0.0 : frameStepMs,
                                          .timeDiscontinuityId = 0};
        }

        const auto now = Clock::now();
        const double chartTimeMs =
            std::chrono::duration<double, std::milli>(now - started_).count();
        const double deltaTimeMs =
            frameIndex == 0 ? 0.0
                            : std::chrono::duration<double, std::milli>(now - previous_).count();
        previous_ = now;
        return playback::RuntimeFrame{.chartTimeMs = chartTimeMs,
                                      .simulationDeltaTimeMs = std::max(0.0, deltaTimeMs),
                                      .timeDiscontinuityId = 0};
    }

  private:
    const Clock::time_point started_{Clock::now()};
    Clock::time_point previous_{started_};
};

[[nodiscard]] auto parseOptions(int argumentCount, char** arguments)
    -> core::Result<PlayerOptions> {
    PlayerOptions options;
    for (int index = 1; index < argumentCount; ++index) {
        const std::string_view argument{arguments[index]};
        if (argument == "--smoke-test") {
            options.smokeTest = true;
            continue;
        }
        if (argument == "--chart") {
            if (options.chartPath.has_value()) {
                return core::unexpected(core::Error{"player.arguments.duplicate_chart",
                                                    "The chart option may only be provided once"});
            }
            if (++index >= argumentCount || std::string_view{arguments[index]}.empty() ||
                std::string_view{arguments[index]}.starts_with("--")) {
                return core::unexpected(core::Error{"player.arguments.chart_path_missing",
                                                    "The chart option requires a path"});
            }
            options.chartPath = std::filesystem::path{arguments[index]};
            continue;
        }
        if (argument == "--project") {
            if (options.projectPath.has_value()) {
                return core::unexpected(
                    core::Error{"player.arguments.duplicate_project",
                                "The project option may only be provided once"});
            }
            if (++index >= argumentCount || std::string_view{arguments[index]}.empty() ||
                std::string_view{arguments[index]}.starts_with("--")) {
                return core::unexpected(core::Error{
                    "player.arguments.project_path_missing",
                    "The project option requires a directory or cuexis.project.json path"});
            }
            options.projectPath = std::filesystem::path{arguments[index]};
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
    return options;
}

[[nodiscard]] auto defaultProjectPath() -> core::Result<std::filesystem::path> {
    auto basePath = platform_sdl::executableBasePath();
    if (!basePath) {
        return core::unexpected(std::move(basePath.error()));
    }
    return *basePath / "assets" / "projects" / defaultProjectDirectory;
}

[[nodiscard]] auto readBoundedFile(const std::filesystem::path& path, std::size_t maxBytes,
                                   std::string_view errorPrefix, std::string_view description)
    -> core::Result<std::string> {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return core::unexpected(
            core::Error{std::string{errorPrefix} + ".open_failed",
                        "Could not inspect the requested " + std::string{description} + " file"}
                .withContext("file", path.filename().string()));
    }
    if (size > maxBytes) {
        return core::unexpected(core::Error{std::string{errorPrefix} + ".file_too_large",
                                            "The requested " + std::string{description} +
                                                " exceeds the input size limit"}
                                    .withContext("size_bytes", std::to_string(size))
                                    .withContext("limit_bytes", std::to_string(maxBytes)));
    }

    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return core::unexpected(
            core::Error{std::string{errorPrefix} + ".open_failed",
                        "Could not open the requested " + std::string{description} + " file"}
                .withContext("file", path.filename().string()));
    }

    std::string text(static_cast<std::size_t>(size), '\0');
    if (!text.empty()) {
        stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    }
    if (stream.gcount() != static_cast<std::streamsize>(text.size())) {
        return core::unexpected(
            core::Error{std::string{errorPrefix} + ".read_failed",
                        "Could not read the complete " + std::string{description} + " file"}
                .withContext("file", path.filename().string()));
    }
    return text;
}

[[nodiscard]] auto describeDiagnostic(const core::Diagnostic& diagnostic) -> std::string {
    std::ostringstream output;
    output << diagnostic.code() << ": " << diagnostic.message();
    if (!diagnostic.fieldPath().empty()) {
        output << " [path=" << diagnostic.fieldPath() << ']';
    }
    for (const auto& item : diagnostic.context()) {
        output << " [" << item.key << '=' << item.value << ']';
    }
    return output.str();
}

void logDiagnostics(std::string_view category, core::Diagnostics& diagnostics) {
    diagnostics.sortDeterministically();
    for (const auto& diagnostic : diagnostics.items()) {
        const auto description = describeDiagnostic(diagnostic);
        switch (diagnostic.severity()) {
        case core::DiagnosticSeverity::Info:
            core::log::info(category, description);
            break;
        case core::DiagnosticSeverity::Warning:
            core::log::warn(category, description);
            break;
        case core::DiagnosticSeverity::Error:
            core::log::error(category, description);
            break;
        }
    }
}

[[nodiscard]] auto diagnosticsError(std::string code, std::string message,
                                    const core::Diagnostics& diagnostics) -> core::Error {
    core::Error error{std::move(code), std::move(message)};
    if (!diagnostics.items().empty()) {
        const auto& first = diagnostics.items().front();
        error.withContext("diagnostic_code", std::string{first.code()});
        if (!first.fieldPath().empty()) {
            error.withContext("field_path", std::string{first.fieldPath()});
        }
    }
    return error;
}

[[nodiscard]] auto toAssetType(project::AssetType type) noexcept -> assets::AssetType {
    switch (type) {
    case project::AssetType::Mesh:
        return assets::AssetType::Mesh;
    case project::AssetType::Material:
        return assets::AssetType::Material;
    case project::AssetType::Texture:
        return assets::AssetType::Texture;
    }
    return assets::AssetType::Mesh;
}

[[nodiscard]] auto buildAssetDatabase(const project::PreparedProject& preparedProject)
    -> core::Result<assets::AssetDatabase> {
    assets::AssetDatabaseInput input;
    input.roots.reserve(preparedProject.assetRoots.size());

    for (const auto& root : preparedProject.assetRoots) {
        const project::AssetIndexLimits limits;
        auto text = readBoundedFile(root.assetIndexFile, limits.maxInputBytes, "player.asset_index",
                                    "asset index");
        if (!text) {
            return core::unexpected(
                std::move(text.error()).withContext("root_id", root.declaration.id));
        }

        auto parsed = project::AssetIndexReader::read(*text, limits);
        logDiagnostics("player.asset_index", parsed.diagnostics);
        if (!parsed.hasValue()) {
            return core::unexpected(diagnosticsError("player.asset_index.load_failed",
                                                     "Asset Index loading produced errors",
                                                     parsed.diagnostics));
        }

        assets::AssetRootIndex converted;
        converted.root = {.id = root.declaration.id, .path = root.absolutePath};
        converted.index.format = parsed.document->format;
        converted.index.version = parsed.document->version;
        converted.index.assets.reserve(parsed.document->assets.size());
        for (const auto& record : parsed.document->assets) {
            assets::AssetRecord asset{
                .id = {record.id},
                .type = toAssetType(record.type),
                .source = record.source,
                .dependencies = {},
            };
            asset.dependencies.reserve(record.dependencies.size());
            for (const auto& dependency : record.dependencies) {
                asset.dependencies.push_back(assets::AssetId{dependency});
            }
            converted.index.assets.push_back(std::move(asset));
        }
        input.roots.push_back(std::move(converted));
    }

    auto built = assets::AssetDatabase::build(input);
    logDiagnostics("player.asset_database", built.diagnostics);
    if (!built.hasValue()) {
        return core::unexpected(diagnosticsError("player.asset_database.build_failed",
                                                 "AssetDatabase construction produced errors",
                                                 built.diagnostics));
    }
    return std::move(*built.database);
}

[[nodiscard]] auto matrixFrom(const float (&values)[16]) noexcept -> core::Mat4 {
    core::Mat4 matrix;
    std::copy(std::begin(values), std::end(values), matrix.values.begin());
    return matrix;
}

[[nodiscard]] auto appendSnapshotAxes(const playback::FrameSnapshot& snapshot,
                                      render::RenderScene& scene) -> core::Result<void> {
    constexpr float axisLength = 0.15F;
    for (const auto& object : snapshot.objects) {
        if (!object.visible) {
            continue;
        }
        const auto matrix = matrixFrom(object.worldMatrix);
        if (!core::isFinite(matrix)) {
            return core::unexpected(
                core::Error{"player.snapshot.matrix_invalid", "Snapshot matrix is not finite"}
                    .withContext("object_id", object.id));
        }

        const core::Vec3 origin = core::transformPoint(matrix, {});
        const core::Vec3 xEnd = core::transformPoint(matrix, {axisLength, 0.0F, 0.0F});
        const core::Vec3 yEnd = core::transformPoint(matrix, {0.0F, axisLength, 0.0F});
        const core::Vec3 zEnd = core::transformPoint(matrix, {0.0F, 0.0F, axisLength});
        if (auto result = scene.addDebugLine(origin, xEnd, {1.0F, 0.2F, 0.2F, 1.0F}); !result) {
            return result;
        }
        if (auto result = scene.addDebugLine(origin, yEnd, {0.2F, 1.0F, 0.2F, 1.0F}); !result) {
            return result;
        }
        if (auto result = scene.addDebugLine(origin, zEnd, {0.2F, 0.45F, 1.0F, 1.0F}); !result) {
            return result;
        }
    }
    return {};
}

[[nodiscard]] auto viewProjectionFrom(const playback::FrameSnapshot& snapshot) noexcept
    -> core::Mat4 {
    if (!snapshot.camera.active) {
        return {};
    }
    return core::multiply(matrixFrom(snapshot.camera.projectionMatrix),
                          matrixFrom(snapshot.camera.viewMatrix));
}

} // namespace

auto run(int argumentCount, char** arguments) -> core::Result<void> {
    auto optionsResult = parseOptions(argumentCount, arguments);
    if (!optionsResult) {
        return core::unexpected(std::move(optionsResult.error()));
    }
    auto options = std::move(optionsResult).value();

    if (!options.chartPath && !options.projectPath) {
        auto pathResult = defaultProjectPath();
        if (!pathResult) {
            return core::unexpected(std::move(pathResult.error()));
        }
        options.projectPath = std::move(pathResult).value();
    }

    core::log::info("player.startup",
                    std::string{"Starting Cuexis Player "} + std::string{version::display});

    std::optional<project::PreparedProject> preparedProject;
    std::filesystem::path chartPath;
    if (options.projectPath.has_value()) {
        auto loadedProject = project::ProjectLoader::load(*options.projectPath);
        logDiagnostics("player.project", loadedProject.diagnostics);
        if (!loadedProject.hasValue()) {
            return core::unexpected(diagnosticsError("player.project.load_failed",
                                                     "ProjectConfig loading produced errors",
                                                     loadedProject.diagnostics));
        }
        preparedProject = std::move(*loadedProject.project);
        core::log::info("player.project", std::string{"Format: "} + preparedProject->config.format +
                                              " v" +
                                              std::to_string(preparedProject->config.version));
        core::log::info("player.project", std::string{"Asset roots: "} +
                                              std::to_string(preparedProject->assetRoots.size()));

        auto database = buildAssetDatabase(*preparedProject);
        if (!database) {
            return core::unexpected(std::move(database.error()));
        }
        core::log::info("player.asset_database",
                        std::string{"Indexed assets: "} + std::to_string(database->size()));
        chartPath = preparedProject->chartFile;
    } else {
        std::error_code error;
        if (!std::filesystem::is_regular_file(*options.chartPath, error) || error) {
            return core::unexpected(
                core::Error{"player.chart.open_failed",
                            "Could not inspect the requested chart file"}
                    .withContext("file", options.chartPath->filename().string()));
        }
        chartPath = *options.chartPath;
    }

    auto chartText = readBoundedFile(chartPath, chartInputMaxBytes, "player.chart", "chart");
    if (!chartText) {
        return core::unexpected(std::move(chartText.error()));
    }

    playback::PlaybackSession playbackSession;
    if (auto result = playbackSession.loadChart(*chartText); !result) {
        return core::unexpected(std::move(result.error()));
    }
    auto chartInfo = playbackSession.chartInfo();
    if (!chartInfo) {
        return core::unexpected(std::move(chartInfo.error()));
    }
    if (chartInfo->objectCount == 0) {
        return core::unexpected(
            core::Error{"player.chart.empty", "The committed chart contains no objects"});
    }
    core::log::info("player.playback",
                    std::string{"Prepared objects: "} + std::to_string(chartInfo->objectCount) +
                        ", behaviors: " + std::to_string(chartInfo->behaviorCount));

    auto runtimeResult = platform_sdl::SdlRuntime::create();
    if (!runtimeResult) {
        return core::unexpected(
            std::move(runtimeResult.error()).withContext("operation", "initialize_sdl"));
    }
    auto sdlRuntime = std::move(runtimeResult).value();

    const auto videoDriver = sdlRuntime.videoDriver();
    core::log::info("player.sdl",
                    std::string{"Video driver: "} +
                        (videoDriver.empty() ? std::string{"unknown"} : std::string{videoDriver}));

    const render_opengl::OpenGlConfig openGlConfig{};
    auto configureResult = render_opengl::configureOpenGlContext(sdlRuntime, openGlConfig);
    if (!configureResult) {
        return core::unexpected(
            std::move(configureResult.error()).withContext("operation", "configure_opengl"));
    }

    platform_sdl::WindowConfig windowConfig{};
    windowConfig.title = std::string{"Cuexis Player "} + std::string{version::display};
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.resizable = true;
    windowConfig.highDpi = true;
    windowConfig.openGl = true;

    auto windowResult = platform_sdl::SdlWindow::create(sdlRuntime, windowConfig);
    if (!windowResult) {
        return core::unexpected(
            std::move(windowResult.error()).withContext("operation", "create_player_window"));
    }
    auto window = std::move(windowResult).value();

    auto backendResult =
        render_opengl::OpenGlBackend::create(window, std::move(configureResult).value());
    if (!backendResult) {
        return core::unexpected(
            std::move(backendResult.error()).withContext("operation", "create_opengl_backend"));
    }
    auto backend = std::move(backendResult).value();

    const auto& openGlInfo = backend.info();
    core::log::info("player.opengl", std::string{"Version: "} + openGlInfo.version);
    core::log::info("player.opengl", std::string{"Vendor: "} + openGlInfo.vendor);
    core::log::info("player.opengl", std::string{"Renderer: "} + openGlInfo.renderer);

    PlayerClock clock;
    const NullInputSource inputSource;
    const NullJudgeSystem judgeSystem;
    std::uint32_t renderedFrames = 0;
    bool quitRequested = false;
    while (!quitRequested) {
        quitRequested = window.pollEvents().quitRequested;
        if (quitRequested) {
            break;
        }

        inputSource.poll();
        const auto runtimeFrame = clock.nextFrame(options.smokeTest, renderedFrames);
        judgeSystem.update(runtimeFrame.chartTimeMs);
        if (auto result = playbackSession.update(runtimeFrame); !result) {
            return core::unexpected(std::move(result.error()));
        }

        auto drawableSizeResult = window.drawableSize();
        if (!drawableSizeResult) {
            return core::unexpected(std::move(drawableSizeResult.error())
                                        .withContext("operation", "query_drawable_size"));
        }
        const auto drawableSize = *drawableSizeResult;
        const auto width = static_cast<std::uint32_t>(std::max(drawableSize.width, 1));
        const auto height = static_cast<std::uint32_t>(std::max(drawableSize.height, 1));
        auto snapshot = playbackSession.extractFrame({.width = width, .height = height});
        if (!snapshot) {
            return core::unexpected(std::move(snapshot.error()));
        }

        render::RenderScene scene;
        if (auto result = appendSnapshotAxes(*snapshot, scene); !result) {
            return core::unexpected(std::move(result.error()));
        }
        if (scene.empty()) {
            return core::unexpected(core::Error{
                "player.render_scene.empty", "The frame snapshot did not produce debug geometry"});
        }
        if (renderedFrames == 0) {
            core::log::info("player.snapshot",
                            std::string{"Objects: "} + std::to_string(snapshot->objects.size()) +
                                ", debug commands: " + std::to_string(scene.size()));
        }

        const render::RenderFrame renderOutput{
            .extent = {.width = width, .height = height},
            .clearColor = {.red = snapshot->clearRed,
                           .green = snapshot->clearGreen,
                           .blue = snapshot->clearBlue,
                           .alpha = snapshot->clearAlpha},
            .viewProjection = viewProjectionFrom(*snapshot),
            .scene = &scene,
        };

        if (auto result = backend.renderFrame(renderOutput); !result) {
            return core::unexpected(
                std::move(result.error()).withContext("frame", std::to_string(renderedFrames)));
        }

        ++renderedFrames;
        if (options.smokeTest && renderedFrames >= smokeTestFrameCount) {
            quitRequested = true;
        }
    }

    if (options.smokeTest && renderedFrames != smokeTestFrameCount) {
        return core::unexpected(
            core::Error{"player.smoke_test.incomplete",
                        "Smoke test ended before rendering required frames"}
                .withContext("expected_frames", std::to_string(smokeTestFrameCount))
                .withContext("rendered_frames", std::to_string(renderedFrames)));
    }
    if (options.smokeTest) {
        core::log::info("player.smoke_test",
                        std::string{"Completed frames: "} + std::to_string(renderedFrames));
    }

    if (auto result = playbackSession.unload(); !result) {
        return core::unexpected(std::move(result.error()));
    }
    return {};
}

} // namespace cuexis::player
