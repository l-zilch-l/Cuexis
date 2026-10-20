#pragma once
// Include once per test executable. The one-shot failure leaves error construction usable.
#include <cstdlib>
#include <new>
#if defined(_MSC_VER) && _ITERATOR_DEBUG_LEVEL != 0
#include <xmemory>
#endif
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace cuexis::pd_allocations {
inline thread_local bool active{}, failed{};
inline thread_local std::size_t count{}, failAt{};
inline void begin(std::size_t fail = 0) noexcept {
    count = 0;
    failAt = fail;
    failed = false;
    active = true;
}
inline auto end() noexcept -> std::size_t {
    active = false;
    return count;
}
inline void observe(std::size_t size) {
#if defined(_MSC_VER) && _ITERATOR_DEBUG_LEVEL != 0
    // MSVC constructs iterator proxies inside noexcept container constructors. Injecting
    // there terminates inside the STL instead of exercising a recoverable transaction.
    // Debug omits this size class; Release (no proxies) sweeps every allocation.
    if (size == sizeof(std::_Container_proxy))
        return;
#else
    static_cast<void>(size);
#endif
    if (active && ++count == failAt) {
        failAt = 0;
        failed = true;
        throw std::bad_alloc{};
    }
}
} // namespace cuexis::pd_allocations
void* operator new(std::size_t size) {
    cuexis::pd_allocations::observe(size);
    if (auto p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete[](void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}
void* operator new(std::size_t size, std::align_val_t alignment) {
    cuexis::pd_allocations::observe(size);
    const auto a = static_cast<std::size_t>(alignment);
#if defined(_WIN32)
    if (auto p = _aligned_malloc(size ? size : 1, a))
        return p;
#else
    if (auto p = std::aligned_alloc(a, ((size ? size : 1) + a - 1) / a * a))
        return p;
#endif
    throw std::bad_alloc{};
}
void* operator new[](std::size_t n, std::align_val_t a) {
    return ::operator new(n, a);
}
void operator delete(void* p, std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept {
    ::operator delete(p, a);
}
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept {
    ::operator delete(p, a);
}
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept {
    ::operator delete(p, a);
}
