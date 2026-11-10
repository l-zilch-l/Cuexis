"""Byte-level patcher: opt-in device-first Player startup.

The default startup prepares content before any device exists, so at 40 000 entities the process
owns no window for the first ~2.4 s. The patch below adds an opt-in order that creates the runtime,
the window, the surface, and the GL backend first, prepares the bundle on a worker thread, and keeps
the main thread draining window events and presenting a still frame until the worker finishes.

The patcher is byte-exact on purpose: it refuses a BOM, refuses mixed line endings, refuses an
already patched file, asserts that every anchor occurs exactly once, and prints the before/after
size and sha256. Newlines are normalised to LF for editing and re-expanded to CRLF, which is the
established convention of this source file.
"""

from __future__ import annotations

import hashlib
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
TARGET = REPO / "app" / "player" / "src" / "player_app.cpp"

# --------------------------------------------------------------------------------------------------
# 1. includes
# --------------------------------------------------------------------------------------------------

INCLUDE_OLD = """#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/player_support/config_location.hpp>
#include <cuexis/version.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
"""

INCLUDE_NEW = """#include <cuexis/audio/audio_transport.hpp>
#include <cuexis/player_support/config_location.hpp>
#include <cuexis/render/render_backend.hpp>
#include <cuexis/version.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
"""

# --------------------------------------------------------------------------------------------------
# 2. helper next to the other startup helpers
# --------------------------------------------------------------------------------------------------

HELPER_OLD = """} // namespace

auto run(int argumentCount, char** arguments, PlayerLogger& logger) -> core::Result<void> {
"""

HELPER_NEW = """// The still frame the window presents while the content prepares. It is deliberately dark: a
// white flash would be more startling than the previous total absence of a window.
constexpr render::Color stillClearColor{0.06F, 0.06F, 0.08F, 1.0F};

// Opt-in start-up order: create the window before the content is prepared. Off by default, because
// a recorded startup measurement describes the historical order and the two orders are not
// interchangeable evidence.
[[nodiscard]] auto windowFirstStartupRequested() -> bool {
    static const bool requested = [] {
        char buffer[2]{};
        std::size_t length = 0;
        if (getenv_s(&length, buffer, sizeof buffer, "CUEXIS_PLAYER_WINDOW_FIRST") != 0 ||
            length != 2) {
            return false;
        }
        return buffer[0] == '1';
    }();
    return requested;
}

} // namespace

auto run(int argumentCount, char** arguments, PlayerLogger& logger) -> core::Result<void> {
"""

# --------------------------------------------------------------------------------------------------
# 3. the startup order itself
# --------------------------------------------------------------------------------------------------

STARTUP_OLD = """    // The CPU half of the startup Load runs before any device exists: no window, no GL context, no
    // audio device. Preparing here means a large bundle never holds a message-free window on the
    // desktop, and a content that cannot be prepared fails before the first device is created. The
    // controller adopts the finished bundle once the presentation backend can accept it.
    auto startupLoad = preparePlayerLoadBundle(std::move(*content), startupMode,
                                               appConfig->profile.outputCorrectionUs);
    if (!startupLoad) {
        return core::unexpected(std::move(startupLoad.error()).withContext("command", "load"));
    }
    startup.mark("content_prepared");

    audio::AudioClipStore audioStore;

    auto runtimeResult = createPlayerRuntime(logger);
    if (!runtimeResult) {
        return core::unexpected(std::move(runtimeResult.error()));
    }
    auto sdlRuntime = std::move(*runtimeResult);
    auto windowResult = createPlayerWindow(sdlRuntime, appConfig->app.requested);
    if (!windowResult) {
        return core::unexpected(std::move(windowResult.error()));
    }
    auto window = std::move(*windowResult);
    startup.mark("window_created");
    std::vector<std::uint32_t> gameplayKeys;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    std::optional<PlayerGameplayProfile> gameplayProfile;
    if (options.gameplayConfiguration) {
        auto profile = readPlayerGameplay(options);
        if (!profile)
            return core::unexpected(std::move(profile.error()));
        for (const auto& key : profile->keys)
            gameplayKeys.push_back(key.scanCode);
        gameplayProfile = std::move(*profile);
    }
#endif
    auto surface = makePlayerSurface(window, std::move(gameplayKeys));
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
    ScriptedPlaybackTrace measurementTrace;
    if (measurementTrace.active())
        surface = std::make_unique<ScriptedPlaybackSurface>(
            std::move(surface), static_cast<SDL_Window*>(window.lease().nativeHandle()),
            measurementTrace);
#endif
    auto backendResult = createPlayerBackend(sdlRuntime, window, appConfig->app.requested.vsync,
                                             options.shaderCacheDirectory, logger);
    if (!backendResult) {
        return core::unexpected(std::move(backendResult.error()));
    }
    auto backend = std::move(*backendResult);
    startup.mark("backend_created");
"""

