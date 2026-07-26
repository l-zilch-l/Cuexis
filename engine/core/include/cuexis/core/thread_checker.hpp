#pragma once

//  ThreadChecker - thread ownership checker
//  Records the creating thread on construction so Debug builds can assert that the caller is
//  on the correct thread
//  The ownership rules for the Main/Render/Audio threads rely on this class for verification

#include <thread>

namespace cuexis::core {

class ThreadChecker final {
  public:
    ThreadChecker() noexcept;

    [[nodiscard]] bool isCurrent() const noexcept;
    void assertCurrent() const noexcept;

  private:
    std::thread::id owner_;
};

} // namespace cuexis::core
