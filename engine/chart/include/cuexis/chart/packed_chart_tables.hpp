#pragma once

// Candidate Packed Chart tables and static component streams.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/packed_chart_primitives.hpp>
#include <cuexis/core/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace cuexis::chart::packed {

struct PackedChartProfile final {
    std::uint32_t candidateRevision{1};
    std::uint32_t flags{1};
};

struct PackedChartStatistics final {
    std::size_t packedBytes{};
    std::size_t decodedBytes{};
    std::size_t stringCount{};
    std::size_t referenceCount{};
    std::size_t entityCount{};
    std::size_t requirementCount{};
};

// Packed Spec 10.1 semantic identity digest.
using PackedSemanticIdentity = std::array<std::uint8_t, 32>;

// Computes the Spec 10.1 typed semantic preimage bytes. The preimage covers the canonical
// semantic model only: no dictionary indices, no section layout, no file bytes.
// It fails with a stable error when a hash precondition is violated (duplicate or missing
// identities, dangling parents, non-finite floats, unsupported constraints, ...).
[[nodiscard]] auto semanticPreimage(const CanonicalSemanticChart& chart)
    -> core::Result<std::vector<std::byte>>;

// SHA-256 over the Spec 10.1 preimage.
[[nodiscard]] auto semanticIdentity(const CanonicalSemanticChart& chart)
    -> core::Result<PackedSemanticIdentity>;

// Lowercase hexadecimal form of a semantic identity digest.
[[nodiscard]] auto semanticIdentityHex(const PackedSemanticIdentity& identity) -> std::string;

// Encodes a chart after validating the Spec 7.6 Foundation profile. A model outside the
// registered subset is refused before any identity or artifact bytes are produced.
[[nodiscard]] auto encode(const CanonicalSemanticChart& chart, PackedChartProfile profile = {})
    -> core::Result<std::vector<std::byte>>;

// Verifies the registered section registry, the Foundation profile of Spec 7.6 and
// Header.semanticIdentity against the recomputed digest. Structural, profile and semantic
// rejections all precede the identity comparison, so an illegal artifact never reports
// packed.identity.mismatch. A mismatch is rejected before any semantic chart is published.
[[nodiscard]] auto decode(std::span<const std::byte> bytes, PackedChartLimits limits = {})
    -> core::Result<CanonicalSemanticChart>;

// Structural inspection only: header, directory, section CRCs and declared counters. It does
// NOT verify the semantic identity, the Foundation profile or any semantic precondition, so
// callers that consume semantics must use decode(). It shares the decode() section registry
// decision (Spec 5.2/5.3) so both entry points agree on which sections an artifact may carry.
[[nodiscard]] auto inspect(std::span<const std::byte> bytes, PackedChartLimits limits = {})
    -> core::Result<PackedChartStatistics>;

} // namespace cuexis::chart::packed
