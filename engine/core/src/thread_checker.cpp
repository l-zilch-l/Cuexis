//  ThreadChecker 实现 — 构造时记录创建线程 ID
//  assertCurrent 仅在 Debug 构建（NDEBUG 未定义）时断言，Release 为零开销

#include <cuexis/core/thread_checker.hpp>

#include <cassert>

namespace cuexis::core {

ThreadChecker::ThreadChecker() noexcept : owner_(std::this_thread::get_id()) {}

bool ThreadChecker::isCurrent() const noexcept {
    return owner_ == std::this_thread::get_id();
}

void ThreadChecker::assertCurrent() const noexcept {
#ifndef NDEBUG
    assert(isCurrent() && "ThreadChecker accessed from a non-owner thread");
#endif
}

} // namespace cuexis::core
