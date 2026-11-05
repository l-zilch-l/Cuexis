#pragma once

// Noninstalled, opt-in diagnostic bridge. A root is a non-owning identity, never a handle.
// Narrow scopes may claim only allocations exclusively owned by that root. Uncovered
// allocations remain unknown; this book is a lower bound, not a complete partition.
#include <cuexis_internal/allocation_observer.hpp>
#include <span>

namespace cuexis::internal::pd::memory {
inline constexpr std::size_t semanticRootLimit = 256;
enum class SemanticPhase : std::uint8_t {
    constructing,
    prepared,
    installed,
    active,
    retiring,
    released
};
inline constexpr const char* semanticPhaseNames[] = {
    "constructing", "prepared", "runtime-installed", "active", "retiring", "released"};

struct SemanticSnapshot final {
    std::uint64_t rootId{}, bornUs{}, retiredUs{}, releasedUs{};
    SemanticPhase phase{};
    Cell memory;
    bool valid{};
};
struct SemanticPage final {
    const void* allocation;
    std::size_t bytes;
};

#if defined(CUEXIS_EXECUTION_ALLOCATION_OBSERVER)
inline thread_local const void* semanticOwner{};
inline void (*semanticPhaseSink)(const void*, SemanticPhase) noexcept {};
inline void (*semanticCensusSink)(const void*, std::span<const SemanticPage>) noexcept {};
inline void semanticPhase(const void* root, SemanticPhase phase) noexcept {
    if (semanticPhaseSink)
        semanticPhaseSink(root, phase);
}
inline void semanticCensus(const void* root, std::span<const SemanticPage> pages) noexcept {
    if (semanticCensusSink)
        semanticCensusSink(root, pages);
}
auto semanticSnapshot(const void*) noexcept -> SemanticSnapshot;
struct SemanticOwnerScope final {
    explicit SemanticOwnerScope(const void* root) noexcept : previous_(semanticOwner) {
        semanticOwner = root;
    }
    ~SemanticOwnerScope() {
        semanticOwner = previous_;
    }
    SemanticOwnerScope(const SemanticOwnerScope&) = delete;
    auto operator=(const SemanticOwnerScope&) -> SemanticOwnerScope& = delete;

  private:
    const void* previous_;
};
#else
inline void semanticPhase(const void*, SemanticPhase) noexcept {}
struct SemanticOwnerScope final {
    explicit SemanticOwnerScope(const void*) noexcept {}
};
#endif

template <typename Function>
decltype(auto) withSemanticOwner(const void* root, Function&& function) {
    const SemanticOwnerScope scope{root};
    return static_cast<Function&&>(function)();
}
} // namespace cuexis::internal::pd::memory
