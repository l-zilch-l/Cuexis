#pragma once
// Noninstalled observer contract. No production allocator or SDK API change.
#include <cstddef>
#include <cstdint>

namespace cuexis::internal::pd::memory {
struct Snapshot final {
    std::uint64_t liveCount{}, requestedBytes{}, storageBytes{};
    std::uint64_t peakCount{}, peakRequestedBytes{}, peakStorageBytes{}, oldestAgeUs{};
    bool exceeded{};
};
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
auto snapshot() noexcept -> Snapshot;
inline void (*structureSink)(const char*, std::size_t, std::size_t) noexcept {};
inline void structure(const char* name, std::size_t count, std::size_t capacity) noexcept {
    if (structureSink)
        structureSink(name, count, capacity);
}
void checkpoint(const char* label);
#else
inline void structure(const char*, std::size_t, std::size_t) noexcept {}
inline void checkpoint(const char*) {}
#endif
} // namespace cuexis::internal::pd::memory
