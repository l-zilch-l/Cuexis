// Linked exclusively into opt-in observer executables. Every allocation carries
// its lifetime record, including cross-thread frees; the registry allocates nothing.
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/semantic_memory.hpp>
#include <exception>
#include <iostream>
#include <limits>
#include <malloc.h>
#include <new>
#include <string_view>
#include <thread>

#include <Windows.h>

#include <Psapi.h>

namespace cuexis::internal::pd::memory {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::uint32_t magicSignature = 0x5044354dU; // "PD5M"
constexpr std::uint64_t maxCount = 8388608;
// O1 process guard is independent of the per-domain product budgets.
constexpr std::uint64_t maxRequested = 17179869184ULL, maxStorage = 17179869184ULL;
// Extended collector v2: signature(32) | origin tag(16) | root slot(15) | tagged(1).
// The historical collector is unchanged. The additional allocation ID costs 8 bytes.
constexpr std::uint64_t flagTagged = 1ULL;
constexpr unsigned tagShift = 16, signatureShift = 32;
struct Header final {
    Header* previous{};
    Header* next{};
    void* base{};
    std::uint64_t packed{};
    std::uint64_t requested{}, charge{}, born{};
    std::uint64_t allocationId{};
};
static_assert(sizeof(Header) == 64, "extended observer v2 header identity changed");
std::atomic_flag lock = ATOMIC_FLAG_INIT;
Header* first{};
Snapshot totals{};
constexpr std::size_t semanticRootCapacity = semanticRootLimit;
struct SemanticRoot final {
    const void* address{};
    SemanticSnapshot snapshot;
};
std::array<SemanticRoot, semanticRootCapacity> semanticRoots;
std::uint64_t nextAllocationId{}, nextRootId{}, nextCensusId{};
// Framework/bootstrap allocations are fixture overhead. Worker entry points establish
// their product owner before constructing any session or device state.
thread_local std::uint16_t activeTag = cellId(ownerProbe, domainFixture);
std::array<Cell, cellCapacity> cells;
// Unique-allocation aggregates are updated under the same lock as the global book.
constexpr std::size_t budgetCount = 25;
constexpr const char* budgetNames[budgetCount] = {"fixture",
                                                  "content.decoded",
                                                  "prepare.transient",
                                                  "owning.results",
                                                  "execution.retained",
                                                  "state.current",
                                                  "state.candidate",
                                                  "state.scratch",
                                                  "state.index",
                                                  "delivery.attempt",
                                                  "history.request",
                                                  "delivery.receipt",
                                                  "journal",
                                                  "static",
                                                  "unclassified",
                                                  "state.world",
                                                  "delivery.payload",
                                                  "command.session",
                                                  "command.audio",
                                                  "command.host",
                                                  "command.capture",
                                                  "steps",
                                                  "codec.buffers",
                                                  "audio",
                                                  "delivery.ready"};
constexpr std::uint8_t domainBudget[domainCount] = {
#define CUEXIS_MEMORY_DOMAIN(origin, budget, slot) slot,
#include <cuexis_internal/execution_memory_domains.def>
#undef CUEXIS_MEMORY_DOMAIN
};
constexpr auto mappingMatches() -> bool {
    constexpr const char* origins[domainCount] = {
#define CUEXIS_MEMORY_DOMAIN(origin, budget, slot) origin,
#include <cuexis_internal/execution_memory_domains.def>
#undef CUEXIS_MEMORY_DOMAIN
    };
    constexpr const char* budgets[domainCount] = {
#define CUEXIS_MEMORY_DOMAIN(origin, budget, slot) budget,
#include <cuexis_internal/execution_memory_domains.def>
#undef CUEXIS_MEMORY_DOMAIN
    };
    for (std::size_t index = 0; index < domainCount; ++index)
        if (std::string_view{origins[index]} != domainNames[index] ||
            domainBudget[index] >= budgetCount ||
            std::string_view{budgets[index]} != budgetNames[domainBudget[index]])
            return false;
    return true;
}
static_assert(mappingMatches(), "extended observer names and budget slots must agree");

std::array<Cell, budgetCount> aggregate;
Snapshot productTotals;
auto budgetIndex(std::uint16_t tag) noexcept -> std::size_t {
    return tag / domainCount == ownerProbe ? 0 : domainBudget[tag % domainCount];
}

std::size_t usedCells{};
struct Structure final {
    const char* name{};
    std::size_t peakCount{}, peakCapacity{};
};
std::array<Structure, 128> structures;
struct Guard final {
    Guard() noexcept {
        while (lock.test_and_set(std::memory_order_acquire))
            std::this_thread::yield();
    }
    ~Guard() {
        lock.clear(std::memory_order_release);
    }
};
auto now() noexcept -> std::uint64_t {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch())
            .count());
}
auto packCell(std::uint16_t tag) noexcept -> std::uint64_t {
    return (static_cast<std::uint64_t>(tag) << tagShift) | flagTagged;
}
auto unpackCell(std::uint64_t packed) noexcept -> std::uint16_t {
    if (!(packed & flagTagged))
        return untaggedCell;
    const auto tag = static_cast<std::uint16_t>((packed >> tagShift) & 0xffffU);
    return tag < cellCount ? tag : untaggedCell;
}
auto cellFor(std::uint16_t tag) noexcept -> Cell& {
    auto& cell = cells[tag];
    if (!cell.used) {
        cell.used = true;
        cell.owner = static_cast<std::uint8_t>(tag / domainCount);
        cell.domain = static_cast<std::uint8_t>(tag % domainCount);
        ++usedCells;
    }
    return cell;
}
auto rootSlot(const void* address) noexcept -> std::size_t {
    for (std::size_t index = 0; index < semanticRoots.size(); ++index)
        if (semanticRoots[index].address == address &&
            semanticRoots[index].snapshot.phase != SemanticPhase::released)
            return index;
    return semanticRootCapacity;
}
auto rootSlot(const Header& block) noexcept -> std::size_t {
    const auto encoded = (block.packed & 0xfffeULL) >> 1;
    return encoded ? static_cast<std::size_t>(encoded - 1) : semanticRootCapacity;
}
void attachRoot(Header& block, std::size_t index) noexcept {
    if (index >= semanticRootCapacity) {
        totals.exceeded = true;
        return;
    }
    const auto existing = rootSlot(block);
    if (existing == index)
        return; // An alias to the same allocation never incurs a second charge.
    if (existing != semanticRootCapacity) {
        totals.exceeded = true;
        return;
    }
    block.packed |= (static_cast<std::uint64_t>(index) + 1) << 1;
    auto& cell = semanticRoots[index].snapshot.memory;
    ++cell.liveCount;
    cell.requestedBytes += block.requested;
    cell.storageBytes += block.charge;
    cell.peakLiveCount = std::max(cell.peakLiveCount, cell.liveCount);
    cell.peakRequestedBytes = std::max(cell.peakRequestedBytes, cell.requestedBytes);
    cell.peakStorageBytes = std::max(cell.peakStorageBytes, cell.storageBytes);
}
void recordSemanticPhase(const void* address, SemanticPhase phase) noexcept {
    Guard guard;
    auto index = rootSlot(address);
    if (phase == SemanticPhase::constructing) {
        if (!address || index != semanticRootCapacity) {
            totals.exceeded = true;
            return;
        }
        for (index = 0; index < semanticRoots.size(); ++index)
            if (!semanticRoots[index].address)
                break;
        if (index == semanticRootCapacity || nextRootId == UINT64_MAX) {
            totals.exceeded = true;
            return;
        }
        auto& root = semanticRoots[index];
        root.address = address;
        root.snapshot.rootId = ++nextRootId;
        root.snapshot.bornUs = now();
        root.snapshot.valid = true;
    } else if (index == semanticRootCapacity) {
        totals.exceeded = true;
        return;
    }
    auto& value = semanticRoots[index].snapshot;
    const bool validTransition =
        phase == SemanticPhase::constructing || phase == SemanticPhase::retiring ||
        (phase == SemanticPhase::prepared && value.phase == SemanticPhase::constructing) ||
        (phase == SemanticPhase::installed && value.phase == SemanticPhase::prepared) ||
        (phase == SemanticPhase::active && value.phase == SemanticPhase::installed);
    if (!validTransition) {
        totals.exceeded = true;
        value.valid = false;
        return;
    }
    value.phase = phase;
    if (phase == SemanticPhase::retiring) {
        value.retiredUs = now();
        if (!value.memory.liveCount) {
            value.phase = SemanticPhase::released;
            value.releasedUs = value.retiredUs;
        }
    }
}
void recordSemanticCensus(const void* address, std::span<const SemanticPage> pages) noexcept {
    struct Entry final {
        std::uint64_t id{}, requested{}, storage{}, age{}, charged{};
        std::uint16_t origin{};
        bool instantaneous{};
        bool extended{};
    };
    struct PageSpan final {
        std::uint64_t id{}, offset{}, bytes{};
        SemanticPage::Kind kind{};
    };
    std::array<Entry, 4096> entries{};
    std::array<PageSpan, 4096> spans{};
    std::size_t count{}, spanCount{};
    std::uint64_t censusId{}, rootId{}, requested{}, backingRequested{}, storage{}, oldest{},
        stamp{};
    SemanticPhase phase{};
    bool valid{};
    {
        Guard guard;
        const auto index = rootSlot(address);
        stamp = now();
        if (nextCensusId == UINT64_MAX || index == semanticRootCapacity) {
            totals.exceeded = true;
        } else {
            censusId = ++nextCensusId;
            rootId = semanticRoots[index].snapshot.rootId;
            phase = semanticRoots[index].snapshot.phase;
            auto append = [&](const Header& block, bool instantaneous) {
                for (std::size_t row = 0; row < count; ++row)
                    if (entries[row].id == block.allocationId)
                        return; // Repeated references to one live allocation are non-additive.
                if (count == entries.size()) {
                    totals.exceeded = true;
                    return;
                }
                entries[count++] = {block.allocationId,
                                    block.requested,
                                    block.charge,
                                    stamp - block.born,
                                    instantaneous ? 0 : block.requested,
                                    unpackCell(block.packed),
                                    instantaneous};
            };
            for (auto* block = first; block; block = block->next)
                if (rootSlot(*block) == index)
                    append(*block, false);
            for (const auto& page : pages) {
                if (page.kind >= SemanticPage::Kind::count) {
                    totals.exceeded = true;
                    continue;
                }
                bool found{};
                for (auto* block = first; block; block = block->next) {
                    const auto start = reinterpret_cast<std::uintptr_t>(block) + sizeof(Header);
                    const auto pointer = reinterpret_cast<std::uintptr_t>(page.allocation);
                    if (pointer >= start && pointer - start <= block->requested && page.bytes &&
                        page.bytes <= block->requested - (pointer - start)) {
                        const auto existing = rootSlot(*block);
                        if (existing != semanticRootCapacity && existing != index)
                            break;
                        append(*block, true);
                        for (std::size_t row = 0; row < count; ++row)
                            if (entries[row].id == block->allocationId)
                                entries[row].extended |=
                                    page.kind != SemanticPage::Kind::legacyComponentPage;
                        if (spanCount == spans.size()) {
                            totals.exceeded = true;
                            break;
                        }
                        spans[spanCount++] = {block->allocationId, pointer - start, page.bytes,
                                              page.kind};
                        found = true;
                        break;
                    }
                }
                if (!found)
                    totals.exceeded = true;
            }
            std::sort(spans.begin(), spans.begin() + spanCount,
                      [](const PageSpan& a, const PageSpan& b) {
                          return a.id < b.id || (a.id == b.id && a.offset < b.offset);
                      });
            for (std::size_t row = 0; row < count; ++row) {
                auto& entry = entries[row];
                if (entry.instantaneous) {
                    // Count the union of observed payload spans. Do not charge STL's
                    // private alignment prefix/tail or the observer header to this proof.
                    std::uint64_t end{};
                    for (std::size_t span = 0; span < spanCount; ++span) {
                        const auto& part = spans[span];
                        if (part.id == entry.id) {
                            const auto next = part.offset + part.bytes; // Range checked above.
                            if (next > end)
                                entry.charged += next - std::max(end, part.offset);
                            end = std::max(end, next);
                        }
                    }
                }
                requested += entry.charged;
                backingRequested += entry.requested;
                storage += entry.storage;
                oldest = std::max(oldest, entry.age);
            }
        }
        valid = !totals.exceeded;
    }
    // Formatting happens outside the allocator lock and allocates no C++ objects.
    std::printf(
        "RPA_MEMORY_WORLD_PROOF {\"schema\":\"rpa.execution.memory.world-proof.v1\","
        "\"census_id\":%llu,\"root_id\":%llu,\"phase\":\"%s\",\"stamp_us\":%llu,"
        "\"domain\":\"state.world\",\"bound_kind\":\"instantaneous_unique_lower_bound\","
        "\"complete_world_graph\":false,\"valid\":%s,\"live_count\":%zu,"
        "\"lower_bound_requested_cpp_bytes\":%llu,\"backing_requested_cpp_bytes\":%llu,"
        "\"backing_crt_usable_storage_bytes\":%llu,"
        "\"oldest_live_age_us\":%llu,\"census_stack_bytes\":%zu}\n",
        static_cast<unsigned long long>(censusId), static_cast<unsigned long long>(rootId),
        semanticPhaseNames[static_cast<unsigned>(phase)], static_cast<unsigned long long>(stamp),
        valid ? "true" : "false", count, static_cast<unsigned long long>(requested),
        static_cast<unsigned long long>(backingRequested), static_cast<unsigned long long>(storage),
        static_cast<unsigned long long>(oldest), sizeof(entries) + sizeof(spans));
    for (std::size_t row = 0; row < count; ++row) {
        const auto& entry = entries[row];
        std::printf(
            "RPA_MEMORY_WORLD_ALLOCATION {\"schema\":\"rpa.execution.memory.world-allocation.v1\","
            "\"census_id\":%llu,\"allocation_id\":%llu,\"root_id\":%llu,"
            "\"requested_cpp_bytes\":%llu,\"crt_usable_storage_bytes\":%llu,\"charged_cpp_bytes\":%"
            "llu,"
            "\"age_us\":%llu,\"origin_cell\":%u,\"membership\":\"%s\"}\n",
            static_cast<unsigned long long>(censusId), static_cast<unsigned long long>(entry.id),
            static_cast<unsigned long long>(rootId),
            static_cast<unsigned long long>(entry.requested),
            static_cast<unsigned long long>(entry.storage),
            static_cast<unsigned long long>(entry.charged),
            static_cast<unsigned long long>(entry.age), static_cast<unsigned>(entry.origin),
            entry.instantaneous ? (entry.extended ? "actual_owned_span" : "actual_component_page")
                                : "stable_registry_subset");
    }
    for (std::size_t row = 0; row < spanCount; ++row) {
        const auto& span = spans[row];
        std::printf("RPA_MEMORY_WORLD_SPAN {\"schema\":\"rpa.execution.memory.world-span.v1\","
                    "\"census_id\":%llu,\"allocation_id\":%llu,\"offset\":%llu,\"bytes\":%llu,"
                    "\"kind\":\"%s\"}\n",
                    static_cast<unsigned long long>(censusId),
                    static_cast<unsigned long long>(span.id),
                    static_cast<unsigned long long>(span.offset),
                    static_cast<unsigned long long>(span.bytes),
                    semanticPageKindNames[static_cast<std::size_t>(span.kind)]);
    }
}
} // namespace

