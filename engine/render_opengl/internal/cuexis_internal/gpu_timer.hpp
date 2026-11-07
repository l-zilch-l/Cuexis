#pragma once
// Noninstalled GPU timestamp observer for explicit device probes.
#include <array>
#include <cstdint>
#include <cuexis/core/result.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <optional>

namespace cuexis::render_opengl::test_only {
class GpuTimer final {
  public:
    GpuTimer() = default;
    ~GpuTimer() noexcept;
    GpuTimer(const GpuTimer&) = delete;
    auto operator=(const GpuTimer&) -> GpuTimer& = delete;
    [[nodiscard]] auto initialize() -> core::Result<void>;
    [[nodiscard]] auto close() -> core::Result<void>;
    [[nodiscard]] auto begin(std::uint64_t ordinal) -> core::Result<void>;
    [[nodiscard]] auto end() -> core::Result<void>;
    struct Sample final {
        std::uint64_t ordinal{}, nanoseconds{};
    };
    [[nodiscard]] auto poll() -> core::Result<std::optional<Sample>>;
    [[nodiscard]] auto pending() const noexcept -> std::size_t {
        return count_;
    }

  private:
    struct Slot final {
        std::array<unsigned, 2> queries{};
        std::uint64_t ordinal{};
    };
    std::array<Slot, 64> slots_{};
    std::size_t head_{}, count_{};
    bool initialized_{}, active_{};
    core::ThreadChecker owner_;
};
} // namespace cuexis::render_opengl::test_only
