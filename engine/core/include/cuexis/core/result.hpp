#pragma once

//  Result<T, E> - the unified error handling type, built on tl::expected
//  Expected runtime errors must be returned through Result; exceptions must never cross a
//  module public boundary
//  Ignoring a Result is forbidden; when no handling is needed, call the discard/log helper
//  explicitly

#include <cuexis/core/error.hpp>

#include <tl/expected.hpp>

#include <exception>
#include <functional>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cuexis::core {

template <typename T, typename E = Error> using Result = tl::expected<T, E>;

//  Builds an unexpected error value, used to return failure early
//  [[nodiscard]] is required so callers cannot accidentally drop the error
template <typename E> [[nodiscard]] constexpr auto unexpected(E&& error) {
    return tl::make_unexpected(std::forward<E>(error));
}

namespace detail {

template <typename T> struct GuardedResult final {
    using Type = Result<T>;
};

template <typename T> struct GuardedResult<tl::expected<T, Error>> final {
    using Type = Result<T>;
};

} // namespace detail

template <typename Callback, typename... Arguments>
using GuardedInvokeResult =
    typename detail::GuardedResult<std::invoke_result_t<Callback, Arguments...>>::Type;

// Invokes a synchronous public callback and converts non-OOM exceptions to a stable Error.
// Result-returning callbacks are flattened so callers receive a single Result layer.
template <typename Callback, typename... Arguments>
[[nodiscard]] auto invokeGuarded(std::string_view errorCode, std::string_view errorMessage,
                                 Callback&& callback, Arguments&&... arguments)
    -> GuardedInvokeResult<Callback&&, Arguments&&...> {
    using ReturnType = std::invoke_result_t<Callback&&, Arguments&&...>;
    try {
        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<Callback>(callback), std::forward<Arguments>(arguments)...);
            return {};
        } else {
            return std::invoke(std::forward<Callback>(callback),
                               std::forward<Arguments>(arguments)...);
        }
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& exception) {
        return unexpected(Error{std::string{errorCode}, std::string{errorMessage}}.withContext(
            "exception", exception.what()));
    } catch (...) {
        return unexpected(Error{std::string{errorCode}, std::string{errorMessage}});
    }
}

} // namespace cuexis::core
