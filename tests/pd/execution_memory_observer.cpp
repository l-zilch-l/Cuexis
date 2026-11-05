// Linked exclusively into opt-in observer executables. Every allocation carries
// its lifetime record, including cross-thread frees; the registry allocates nothing.
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cuexis_internal/allocation_observer.hpp>
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
// The header keeps its 56-byte layout: the old 8-byte signature slot now
// carries signature(32) | tag(16) | flags(16), so storage accounting is unchanged.
constexpr std::uint64_t flagTagged = 1ULL;
constexpr unsigned tagShift = 16, signatureShift = 32;
struct Header final {
    Header* previous{};
    Header* next{};
    void* base{};
    std::uint64_t packed{};
    std::uint64_t requested{}, charge{}, born{};
};
static_assert(sizeof(Header) == 56, "observer header must keep its frozen size");
std::atomic_flag lock = ATOMIC_FLAG_INIT;
Header* first{};
Snapshot totals{};
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
    }
} connect;
} // namespace
void checkpoint(const char* label) {
    Snapshot value;
    Partition parts;
    std::array<Cell, budgetCount> budgetCopy;
    Snapshot product;
    {
        Guard guard;
        budgetCopy = aggregate;
        product = productTotals;
        value = totals;
        parts.cells = cells;
        parts.usedCells = usedCells;
        parts.liveCount = totals.liveCount;
        parts.requestedBytes = totals.requestedBytes;
        parts.storageBytes = totals.storageBytes;
        const auto stamp = now();
        for (auto* block = first; block; block = block->next) {
            const auto tag = unpackCell(block->packed);
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
              << sizeof(cells) + sizeof(aggregate) + sizeof(structures) << "}\n";
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
