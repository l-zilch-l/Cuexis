#pragma once
// Logical GL buffer/texture storage only. Driver residency is a separate OS counter.
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
#include <array>
#include <cstdint>
#include <cuexis_internal/allocation_observer.hpp>
#include <iostream>
namespace cuexis::render_opengl::test_only {
struct GpuStorage final {
    struct Entry {
        unsigned kind{}, handle{};
        std::uint64_t bytes{};
    };
    inline static std::array<Entry, 8192> entries{};
    inline static std::uint64_t count{}, bytes{}, peakCount{}, peakBytes{};
    inline static bool exceeded{};
    static void charge(unsigned kind, unsigned handle, std::uint64_t value) noexcept {
        if (!handle) {
            exceeded = true;
            return;
        }
        for (auto& entry : entries) {
            if (entry.handle == handle && entry.kind == kind) {
                bytes -= entry.bytes;
                entry.bytes = value;
                bytes += value;
                highWater();
                return;
            }
        }
        for (auto& entry : entries) {
            if (!entry.handle) {
                entry = {kind, handle, value};
                ++count;
                bytes += value;
                highWater();
                return;
            }
        }
        exceeded = true;
    }
    static void highWater() noexcept {
        peakCount = count > peakCount ? count : peakCount;
        peakBytes = bytes > peakBytes ? bytes : peakBytes;
        exceeded |= bytes > 268435456;
    }
    static void release(unsigned kind, unsigned handle) noexcept {
        for (auto& entry : entries) {
            if (entry.kind == kind && entry.handle == handle && handle) {
                --count;
                bytes -= entry.bytes;
                entry = {};
                return;
            }
        }
    }
    static void report(const char* label, bool drained) {
        std::cout
            << "PD5_GPU_MEMORY {\"label\":\"" << label << "\",\"live_count\":" << count
            << ",\"logical_storage_bytes\":" << bytes << ",\"peak_count\":" << peakCount
            << ",\"peak_logical_storage_bytes\":" << peakBytes
            << ",\"limit_count\":8192,\"limit_logical_storage_bytes\":268435456,\"gpu_drained\":"
            << (drained ? "true" : "false") << ",\"exceeded\":" << (exceeded ? "true" : "false")
            << ",\"coverage\":\"GL buffer and RGBA8 texture backing; shader/driver/framebuffer "
               "excluded\"}\n";
    }
};
} // namespace cuexis::render_opengl::test_only
#else
namespace cuexis::render_opengl::test_only {
struct GpuStorage final {
    static void charge(unsigned, unsigned, unsigned long long) noexcept {}
    static void release(unsigned, unsigned) noexcept {}
    static void report(const char*, bool) {}
};
} // namespace cuexis::render_opengl::test_only
#endif
