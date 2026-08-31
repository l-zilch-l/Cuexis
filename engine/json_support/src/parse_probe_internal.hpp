#pragma once

#include <cstddef>

namespace cuexis::json::detail {

// Test-only thread-local observation of json::parse calls. This header is not installed.
class ScopedParseCounter final {
  public:
    ScopedParseCounter() noexcept;
    ~ScopedParseCounter();

    ScopedParseCounter(const ScopedParseCounter&) = delete;
    auto operator=(const ScopedParseCounter&) -> ScopedParseCounter& = delete;

    void reset() noexcept;
    [[nodiscard]] auto count() const noexcept -> std::size_t;

  private:
    std::size_t count_{};
    std::size_t* previous_{};
};

} // namespace cuexis::json::detail
