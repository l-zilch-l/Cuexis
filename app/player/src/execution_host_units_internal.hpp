#pragma once

// The execution host is one owner-thread state machine; its units share the small helpers that
// translate a session reply into a value or a failure.

#include "execution_host.hpp"

#include <cuexis/core/error.hpp>

#include <cstdint>

namespace cuexis::player {

using namespace playback;
using namespace playback::detail;

struct Failure {
    core::Error error;
};

inline auto error(const char* code) -> core::Error {
    return {std::string{"host.rpa."} + code, "Execution host operation is incomplete"};
}

template <class T> auto take(core::Result<T> result) -> T {
    if (!result)
        throw Failure{std::move(result.error())};
    return std::move(*result);
}

inline void take(core::Result<void> result) {
    if (!result)
        throw Failure{std::move(result.error())};
}

template <class T> auto value(SessionReply reply) -> T {
    auto* result = std::get_if<T>(&reply);
    if (!result)
        throw Failure{error("message_conflict")};
    return std::move(*result);
}

inline auto nowUs() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct Busy final {
    bool& flag;
    explicit Busy(bool& value) : flag(value) {
        if (flag)
            throw Failure{error("control_pending")};
        flag = true;
    }
    ~Busy() {
        flag = false;
    }
};

[[nodiscard]] auto error(const char* code) -> core::Error;

void take(core::Result<void> result);

[[nodiscard]] auto nowUs() noexcept -> std::int64_t;

} // namespace cuexis::player
