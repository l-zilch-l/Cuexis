#pragma once

#include <chrono>
#include <cstdint>
#include <stdexcept>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace pd_measure {
struct Counters {
    std::uint64_t cpu100ns{}, cycles{};
};
struct Sample {
    double wallUs{}, cpuUs{};
    std::uint64_t cycles{};
    std::uint64_t qpcStart{}, qpcEnd{};
};
inline auto qpc() -> std::uint64_t {
#if defined(_WIN32)
    LARGE_INTEGER value{};
    if (!QueryPerformanceCounter(&value) || value.QuadPart < 0)
        throw std::runtime_error("PD QPC query failed");
    return static_cast<std::uint64_t>(value.QuadPart);
#else
    throw std::runtime_error("PD QPC requires Windows");
#endif
}
inline auto qpcFrequency() -> std::uint64_t {
#if defined(_WIN32)
    static const auto frequency = [] {
        LARGE_INTEGER value{};
        if (!QueryPerformanceFrequency(&value) || value.QuadPart <= 0)
            throw std::runtime_error("PD QPC frequency query failed");
        return static_cast<std::uint64_t>(value.QuadPart);
    }();
    return frequency;
#else
    throw std::runtime_error("PD QPC requires Windows");
#endif
}
inline auto counters() -> Counters {
#if defined(_WIN32)
    FILETIME creation{}, exit{}, kernel{}, user{};
    ULONG64 cycles{};
    if (!GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user) ||
        !QueryThreadCycleTime(GetCurrentThread(), &cycles))
        throw std::runtime_error("PD thread clock query failed");
    const auto value = [](FILETIME t) -> std::uint64_t {
        return (static_cast<std::uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return {value(kernel) + value(user), cycles};
#else
    throw std::runtime_error("PD thread clocks require Windows");
#endif
}
inline auto delta(Counters start, Counters end, double wallUs, std::uint64_t qpcStart,
                  std::uint64_t qpcEnd) -> Sample {
    if (end.cpu100ns < start.cpu100ns || end.cycles < start.cycles)
        throw std::runtime_error("PD thread clock moved backwards");
    return {wallUs, static_cast<double>(end.cpu100ns - start.cpu100ns) / 10.0,
            end.cycles - start.cycles, qpcStart, qpcEnd};
}
template <class Work> auto measure(Work&& work) -> Sample {
    const auto frequency = qpcFrequency();
    const auto before = counters();
    const auto start = qpc();
    work();
    const auto end = qpc();
    if (end < start)
        throw std::runtime_error("PD QPC moved backwards");
    return delta(before, counters(),
                 static_cast<double>(end - start) * 1000000.0 / static_cast<double>(frequency),
                 start, end);
}
} // namespace pd_measure
