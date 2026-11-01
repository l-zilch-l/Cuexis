"""Apply the startup-observability instrumentation (probe instrument v2).

The probe keeps its registered measurement contract: the whole synchronous startup still runs
before the window exists and the measured loop is untouched. The patch only adds reporting that
runs before the loop: a per-stage absolute timeline, a liveness heartbeat for long stages, and
window/first-present markers. Existing ``GPU_STARTUP_BEGIN`` / ``GPU_STARTUP_END`` lines keep
their exact previous shape so every existing parser still works.
"""

import hashlib
import pathlib
import sys

SOURCE = pathlib.Path("tests/player/gpu_throughput_probe.cpp")

INCLUDES_OLD = """#include <array>
#include <chrono>"""
INCLUDES_NEW = """#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>"""

INCLUDES2_OLD = """#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>"""
INCLUDES2_NEW = """#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>"""

STAGE_OLD = """template <class Function> auto throughputStartupStage(const char* name, Function&& function) {
    const auto begin = Clock::now();
    std::cout << "GPU_STARTUP_BEGIN " << name << std::endl;
    auto result = function();
    std::cout << "GPU_STARTUP_END " << name << " " << std::setprecision(17)
              << throughputUs(begin, Clock::now()) << std::endl;
    return result;
}"""

STAGE_NEW = """// Startup observability (probe instrument v2). The registered measurement contract runs the
// whole synchronous startup before the window exists, which is what made the first seconds
// unobservable: nothing was printed while a stage ran and nothing recorded when the window
// appeared. The heartbeat only exists while a startup stage is active, prints nothing once the
// window is live, and is joined before warmup, so the measured loop keeps its previous shape.
struct StartupTrace final {
    std::mutex mutex;
    std::atomic<const char*> stage{nullptr};
    std::atomic<std::int64_t> stageBeginUs{0};
    std::atomic<bool> stop{false};
    std::thread heartbeat;
    Clock::time_point origin = Clock::now();
};

auto startupTrace() -> StartupTrace& {
    static StartupTrace trace;
    return trace;
}

auto startupUs(Clock::time_point point) -> double {
    return throughputUs(startupTrace().origin, point);
}

void startupLine(const std::string& line) {
    auto& trace = startupTrace();
    const std::lock_guard<std::mutex> lock{trace.mutex};
    std::cout << line << std::endl;
}

void startupHeartbeat() {
    auto& trace = startupTrace();
    while (!trace.stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds{250});
        if (trace.stop.load(std::memory_order_relaxed))
            break;
        const auto* name = trace.stage.load(std::memory_order_relaxed);
        if (name == nullptr)
            continue;
        const auto nowUs = startupUs(Clock::now());
        const auto beginUs =
            static_cast<double>(trace.stageBeginUs.load(std::memory_order_relaxed));
        std::ostringstream line;
        line << std::setprecision(17) << "GPU_STARTUP_HEARTBEAT " << name << ' '
             << nowUs - beginUs << ' ' << nowUs;
        startupLine(line.str());
    }
}

void startupBegin() {
    auto& trace = startupTrace();
    trace.origin = Clock::now();
    trace.stop.store(false, std::memory_order_relaxed);
    trace.stage.store(nullptr, std::memory_order_relaxed);
    trace.heartbeat = std::thread{startupHeartbeat};
}

void startupFinish() {
    auto& trace = startupTrace();
    trace.stop.store(true, std::memory_order_relaxed);
    if (trace.heartbeat.joinable())
        trace.heartbeat.join();
}

template <class Function> auto throughputStartupStage(const char* name, Function&& function) {
    auto& trace = startupTrace();
    const auto begin = Clock::now();
    trace.stageBeginUs.store(static_cast<std::int64_t>(startupUs(begin)),
                             std::memory_order_relaxed);
    trace.stage.store(name, std::memory_order_relaxed);
    {
        const std::lock_guard<std::mutex> lock{trace.mutex};
        std::cout << "GPU_STARTUP_BEGIN " << name << std::endl;
    }
    auto result = function();
    const auto end = Clock::now();
    trace.stage.store(nullptr, std::memory_order_relaxed);
    {
        const std::lock_guard<std::mutex> lock{trace.mutex};
        std::cout << "GPU_STARTUP_END " << name << " " << std::setprecision(17)
                  << throughputUs(begin, end) << std::endl;
        std::cout << std::setprecision(17) << "GPU_STARTUP_AT " << name << ' ' << startupUs(end)
                  << std::endl;
    }
    return result;
}"""

