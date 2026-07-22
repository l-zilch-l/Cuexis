#include <cuexis/render_opengl/open_gl_backend.hpp>

#include <catch2/catch_test_macros.hpp>

#include <future>
#include <type_traits>
#include <utility>

namespace {

auto dummyWindowConfig() -> cuexis::platform_sdl::WindowConfig {
    cuexis::platform_sdl::WindowConfig config;
    config.title = "Cuexis OpenGL Contract Test";
    config.width = 320;
    config.height = 240;
    config.resizable = false;
    config.highDpi = false;
    config.openGl = false;
    return config;
}

} // namespace

static_assert(!std::is_default_constructible_v<cuexis::render_opengl::OpenGlContextConfiguration>);
static_assert(!std::is_copy_constructible_v<cuexis::render_opengl::OpenGlContextConfiguration>);
static_assert(std::is_move_constructible_v<cuexis::render_opengl::OpenGlContextConfiguration>);
static_assert(!std::is_invocable_v<decltype(&cuexis::render_opengl::OpenGlBackend::create),
                                   cuexis::platform_sdl::SdlWindow&,
                                   const cuexis::render_opengl::OpenGlConfig&>);
static_assert(std::is_invocable_v<decltype(&cuexis::render_opengl::OpenGlBackend::create),
                                  cuexis::platform_sdl::SdlWindow&,
                                  cuexis::render_opengl::OpenGlContextConfiguration&&>);
static_assert(
    std::is_same_v<decltype(std::declval<cuexis::render_opengl::OpenGlBackend&>().close()),
                   cuexis::core::Result<void>>);

TEST_CASE("OpenGL configuration rejects invalid version components", "[render][opengl]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    cuexis::render_opengl::OpenGlConfig config;
    config.majorVersion = 0;

    const auto invalidMajor =
        cuexis::render_opengl::configureOpenGlContext(runtime.value(), config);
    REQUIRE_FALSE(invalidMajor.has_value());
    REQUIRE(invalidMajor.error().code() == "render.opengl.invalid_config");

    config.majorVersion = 3;
    config.minorVersion = -1;
    const auto invalidMinor =
        cuexis::render_opengl::configureOpenGlContext(runtime.value(), config);
    REQUIRE_FALSE(invalidMinor.has_value());
    REQUIRE(invalidMinor.error().code() == "render.opengl.invalid_config");
}

TEST_CASE("OpenGL configuration enforces the Core Profile version boundary", "[render][opengl]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    cuexis::render_opengl::OpenGlConfig config;
    config.majorVersion = 3;
    config.minorVersion = 1;
    const auto belowMinimum =
        cuexis::render_opengl::configureOpenGlContext(runtime.value(), config);
    REQUIRE_FALSE(belowMinimum.has_value());
    CHECK(belowMinimum.error().code() == "render.opengl.invalid_config");

    config.minorVersion = 2;
    const auto minimum = cuexis::render_opengl::configureOpenGlContext(runtime.value(), config);
    REQUIRE(minimum.has_value());
}

TEST_CASE("OpenGL configuration produces a backend creation token", "[render][opengl]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    const auto configured = cuexis::render_opengl::configureOpenGlContext(runtime.value(), {});
    REQUIRE(configured.has_value());
}

TEST_CASE("OpenGL configuration rejects a worker thread", "[render][opengl][thread]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    auto worker = std::async(std::launch::async, [&runtime] {
        return cuexis::render_opengl::configureOpenGlContext(runtime.value(), {});
    });
    const auto result = worker.get();

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "render.opengl.not_main_thread");
}

TEST_CASE("OpenGL backend creation rejects a worker thread before accessing the window",
          "[render][opengl][thread]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    auto configured = cuexis::render_opengl::configureOpenGlContext(runtime.value(), {});
    REQUIRE(configured.has_value());

    auto window = cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
    REQUIRE(window.has_value());

    auto worker = std::async(std::launch::async,
                             [&window, configuration = std::move(configured).value()]() mutable {
                                 return cuexis::render_opengl::OpenGlBackend::create(
                                     window.value(), std::move(configuration));
                             });
    const auto result = worker.get();

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "render.opengl.not_main_thread");
}

TEST_CASE("Moving an OpenGL configuration token clears its source", "[render][opengl]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    auto configured = cuexis::render_opengl::configureOpenGlContext(runtime.value(), {});
    REQUIRE(configured.has_value());
    auto original = std::move(configured).value();
    auto transferred = std::move(original);

    auto window = cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
    REQUIRE(window.has_value());

    const auto unavailable =
        cuexis::render_opengl::OpenGlBackend::create(window.value(), std::move(original));
    REQUIRE_FALSE(unavailable.has_value());
    CHECK(unavailable.error().code() == "render.opengl.configuration_unavailable");

    static_cast<void>(transferred);
}

TEST_CASE("A newer OpenGL configuration makes an older token stale", "[render][opengl]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    auto older = cuexis::render_opengl::configureOpenGlContext(runtime.value(), {});
    REQUIRE(older.has_value());

    cuexis::render_opengl::OpenGlConfig newerConfig;
    newerConfig.minorVersion = 2;
    const auto newer = cuexis::render_opengl::configureOpenGlContext(runtime.value(), newerConfig);
    REQUIRE(newer.has_value());

    auto window = cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
    REQUIRE(window.has_value());

    const auto stale =
        cuexis::render_opengl::OpenGlBackend::create(window.value(), std::move(older).value());
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code() == "render.opengl.configuration_stale");
}

TEST_CASE("A failed OpenGL configuration invalidates an older token", "[render][opengl]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    auto older = cuexis::render_opengl::configureOpenGlContext(runtime.value(), {});
    REQUIRE(older.has_value());

    cuexis::render_opengl::OpenGlConfig invalidConfig;
    invalidConfig.minorVersion = -1;
    const auto failed =
        cuexis::render_opengl::configureOpenGlContext(runtime.value(), invalidConfig);
    REQUIRE_FALSE(failed.has_value());

    auto window = cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
    REQUIRE(window.has_value());

    const auto stale =
        cuexis::render_opengl::OpenGlBackend::create(window.value(), std::move(older).value());
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code() == "render.opengl.configuration_stale");
}