auto allocate(std::size_t bytes, std::size_t alignment) -> void* {
    alignment = std::max(alignment, alignof(Header));
    if (alignment == 0 || (alignment & (alignment - 1)) ||
        bytes > std::numeric_limits<std::size_t>::max() - sizeof(Header) - alignment)
        throw std::bad_alloc{};
    auto* base = std::malloc(std::max(bytes, std::size_t{1}) + sizeof(Header) + alignment);
    if (!base)
        throw std::bad_alloc{};
    const auto address = (reinterpret_cast<std::uintptr_t>(base) + sizeof(Header) + alignment - 1) &
                         ~(alignment - 1);
    auto* header = reinterpret_cast<Header*>(address - sizeof(Header));
    *header = {nullptr,
               nullptr,
               base,
               (static_cast<std::uint64_t>(magicSignature) << signatureShift) | packCell(activeTag),
               bytes,
               _msize(base),
               now()};
    {
        Guard guard;
        if (nextAllocationId == UINT64_MAX)
            totals.exceeded = true;
        header->allocationId = ++nextAllocationId;
        if (semanticOwner)
            attachRoot(*header, rootSlot(semanticOwner));
        header->next = first;
        if (first)
            first->previous = header;
        first = header;
        ++totals.liveCount;
        totals.requestedBytes += bytes;
        totals.storageBytes += header->charge;
        totals.peakCount = std::max(totals.peakCount, totals.liveCount);
        totals.peakRequestedBytes = std::max(totals.peakRequestedBytes, totals.requestedBytes);
        totals.peakStorageBytes = std::max(totals.peakStorageBytes, totals.storageBytes);
        totals.exceeded = totals.exceeded || totals.liveCount > maxCount ||
                          totals.requestedBytes > maxRequested || totals.storageBytes > maxStorage;
        auto& cell = cellFor(unpackCell(header->packed));
        ++cell.liveCount;
        cell.requestedBytes += bytes;
        cell.storageBytes += header->charge;
        cell.peakLiveCount = std::max(cell.peakLiveCount, cell.liveCount);
        cell.peakRequestedBytes = std::max(cell.peakRequestedBytes, cell.requestedBytes);
        cell.peakStorageBytes = std::max(cell.peakStorageBytes, cell.storageBytes);
        const auto index = budgetIndex(unpackCell(header->packed));
        auto& total = aggregate[index];
        total.used = true;
        ++total.liveCount;
        total.requestedBytes += bytes;
        total.storageBytes += header->charge;
        total.peakLiveCount = std::max(total.peakLiveCount, total.liveCount);
        total.peakRequestedBytes = std::max(total.peakRequestedBytes, total.requestedBytes);
        total.peakStorageBytes = std::max(total.peakStorageBytes, total.storageBytes);
        if (index != 0) {
            ++productTotals.liveCount;
            productTotals.requestedBytes += bytes;
            productTotals.storageBytes += header->charge;
            productTotals.peakRequestedBytes =
                std::max(productTotals.peakRequestedBytes, productTotals.requestedBytes);
            productTotals.peakStorageBytes =
                std::max(productTotals.peakStorageBytes, productTotals.storageBytes);
        }
    }
    return reinterpret_cast<void*>(address);
}
void release(void* pointer) noexcept {
    if (!pointer)
        return;
    auto* header =
        reinterpret_cast<Header*>(reinterpret_cast<std::uintptr_t>(pointer) - sizeof(Header));
    if ((header->packed >> signatureShift) != magicSignature)
        std::terminate(); // Mixed allocator or corrupt lifetime record invalidates this process.
    {
        Guard guard;
        const auto semanticIndex = rootSlot(*header);
        if (semanticIndex < semanticRootCapacity) {
            auto& root = semanticRoots[semanticIndex].snapshot;
            --root.memory.liveCount;
            root.memory.requestedBytes -= header->requested;
            root.memory.storageBytes -= header->charge;
            if (!root.memory.liveCount && root.phase == SemanticPhase::retiring) {
                root.phase = SemanticPhase::released;
                root.releasedUs = now();
            }
        }
        if (header->previous)
            header->previous->next = header->next;
        else
            first = header->next;
        if (header->next)
            header->next->previous = header->previous;
        --totals.liveCount;
        totals.requestedBytes -= header->requested;
        totals.storageBytes -= header->charge;
        const auto tag = unpackCell(header->packed);
        if (tag < cellCount) {
            auto& cell = cells[tag];
            --cell.liveCount;
            cell.requestedBytes -= header->requested;
            cell.storageBytes -= header->charge;
        }
        const auto index = budgetIndex(tag);
        auto& total = aggregate[index];
        --total.liveCount;
        total.requestedBytes -= header->requested;
        total.storageBytes -= header->charge;
        if (index != 0) {
            --productTotals.liveCount;
            productTotals.requestedBytes -= header->requested;
            productTotals.storageBytes -= header->charge;
        }
        header->packed = 0;
    }
    std::free(header->base);
}
auto snapshot() noexcept -> Snapshot {
    Guard guard;
    auto result = totals;
    const auto stamp = now();
    for (auto* block = first; block; block = block->next) {
        result.oldestAgeUs = std::max(result.oldestAgeUs, stamp - block->born);
        const auto requested = block->requested;
        const auto shift =
            requested
                ? std::min<std::size_t>(static_cast<std::size_t>(std::bit_width(requested)) - 1,
                                        sizeBucketCount - 1)
                : 0;
        ++result.sizeCount[shift];
        result.sizeBytes[shift] += requested;
    }
    return result;
}
auto partition() noexcept -> Partition {
    Guard guard;
    Partition result;
    result.cells = cells;
    result.usedCells = usedCells;
    result.liveCount = totals.liveCount;
    result.requestedBytes = totals.requestedBytes;
    result.storageBytes = totals.storageBytes;
    const auto stamp = now();
    for (auto* block = first; block; block = block->next) {
        const auto tag = unpackCell(block->packed);
        auto& cell = result.cells[tag];
        cell.oldestAgeUs = std::max(cell.oldestAgeUs, stamp - block->born);
    }
    for (std::size_t index = 0; index < cellCapacity; ++index) {
        const auto& cell = result.cells[index];
        if (!cell.used)
            continue;
        result.sumLiveCount += cell.liveCount;
        result.sumRequestedBytes += cell.requestedBytes;
        result.sumStorageBytes += cell.storageBytes;
        if (index == untaggedCell)
            result.untaggedLiveCount = cell.liveCount;
    }
    result.exact = result.sumLiveCount == result.liveCount &&
                   result.sumRequestedBytes == result.requestedBytes &&
                   result.sumStorageBytes == result.storageBytes;
    return result;
}
TagScope::TagScope(std::uint8_t owner, std::uint8_t domain) noexcept : previous_(activeTag) {
    const auto previousOwner = static_cast<std::uint8_t>(previous_ / domainCount);
    const auto previousDomain = static_cast<std::uint8_t>(previous_ % domainCount);
    // Existing SDK tags name main, but execution hosts call them on their actual owner.
    if (owner == ownerMain && previousOwner != ownerProbe && previousOwner != ownerUnknown)
        owner = previousOwner;
    // Frame extraction into an owning delivery payload is charged once to that payload.
    if (domain == domainSnapshot && previousDomain == domainPayload)
        domain = domainPayload;
    activeTag = owner < ownerCount && domain < domainCount ? cellId(owner, domain) : untaggedCell;
}
TagScope::~TagScope() {
    activeTag = previous_;
}
auto currentTag() noexcept -> std::uint16_t {
    return activeTag;
}
auto semanticSnapshot(const void* address) noexcept -> SemanticSnapshot {
    Guard guard;
    // The newest generation wins if an address has already been reused.
    const SemanticRoot* selected{};
    for (const auto& root : semanticRoots)
        if (root.address == address &&
            (!selected || root.snapshot.rootId > selected->snapshot.rootId))
            selected = &root;
    auto result = selected ? selected->snapshot : SemanticSnapshot{};
    if (selected) {
        const auto index = static_cast<std::size_t>(selected - semanticRoots.data());
        const auto stamp = now();
        for (auto* block = first; block; block = block->next)
            if (rootSlot(*block) == index)
                result.memory.oldestAgeUs =
                    std::max(result.memory.oldestAgeUs, stamp - block->born);
    }
    result.valid = result.valid && !totals.exceeded;
    return result;
}
void recordStructure(const char* name, std::size_t count, std::size_t capacity) noexcept {
    Guard guard;
    for (auto& row : structures) {
        if (!row.name || std::strcmp(row.name, name) == 0) {
            row.name = name;
            row.peakCount = std::max(row.peakCount, count);
            row.peakCapacity = std::max(row.peakCapacity, capacity);
            return;
        }
    }
    totals.exceeded = true;
}
namespace {
struct Connect final {
    Connect() noexcept {
        structureSink = recordStructure;
        semanticPhaseSink = recordSemanticPhase;
        semanticCensusSink = recordSemanticCensus;
    }
} connect;
} // namespace
void checkpoint(const char* label) {
    Snapshot value;
    Partition parts;
    std::array<Cell, budgetCount> budgetCopy;
    Snapshot product;
    std::array<SemanticRoot, semanticRootCapacity> roots;
    struct SemanticAllocation final {
        std::uint64_t allocationId{}, rootId{}, requested{}, storage{}, age{};
        std::uint16_t origin{};
    };
    std::array<SemanticAllocation, 4096> allocations{};
    std::size_t allocationCount{};
    // Optional: every live allocation charged to one named budget domain. The
    // domain is chosen by an environment variable so the default log is unchanged;
    // the lookup uses a fixed buffer and the row count is bounded.
    char traceName[64]{};
    const auto traceLength = ::GetEnvironmentVariableA("CUEXIS_RPA_MEMORY_TRACE_DOMAIN",
                                                       traceName, sizeof(traceName));
    std::size_t traceSlot = budgetCount;
    if (traceLength > 0 && traceLength < sizeof(traceName))
        for (std::size_t index = 0; index < budgetCount; ++index)
            if (std::string_view{budgetNames[index]} == traceName) {
                traceSlot = index;
                break;
            }
    struct TracedAllocation final {
        std::uint64_t allocationId{}, requested{}, storage{}, age{};
        std::uint16_t origin{};
    };
    std::array<TracedAllocation, 1024> traced{};
    std::size_t tracedCount{}, tracedTotalCount{};
    std::uint64_t tracedTotalBytes{};
    {
        Guard guard;
        budgetCopy = aggregate;
        product = productTotals;
        roots = semanticRoots;
        value = totals;
        parts.cells = cells;
        parts.usedCells = usedCells;
        parts.liveCount = totals.liveCount;
        parts.requestedBytes = totals.requestedBytes;
        parts.storageBytes = totals.storageBytes;
        const auto stamp = now();
        for (auto* block = first; block; block = block->next) {
            const auto semanticIndex = rootSlot(*block);
            if (semanticIndex < semanticRootCapacity) {
                auto& root = roots[semanticIndex].snapshot;
                root.memory.oldestAgeUs = std::max(root.memory.oldestAgeUs, stamp - block->born);
                if (allocationCount < allocations.size())
                    allocations[allocationCount++] = {
                        block->allocationId, root.rootId,         block->requested,
                        block->charge,       stamp - block->born, unpackCell(block->packed)};
                else
                    totals.exceeded = true;
            }
            const auto tag = unpackCell(block->packed);
            if (traceSlot < budgetCount && tag < cellCount && budgetIndex(tag) == traceSlot) {
                ++tracedTotalCount;
                tracedTotalBytes += block->requested;
                if (tracedCount < traced.size())
                    traced[tracedCount++] = {block->allocationId, block->requested, block->charge,
                                             stamp - block->born, tag};
            }
            auto& cell = parts.cells[tag];
            cell.oldestAgeUs = std::max(cell.oldestAgeUs, stamp - block->born);
            value.oldestAgeUs = std::max(value.oldestAgeUs, stamp - block->born);
            const auto bucket = block->requested
                                    ? std::min<std::size_t>(std::bit_width(block->requested) - 1,
                                                            sizeBucketCount - 1)
                                    : 0;
            ++value.sizeCount[bucket];
            value.sizeBytes[bucket] += block->requested;
        }
        for (std::size_t index = 0; index < cellCount; ++index) {
            const auto& cell = parts.cells[index];
            parts.sumLiveCount += cell.liveCount;
            parts.sumRequestedBytes += cell.requestedBytes;
            parts.sumStorageBytes += cell.storageBytes;
            if (index == untaggedCell)
                parts.untaggedLiveCount = cell.liveCount;
        }
        parts.exact = parts.sumLiveCount == parts.liveCount &&
                      parts.sumRequestedBytes == parts.requestedBytes &&
                      parts.sumStorageBytes == parts.storageBytes;
        value.exceeded = totals.exceeded;
    }
    std::array<Structure, 128> copied;
    {
        Guard guard;
        copied = structures;
    }
    PROCESS_MEMORY_COUNTERS_EX process{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process),
                              sizeof(process)))
        throw std::runtime_error("PD5 process memory query failed");
    std::cout << "RPA_MEMORY {\"schema\":\"rpa.execution.memory.v1\",\"label\":\"" << label
              << "\",\"bound_kind\":\"global_cpp_new_envelope\",\"exact_partition\":true,"
                 "\"overlap_group\":\"process_cpp_new\",\"allocator\":\"lifetime-observer\","
                 "\"live_count\":"
              << value.liveCount << ",\"requested_cpp_bytes\":" << value.requestedBytes
              << ",\"crt_usable_storage_bytes\":" << value.storageBytes
              << ",\"peak_live_count\":" << value.peakCount
              << ",\"peak_requested_cpp_bytes\":" << value.peakRequestedBytes
              << ",\"peak_crt_usable_storage_bytes\":" << value.peakStorageBytes
              << ",\"oldest_live_age_us\":" << value.oldestAgeUs
              << ",\"working_set_bytes\":" << process.WorkingSetSize
              << ",\"private_commit_bytes\":" << process.PrivateUsage
              << ",\"limit_count\":" << maxCount
              << ",\"limit_requested_cpp_bytes\":" << maxRequested
              << ",\"limit_crt_storage_bytes\":" << maxStorage
              << ",\"exceeded\":" << (value.exceeded ? "true" : "false")
              << ",\"coverage\":\"all linked C++ new/delete including aligned/array/nothrow; "
                 "excludes direct malloc, SDL, driver and OS heaps\"}\n";
    std::cout << "RPA_MEMORY_PRODUCT {\"schema\":\"rpa.execution.memory.product.v1\",\"label\":\""
              << label << "\",\"requested_cpp_bytes\":" << product.requestedBytes
              << ",\"crt_usable_storage_bytes\":" << product.storageBytes
              << ",\"peak_requested_cpp_bytes\":" << product.peakRequestedBytes
              << ",\"peak_crt_usable_storage_bytes\":" << product.peakStorageBytes
              << ",\"observer_header_bytes\":" << value.liveCount * sizeof(Header)
              << ",\"observer_static_bytes\":"
              << sizeof(cells) + sizeof(aggregate) + sizeof(structures) + sizeof(semanticRoots)
              << ",\"observer_checkpoint_stack_bytes\":" << sizeof(roots) + sizeof(allocations)
              << ",\"observer_header_version\":2,\"observer_header_size\":" << sizeof(Header)
              << "}\n";
    for (const auto& root : roots) {
        const auto& state = root.snapshot;
        if (!state.rootId)
            continue;
        const auto& cell = state.memory;
        std::cout << "RPA_MEMORY_SEMANTIC_ROOT {\"schema\":\"rpa.execution.memory.root.v1\","
                     "\"label\":\""
                  << label << "\",\"root_id\":" << state.rootId
                  << ",\"domain\":\"state.world\",\"phase\":\""
                  << semanticPhaseNames[static_cast<unsigned>(state.phase)]
                  << "\",\"bound_kind\":\"exclusive_registry_subset_lower_bound\","
                     "\"complete_world_graph\":false,\"valid\":"
                  << (state.valid && !value.exceeded ? "true" : "false")
                  << ",\"live_count\":" << cell.liveCount
                  << ",\"requested_cpp_bytes\":" << cell.requestedBytes
                  << ",\"crt_usable_storage_bytes\":" << cell.storageBytes
                  << ",\"scope_origin_peak_requested_bytes\":" << cell.peakRequestedBytes
                  << ",\"oldest_live_age_us\":" << cell.oldestAgeUs
                  << ",\"born_us\":" << state.bornUs << ",\"retired_us\":" << state.retiredUs
                  << ",\"released_us\":" << state.releasedUs << "}\n";
    }
    for (std::size_t index = 0; index < allocationCount; ++index) {
        const auto& block = allocations[index];
        std::cout << "RPA_MEMORY_SEMANTIC_ALLOCATION {"
                     "\"schema\":\"rpa.execution.memory.root-allocation.v1\",\"label\":\""
                  << label << "\",\"allocation_id\":" << block.allocationId
                  << ",\"root_id\":" << block.rootId
                  << ",\"requested_cpp_bytes\":" << block.requested
                  << ",\"crt_usable_storage_bytes\":" << block.storage
                  << ",\"age_us\":" << block.age << ",\"origin_cell\":" << block.origin << "}\n";
    }
    if (traceSlot < budgetCount) {
        for (std::size_t index = 0; index < tracedCount; ++index) {
            const auto& block = traced[index];
            std::cout << "RPA_MEMORY_DOMAIN_ALLOCATION {"
                         "\"schema\":\"rpa.execution.memory.domain-allocation.v1\",\"label\":\""
                      << label << "\",\"domain\":\"" << budgetNames[traceSlot]
                      << "\",\"allocation_id\":" << block.allocationId
                      << ",\"requested_cpp_bytes\":" << block.requested
                      << ",\"crt_usable_storage_bytes\":" << block.storage
                      << ",\"age_us\":" << block.age << ",\"origin_cell\":" << block.origin
                      << "}\n";
        }
        std::cout << "RPA_MEMORY_DOMAIN_ALLOCATION_SUMMARY {"
                     "\"schema\":\"rpa.execution.memory.domain-allocation-summary.v1\","
                     "\"label\":\""
                  << label << "\",\"domain\":\"" << budgetNames[traceSlot]
                  << "\",\"live_count\":" << tracedTotalCount
                  << ",\"requested_cpp_bytes\":" << tracedTotalBytes
                  << ",\"emitted\":" << tracedCount
                  << ",\"truncated\":" << (tracedTotalCount > tracedCount ? "true" : "false")
                  << "}\n";
    }
    for (std::size_t index = 0; index < budgetCount; ++index) {
        const auto& total = budgetCopy[index];
        if (!total.used)
            continue;
        std::cout
            << "RPA_MEMORY_AGGREGATE {\"schema\":\"rpa.execution.memory.aggregate.v1\",\"label\":\""
            << label << "\",\"domain\":\"" << budgetNames[index]
            << "\",\"live_count\":" << total.liveCount
            << ",\"requested_cpp_bytes\":" << total.requestedBytes
            << ",\"crt_usable_storage_bytes\":" << total.storageBytes
            << ",\"peak_requested_cpp_bytes\":" << total.peakRequestedBytes
            << ",\"peak_crt_usable_storage_bytes\":" << total.peakStorageBytes << "}\n";
    }
    std::cout << "RPA_MEMORY_SIZE {\"schema\":\"rpa.execution.memory.size.v1\",\"label\":\""
              << label << "\",\"unit\":\"requested_cpp_bytes\",\"buckets\":[";
    bool firstBucket = true;
    for (std::size_t shift = 0; shift < sizeBucketCount; ++shift) {
        if (!value.sizeCount[shift])
            continue;
        if (!firstBucket)
            std::cout << ',';
        firstBucket = false;
        std::cout << "{\"shift\":" << shift << ",\"upper\":" << (std::uint64_t{1} << shift)
                  << ",\"count\":" << value.sizeCount[shift]
                  << ",\"bytes\":" << value.sizeBytes[shift] << '}';
    }
    std::cout << "]}\n";
    for (const auto& row : copied)
        if (row.name)
            std::cout << "RPA_STRUCTURE {\"label\":\"" << label << "\",\"domain\":\"" << row.name
                      << "\",\"peak_count\":" << row.peakCount
                      << ",\"peak_capacity\":" << row.peakCapacity
                      << ",\"scope\":\"per-instance high-water; bytes covered by overlapping "
                         "process_cpp_new envelope\"}\n";
    for (std::size_t index = 0; index < cellCapacity; ++index) {
        const auto& cell = parts.cells[index];
        if (!cell.used || (!cell.liveCount && !cell.peakLiveCount))
            continue;
        std::cout << "RPA_MEMORY_DOMAIN {\"schema\":\"rpa.execution.memory.domain.v1\",\"label\":\""
                  << label << "\",\"owner\":\"" << ownerNames[cell.owner] << "\",\"domain\":\""
                  << domainNames[cell.domain] << "\",\"cell\":\"" << ownerNames[cell.owner] << '.'
                  << domainNames[cell.domain] << "\",\"live_count\":" << cell.liveCount
                  << ",\"requested_cpp_bytes\":" << cell.requestedBytes
                  << ",\"crt_usable_storage_bytes\":" << cell.storageBytes
                  << ",\"peak_live_count\":" << cell.peakLiveCount
                  << ",\"peak_requested_cpp_bytes\":" << cell.peakRequestedBytes
                  << ",\"peak_crt_usable_storage_bytes\":" << cell.peakStorageBytes
                  << ",\"oldest_live_age_us\":" << cell.oldestAgeUs << "}\n";
    }
    std::cout
        << "RPA_MEMORY_PARTITION {\"schema\":\"rpa.execution.memory.partition.v1\",\"label\":\""
        << label << "\",\"exact\":" << (parts.exact ? "true" : "false")
        << ",\"sum_live_count\":" << parts.sumLiveCount
        << ",\"global_live_count\":" << parts.liveCount
        << ",\"sum_requested_cpp_bytes\":" << parts.sumRequestedBytes
        << ",\"global_requested_cpp_bytes\":" << parts.requestedBytes
        << ",\"sum_crt_usable_storage_bytes\":" << parts.sumStorageBytes
        << ",\"global_crt_usable_storage_bytes\":" << parts.storageBytes
        << ",\"cells\":" << parts.usedCells
        << ",\"untagged_live_count\":" << parts.untaggedLiveCount
        << ",\"coverage\":\"linked C++ new/delete only; direct malloc, SDL, driver and OS "
           "heaps excluded\""
        << ",\"exceeded\":" << (parts.exact ? "false" : "true")
        << ",\"notes\":\"tag is carried in the header signature slot; untagged "
           "allocations fall into unknown.other\"}\n";
}
} // namespace cuexis::internal::pd::memory

void* operator new(std::size_t n) {
    return cuexis::internal::pd::memory::allocate(n, alignof(std::max_align_t));
}
void* operator new[](std::size_t n) {
    return ::operator new(n);
}
void operator delete(void* p) noexcept {
    cuexis::internal::pd::memory::release(p);
}
void operator delete[](void* p) noexcept {
    ::operator delete(p);
}
void operator delete(void* p, std::size_t) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    ::operator delete(p);
}
void* operator new(std::size_t n, std::align_val_t a) {
    return cuexis::internal::pd::memory::allocate(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a) {
    return ::operator new(n, a);
}
void operator delete(void* p, std::align_val_t) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    ::operator delete(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    ::operator delete(p);
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(n);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept {
    return ::operator new(n, t);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(n, a);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept {
    return ::operator new(n, a, t);
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}
