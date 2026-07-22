#pragma once

//  Result<T, E> — 基于 tl::expected 的统一错误处理类型
//  可预期的运行时错误必须通过 Result 返回，异常不得跨模块公共边界
//  禁止忽略 Result；若不需处理则必须显式调用 discard/log helper

#include <cuexis/core/error.hpp>

#include <tl/expected.hpp>

#include <utility>

namespace cuexis::core {

template <typename T, typename E = Error> using Result = tl::expected<T, E>;

//  构造 unexpected 错误值，用于提前返回失败
//  必须使用 [[nodiscard]] 防止调用方意外丢弃错误
template <typename E> [[nodiscard]] constexpr auto unexpected(E&& error) {
    return tl::make_unexpected(std::forward<E>(error));
}

} // namespace cuexis::core