STARTUP_NEW = """    // The CPU half of the startup Load normally runs before any device exists: no window, no GL
    // context, and no audio device. Preparing there means a large bundle never holds a
    // message-free window on the desktop, and content that cannot be prepared fails before the
    // first device is created. The controller adopts the finished bundle once the presentation
    // backend can accept it.
    //
    // That order hides the whole cost inside one silent phase. Preparing the 40 000 entity chart
    // spends about two seconds of CPU before the process owns a window, so there is nothing on the
    // desktop to move, paint, or close, and a user who started the wrong file can only kill the
    // process. The opt-in order below reverses the two steps: the window and the GL context exist
    // first, a worker prepares the bundle, and this thread keeps draining window events and
    // presenting a still frame, so the window stays responsive throughout. A close request during
    // the prepare is recorded and honoured as soon as the worker finishes, because prepare itself
    // is not interruptible. The default order is unchanged, because every recorded startup number
    // describes it.
    const bool deviceFirst = windowFirstStartupRequested();
    std::optional<platform_sdl::SdlRuntime> runtimeHolder;
    std::optional<platform_sdl::SdlWindow> windowHolder;
    std::optional<render_opengl::OpenGlBackend> backendHolder;
    std::unique_ptr<PlayerSurface> surfaceHolder;
    std::vector<std::uint32_t> gameplayKeys;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    std::optional<PlayerGameplayProfile> gameplayProfile;
#endif
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
    ScriptedPlaybackTrace measurementTrace;
#endif

    auto openDevices = [&]() -> core::Result<void> {
        auto runtimeResult = createPlayerRuntime(logger);
        if (!runtimeResult) {
            return core::unexpected(std::move(runtimeResult.error()));
        }
        runtimeHolder.emplace(std::move(*runtimeResult));
        auto windowResult = createPlayerWindow(*runtimeHolder, appConfig->app.requested);
        if (!windowResult) {
            return core::unexpected(std::move(windowResult.error()));
        }
        windowHolder.emplace(std::move(*windowResult));
        startup.mark("window_created");
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
        if (options.gameplayConfiguration) {
            auto profile = readPlayerGameplay(options);
            if (!profile) {
                return core::unexpected(std::move(profile.error()));
            }
            for (const auto& key : profile->keys) {
                gameplayKeys.push_back(key.scanCode);
            }
            gameplayProfile = std::move(*profile);
        }
#endif
        surfaceHolder = makePlayerSurface(*windowHolder, std::move(gameplayKeys));
#if defined(CUEXIS_PLAYBACK_TRACE_HARNESS)
        if (measurementTrace.active()) {
            surfaceHolder = std::make_unique<ScriptedPlaybackSurface>(
                std::move(surfaceHolder),
                static_cast<SDL_Window*>(windowHolder->lease().nativeHandle()), measurementTrace);
        }
#endif
        auto backendResult =
            createPlayerBackend(*runtimeHolder, *windowHolder, appConfig->app.requested.vsync,
                                options.shaderCacheDirectory, logger);
        if (!backendResult) {
            return core::unexpected(std::move(backendResult.error()));
        }
        backendHolder.emplace(std::move(*backendResult));
        startup.mark("backend_created");
        return {};
    };

    // The prepare below writes both of these and the main thread reads them only after the join,
    // so the join is the only handoff the two orders need.
    std::optional<PlayerLoadBundle> preparedBundle;
    std::optional<core::Error> prepareFailure;
    auto prepareBundle = [&]() {
        auto prepared = preparePlayerLoadBundle(std::move(*content), startupMode,
                                                appConfig->profile.outputCorrectionUs);
        if (!prepared) {
            prepareFailure.emplace(std::move(prepared.error()));
            return;
        }
        preparedBundle.emplace(std::move(*prepared));
    };

    if (deviceFirst) {
        if (auto opened = openDevices(); !opened) {
            return core::unexpected(std::move(opened.error()));
        }
        std::atomic<bool> finished{false};
        std::thread worker{[&]() {
            prepareBundle();
            finished.store(true, std::memory_order_release);
        }};
        bool closeRequested = false;
        bool stillFrameReported = false;
        auto lastPresent = std::chrono::steady_clock::now();
        while (!finished.load(std::memory_order_acquire)) {
            if (auto input = surfaceHolder->pollInput(); !input) {
                closeRequested = true;
            } else if (input->quitRequested) {
                closeRequested = true;
            }
            const auto now = std::chrono::steady_clock::now();
            if (!closeRequested && now - lastPresent >= std::chrono::milliseconds{16}) {
                lastPresent = now;
                render::RenderFrame still{};
                still.clearColor = stillClearColor;
                if (auto size = surfaceHolder->drawableSize(); size) {
                    still.extent.width = static_cast<std::uint32_t>(std::max(size->width, 0));
                    still.extent.height = static_cast<std::uint32_t>(std::max(size->height, 0));
                }
                if (auto presented = backendHolder->renderFrame(still); !presented) {
                    // A window that went away mid-prepare must not spin on a failing swap.
                    closeRequested = true;
                    if (!stillFrameReported) {
                        stillFrameReported = true;
                        logger.warn("player.startup_still_frame",
                                    std::string{"The startup still frame failed: "} +
                                        std::string{presented.error().message()});
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{4});
        }
        worker.join();
        startup.mark("content_prepared");
        if (prepareFailure) {
            return core::unexpected(std::move(*prepareFailure).withContext("command", "load"));
        }
        if (closeRequested) {
            logger.info("player.startup_close_requested",
                        "The window closed while the content was still preparing");
            return {};
        }
    } else {
        prepareBundle();
        if (prepareFailure) {
            return core::unexpected(std::move(*prepareFailure).withContext("command", "load"));
        }
        startup.mark("content_prepared");
        if (auto opened = openDevices(); !opened) {
            return core::unexpected(std::move(opened.error()));
        }
    }

    auto& window = *windowHolder;
    auto& backend = *backendHolder;
    auto surface = std::move(surfaceHolder);
    audio::AudioClipStore audioStore;
"""

