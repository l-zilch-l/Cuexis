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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::packed::test {

// Spec 5.1 fixed header offsets.
inline constexpr std::size_t headerSize = 96U;
inline constexpr std::size_t packedVersionOffset = 8U;
inline constexpr std::size_t headerFlagsOffset = 12U;
inline constexpr std::size_t semanticIdentityOffset = 32U;
inline constexpr std::size_t eventCountOffset = 72U;
inline constexpr std::size_t candidateRevisionOffset = 88U;

[[nodiscard]] auto readU8(std::span<const std::byte> bytes, std::size_t offset) -> std::uint8_t;
[[nodiscard]] auto readU16(std::span<const std::byte> bytes, std::size_t offset) -> std::uint16_t;
[[nodiscard]] auto readU32(std::span<const std::byte> bytes, std::size_t offset) -> std::uint32_t;
[[nodiscard]] auto readU64(std::span<const std::byte> bytes, std::size_t offset) -> std::uint64_t;
void writeU8(std::vector<std::byte>& bytes, std::size_t offset, std::uint8_t value);
void writeU16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value);
void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value);
void writeU64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value);

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

// Replaces one existing section payload, keeping the directory order and every other payload
// intact. Section offsets, totalBytes, decodedBytes, the replaced section CRC and the header CRC
// are recomputed, so the result stays structurally valid and isolates the rule under test.
[[nodiscard]] auto replaceSection(std::span<const std::byte> input, std::size_t sectionIndex,
                                  std::span<const std::byte> payload) -> std::vector<std::byte>;

// STR0 payload accessors for crafting non-canonical dictionaries.
[[nodiscard]] auto readStrings(std::span<const std::byte> payload) -> std::vector<std::string>;
[[nodiscard]] auto buildStrings(const std::vector<std::string>& values) -> std::vector<std::byte>;

struct ReferenceRow final {
    std::uint8_t kind{};
    std::uint32_t stringIndex{};
};
[[nodiscard]] auto readReferences(std::span<const std::byte> payload) -> std::vector<ReferenceRow>;
[[nodiscard]] auto buildReferences(const std::vector<ReferenceRow>& rows) -> std::vector<std::byte>;

// Byte ranges (begin, end) of the rows of one table payload. `requirementRowRanges` skips the
// two Beat codec bytes of REQ0; the identity ranges only apply to charts whose entities are all
// explicit, where the IDN0 header is scopeCount, pathCount and identityCount.
[[nodiscard]] auto requirementRowRanges(std::span<const std::byte> payload)
    -> std::vector<std::pair<std::size_t, std::size_t>>;
[[nodiscard]] auto constraintRowRanges(std::span<const std::byte> payload)
    -> std::vector<std::pair<std::size_t, std::size_t>>;
[[nodiscard]] auto archetypeRowRanges(std::span<const std::byte> payload)
    -> std::vector<std::pair<std::size_t, std::size_t>>;
[[nodiscard]] auto explicitIdentityRecordRanges(std::span<const std::byte> payload)
    -> std::vector<std::pair<std::size_t, std::size_t>>;

// Rebuilds a table payload by concatenating the given row ranges in `order`, prefixed by the
// first `prefixBytes` bytes of the original payload (REQ0's codec header, IDN0's counters, ...).
[[nodiscard]] auto reorderRows(std::span<const std::byte> payload,
                               const std::vector<std::pair<std::size_t, std::size_t>>& rows,
                               const std::vector<std::size_t>& order, std::size_t prefixBytes)
    -> std::vector<std::byte>;

} // namespace cuexis::chart::packed::test
