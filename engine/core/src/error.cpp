//  Error 的实现 — 统一错误值类型，用于可预期的运行时错误返回
//  程序判断必须依赖稳定的 code 字符串，不能依赖可能本地化的 message 文本

#include <cuexis/core/error.hpp>

#include <type_traits>
#include <utility>

namespace cuexis::core {
namespace {

template <typename Text> auto textView(const Text& text) noexcept -> std::string_view {
    if constexpr (std::is_same_v<Text, InternedText>)
        return text.view();
    else
        return text;
}

//  Empty context is served from one process-wide vector. Building it once at load time keeps
//  context() free of allocation even when a Debug STL allocates a proxy per container.
const std::vector<ErrorContext> noContext;

} // namespace

Error::Error(std::string code, std::string message)
    : text_(OwnedTextPair{std::move(code), std::move(message)}) {}

Error::Error(InternedTag, InternedText code, InternedText message, ErrorStage stage) noexcept
    : text_(InternedTextPair{code, message}), stage_(stage) {}

auto Error::interned(InternedText code, InternedText message, ErrorStage stage) noexcept -> Error {
    return Error{InternedTag{}, code, message, stage};
}

std::string_view Error::code() const noexcept {
    return std::visit(
        [](const auto& text) -> std::string_view {
            if constexpr (std::is_same_v<std::decay_t<decltype(text)>, InternedTextPair>)
                return textView(text.code);
            else
                return text.code;
        },
        text_);
}

std::string_view Error::message() const noexcept {
    return std::visit(
        [](const auto& text) -> std::string_view {
            if constexpr (std::is_same_v<std::decay_t<decltype(text)>, InternedTextPair>)
                return textView(text.message);
            else
                return text.message;
        },
        text_);
}

const std::vector<ErrorContext>& Error::context() const noexcept {
    return context_ ? *context_ : noContext;
}

bool Error::isInterned() const noexcept {
    return std::holds_alternative<InternedTextPair>(text_);
}

ErrorStage Error::stage() const noexcept {
    return isInterned() ? stage_ : ErrorStage::none;
}

const Error* Error::cause() const noexcept {
    return cause_.get();
}

Error& Error::withContext(std::string key, std::string value) & {
    if (!context_ || context_.use_count() != 1) {
        auto replacement = std::make_shared<std::vector<ErrorContext>>();
        if (context_)
            replacement->assign(context_->begin(), context_->end());
        context_ = std::move(replacement);
    }
    context_->push_back(ErrorContext{std::move(key), std::move(value)});
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
