//  Error 的实现 — 统一错误值类型，用于可预期的运行时错误返回
//  程序判断必须依赖稳定的 code 字符串，不能依赖可能本地化的 message 文本

#include <cuexis/core/error.hpp>

#include <utility>

namespace cuexis::core {

Error::Error(std::string code, std::string message)
    : code_(std::move(code)), message_(std::move(message)) {}

std::string_view Error::code() const noexcept {
    return code_;
}

std::string_view Error::message() const noexcept {
    return message_;
}

const std::vector<ErrorContext>& Error::context() const noexcept {
    return context_;
}

const Error* Error::cause() const noexcept {
    return cause_.get();
}

Error& Error::withContext(std::string key, std::string value) & {
    context_.push_back(ErrorContext{std::move(key), std::move(value)});
    return *this;
}

Error&& Error::withContext(std::string key, std::string value) && {
    withContext(std::move(key), std::move(value));
    return std::move(*this);
}

Error& Error::withCause(Error cause) & {
    cause_ = std::make_shared<Error>(std::move(cause));
    return *this;
}

Error&& Error::withCause(Error cause) && {
    withCause(std::move(cause));
    return std::move(*this);
}

} // namespace cuexis::core
