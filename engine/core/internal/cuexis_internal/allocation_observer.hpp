#pragma once
// Noninstalled observer contract. No production allocator or SDK API change.
#include <array>
#include <cstddef>
#include <cstdint>

namespace cuexis::internal::pd::memory {
// Logarithmic requested-size buckets: index i covers (2^i, 2^(i+1)] requested bytes.
inline constexpr std::size_t sizeBucketCount = 24;

// Two-axis partition: owner x domain. The table is static and allocation-free; a
// cell is live only while a tag scope or a tagged block references it.
inline constexpr std::size_t ownerCount = 7;
#if defined(CUEXIS_EXECUTION_ALLOCATION_OBSERVER)
inline constexpr std::size_t domainCount = 30;
#else
inline constexpr std::size_t domainCount = 20;
#endif
inline constexpr std::size_t cellCount = ownerCount * domainCount;
inline constexpr std::size_t cellCapacity = 256;
static_assert(cellCount <= cellCapacity, "owner x domain must fit the static table");

inline constexpr const char* ownerNames[ownerCount] = {
    "probe", "main", "playback-worker", "renderer-owner", "audio-owner", "audio-worker", "unknown"};
inline constexpr const char* domainNames[domainCount] = {"fixture",
                                                         "load",
                                                         "decode",
                                                         "prepare",
                                                         "snapshot",
                                                         "advance",
                                                         "current",
                                                         "candidate",
                                                         "scratch",
                                                         "index",
                                                         "ready",
                                                         "history",
                                                         "receipt",
                                                         "journal",
                                                         "replay",
                                                         "gpu",
                                                         "audio",
                                                         "retained",
                                                         "static",
                                                         "other"
#if defined(CUEXIS_EXECUTION_ALLOCATION_OBSERVER)
                                                         ,
                                                         "world",
                                                         "payload",
                                                         "attempt",
                                                         "command-session",
                                                         "command-audio",
                                                         "command-host",
                                                         "command-capture",
                                                         "steps",
                                                         "codec",
                                                         "owning"
#endif
};

inline constexpr std::uint8_t ownerProbe = 0, ownerMain = 1, ownerPlaybackWorker = 2,
                              ownerRendererOwner = 3, ownerAudioOwner = 4, ownerAudioWorker = 5,
                              ownerUnknown = 6;
inline constexpr std::uint8_t domainFixture = 0, domainLoad = 1, domainDecode = 2,
                              domainPrepare = 3, domainSnapshot = 4, domainAdvance = 5,
                              domainCurrent = 6, domainCandidate = 7, domainScratch = 8,
                              domainIndex = 9, domainReady = 10, domainHistory = 11,
                              domainReceipt = 12, domainJournal = 13, domainReplay = 14,
                              domainGpu = 15, domainAudio = 16, domainRetained = 17,
                              domainStatic = 18, domainOther = 19;
inline constexpr std::uint8_t domainWorld = 20, domainPayload = 21, domainAttempt = 22,
                              domainCommandSession = 23, domainCommandAudio = 24,
                              domainCommandHost = 25, domainCommandCapture = 26, domainSteps = 27,
                              domainCodec = 28, domainOwning = 29;

inline constexpr std::uint16_t untaggedCell =
    static_cast<std::uint16_t>(ownerUnknown * domainCount + domainOther);

constexpr auto cellId(std::uint8_t owner, std::uint8_t domain) noexcept -> std::uint16_t {
    return static_cast<std::uint16_t>(owner * domainCount + domain);
}

struct Snapshot final {
    std::uint64_t liveCount{}, requestedBytes{}, storageBytes{};
    std::uint64_t peakCount{}, peakRequestedBytes{}, peakStorageBytes{}, oldestAgeUs{};
    std::array<std::uint64_t, sizeBucketCount> sizeCount{}, sizeBytes{};
    bool exceeded{};
};

struct Cell final {
    std::uint8_t owner{}, domain{};
    bool used{};
    std::uint64_t liveCount{}, requestedBytes{}, storageBytes{};
    std::uint64_t peakLiveCount{}, peakRequestedBytes{}, peakStorageBytes{}, oldestAgeUs{};
};

struct Partition final {
    std::array<Cell, cellCapacity> cells{};
    std::uint64_t liveCount{}, requestedBytes{}, storageBytes{};
    std::uint64_t sumLiveCount{}, sumRequestedBytes{}, sumStorageBytes{};
    std::uint64_t untaggedLiveCount{};
    std::size_t usedCells{};
    bool exact{};
};

#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
auto snapshot() noexcept -> Snapshot;
auto partition() noexcept -> Partition;
inline void (*structureSink)(const char*, std::size_t, std::size_t) noexcept {};
inline void structure(const char* name, std::size_t count, std::size_t capacity) noexcept {
    if (structureSink)
        structureSink(name, count, capacity);
}
void checkpoint(const char* label);

// Nestable thread-local tag. The current tag is read by every observed allocation
// on the same thread; untagged allocations fall into unknown.other so the
// partition stays exact instead of silently dropping bytes.
struct TagScope final {
    TagScope(std::uint8_t owner, std::uint8_t domain) noexcept;
    ~TagScope();
    TagScope(const TagScope&) = delete;
    auto operator=(const TagScope&) -> TagScope& = delete;

  private:
    std::uint16_t previous_{};
};
auto currentTag() noexcept -> std::uint16_t;
#else
auto partition() noexcept -> Partition;
inline void structure(const char*, std::size_t, std::size_t) noexcept {}
inline void checkpoint(const char*) {}
struct TagScope final {
    TagScope(std::uint8_t, std::uint8_t) noexcept {}
};
inline auto currentTag() noexcept -> std::uint16_t {
    return untaggedCell;
}
#endif

// Extended attribution is compiled out for ordinary and historical observer builds.
#if defined(CUEXIS_EXECUTION_ALLOCATION_OBSERVER)
using ExecutionTagScope = TagScope;
struct ExecutionDomainScope final {
    explicit ExecutionDomainScope(std::uint8_t domain) noexcept
        : scope(static_cast<std::uint8_t>(currentTag() / domainCount == ownerProbe
                                              ? ownerMain
                                              : currentTag() / domainCount),
                domain) {}
    TagScope scope;
};
#else
struct ExecutionTagScope final {
    ExecutionTagScope(std::uint8_t, std::uint8_t) noexcept {}
};
struct ExecutionDomainScope final {
    explicit ExecutionDomainScope(std::uint8_t) noexcept {}
};
#endif
template <typename Function>
decltype(auto) withExecutionTag(std::uint8_t owner, std::uint8_t domain, Function&& function) {
    const ExecutionTagScope scope{owner, domain};
    return static_cast<Function&&>(function)();
}
} // namespace cuexis::internal::pd::memory
