#pragma once

// Counts requested allocation bytes, not CRT heap overhead. Global replacement is static-only:
// shared-library tests must use RESOURCE_ONLY and explicitly pair allocate/release in a resource.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cuexis_internal/presentation_metrics.hpp>
#include <limits>
#include <new>

namespace pd_allocation {
inline std::atomic<std::size_t> live{}, peak{}, count{}, bytes{};
inline thread_local bool tracking{};
struct Header {
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
    *header = {base, size};
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
void operator delete(void* value, const std::nothrow_t&) noexcept {
    pd_allocation::release(value);
}
void operator delete[](void* value, const std::nothrow_t&) noexcept {
    pd_allocation::release(value);
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
void operator delete(void* value, std::align_val_t, const std::nothrow_t&) noexcept {
    pd_allocation::release(value);
}
void operator delete[](void* value, std::align_val_t, const std::nothrow_t&) noexcept {
    pd_allocation::release(value);
}
#endif
