#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cuexis::internal::execution_diagnostics {
enum class Part { copy, timers, validate, ledger, reserve, foldCopy, fold, idleDeadline, count };
} // namespace cuexis::internal::execution_diagnostics

#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace cuexis::internal::execution_diagnostics {
inline auto now() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
inline auto enabled() noexcept -> bool {
#ifdef _MSC_VER
    char value[8]{};
    std::size_t length{};
    return getenv_s(&length, value, sizeof(value), "CUEXIS_EXECUTION_DIAGNOSTIC_RUN") == 0 &&
           length == 2 && value[0] == '1';
#else
    const auto* value = std::getenv("CUEXIS_EXECUTION_DIAGNOSTIC_RUN");
    return value && value[0] == '1' && value[1] == 0;
#endif
}
struct Trace;
inline thread_local Trace* current{};
inline std::mutex outputMutex;
inline std::atomic<std::uint64_t> nextId{};
struct Trace final {
    const char* name;
    Trace* previous;
    bool recording;
    std::uint64_t id{}, timers{}, cursor{}, ticks{}, job{};
    std::int64_t horizon{}, tick{}, started{};
    int exceptions{};
    std::array<std::uint64_t, static_cast<unsigned>(Part::count)> calls{}, nanoseconds{};
    explicit Trace(const char* stage, std::int64_t target = 0, std::uint64_t count = 0,
                   std::uint64_t request = 0) noexcept
        : name(stage), previous(current), recording(enabled()), timers(count), job(request),
          horizon(target), exceptions(std::uncaught_exceptions()) {
        if (recording) {
            id = ++nextId;
            started = now();
            current = this;
            emit("begin");
        }
    }
    void emit(const char* state) const noexcept {
        if (!recording)
            return;
        try {
            std::lock_guard lock(outputMutex);
            std::fprintf(stderr,
                         "EXECUTION_DIAGNOSTIC {\"stage\":\"%s\",\"state\":\"%s\","
                         "\"id\":%llu,\"job_id\":%llu,\"parent_id\":%llu,\"thread\":%llu,\"time_"
                         "ns\":%lld,\"started_ns\":%lld,"
                         "\"elapsed_ns\":%lld,\"horizon\":%lld,\"timers\":%llu,"
                         "\"timer_cursor\":%llu,\"ticks\":%llu,\"tick\":%lld,\"calls\":[",
                         name, state, static_cast<unsigned long long>(id),
                         static_cast<unsigned long long>(job),
                         static_cast<unsigned long long>(previous ? previous->id : 0),
                         static_cast<unsigned long long>(
                             std::hash<std::thread::id>{}(std::this_thread::get_id())),
                         static_cast<long long>(now()), static_cast<long long>(started),
                         static_cast<long long>(now() - started), static_cast<long long>(horizon),
                         static_cast<unsigned long long>(timers),
                         static_cast<unsigned long long>(cursor),
                         static_cast<unsigned long long>(ticks), static_cast<long long>(tick));
            for (unsigned i = 0; i < calls.size(); ++i)
                std::fprintf(stderr, "%s%llu", i ? "," : "",
                             static_cast<unsigned long long>(calls[i]));
            std::fputs("],\"part_ns\":[", stderr);
            for (unsigned i = 0; i < nanoseconds.size(); ++i)
                std::fprintf(stderr, "%s%llu", i ? "," : "",
                             static_cast<unsigned long long>(nanoseconds[i]));
            std::fputs("]}\n", stderr);
            std::fflush(stderr);
        } catch (...) {
            // Diagnostic output cannot change a transaction or a worker outcome.
        }
    }
    void progress(std::int64_t at, std::uint64_t position) noexcept {
        tick = at;
        cursor = position;
        ++ticks;
        if ((ticks & (ticks - 1)) == 0)
            emit("progress");
    }
    void sealed(std::int64_t at, std::uint64_t position) noexcept {
        tick = at;
        cursor = position;
    }
    ~Trace() noexcept {
        if (recording) {
            emit(std::uncaught_exceptions() > exceptions ? "unwinding" : "end");
            current = previous;
        }
    }
};
struct PartScope final {
    Trace* trace{current};
    unsigned part;
    std::int64_t started{};
    explicit PartScope(Part value) noexcept : part(static_cast<unsigned>(value)) {
        if (trace)
            started = now();
    }
    ~PartScope() noexcept {
        if (trace) {
            ++trace->calls[part];
            trace->nanoseconds[part] += static_cast<std::uint64_t>(now() - started);
        }
    }
};
} // namespace cuexis::internal::execution_diagnostics
#else
namespace cuexis::internal::execution_diagnostics {
struct Trace final {
    explicit Trace(const char*, std::int64_t = 0, std::uint64_t = 0, std::uint64_t = 0) noexcept {}
    void emit(const char*) const noexcept {}
    void progress(std::int64_t, std::uint64_t) noexcept {}
    void sealed(std::int64_t, std::uint64_t) noexcept {}
};
struct PartScope final {
    explicit PartScope(Part) noexcept {}
};
} // namespace cuexis::internal::execution_diagnostics
#endif

// The RPA observation channel is a separate, compile-time-guarded part.
#include "execution_diagnostics_rpa.hpp"
