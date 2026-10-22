#pragma once

#include "hash_work_internal.hpp"
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>

#include <array>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>

namespace cuexis::gameplay_packed::detail {

struct RequirementIndices final {
    std::uint32_t entity{}, pattern{}, measure{}, domain{}, solver{};
    std::vector<std::uint32_t> claims, bindings;
};
struct ClaimRow final {
    std::uint32_t requirement{}, resource{};
    judgement::ResourceClaimDeclaration declaration;
    std::optional<std::string> deferredClaimKey{};
};

// Borrow an already validated canonical table set. It must remain unchanged for the
// synchronous operation. These are the source tables, not copies created for hashing.
// The eventual Reader/Writer validator owns construction and lifetime of this view.
// Ix tables and definition spans are authoritative; linked RequirementRecord copies
// and provenance fields in the graph container are not read by this codec.
struct SemanticInput final {
    const chart::CanonicalSemanticChart& chart;
    const judgement::CanonicalGameplayGraph& graph;
    std::span<const chart::CanonicalEntityIdentity> entities, owners;
    std::span<const RequirementIndices> indices;
    std::span<const judgement::PatternDeclaration> patterns;
    std::span<const MeasureDefinition> measures;
    std::span<const ClaimRow> claims;
    const CapsuleProfiles& profiles;
    const judgement::TimebaseProfile& timebase;
    const judgement::LatePolicyParameters& latePolicy;
    std::string_view profileId, unit;
    // Semantic selector: 2 = Gameplay.1, 3 = Gameplay.2. This is not wire dispatch.
    std::uint32_t candidateRevision{3};
    std::string_view resolver{"grace.resolver.s2.v1"}, close{"local.close.t4.v1"};
    std::span<const std::uint32_t> counts{};
    std::span<const std::uint32_t> relationResources{};
};

struct HashObservation final : HashAllocationStats {
    std::uint64_t inlineObjectBytes{};
    std::uint64_t uniqueRows{}, cacheHits{}, patternCountNodes{}, patternHashNodes{};
    std::uint64_t shaComputations{}, shaUpdates{}, shaBytes{}, exportedBytes{};
    bool budgetAccepted{};
};
struct HashOptions final {
    HashWorkLimits requested{};
    // The operation allocates this entire capacity through scratch before filling it.
    // A present zero requests literal zero-byte export; absence requests no export.
    std::optional<std::uint64_t> exportCapacity;
    // Like a PMR container's allocator, this must outlive returned exported bytes.
    std::pmr::memory_resource* upstream{std::pmr::new_delete_resource()};
};
struct SemanticHashResult final {
    HashCost cost;
    std::array<std::uint8_t, 32> digest;
    std::uint64_t exportedBytes{};
    HashOwnedBytes exportData;
    auto bytes() const noexcept -> std::span<const std::byte> {
        return {exportData.get(), static_cast<std::size_t>(exportedBytes)};
    }
};

// Only entry to the new core: count -> budget acceptance -> one SHA, optionally teeing
// bytes into a charged caller buffer. Header and metadata can reuse this fixed result.
// This does not register a revision-4 production entry.
[[nodiscard]] auto calculateSemanticHash(const SemanticInput& input, HashOptions options = {},
                                         HashObservation* observation = nullptr)
    -> core::Result<SemanticHashResult>;

} // namespace cuexis::gameplay_packed::detail