# --------------------------------------------------------------------------------------------------
# 4. the adoption site
# --------------------------------------------------------------------------------------------------

ADOPT_OLD = """    if (auto adopted = controller.adoptStartupLoad(std::move(*startupLoad)); !adopted) {
"""

ADOPT_NEW = """    if (auto adopted = controller.adoptStartupLoad(std::move(*preparedBundle)); !adopted) {
"""

EDITS = [
    ("includes", INCLUDE_OLD, INCLUDE_NEW),
    ("startup helper", HELPER_OLD, HELPER_NEW),
    ("startup order", STARTUP_OLD, STARTUP_NEW),
    ("adoption", ADOPT_OLD, ADOPT_NEW),
]


def main() -> int:
    raw = TARGET.read_bytes()
    if raw.startswith(b"\xef\xbb\xbf"):
        print("refusing: file starts with a UTF-8 BOM")
        return 1
    crlf = raw.count(b"\r\n")
    lf_only = raw.count(b"\n") - crlf
    if lf_only != 0:
        print(f"refusing: mixed line endings (crlf={crlf} lf_only={lf_only})")
        return 1
    text = raw.decode("utf-8").replace("\r\n", "\n")
    if "windowFirstStartupRequested" in text:
        print("refusing: already patched")
        return 1

    print(f"before: bytes={len(raw)} sha256={hashlib.sha256(raw).hexdigest()} "
          f"crlf={crlf} lf_only={lf_only}")

    for name, old, new in EDITS:
        count = text.count(old)
        if count != 1:
            print(f"refusing: anchor {name!r} occurs {count} times, expected 1")
            return 1
        text = text.replace(old, new, 1)
        print(f"anchor {name!r}: 1 occurrence replaced")

    out = text.replace("\n", "\r\n").encode("utf-8")
    TARGET.write_bytes(out)
    crlf_out = out.count(b"\r\n")
    lf_out = out.count(b"\n") - crlf_out
    print(f"after: bytes={len(out)} sha256={hashlib.sha256(out).hexdigest()} "
          f"crlf={crlf_out} lf_only={lf_out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
