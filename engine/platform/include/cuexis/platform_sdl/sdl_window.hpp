#pragma once

// SDL3 window ownership and lease types.
// SdlWindow owns the window; SdlWindowLease keeps the window and SDL runtime alive.
// Every operation must run on the associated SDL main thread.
// Video backends use nativeHandle() without exposing SDL types.

#include <memory>
#include <string>

#include <cuexis/core/result.hpp>

namespace cuexis::platform_sdl {

namespace detail {
struct WindowState;
}

class SdlRuntime;

struct WindowConfig final {
    std::string title{"Cuexis Player"};
    int width{1280};
    int height{720};
    bool resizable{true};
    bool highDpi{true};
    bool openGl{true};
};

struct WindowEvents final {
    bool quitRequested{false};
};

struct DrawableSize final {
    int width{0};
    int height{0};
};

// Keeps both the native window and its SDL video runtime alive for a backend.
//
// Thread contract: every operation on a non-empty lease, including copy/move construction,
// copy/move assignment, queries, and destruction, must run on the associated runtime owner
// thread (the SDL main thread). Releasing the final lease can destroy both the native window and
// its SDL runtime, so the final release has the same requirement. A default or moved-from lease
// is empty and has no thread affinity until assigned a live lease. Debug builds assert this
// contract.
class SdlWindowLease final {
  public:
    SdlWindowLease() noexcept = default;
    ~SdlWindowLease();

    SdlWindowLease(const SdlWindowLease& other);
    SdlWindowLease& operator=(const SdlWindowLease& other);
    SdlWindowLease(SdlWindowLease&& other) noexcept;
    SdlWindowLease& operator=(SdlWindowLease&& other) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] void* nativeHandle() const noexcept;

  private:
    friend class SdlWindow;

    explicit SdlWindowLease(std::shared_ptr<detail::WindowState> state) noexcept;

    std::shared_ptr<detail::WindowState> state_;
};

// Owns the public handle to an SDL window.
//
// Thread contract: create() and every operation on a non-empty window, including move
// construction, move assignment, and destruction, must run on the associated runtime owner
// thread (the SDL main thread). A moved-from window is empty and has no thread affinity until it
// receives another window. Debug builds assert this contract.
class SdlWindow final {
  public:
    [[nodiscard]] static core::Result<SdlWindow> create(SdlRuntime& runtime,
                                                        const WindowConfig& config = {});

    ~SdlWindow();

    SdlWindow(const SdlWindow&) = delete;
    SdlWindow& operator=(const SdlWindow&) = delete;
    SdlWindow(SdlWindow&& other) noexcept;
    SdlWindow& operator=(SdlWindow&& other) noexcept;

    [[nodiscard]] WindowEvents pollEvents();
    [[nodiscard]] core::Result<DrawableSize> drawableSize() const;
    [[nodiscard]] SdlWindowLease lease() const;

  private:
    explicit SdlWindow(std::shared_ptr<detail::WindowState> state) noexcept;

    std::shared_ptr<detail::WindowState> state_;
};

} // namespace cuexis::platform_sdl
