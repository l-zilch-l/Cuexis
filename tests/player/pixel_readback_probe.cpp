#include "../rpa/execution_fixture.hpp"
#include "../support/environment.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>
#include <cuexis_internal/pixel_probe.hpp>
#include <iostream>

TEST_CASE("OpenGL ordinary submission omits diagnostic pixel readback",
          "[.][device][r3][pixel-readback]") {
    using namespace cuexis;
    using Clock = std::chrono::steady_clock;
    if (test_support::environmentVariable("R3_PIXEL_BARRIER") == "1") {
        std::string go;
        std::cout << "R3_PIXEL_READY\n" << std::flush;
        REQUIRE(static_cast<bool>(std::getline(std::cin, go)));
        REQUIRE(go == "go");
    }
    auto runtime = platform_sdl::SdlRuntime::create();
    REQUIRE(runtime);
    render_opengl::OpenGlConfig config;
    config.vsync = false;
    auto context = render_opengl::configureOpenGlContext(*runtime, config);
    REQUIRE(context);
    platform_sdl::WindowConfig settings;
    settings.width = 1920;
    settings.height = 1080;
    settings.highDpi = false;
    settings.title = "Cuexis explicit pixel readback diagnostic";
    auto window = platform_sdl::SdlWindow::create(*runtime, settings);
    REQUIRE(window);
    auto backend = render_opengl::OpenGlBackend::create(*window, std::move(*context));
    REQUIRE(backend);
    playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(test_support::presentation(true), playback::PlaybackMode::ChartClock);
    REQUIRE(prepared);
    auto candidate = backend->prepare(*prepared);
    REQUIRE(candidate);
    REQUIRE(backend->accepts(*candidate));
    REQUIRE(session.commit(std::move(*prepared)));
    backend->activate(std::move(*candidate));
    REQUIRE(session.update({0, 0, 0}));
    auto frame = session.extractFrame({1920, 1080});
    REQUIRE(frame);
    const std::array<std::uint8_t, 4> empty{}, orange{255, 204, 51, 255};
    const auto draw = [&] {
        auto submitted = backend->submit(*frame);
        REQUIRE(submitted);
        REQUIRE(submitted->opaque.size() == 1);
        REQUIRE(backend->lastPixelProbe().presentationDrawn);
        REQUIRE(backend->present());
    };
    const auto defaultBegin = Clock::now();
    draw();
    const auto defaultEnd = Clock::now();
    CHECK(backend->lastPixelProbe().rgba == empty);
    REQUIRE(render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(*backend, true));
    const auto proofBegin = Clock::now();
    draw();
    const auto proofEnd = Clock::now();
    CHECK(backend->lastPixelProbe().rgba == orange);
    const bool enabled = test_support::environmentVariable("R3_PIXEL_READBACK") == "1";
    REQUIRE(render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(*backend, enabled));
    for (unsigned i = 0; i < 4; ++i)
        draw();
    for (unsigned i = 0; i < 32; ++i) {
        SDL_PumpEvents();
        const auto begin = Clock::now();
        auto submitted = backend->submit(*frame);
        const auto submittedAt = Clock::now();
        REQUIRE(submitted);
        REQUIRE(submitted->opaque.size() == 1);
        REQUIRE(backend->present());
        const auto end = Clock::now();
        CHECK(backend->lastPixelProbe().presentationDrawn);
        CHECK(backend->lastPixelProbe().rgba == (enabled ? orange : empty));
        std::cout
            << "{\"event\":\"r3.pixel.policy\",\"ordinal\":" << i << ",\"readback\":" << enabled
            << ",\"submit_us\":"
            << std::chrono::duration_cast<std::chrono::microseconds>(submittedAt - begin).count()
            << ",\"swap_us\":"
            << std::chrono::duration_cast<std::chrono::microseconds>(end - submittedAt).count()
            << ",\"default_first_us\":"
            << std::chrono::duration_cast<std::chrono::microseconds>(defaultEnd - defaultBegin)
                   .count()
            << ",\"proof_second_us\":"
            << std::chrono::duration_cast<std::chrono::microseconds>(proofEnd - proofBegin).count()
            << ",\"scope\":\"CPU submit/swap; GPU pixels checked before and "
               "after\",\"formal\":false}\n";
    }
    REQUIRE(render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(*backend, true));
    draw();
    CHECK(backend->lastPixelProbe().rgba == orange);
    REQUIRE(backend->close());
}
