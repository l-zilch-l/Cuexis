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

// These definitions replace the program's global allocation functions: `operator new` returns
// malloc memory and the matching `operator delete` hands it back to free. GCC inlines both and
// then reports the paired calls as a new/free mismatch, which is a false positive for a
// replacement pair, so the diagnostic is disabled around the definitions.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
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
// The nothrow forms belong to the same replaceable set. Without them libstdc++ allocates the
// std::stable_sort temporary buffer (used to order diagnostics) through the library's nothrow
// new, and the probe's malloc-backed delete then releases a library allocation, which
// AddressSanitizer reports as an alloc-dealloc mismatch. Delegate to the tracked throwing forms
// so the existing failure-injection counts keep their positions; an injected failure returns
// nullptr instead of propagating out of a nothrow function.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return ::operator new(size, std::nothrow);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
    ::operator delete[](p);
}
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size, alignment);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return ::operator new(size, alignment, std::nothrow);
}
void operator delete(void* p, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    ::operator delete(p, alignment);
}
void operator delete[](void* p, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    ::operator delete[](p, alignment);
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
