#pragma once

//  Result<T, E> - the unified error handling type, built on tl::expected
//  Expected runtime errors must be returned through Result; exceptions must never cross a
//  module public boundary
//  Ignoring a Result is forbidden; when no handling is needed, call the discard/log helper
//  explicitly

#include <cuexis/core/error.hpp>

#include <tl/expected.hpp>

#include <utility>

namespace cuexis::core {

template <typename T, typename E = Error> using Result = tl::expected<T, E>;

//  Builds an unexpected error value, used to return failure early
//  [[nodiscard]] is required so callers cannot accidentally drop the error
template <typename E> [[nodiscard]] constexpr auto unexpected(E&& error) {
    return tl::make_unexpected(std::forward<E>(error));
}

} // namespace cuexis::core
