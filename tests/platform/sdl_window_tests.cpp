#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <future>
#include <optional>
#include <type_traits>
#include <utility>

namespace {

cuexis::platform_sdl::WindowConfig dummyWindowConfig() {
    cuexis::platform_sdl::WindowConfig config;
    config.title = "Cuexis Platform Test";
    config.width = 320;
    config.height = 240;
    config.resizable = false;
    config.highDpi = false;
    config.openGl = false;
    return config;
}

} // namespace

static_assert(std::is_copy_constructible_v<cuexis::platform_sdl::SdlWindowLease>);
static_assert(std::is_copy_assignable_v<cuexis::platform_sdl::SdlWindowLease>);

TEST_CASE("Executable base path is independent of the process working directory",
          "[platform][filesystem]") {
    const auto basePath = cuexis::platform_sdl::executableBasePath();
    REQUIRE(basePath.has_value());
    std::error_code error;
    CHECK(std::filesystem::is_directory(*basePath, error));
    CHECK_FALSE(error);
}

TEST_CASE("Window rejects invalid configuration before calling SDL", "[platform][window]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    auto config = dummyWindowConfig();
    config.title.clear();
    const auto emptyTitle = cuexis::platform_sdl::SdlWindow::create(runtime.value(), config);
    REQUIRE_FALSE(emptyTitle.has_value());
    REQUIRE(emptyTitle.error().code() == "platform.sdl.invalid_config");

    config = dummyWindowConfig();
    config.width = 0;
    const auto zeroWidth = cuexis::platform_sdl::SdlWindow::create(runtime.value(), config);
    REQUIRE_FALSE(zeroWidth.has_value());
    REQUIRE(zeroWidth.error().code() == "platform.sdl.invalid_config");

    config = dummyWindowConfig();
    config.height = -1;
    const auto negativeHeight = cuexis::platform_sdl::SdlWindow::create(runtime.value(), config);
    REQUIRE_FALSE(negativeHeight.has_value());
    REQUIRE(negativeHeight.error().code() == "platform.sdl.invalid_config");
}

TEST_CASE("Window keeps SDL alive after the runtime wrapper is destroyed", "[platform][window]") {
    std::optional<cuexis::platform_sdl::SdlWindow> window;
    {
        auto runtime = cuexis::platform_sdl::SdlRuntime::create();
        REQUIRE(runtime.has_value());

        auto created =
            cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
        REQUIRE(created.has_value());
        window.emplace(std::move(created).value());
    }

    const auto size = window->drawableSize();
    REQUIRE(size.has_value());
    CHECK(size->width > 0);
    CHECK(size->height > 0);
}

TEST_CASE("Runtime creation rejects a worker thread", "[platform][runtime]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    auto workerResult =
        std::async(std::launch::async, [] { return cuexis::platform_sdl::SdlRuntime::create(); });
    const auto result = workerResult.get();

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "platform.sdl.not_main_thread");
}

TEST_CASE("Moved-from SDL wrappers reject operations without invalidating their replacements",
          "[platform][runtime][window][move]") {
    auto runtimeResult = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtimeResult.has_value());
    auto runtime = std::move(runtimeResult).value();
    auto replacementRuntime = std::move(runtime);

    const auto missingRuntime = cuexis::platform_sdl::SdlWindow::create(runtime, dummyWindowConfig());
    REQUIRE_FALSE(missingRuntime.has_value());
    CHECK(missingRuntime.error().code() == "platform.sdl.runtime_unavailable");

    auto created =
        cuexis::platform_sdl::SdlWindow::create(replacementRuntime, dummyWindowConfig());
    REQUIRE(created.has_value());
    auto window = std::move(created).value();
    auto replacementWindow = std::move(window);

    const auto missingWindow = window.drawableSize();
    REQUIRE_FALSE(missingWindow.has_value());
    CHECK(missingWindow.error().code() == "platform.sdl.window_size_failed");
    CHECK_FALSE(window.pollEvents().quitRequested);

    const auto replacementSize = replacementWindow.drawableSize();
    REQUIRE(replacementSize.has_value());
    CHECK(replacementSize->width > 0);
    CHECK(replacementSize->height > 0);
}

TEST_CASE("Stage 0 rejects a second active window", "[platform][window]") {
    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());

    auto first = cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
    REQUIRE(first.has_value());

    const auto second =
        cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code() == "platform.sdl.window_already_active");
}

TEST_CASE("A copied window lease outlives the window wrapper", "[platform][window][lease]") {
    cuexis::platform_sdl::SdlWindowLease lease;
    {
        auto runtime = cuexis::platform_sdl::SdlRuntime::create();
        REQUIRE(runtime.has_value());

        auto created =
            cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
        REQUIRE(created.has_value());

        auto window = std::move(created).value();
        lease = window.lease();
        REQUIRE(lease.valid());
        REQUIRE(lease.nativeHandle() != nullptr);
    }

    auto copiedLease = lease;
    lease = {};
    CHECK(copiedLease.valid());
    CHECK(copiedLease.nativeHandle() != nullptr);

    auto runtime = cuexis::platform_sdl::SdlRuntime::create();
    REQUIRE(runtime.has_value());
    const auto blocked =
        cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
    REQUIRE_FALSE(blocked.has_value());
    CHECK(blocked.error().code() == "platform.sdl.window_already_active");

    copiedLease = {};
    const auto replacement =
        cuexis::platform_sdl::SdlWindow::create(runtime.value(), dummyWindowConfig());
    REQUIRE(replacement.has_value());
}
