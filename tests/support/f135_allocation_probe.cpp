// F135 test-only replacement of the replaceable global allocation functions. Forwarding to
// malloc/free keeps the normal topology unchanged; only the probe flags change behaviour.

#include "f135_allocation_probe.hpp"

namespace {

void* allocate(std::size_t size) {
    if (f135_probe::tracking.load(std::memory_order_relaxed))
        f135_probe::attempts.fetch_add(1, std::memory_order_relaxed);
    if (f135_probe::failAll.load(std::memory_order_relaxed))
        throw std::bad_alloc{};
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr)
        throw std::bad_alloc{};
    if (f135_probe::tracking.load(std::memory_order_relaxed))
        f135_probe::successes.fetch_add(1, std::memory_order_relaxed);
    return memory;
}

void release(void* memory) noexcept {
    std::free(memory);
}

} // namespace

void* operator new(std::size_t size) {
    return allocate(size);
}
void* operator new[](std::size_t size) {
    return allocate(size);
}
void operator delete(void* memory) noexcept {
    release(memory);
}
void operator delete[](void* memory) noexcept {
    release(memory);
}
void operator delete(void* memory, std::size_t) noexcept {
    release(memory);
}
void operator delete[](void* memory, std::size_t) noexcept {
    release(memory);
}
