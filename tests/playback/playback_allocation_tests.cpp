#include <cuexis/playback/playback_session.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#if defined(_MSC_VER)
#include <malloc.h>
#endif
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::atomic_size_t allocationCount{};
std::atomic_bool trackingAllocations{};

#if defined(_MSC_VER) && defined(_DEBUG)
_CRT_ALLOC_HOOK previousAllocationHook{};

int allocationHook(int allocationType, void* data, std::size_t size, int blockType,
                   long requestNumber, const unsigned char* fileName, int lineNumber) {
    if (previousAllocationHook != nullptr &&
        previousAllocationHook(allocationType, data, size, blockType, requestNumber, fileName,
                               lineNumber) == 0) {
        return 0;
    }
    if (allocationType == _HOOK_ALLOC && trackingAllocations.load(std::memory_order_relaxed)) {
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    }
    return 1;
}
#endif

void beginAllocationTracking() {
    allocationCount.store(0, std::memory_order_relaxed);
#if defined(_MSC_VER) && defined(_DEBUG)
    previousAllocationHook = _CrtSetAllocHook(allocationHook);
#endif
    trackingAllocations.store(true, std::memory_order_relaxed);
}

[[nodiscard]] auto endAllocationTracking() -> std::size_t {
    trackingAllocations.store(false, std::memory_order_relaxed);
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetAllocHook(previousAllocationHook);
    previousAllocationHook = nullptr;
#endif
    return allocationCount.load(std::memory_order_relaxed);
}

#if !defined(_MSC_VER) || !defined(_DEBUG)
[[nodiscard]] auto allocate(std::size_t size) -> void* {
    if (trackingAllocations.load(std::memory_order_relaxed)) {
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    }
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc{};
}

[[nodiscard]] auto allocateAligned(std::size_t size, std::size_t alignment) -> void* {
    if (trackingAllocations.load(std::memory_order_relaxed)) {
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    }
#if defined(_MSC_VER)
    void* memory = _aligned_malloc(size == 0 ? 1 : size, alignment);
#else
    const std::size_t requested = size == 0 ? alignment : size;
    const std::size_t remainder = requested % alignment;
    if (remainder != 0 && requested > std::numeric_limits<std::size_t>::max() - alignment + 1) {
        throw std::bad_alloc{};
    }
    const std::size_t adjusted = remainder == 0 ? requested : requested + alignment - remainder;
    void* memory = std::aligned_alloc(alignment, adjusted);
#endif
    if (memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc{};
}
#endif

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not read Stage 2 fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

} // namespace

#if !defined(_MSC_VER) || !defined(_DEBUG)
void* operator new(std::size_t size) {
    return allocate(size);
}

void* operator new[](std::size_t size) {
    return allocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}

void deallocate(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory) noexcept {
    deallocate(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    deallocate(memory);
}

void operator delete(void* memory, std::align_val_t) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

void operator delete[](void* memory, std::align_val_t alignment) noexcept {
    operator delete(memory, alignment);
}

void operator delete(void* memory, std::size_t, std::align_val_t alignment) noexcept {
    operator delete(memory, alignment);
}

void operator delete[](void* memory, std::size_t, std::align_val_t alignment) noexcept {
    operator delete(memory, alignment);
}
#endif

TEST_CASE("Stage 2 playback update and reusable extraction allocate nothing after warmup",
          "[playback][stage2][allocation]") {
    const auto chart = readFile(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "charts" /
                                "stage2_example.cuexis.chart.json");
    cuexis::playback::PlaybackSession session;
    REQUIRE(session.loadChart(chart).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    cuexis::playback::FrameSnapshot snapshot;
    REQUIRE(session.extractFrame({.width = 1280, .height = 720}, snapshot).has_value());

    beginAllocationTracking();
    bool succeeded = true;
    for (std::size_t index = 1; index <= 64; ++index) {
        const double chartTimeMs = static_cast<double>(index) * 10.0;
        const auto updated =
            session.update({.chartTimeMs = chartTimeMs, .simulationDeltaTimeMs = 10.0});
        const auto extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
        if (!updated || !extracted) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = endAllocationTracking();

    std::cout << "FrameSnapshot bytes=" << sizeof(cuexis::playback::FrameSnapshot)
              << " ObjectSnapshot bytes=" << sizeof(cuexis::playback::FrameSnapshot::ObjectSnapshot)
              << '\n';
    CHECK(succeeded);
    CHECK(allocations == 0);
}
