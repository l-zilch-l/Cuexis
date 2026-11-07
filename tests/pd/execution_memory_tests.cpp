#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/semantic_memory.hpp>
#include <string>
#include <thread>
#include <vector>

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
    const memory::SemanticPage pages[] = {{static_cast<char*>(page) + 16, 256},
                                          {static_cast<char*>(page) + 128, 384},
                                          {static_cast<char*>(page) + 16, 256},
                                          {block, 12345}};
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

TEST_CASE("Execution semantic census recognizes actual standard allocator pages",
          "[execution][memory][semantic-contract]") {
    int identity{};
    memory::semanticPhase(&identity, memory::SemanticPhase::constructing);
    std::vector<std::uint64_t> page(1024);
    const memory::SemanticPage pages[] = {{page.data(), page.size() * sizeof(page[0])}};
    memory::semanticPhase(&identity, memory::SemanticPhase::prepared);
    const auto before = memory::snapshot();
    memory::semanticCensus(&identity, pages);
    const auto after = memory::snapshot();
    CHECK_FALSE(after.exceeded);
    CHECK(after.requestedBytes == before.requestedBytes);
    CHECK(memory::semanticSnapshot(&identity).memory.requestedBytes == 0);
    memory::semanticPhase(&identity, memory::SemanticPhase::retiring);
}

TEST_CASE("Execution semantic buffer traversal preserves capacity and inline storage",
          "[execution][memory][semantic-contract]") {
    using Kind = memory::SemanticPage::Kind;
    std::array<memory::SemanticPage, 4> storage{};
    memory::SemanticPages pages{storage};
    std::string inlineText;
    std::string heapText(160, 'x');
    heapText.reserve(512);
    std::vector<unsigned> values;
    values.reserve(64);
    values.push_back(7);
    const auto before = memory::snapshot();
    pages.string(inlineText, Kind::cameraString);
    pages.string(heapText, Kind::materialString);
    pages.vector(values, Kind::transformOrder);
    REQUIRE(pages.valid());
    const auto spans = pages.pages();
    REQUIRE(spans.size() == 2);
    CHECK(spans[0].allocation == heapText.data());
    CHECK(spans[0].bytes == heapText.capacity() + 1);
    CHECK(spans[0].kind == Kind::materialString);
    CHECK(spans[1].bytes == values.capacity() * sizeof(unsigned));
    CHECK(spans[1].kind == Kind::transformOrder);
    CHECK(memory::snapshot().requestedBytes == before.requestedBytes);
    int identity{};
    memory::semanticPhase(&identity, memory::SemanticPhase::constructing);
    memory::semanticPhase(&identity, memory::SemanticPhase::prepared);
    memory::semanticCensus(&identity, spans);
    CHECK_FALSE(memory::snapshot().exceeded);
    memory::semanticPhase(&identity, memory::SemanticPhase::retiring);

    memory::SemanticPages bounded{std::span<memory::SemanticPage>{storage}.first(1)};
    bounded.string(heapText, Kind::materialString);
    bounded.vector(values, Kind::transformOrder);
    CHECK_FALSE(bounded.valid());
    REQUIRE(bounded.pages().size() == 1);
    CHECK(bounded.pages()[0].allocation == nullptr);
    CHECK(bounded.pages()[0].bytes == 0);
}

TEST_CASE("Execution semantic census handles a buffer moving between owners",
          "[execution][memory][semantic-contract]") {
    using Kind = memory::SemanticPage::Kind;
    int identity{};
    memory::semanticPhase(&identity, memory::SemanticPhase::constructing);
    memory::semanticPhase(&identity, memory::SemanticPhase::prepared);
    std::string component(160, 'x');
    std::string candidate;
    auto census = [&] {
        std::array<memory::SemanticPage, 1> storage{};
        memory::SemanticPages pages{storage};
        pages.string(component, Kind::materialString);
        memory::semanticCensus(&identity, pages.pages());
    };
    census();
    component.swap(candidate);
    census(); // The old component allocation now belongs only to the candidate.
    CHECK(component.empty());
    CHECK(candidate.size() == 160);
    CHECK(memory::semanticSnapshot(&identity).memory.liveCount == 0);
    CHECK_FALSE(memory::snapshot().exceeded);
    memory::semanticPhase(&identity, memory::SemanticPhase::retiring);
}

// These intentional-invalidity tests each need their own process. They never run
// the capacity workload or contribute diagnostic/acceptance measurements.
TEST_CASE("Execution semantic observer rejects an unregistered owner",
          "[.][execution][memory-invalid-contract]") {
    int unregistered{};
    void* block{};
    {
        const memory::SemanticOwnerScope root{&unregistered};
        block = ::operator new(64);
    }
    ::operator delete(block);
    CHECK(memory::snapshot().exceeded);
}

TEST_CASE("Execution semantic observer rejects root table exhaustion",
          "[.][execution][memory-invalid-contract]") {
    std::array<int, memory::semanticRootLimit + 1> identities{};
    for (auto& identity : identities)
        memory::semanticPhase(&identity, memory::SemanticPhase::constructing);
    CHECK(memory::snapshot().exceeded);
}

TEST_CASE("Execution semantic observer rejects a page span beyond its backing allocation",
          "[.][execution][memory-invalid-contract]") {
    int identity{};
    memory::semanticPhase(&identity, memory::SemanticPhase::constructing);
    auto* block = ::operator new(64);
    const memory::SemanticPage pages[] = {{static_cast<char*>(block) + 32, 33}};
    memory::semanticCensus(&identity, pages);
    ::operator delete(block);
    memory::semanticPhase(&identity, memory::SemanticPhase::retiring);
    CHECK(memory::snapshot().exceeded);
}
