// Separates two questions the Catch2-based reproduction cannot separate: does the offset-payload
// replacement survive a plain new/delete round trip, and does it survive a `new` whose object is
// released with `std::free`?
//
//   isolation-no-framework.exe roundtrip   new[]/delete[], a heap std::string, an aligned new/delete
//   isolation-no-framework.exe mismatch    adds one new + std::free pair
//
// The first mode is the control for "the offset payload alone breaks the heap": if it passes under
// both C++ runtimes, the dynamic/statically linked difference in the framework reproduction is not
// about the payload's shape but about which release paths exist. The second mode is the control for
// the suspected release path itself, and it needs no framework at all.
//
// Built and run by record_isolation_matrix.py with both C++ runtimes.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "pd_allocation_probe.hpp"

extern "C" void* cux_probe_buffer = nullptr;

namespace {
void publish(const std::string& text) {
    cux_probe_buffer = const_cast<char*>(text.data());
    std::printf("PROBE_BLOCK ptr=%p w_minus_16=%p w_minus_8=%zu\n", static_cast<void*>(
                    const_cast<char*>(text.data())),
                *reinterpret_cast<void**>(const_cast<char*>(text.data()) - 16),
                *reinterpret_cast<std::size_t*>(const_cast<char*>(text.data()) - 8));
    std::fflush(stdout);
}
} // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "roundtrip";
    std::printf("MODE %s\n", mode.c_str());

    std::string text(64, 'x');
    publish(text);
    std::printf("STRING ok size=%zu\n", text.size());

    char* buffer = new char[65];
    std::memset(buffer, 'y', 65);
    std::printf("ARRAY ok last=%d\n", static_cast<int>(buffer[64]));
    delete[] buffer;
    std::printf("ARRAY released\n");

    auto* aligned = new (std::align_val_t(32)) char[65];
    std::printf("ALIGNED ok\n");
    delete[] aligned;
    std::printf("ALIGNED released\n");

    if (mode == "mismatch") {
        void* block = ::operator new(65);
        std::printf("MISMATCH block=%p\n", block);
        std::fflush(stdout);
        std::free(block);
        std::printf("MISMATCH released\n");
    }

    std::printf("DONE\n");
    std::fflush(stdout);
    return 0;
}
