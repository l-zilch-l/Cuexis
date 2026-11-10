// Control for the archive's central claim: the corruption comes from the offset payload layout, not
// from replacing the global operators. This file replaces the same operators with a pass-through to
// the C runtime, keeps the payload at the block start, and runs the identical test case. A clean exit
// here next to 0xc0000374 from tiny-repro.cpp isolates the header offset as the trigger.
#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <new>
#include <string>

void* operator new(std::size_t size) {
    if (void* block = std::malloc(size == 0 ? 1 : size))
        return block;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void* block) noexcept {
    std::free(block);
}
void operator delete[](void* block) noexcept {
    std::free(block);
}
void operator delete(void* block, std::size_t) noexcept {
    std::free(block);
}
void operator delete[](void* block, std::size_t) noexcept {
    std::free(block);
}

TEST_CASE("pass-through replacement smoke") {
    auto text = std::string(64, 'x');
    REQUIRE(text.size() == 64);
}
