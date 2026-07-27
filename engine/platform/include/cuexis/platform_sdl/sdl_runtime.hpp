#pragma once

// SDL3 platform adapter for the main-thread video runtime.
// Creation, operations, moves, and destruction must run on the SDL main thread.
// Application code does not depend on SDL types; this module owns platform objects.
// executableBasePath() resolves the executable directory without argv[0] or the working directory.

#include <filesystem>
#include <memory>
#include <string_view>

#include <cuexis/core/result.hpp>

namespace cuexis::platform_sdl {

namespace detail {
struct RuntimeState;
}

class SdlWindow;

// Resolves the executable directory without relying on argv[0] or the working directory.
[[nodiscard]] auto executableBasePath() -> core::Result<std::filesystem::path>;

// Owns the shared SDL video runtime created on the SDL main thread.
//
// Thread contract: create(), every operation on a non-empty instance, move construction,
// move assignment, and destruction must run on the runtime owner thread. The owner thread is
// the SDL main thread that first created the shared runtime. A moved-from instance is empty and
// has no thread affinity until it receives another runtime. Debug builds assert this contract.
class SdlRuntime final {
  public:
    [[nodiscard]] static core::Result<SdlRuntime> create();

    ~SdlRuntime();

    SdlRuntime(const SdlRuntime&) = delete;
    SdlRuntime& operator=(const SdlRuntime&) = delete;
    SdlRuntime(SdlRuntime&& other) noexcept;
    SdlRuntime& operator=(SdlRuntime&& other) noexcept;

    [[nodiscard]] std::string_view videoDriver() const noexcept;

  private:
    friend class SdlWindow;

    explicit SdlRuntime(std::shared_ptr<detail::RuntimeState> state) noexcept;

    std::shared_ptr<detail::RuntimeState> state_;
};

} // namespace cuexis::platform_sdl
