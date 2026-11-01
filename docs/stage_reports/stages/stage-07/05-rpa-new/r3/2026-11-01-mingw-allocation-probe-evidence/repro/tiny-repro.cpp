#include "pd_allocation_probe.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdio>

// Published so a debugger can ask whether the pointer reaching the CRT free is this buffer, and so
// the transcript can show the sixteen bytes that precede it. extern "C" keeps the symbol unmangled,
// which is what lets the breakpoint commands compare against it by name.
extern "C" void* cux_probe_buffer = nullptr;

TEST_CASE("tiny replacement smoke") {
    auto text = std::string(64, 'x');
    cux_probe_buffer = text.data();
    const auto* words =
        reinterpret_cast<const std::uint64_t*>(static_cast<const char*>(text.data()) - 16);
    std::printf("PROBE_BLOCK ptr=%p w_minus_16=0x%016llx w_minus_8=0x%016llx\n",
                static_cast<const void*>(text.data()),
                static_cast<unsigned long long>(words[0]),
                static_cast<unsigned long long>(words[1]));
    std::fflush(stdout);
    REQUIRE(text.size() == 64);
}
