#pragma once

// Counts requested allocation bytes, not CRT heap overhead.
//
// The replacing scheme offsets each payload behind a tagged header inside one malloc block. That
// layout is only legal when every delete of a payload runs the operator delete below, so the
// replacement is compiled only for topologies whose C++ runtime is a static library linked into the
// same module. Two topologies violate that: a shared product library (the modules own their
// allocators) and MinGW, which always links libstdc++-6.dll. In both cases the tests build with
// RESOURCE_ONLY and observe explicit allocate/release pairs instead of global operator traffic.
//
// The MinGW limit is measured, not assumed: a three-line Catch2 test case that includes this header
// passes its assertion and then terminates with 0xc0000374 (STATUS_HEAP_CORRUPTION) while the same
// source links clean with -static-libstdc++ -static-libgcc, and remains clean with RESOURCE_ONLY.
// libstdc++-6.dll exports std::string and std::ostringstream members whose deletes call the DLL's
// own operator delete, so a payload that is not a CRT block reaches the UCRT heap as garbage and
// the corruption surfaces at the next heap call. Stock MinGW operator new is malloc, so the
// mismatch stays latent until a replacement changes the layout.
//
// A replacement operator delete also receives blocks this executable never allocated. Those are
// recognised by magic and freed with the CRT free that pairs with the allocating runtime, and
// foreignFrees records how many deletes escaped the census so a census can state its own coverage.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cuexis_internal/presentation_metrics.hpp>
#include <limits>
#include <new>

namespace pd_allocation {
inline std::atomic<std::size_t> live{}, peak{}, count{}, bytes{};
inline std::atomic<std::size_t> foreignFrees{};
inline thread_local bool tracking{};
inline constexpr std::uint64_t blockMagic = 0x4375584973504441ULL;

struct Header {
    std::uint64_t magic;
    void* base;
    std::size_t size;
};
inline auto allocate(std::size_t size, std::size_t alignment) -> void* {
    alignment = alignment < alignof(Header) ? alignof(Header) : alignment;
    if (size > std::numeric_limits<std::size_t>::max() - sizeof(Header) - alignment)
        throw std::bad_alloc{};
    auto* base = std::malloc(size + sizeof(Header) + alignment);
    if (!base)
        throw std::bad_alloc{};
    const auto address = (reinterpret_cast<std::uintptr_t>(base) + sizeof(Header) + alignment - 1) &
                         ~(static_cast<std::uintptr_t>(alignment) - 1);
    auto* result = reinterpret_cast<void*>(address);
    auto* header = static_cast<Header*>(result) - 1;
    *header = {blockMagic, base, size};
    const auto now = live.fetch_add(size, std::memory_order_relaxed) + size;
    cuexis::internal::pd::Timer::allocated(size);
    auto high = peak.load(std::memory_order_relaxed);
    while (now > high && !peak.compare_exchange_weak(high, now, std::memory_order_relaxed)) {
    }
    if (tracking) {
        count.fetch_add(1, std::memory_order_relaxed);
        bytes.fetch_add(size, std::memory_order_relaxed);
    }
    return result;
}
inline void release(void* value) noexcept {
    if (!value)
        return;
    auto* header = static_cast<Header*>(value) - 1;
    if (header->magic != blockMagic) {
        // Not a block of this executable: free it the way the allocating runtime would.
        foreignFrees.fetch_add(1, std::memory_order_relaxed);
        std::free(value);
        return;
    }
    header->magic = 0;
    cuexis::internal::pd::Timer::released(header->size);
    live.fetch_sub(header->size, std::memory_order_relaxed);
    std::free(header->base);
}
} // namespace pd_allocation

#if !defined(CUEXIS_PD_ALLOCATION_PROBE_RESOURCE_ONLY)
void* operator new(std::size_t size) {
    return pd_allocation::allocate(size, alignof(std::max_align_t));
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void* operator new(std::size_t size, std::align_val_t align) {
    return pd_allocation::allocate(size, static_cast<std::size_t>(align));
}
void* operator new[](std::size_t size, std::align_val_t align) {
    return ::operator new(size, align);
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size, align);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size, align);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void* value) noexcept {
    pd_allocation::release(value);
}
void operator delete[](void* value) noexcept {
    pd_allocation::release(value);
}
void operator delete(void* value, std::size_t) noexcept {
    pd_allocation::release(value);
}
void operator delete[](void* value, std::size_t) noexcept {
    pd_allocation::release(value);
}
void operator delete(void* value, std::align_val_t) noexcept {
    pd_allocation::release(value);
}
void operator delete[](void* value, std::align_val_t) noexcept {
    pd_allocation::release(value);
}
void operator delete(void* value, std::size_t, std::align_val_t) noexcept {
    pd_allocation::release(value);
}
void operator delete[](void* value, std::size_t, std::align_val_t) noexcept {
    pd_allocation::release(value);
}
void operator delete(void* value, const std::nothrow_t&) noexcept {
    pd_allocation::release(value);
}
void operator delete[](void* value, const std::nothrow_t&) noexcept {
    pd_allocation::release(value);
}
void operator delete(void* value, std::align_val_t, const std::nothrow_t&) noexcept {
    pd_allocation::release(value);
}
void operator delete[](void* value, std::align_val_t, const std::nothrow_t&) noexcept {
    pd_allocation::release(value);
}
#endif
