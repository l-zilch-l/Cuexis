#pragma once

//  Error — 统一的错误类型，必须包含稳定 error code 和可读 message
//  程序判断应依赖 code，不能依赖可能本地化的 message 文本
//  Error 支持附加上下文键值对和因果链（cause），便于诊断

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::core {

struct ErrorContext {
    std::string key;
    std::string value;
};

class Error final {
  public:
    Error(std::string code, std::string message);

    [[nodiscard]] std::string_view code() const noexcept;
    [[nodiscard]] std::string_view message() const noexcept;
    [[nodiscard]] const std::vector<ErrorContext>& context() const noexcept;

    // 返回原因链中的上层错误；只要此 Error 持有其 cause，返回的指针就有效
    [[nodiscard]] const Error* cause() const noexcept;

    // 链式附加诊断上下文（左值版本，返回自身引用）
    Error& withContext(std::string key, std::string value) &;
    // 链式附加诊断上下文（右值版本，允许 Error{} << "key" << "value" 风格）
    Error&& withContext(std::string key, std::string value) &&;

    // 设置因果链中的上游错误
    Error& withCause(Error cause) &;
    Error&& withCause(Error cause) &&;

  private:
    std::string code_;
    std::string message_;
    std::vector<ErrorContext> context_;
    std::shared_ptr<const Error> cause_;
};

} // namespace cuexis::core
