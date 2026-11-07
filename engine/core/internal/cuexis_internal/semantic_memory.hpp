#pragma once

// Noninstalled, opt-in diagnostic bridge. A root is a non-owning identity, never a handle.
// Narrow scopes may claim only allocations exclusively owned by that root. Uncovered
// allocations remain unknown; this book is a lower bound, not a complete partition.
#include <cuexis_internal/allocation_observer.hpp>
#include <limits>
#include <span>
#include <string>
#include <vector>

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
    // Actual typed payload, possibly inside a new allocation due to STL alignment.
    // Only the union of these payload spans is charged in an instantaneous census.
    const void* allocation;
    std::size_t bytes;
    // Static diagnostic vocabulary, never a user supplied string.
    enum class Kind : std::uint8_t {
        legacyComponentPage,
        transformPage,
        hierarchyPage,
        worldTransformPage,
        behaviorPage,
        renderablePage,
        appearancePage,
        cameraPage,
        componentEntities,
        worldObject,
        transformCache,
        transformChildren,
        transformOrder,
        materialString,
        cameraString,
        count
    } kind{Kind::legacyComponentPage};
};
inline constexpr const char* semanticPageKindNames[] = {
    "legacy_component_page", "transform_page",  "hierarchy_page",  "world_transform_page",
    "behavior_page",         "renderable_page", "appearance_page", "camera_page",
    "component_entities",    "world_object",    "transform_cache", "transform_children",
    "transform_order",       "material_string", "camera_string"};
static_assert(std::size(semanticPageKindNames) ==
              static_cast<std::size_t>(SemanticPage::Kind::count));

// Fixed caller-owned storage. Overflow is an invalid observation, never truncation.
class SemanticPages final {
  public:
    explicit SemanticPages(std::span<SemanticPage> storage) noexcept : storage_(storage) {}
    void add(const void* data, std::size_t bytes, SemanticPage::Kind kind) noexcept {
        if (count_ == storage_.size() || !data || !bytes || kind >= SemanticPage::Kind::count) {
            valid_ = false;
            return;
        }
        storage_[count_++] = {data, bytes, kind};
    }
    template <typename T>
    void vector(const std::vector<T>& values, SemanticPage::Kind kind) noexcept {
        if (!values.capacity())
            return;
        if (values.capacity() > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            valid_ = false;
            return;
        }
        add(values.data(), values.capacity() * sizeof(T), kind);
    }
    void string(const std::string& value, SemanticPage::Kind kind) noexcept {
        const auto data = reinterpret_cast<std::uintptr_t>(value.data());
        const auto object = reinterpret_cast<std::uintptr_t>(&value);
        // An inline character buffer is already contained in the object's payload.
        if (data >= object && data - object < sizeof(value))
            return;
        if (value.capacity() == std::numeric_limits<std::size_t>::max()) {
            valid_ = false;
            return;
        }
        add(value.data(), value.capacity() + 1, kind);
    }
    [[nodiscard]] auto valid() const noexcept -> bool {
        return valid_;
    }
    [[nodiscard]] auto pages() noexcept -> std::span<const SemanticPage> {
        if (!valid_) {
            invalid_ = {nullptr, 0};
            return {&invalid_, 1};
        }
        return storage_.first(count_);
    }

  private:
    std::span<SemanticPage> storage_;
    std::size_t count_{};
    bool valid_{true};
    SemanticPage invalid_{};
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