SEQUENCE_OLD = """    // Complete synchronous CPU preparation before creating a window. Keep the real startup
    // costs visible in the log; session and prepared state stay on their creating owner thread.
    PlaybackSession session;
    auto source = throughputStartupStage(
        "fixture_source", [&] { return throughputSource(count, mode == "animated"); });
    auto prepared = throughputStartupStage("playback_prepare", [&] {
        return throughputTake(session.prepareLoad(std::move(source), PlaybackMode::ChartClock));
    });
    auto runtime = throughputTake(platform_sdl::SdlRuntime::create());
    render_opengl::OpenGlConfig config;
    config.vsync = false;
    auto configuration = throughputTake(render_opengl::configureOpenGlContext(runtime, config));
    platform_sdl::WindowConfig wc;
    wc.width = static_cast<int>(viewportWidth);
    wc.height = static_cast<int>(viewportHeight);
    wc.highDpi = false;
    wc.title = "Cuexis visible entities GPU throughput";
    auto window = throughputTake(platform_sdl::SdlWindow::create(runtime, wc));
    throughputPumpEvents();
    auto* nativeWindow = static_cast<SDL_Window*>(window.lease().nativeHandle());
    auto backend =
        throughputTake(render_opengl::OpenGlBackend::create(window, std::move(configuration)));
    throughputTake(render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(backend, true));
    int interval{};
    REQUIRE(SDL_GL_GetSwapInterval(&interval));
    REQUIRE(interval == 0);
    int width{}, height{};
    REQUIRE(SDL_GetWindowSizeInPixels(nativeWindow, &width, &height));
    REQUIRE(width == static_cast<int>(viewportWidth));
    REQUIRE(height == static_cast<int>(viewportHeight));
    auto presentation = throughputStartupStage(
        "renderer_prepare", [&] { return throughputTake(backend.prepare(prepared, {})); });"""

SEQUENCE_NEW = """    // Complete synchronous CPU preparation before creating a window. Keep the real startup
    // costs visible in the log; session and prepared state stay on their creating owner thread.
    startupBegin();
    startupLine("GPU_STARTUP_ORIGIN 0");
    PlaybackSession session;
    auto source = throughputStartupStage(
        "fixture_source", [&] { return throughputSource(count, mode == "animated"); });
    auto prepared = throughputStartupStage("playback_prepare", [&] {
        return throughputTake(session.prepareLoad(std::move(source), PlaybackMode::ChartClock));
    });
    auto runtime = throughputStartupStage(
        "sdl_runtime", [&] { return throughputTake(platform_sdl::SdlRuntime::create()); });
    render_opengl::OpenGlConfig config;
    config.vsync = false;
    auto configuration = throughputStartupStage("gl_context", [&] {
        return throughputTake(render_opengl::configureOpenGlContext(runtime, config));
    });
    platform_sdl::WindowConfig wc;
    wc.width = static_cast<int>(viewportWidth);
    wc.height = static_cast<int>(viewportHeight);
    wc.highDpi = false;
    wc.title = "Cuexis visible entities GPU throughput";
    auto window = throughputStartupStage("window_create", [&] {
        return throughputTake(platform_sdl::SdlWindow::create(runtime, wc));
    });
    throughputPumpEvents();
    auto* nativeWindow = static_cast<SDL_Window*>(window.lease().nativeHandle());
    {
        const auto flags = SDL_GetWindowFlags(nativeWindow);
        int windowWidth{}, windowHeight{};
        REQUIRE(SDL_GetWindowSize(nativeWindow, &windowWidth, &windowHeight));
        std::ostringstream line;
        line << std::setprecision(17) << "GPU_STARTUP_WINDOW {\\"at_us\\":" << startupUs(Clock::now())
             << ",\\"window_id\\":" << SDL_GetWindowID(nativeWindow)
             << ",\\"hidden\\":" << ((flags & SDL_WINDOW_HIDDEN) ? "true" : "false")
             << ",\\"opengl\\":" << ((flags & SDL_WINDOW_OPENGL) ? "true" : "false")
             << ",\\"width\\":" << windowWidth << ",\\"height\\":" << windowHeight << '}';
        startupLine(line.str());
    }
    auto backend = throughputStartupStage("backend_create", [&] {
        return throughputTake(render_opengl::OpenGlBackend::create(window, std::move(configuration)));
    });
    throughputStartupStage("pixel_readback", [&] {
        throughputTake(render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(backend, true));
        return true;
    });
    int interval{};
    REQUIRE(SDL_GL_GetSwapInterval(&interval));
    REQUIRE(interval == 0);
    int width{}, height{};
    REQUIRE(SDL_GetWindowSizeInPixels(nativeWindow, &width, &height));
    REQUIRE(width == static_cast<int>(viewportWidth));
    REQUIRE(height == static_cast<int>(viewportHeight));
    auto presentation = throughputStartupStage(
        "renderer_prepare", [&] { return throughputTake(backend.prepare(prepared, {})); });"""

