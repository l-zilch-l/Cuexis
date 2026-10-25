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
#include <thread>

#include <Windows.h>

#include <Psapi.h>

namespace cuexis::internal::pd::memory {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::uint32_t magicSignature = 0x5044354dU; // "PD5M"
constexpr std::uint64_t maxCount = 2097152, maxRequested = 1073741824, maxStorage = 1342177280;
// The header keeps its frozen 56-byte layout: the old 8-byte signature slot now
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
thread_local std::uint16_t activeTag = untaggedCell;
std::array<Cell, cellCapacity> cells;
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
        const auto shift = requested ? std::min<std::size_t>(
                                           static_cast<std::size_t>(std::bit_width(requested)) - 1,
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
    const auto value = snapshot();
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
    std::cout << "PD5_MEMORY {\"schema\":\"pd5.memory.v1\",\"label\":\"" << label
              << "\",\"bound_kind\":\"global_cpp_new_envelope\",\"exact_partition\":false,"
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
    std::cout << "PD5_MEMORY_SIZE {\"schema\":\"pd5.memory.size.v1\",\"label\":\"" << label
              << "\",\"unit\":\"requested_cpp_bytes\",\"buckets\":[";
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
            std::cout << "PD5_STRUCTURE {\"label\":\"" << label << "\",\"domain\":\"" << row.name
                      << "\",\"peak_count\":" << row.peakCount
                      << ",\"peak_capacity\":" << row.peakCapacity
                      << ",\"scope\":\"per-instance high-water; bytes covered by overlapping "
                         "process_cpp_new envelope\"}\n";
    const auto parts = partition();
    for (std::size_t index = 0; index < cellCapacity; ++index) {
        const auto& cell = parts.cells[index];
        if (!cell.used || (!cell.liveCount && !cell.peakLiveCount))
            continue;
        std::cout << "PD5_MEMORY_DOMAIN {\"schema\":\"pd5.memory.domain.v1\",\"label\":\"" << label
                  << "\",\"owner\":\"" << ownerNames[cell.owner] << "\",\"domain\":\""
                  << domainNames[cell.domain] << "\",\"cell\":\"" << ownerNames[cell.owner] << '.'
                  << domainNames[cell.domain] << "\",\"live_count\":" << cell.liveCount
                  << ",\"requested_cpp_bytes\":" << cell.requestedBytes
                  << ",\"crt_usable_storage_bytes\":" << cell.storageBytes
                  << ",\"peak_live_count\":" << cell.peakLiveCount
                  << ",\"peak_requested_cpp_bytes\":" << cell.peakRequestedBytes
                  << ",\"peak_crt_usable_storage_bytes\":" << cell.peakStorageBytes
                  << ",\"oldest_live_age_us\":" << cell.oldestAgeUs << "}\n";
    }
    std::cout << "PD5_MEMORY_PARTITION {\"schema\":\"pd5.memory.partition.v1\",\"label\":\"" << label
              << "\",\"exact\":" << (parts.exact ? "true" : "false")
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
              << ",\"notes\":\"tag is carried in the frozen header signature slot; untagged "
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
