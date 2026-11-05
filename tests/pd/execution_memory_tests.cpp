#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <thread>

namespace memory = cuexis::internal::pd::memory;

TEST_CASE("Execution observer preserves nested allocation domains and cross-thread release",
          "[execution][memory][contract]") {
    const auto initial = memory::snapshot();
    void* block{};
    {
        const memory::ExecutionTagScope outer{memory::ownerMain, memory::domainRetained};
        const auto before = memory::currentTag();
        {
            const memory::ExecutionTagScope inner{memory::ownerMain, memory::domainCodec};
            block = ::operator new(12345, std::align_val_t{256});
            const auto partition = memory::partition();
            const auto& cell =
                partition.cells[memory::cellId(memory::ownerMain, memory::domainCodec)];
            CHECK(cell.requestedBytes >= 12345);
            CHECK(partition.exact);
        }
        CHECK(memory::currentTag() == before);
    }
    const auto during = memory::snapshot();
    CHECK(during.requestedBytes >= initial.requestedBytes + 12345);
    std::thread freeing{[block] { ::operator delete(block, std::align_val_t{256}); }};
    freeing.join();
    const auto final = memory::snapshot();
    CHECK(final.requestedBytes == initial.requestedBytes);
    CHECK(final.liveCount == initial.liveCount);
}

TEST_CASE("Execution observer checkpoint uses a coherent global and domain snapshot",
          "[execution][memory][contract]") {
    std::atomic<bool> stop{};
    std::thread worker{[&] {
        const memory::ExecutionTagScope tag{memory::ownerPlaybackWorker, memory::domainScratch};
        while (!stop.load(std::memory_order_acquire)) {
            auto* block = ::operator new(4096);
            std::atomic_signal_fence(std::memory_order_seq_cst);
            ::operator delete(block);
        }
    }};
    memory::checkpoint("concurrent-contract");
    stop.store(true, std::memory_order_release);
    worker.join();
    CHECK(memory::partition().exact);
}
