#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/semantic_memory.hpp>
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

TEST_CASE("Execution semantic roots preserve identity through transfer and actual last free",
          "[execution][memory][semantic-contract]") {
    int identity{};
    memory::semanticPhase(&identity, memory::SemanticPhase::constructing);
    void* block{};
    {
        const memory::SemanticOwnerScope root{&identity};
        const memory::ExecutionTagScope origin{memory::ownerPlaybackWorker, memory::domainWorld};
        block = ::operator new(12345, std::align_val_t{256});
    }
    auto* page = ::operator new(512);
    const auto before = memory::snapshot();
    const auto initial = memory::semanticSnapshot(&identity);
    REQUIRE(initial.valid);
    CHECK(initial.memory.liveCount == 1);
    CHECK(initial.memory.requestedBytes == 12345);
    memory::semanticPhase(&identity, memory::SemanticPhase::prepared);
    // Repeated page aliases must be charged only once, and only in this census.
    const memory::SemanticPage pages[] = {{page, 512}, {page, 512}, {block, 12345}};
    memory::semanticCensus(&identity, pages);
    memory::semanticPhase(&identity, memory::SemanticPhase::installed);
    memory::semanticPhase(&identity, memory::SemanticPhase::active);
    const auto active = memory::semanticSnapshot(&identity);
    CHECK(active.valid);
    CHECK(active.rootId == initial.rootId);
    CHECK(active.memory.requestedBytes == initial.memory.requestedBytes);
    CHECK(memory::snapshot().requestedBytes == before.requestedBytes);
    CHECK(memory::partition().exact);
    memory::semanticPhase(&identity, memory::SemanticPhase::retiring);
    CHECK(memory::semanticSnapshot(&identity).phase == memory::SemanticPhase::retiring);
    std::thread freeing{[block] { ::operator delete(block, std::align_val_t{256}); }};
    freeing.join();
    const auto released = memory::semanticSnapshot(&identity);
    CHECK(released.phase == memory::SemanticPhase::released);
    CHECK(released.memory.liveCount == 0);
    CHECK(released.memory.requestedBytes == 0);
    CHECK(released.releasedUs >= released.retiredUs);
    ::operator delete(page);
    memory::semanticPhase(&identity, memory::SemanticPhase::constructing);
    const auto reused = memory::semanticSnapshot(&identity);
    CHECK(reused.rootId > released.rootId);
    CHECK(reused.memory.liveCount == 0);
    memory::semanticPhase(&identity, memory::SemanticPhase::retiring);
    CHECK(memory::semanticSnapshot(&identity).phase == memory::SemanticPhase::released);
    memory::checkpoint("semantic-contract-final");
}
