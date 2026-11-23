#include <cuexis/chart/packed_chart_tables.hpp>

#include <cuexis/core/error.hpp>

#include "packed_chart_tables_units_internal.hpp"
#include "packed_identity_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::packed {

auto fail(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

// Checked narrowing for the fixed-width wire fields of the artifact. Every call site is preceded
// by a budget gate that makes the value fit; the check keeps that provable instead of implicit.
// The diagnostic is distinct from the byte budgets on purpose: a wire-range overflow is a
// representability failure, not a budget violation, so it must not be reported as
// `packed.budget.section_bytes` (see Spec 3.3 and the review note SPEC-02).
[[nodiscard]] auto narrowU32(std::uint64_t value) -> core::Result<std::uint32_t> {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        return core::unexpected(
            fail("packed.field.wire_range", "Packed field exceeds the uint32 wire range"));
    }
    return static_cast<std::uint32_t>(value);
}

// Spec 5.3 section registry. inspect() and decode() share this decision so the two entry points
// cannot disagree about which sections an artifact may carry.
[[nodiscard]] auto classifySection(std::string_view name, std::uint8_t flags, bool gameplay)
    -> core::Result<SectionRole> {
    static constexpr std::array<std::string_view, 12> semantic{"META", "TIME", "STR0", "REF0",
                                                               "IDN0", "ARCH", "ENT0", "TRN0",
                                                               "REN0", "CAM0", "CNS0", "REQ0"};
    static constexpr std::array<std::string_view, 4> refused{"ANM0", "BEH0", "BHD0", "FXS0"};
    if (flags > 1U) {
        return core::unexpected(
            fail("packed.directory.flags", "Packed section flags must be 0 or 1"));
    }
    const bool gameplaySection =
        gameplay && (name == "GPH0" || name == "GPR0" || name == "GPD0" || name == "GRC0");
    if (gameplaySection || std::find(semantic.begin(), semantic.end(), name) != semantic.end()) {
        if (flags != 0U) {
            return core::unexpected(
                fail("packed.directory.flags", "Foundation semantic sections require flags=0"));
        }
        return SectionRole::semantic;
    }
    if (name == "DBG0") {
        if (flags != 1U) {
            return core::unexpected(
                fail("packed.directory.flags",
                     "The registered inspection section DBG0 requires flags=1"));
        }
        return SectionRole::inspection;
    }
    if (std::find(refused.begin(), refused.end(), name) != refused.end()) {
        if (flags != 0U) {
            return core::unexpected(
                fail("packed.directory.flags", "Foundation semantic sections require flags=0"));
        }
        return core::unexpected(
            fail("packed.directory.refused",
                 "Packed section belongs to a later field contract and is refused by Foundation"));
    }
    // Spec 5.2: unknown inspection sections may be ignored after length and CRC checks, but they
    // never provide runtime data. Unknown semantic sections are always refused.
    if (flags == 1U) {
        return SectionRole::inspection;
    }
    return core::unexpected(
        fail("packed.directory.invalid", "Packed section code is not registered"));
}

auto identityText(std::span<const std::byte> bytes) -> std::string {
    static constexpr char digits[] = "0123456789abcdef";
    std::string value;
    value.reserve(36);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index == 4U || index == 6U || index == 8U || index == 10U)
            value.push_back('-');
        const auto byteValue = std::to_integer<std::uint8_t>(bytes[index]);
        value.push_back(digits[byteValue >> 4]);
        value.push_back(digits[byteValue & 15]);
    }
    return value;
}

auto readF32(ByteReader& reader) -> core::Result<float> {
    auto v = reader.readU32();
    if (!v)
        return core::unexpected(std::move(v.error()));
    return std::bit_cast<float>(*v);
}
auto readF64(ByteReader& reader) -> core::Result<double> {
    auto v = reader.readU64();
    if (!v)
        return core::unexpected(std::move(v.error()));
    return std::bit_cast<double>(*v);
}

auto validUtf8(std::string_view text) -> bool {
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    unsigned remaining = 0;
    for (const unsigned char byte : text) {
        if (remaining == 0) {
            if (byte < 0x80U) {
                continue;
            }
            if (byte >= 0xc2U && byte <= 0xdfU) {
                value = byte & 0x1fU;
                minimum = 0x80U;
                remaining = 1;
            } else if (byte >= 0xe0U && byte <= 0xefU) {
                value = byte & 0x0fU;
                minimum = 0x800U;
                remaining = 2;
            } else if (byte >= 0xf0U && byte <= 0xf4U) {
                value = byte & 0x07U;
                minimum = 0x10000U;
                remaining = 3;
            } else {
                return false;
            }
        } else {
            if ((byte & 0xc0U) != 0x80U) {
                return false;
            }
            value = (value << 6U) | (byte & 0x3fU);
            if (--remaining == 0 &&
                (value < minimum || value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU))) {
                return false;
            }
        }
    }
    return remaining == 0;
}

auto identityKey(const CanonicalEntityIdentity& identity) -> core::Result<std::vector<std::byte>> {
    return identity_detail::canonicalIdentityBytes(identity);
}

} // namespace cuexis::chart::packed
