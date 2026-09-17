#pragma once

// Test-side helpers for crafting candidate Packed artifacts at the byte level.
//
// The hardening batch tests need two independent negative-example groups: artifacts produced by
// the typed Writer from an out-of-profile model, and hand-patched bytes that the Writer cannot
// produce. These helpers locate fields inside a valid artifact, patch them, and keep the section
// and header CRCs consistent so a test can state the *expected failure order* between the
// structural, profile and semantic-identity checks.

#include <cuexis/chart/packed_chart_tables.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cuexis::chart::packed::test {

// Spec 5.1 fixed header offsets.
inline constexpr std::size_t headerSize = 96U;
inline constexpr std::size_t packedVersionOffset = 8U;
inline constexpr std::size_t headerFlagsOffset = 12U;
inline constexpr std::size_t semanticIdentityOffset = 32U;
inline constexpr std::size_t candidateRevisionOffset = 88U;

[[nodiscard]] auto readU8(std::span<const std::byte> bytes, std::size_t offset) -> std::uint8_t;
[[nodiscard]] auto readU16(std::span<const std::byte> bytes, std::size_t offset) -> std::uint16_t;
[[nodiscard]] auto readU32(std::span<const std::byte> bytes, std::size_t offset) -> std::uint32_t;
void writeU8(std::vector<std::byte>& bytes, std::size_t offset, std::uint8_t value);
void writeU16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value);
void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value);

struct SectionRef final {
    std::size_t index{};  // directory entry index
    std::size_t offset{}; // absolute file offset of the section payload
    std::size_t size{};
    std::uint32_t records{};
};

[[nodiscard]] auto findSection(std::span<const std::byte> bytes, std::string_view code)
    -> std::optional<SectionRef>;

void refreshSectionCrc(std::vector<std::byte>& bytes, const SectionRef& section);
void refreshHeaderCrc(std::vector<std::byte>& bytes);
// Refreshes the patched section CRC and the header CRC.
void refreshCrcs(std::vector<std::byte>& bytes, const SectionRef& section);

// Field offsets of one REQ0 row, relative to the REQ0 section payload.
struct RequirementRow final {
    std::size_t kindOffset{};
    std::size_t intervalOffset{};
    std::size_t domainOffset{};
    std::size_t actionOffset{};
    std::size_t constraintOffset{};
    std::size_t effectOffset{};
};
[[nodiscard]] auto findRequirementRow(std::span<const std::byte> payload, std::uint32_t row)
    -> std::optional<RequirementRow>;

// Field offsets of one CNS0 row, relative to the CNS0 section payload.
struct ConstraintRow final {
    std::size_t kindOffset{};
    std::size_t laneOffset{};
};
[[nodiscard]] auto findConstraintRow(std::span<const std::byte> payload, std::uint32_t row)
    -> std::optional<ConstraintRow>;

// Index of the first REF0 row with the given kind, or nullopt. Indices are one-based wire values
// only for META mainMusic; REF0 rows are referenced by their row index.
[[nodiscard]] auto findReferenceRow(std::span<const std::byte> payload, std::uint8_t kind)
    -> std::optional<std::size_t>;

// Appends a directory entry plus payload for one new section and keeps totalBytes, directory
// counters, decodedBytes, section offsets and both CRCs consistent. Existing section payloads are
// shifted, so callers must re-locate sections afterwards.
[[nodiscard]] auto appendSection(std::span<const std::byte> input, std::string_view code,
                                 std::uint8_t sectionFlags, std::uint32_t records,
                                 std::span<const std::byte> payload) -> std::vector<std::byte>;

// Renames one directory entry in place; the payload and its CRC are unchanged.
void renameSection(std::vector<std::byte>& bytes, const SectionRef& section, std::string_view code);

} // namespace cuexis::chart::packed::test