COMMIT_OLD = """    throughputTake(backend.accepts(presentation));
    throughputTake(session.commit(std::move(prepared)));
    backend.activate(std::move(presentation));
    throughputPumpEvents();"""

COMMIT_NEW = """    throughputTake(backend.accepts(presentation));
    throughputStartupStage("session_commit", [&] {
        throughputTake(session.commit(std::move(prepared)));
        return true;
    });
    throughputStartupStage("backend_activate", [&] {
        backend.activate(std::move(presentation));
        return true;
    });
    throughputPumpEvents();"""

FIRST_PRESENT_OLD = """    throughputStartupStage("first_submit",
                           [&] { return throughputTake(backend.submit(snapshot)); });
    throughputTake(backend.present());
    REQUIRE(backend.lastPixelProbe().rgba[0] > 200);
    REQUIRE(backend.lastPixelProbe().rgba[1] > 100);
    REQUIRE(backend.lastPixelProbe().rgba[2] < 160);
    throughputTake(
        render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(backend, readback));"""

FIRST_PRESENT_NEW = """    throughputStartupStage("first_submit",
                           [&] { return throughputTake(backend.submit(snapshot)); });
    throughputStartupStage("first_present", [&] {
        throughputTake(backend.present());
        return true;
    });
    REQUIRE(backend.lastPixelProbe().rgba[0] > 200);
    REQUIRE(backend.lastPixelProbe().rgba[1] > 100);
    REQUIRE(backend.lastPixelProbe().rgba[2] < 160);
    {
        // Everything after this point is measured: the window exists, shows a proven frame and
        // the heartbeat is stopped so no reporting thread can add wakeups to the measurement.
        std::ostringstream line;
        line << std::setprecision(17) << "GPU_STARTUP_SHOWN " << startupUs(Clock::now());
        startupLine(line.str());
        std::ostringstream total;
        total << std::setprecision(17) << "GPU_STARTUP_TOTAL {\\"first_present_us\\":"
              << startupUs(Clock::now()) << '}';
        startupLine(total.str());
    }
    startupFinish();
    throughputTake(
        render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(backend, readback));"""

EDITS = [
    ("includes", INCLUDES_OLD, INCLUDES_NEW),
    ("includes2", INCLUDES2_OLD, INCLUDES2_NEW),
    ("stage", STAGE_OLD, STAGE_NEW),
    ("sequence", SEQUENCE_OLD, SEQUENCE_NEW),
    ("commit", COMMIT_OLD, COMMIT_NEW),
    ("first_present", FIRST_PRESENT_OLD, FIRST_PRESENT_NEW),
]


def main() -> int:
    raw = SOURCE.read_bytes()
    before = hashlib.sha256(raw).hexdigest()
    text = raw.decode("utf-8")
    crlf = text.count("\r\n")
    lf_only = text.count("\n") - crlf
    print(f"before: bytes={len(raw)} sha256={before} crlf={crlf} lf_only={lf_only}")
    if lf_only:
        print("refusing: file is not uniformly CRLF")
        return 1
    normalised = text.replace("\r\n", "\n")
    for name, old, new in EDITS:
        occurrences = normalised.count(old)
        if occurrences != 1:
            print(f"refusing: anchor {name!r} occurs {occurrences} times")
            return 1
        normalised = normalised.replace(old, new, 1)
    patched = normalised.replace("\n", "\r\n")
    SOURCE.write_bytes(patched.encode("utf-8"))
    after = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    print(f"after:  bytes={len(patched)} sha256={after}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
