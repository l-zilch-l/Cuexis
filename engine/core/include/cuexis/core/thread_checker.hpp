#pragma once

//  ThreadChecker — 线程所有权检查器
//  构造时记录创建线程，用于在 Debug 构建中断言调用方处于正确线程
//  Main/Render/Audio Thread 的所有权规则依赖此类进行验证

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
