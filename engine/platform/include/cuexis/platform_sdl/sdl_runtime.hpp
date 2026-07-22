#pragma once

//  SDL3 平台抽象层 — SdlRuntime 管理 SDL 主线程视频运行时
//  线程契约：创建、所有操作、移动构造/赋值和析构必须在 SDL 主线程执行
//  业务代码不得直接依赖 SDL 类型；平台对象仅在此模块内持有
//  executableBasePath() 解析真实可执行目录，不依赖 argv[0] 或当前工作目录

#include <filesystem>
#include <memory>
#include <string_view>

#include <cuexis/core/result.hpp>

namespace cuexis::platform_sdl {

namespace detail {
struct RuntimeState;
}

class SdlWindow;

// 解析真实可执行文件目录，不依赖 argv[0] 或当前目录
[[nodiscard]] auto executableBasePath() -> core::Result<std::filesystem::path>;

// 持有在 SDL 主线程上创建的共享 SDL 视频运行时
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
