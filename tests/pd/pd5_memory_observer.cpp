// Linked exclusively into opt-in observer executables. Every allocation carries
// its lifetime record, including cross-thread frees; the registry allocates nothing.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cuexis_internal/allocation_observer.hpp>
#include <exception>
#include <iostream>
#include <limits>
#include <malloc.h>
#include <new>
#include <thread>

#include <Windows.h>

#include <Psapi.h>

namespace cuexis::internal::pd::memory {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::uint64_t magic = 0x5044354d454d4f52ULL;
constexpr std::uint64_t maxCount = 2097152, maxRequested = 1073741824, maxStorage = 1342177280;
struct Header final {
    Header* previous{};
    Header* next{};
    void* base{};
    std::uint64_t signature{}, requested{}, charge{}, born{};
};
std::atomic_flag lock = ATOMIC_FLAG_INIT;
Header* first{};
Snapshot totals{};
struct Structure final {
    const char* name{};
    std::size_t peakCount{}, peakCapacity{};
};
std::array<Structure, 128> structures;
struct Guard final {
    Guard() noexcept {
        while (lock.test_and_set(std::memory_order_acquire))
            std::this_thread::yield();
    }
    ~Guard() {
        lock.clear(std::memory_order_release);
    }
};
auto now() noexcept -> std::uint64_t {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch())
            .count());
}
} // namespace

auto allocate(std::size_t bytes, std::size_t alignment) -> void* {
    alignment = std::max(alignment, alignof(Header));
    if (alignment == 0 || (alignment & (alignment - 1)) ||
        bytes > std::numeric_limits<std::size_t>::max() - sizeof(Header) - alignment)
        throw std::bad_alloc{};
    auto* base = std::malloc(std::max(bytes, std::size_t{1}) + sizeof(Header) + alignment);
    if (!base)
        throw std::bad_alloc{};
    const auto address = (reinterpret_cast<std::uintptr_t>(base) + sizeof(Header) + alignment - 1) &
                         ~(alignment - 1);
    auto* header = reinterpret_cast<Header*>(address - sizeof(Header));
    *header = {nullptr, nullptr, base, magic, bytes, _msize(base), now()};
    {
        Guard guard;
        header->next = first;
        if (first)
            first->previous = header;
        first = header;
        ++totals.liveCount;
        totals.requestedBytes += bytes;
        totals.storageBytes += header->charge;
        totals.peakCount = std::max(totals.peakCount, totals.liveCount);
        totals.peakRequestedBytes = std::max(totals.peakRequestedBytes, totals.requestedBytes);
        totals.peakStorageBytes = std::max(totals.peakStorageBytes, totals.storageBytes);
        totals.exceeded = totals.exceeded || totals.liveCount > maxCount ||
                          totals.requestedBytes > maxRequested || totals.storageBytes > maxStorage;
    }
    return reinterpret_cast<void*>(address);
}
void release(void* pointer) noexcept {
    if (!pointer)
        return;
    auto* header =
        reinterpret_cast<Header*>(reinterpret_cast<std::uintptr_t>(pointer) - sizeof(Header));
    if (header->signature != magic)
        std::terminate(); // Mixed allocator or corrupt lifetime record invalidates this process.
    {
        Guard guard;
        if (header->previous)
            header->previous->next = header->next;
        else
            first = header->next;
        if (header->next)
            header->next->previous = header->previous;
        --totals.liveCount;
        totals.requestedBytes -= header->requested;
        totals.storageBytes -= header->charge;
        header->signature = 0;
    }
    std::free(header->base);
}
auto snapshot() noexcept -> Snapshot {
    Guard guard;
    auto result = totals;
    const auto stamp = now();
    for (auto* block = first; block; block = block->next)
        result.oldestAgeUs = std::max(result.oldestAgeUs, stamp - block->born);
    return result;
}
void recordStructure(const char* name, std::size_t count, std::size_t capacity) noexcept {
    Guard guard;
    for (auto& row : structures) {
        if (!row.name || std::strcmp(row.name, name) == 0) {
            row.name = name;
            row.peakCount = std::max(row.peakCount, count);
            row.peakCapacity = std::max(row.peakCapacity, capacity);
            return;
        }
    }
    totals.exceeded = true;
}
namespace {
struct Connect final {
    Connect() noexcept {
        structureSink = recordStructure;
    }
} connect;
} // namespace
void checkpoint(const char* label) {
    const auto value = snapshot();
    std::array<Structure, 128> copied;
    {
        Guard guard;
        copied = structures;
    }
    PROCESS_MEMORY_COUNTERS_EX process{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process),
                              sizeof(process)))
        throw std::runtime_error("PD5 process memory query failed");
    std::cout << "PD5_MEMORY {\"schema\":\"pd5.memory.v1\",\"label\":\"" << label
              << "\",\"bound_kind\":\"global_cpp_new_envelope\",\"exact_partition\":false,"
                 "\"overlap_group\":\"process_cpp_new\",\"allocator\":\"lifetime-observer\","
                 "\"live_count\":"
              << value.liveCount << ",\"requested_cpp_bytes\":" << value.requestedBytes
              << ",\"crt_usable_storage_bytes\":" << value.storageBytes
              << ",\"peak_live_count\":" << value.peakCount
              << ",\"peak_requested_cpp_bytes\":" << value.peakRequestedBytes
              << ",\"peak_crt_usable_storage_bytes\":" << value.peakStorageBytes
              << ",\"oldest_live_age_us\":" << value.oldestAgeUs
              << ",\"working_set_bytes\":" << process.WorkingSetSize
              << ",\"private_commit_bytes\":" << process.PrivateUsage
              << ",\"limit_count\":" << maxCount
              << ",\"limit_requested_cpp_bytes\":" << maxRequested
              << ",\"limit_crt_storage_bytes\":" << maxStorage
              << ",\"exceeded\":" << (value.exceeded ? "true" : "false")
              << ",\"coverage\":\"all linked C++ new/delete including aligned/array/nothrow; "
                 "excludes direct malloc, SDL, driver and OS heaps\"}\n";
    for (const auto& row : copied)
        if (row.name)
            std::cout << "PD5_STRUCTURE {\"label\":\"" << label << "\",\"domain\":\"" << row.name
                      << "\",\"peak_count\":" << row.peakCount
                      << ",\"peak_capacity\":" << row.peakCapacity
                      << ",\"scope\":\"per-instance high-water; bytes covered by overlapping "
                         "process_cpp_new envelope\"}\n";
}
} // namespace cuexis::internal::pd::memory

void* operator new(std::size_t n) {
    return cuexis::internal::pd::memory::allocate(n, alignof(std::max_align_t));
}
void* operator new[](std::size_t n) {
    return ::operator new(n);
}
void operator delete(void* p) noexcept {
    cuexis::internal::pd::memory::release(p);
}
void operator delete[](void* p) noexcept {
    ::operator delete(p);
}
void operator delete(void* p, std::size_t) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    ::operator delete(p);
}
void* operator new(std::size_t n, std::align_val_t a) {
    return cuexis::internal::pd::memory::allocate(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a) {
    return ::operator new(n, a);
}
void operator delete(void* p, std::align_val_t) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    ::operator delete(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    ::operator delete(p);
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(n);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept {
    return ::operator new(n, t);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(n, a);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept {
    return ::operator new(n, a, t);
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
