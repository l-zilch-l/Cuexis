#pragma once

#include <cuexis/chart/packed_chart_tables.hpp>

#include <utility>

namespace cuexis::chart::packed::gameplay_detail {

using Reference = std::pair<std::uint8_t, std::string>;

struct Section final {
    std::array<char, 4> code{};
    std::vector<std::byte> bytes;
    std::uint32_t records{};
};

struct StaticTables final {
    std::vector<Section> sections;
    std::vector<std::string> strings;
    std::vector<Reference> references;
    // Canonical identity-byte order, not the input vector order.
    std::vector<CanonicalEntityIdentity> entityIdentities;
    std::vector<std::uint64_t> entityMasks;
};

struct DecodedStatic final {
    CanonicalSemanticChart chart;
    std::vector<std::string> strings;
    std::vector<Reference> references;
    std::vector<std::uint64_t> entityMasks;
};

// Internal physical building blocks only. The adapter must add Gameplay tables, validate
// ownership/closure, and compute the full revision-2 digest before publishing an artifact.
[[nodiscard]] auto encodeStatic(const CanonicalSemanticChart& chart,
                                std::span<const CanonicalEntityIdentity> gameplayOwners,
                                std::span<const std::string> extraStrings,
                                std::span<const Reference> extraReferences,
                                PackedChartLimits limits = {}) -> core::Result<StaticTables>;

// Foundation global and static entity preimage with the Gameplay domain and bit-2 presence.
// This is a prefix, never the complete Capsule semantic identity.
[[nodiscard]] auto staticPreimage(const CanonicalSemanticChart& chart,
                                  std::span<const CanonicalEntityIdentity> gameplayOwners)
    -> core::Result<std::vector<std::byte>>;

[[nodiscard]] auto inspect(std::span<const std::byte> bytes, PackedChartLimits limits = {})
    -> core::Result<PackedChartStatistics>;

// Structural/static validation only; intentionally does not accept or publish Gameplay.
// The Capsule adapter must validate all four Gameplay payloads and the full digest.
[[nodiscard]] auto decodeStatic(std::span<const std::byte> bytes, PackedChartLimits limits = {})
    -> core::Result<DecodedStatic>;

} // namespace cuexis::chart::packed::gameplay_detail
