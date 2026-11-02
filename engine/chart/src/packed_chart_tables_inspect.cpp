#include <cuexis/chart/packed_chart_tables.hpp>

#include <cuexis/chart/uuid.hpp>
#include <cuexis/core/error.hpp>

#include "packed_chart_tables_units_internal.hpp"
#include "packed_gameplay_internal.hpp"
#include "packed_identity_internal.hpp"
#include "packed_limits_internal.hpp"
#include "packed_profile_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace cuexis::chart::packed {

auto inspectImpl(std::span<const std::byte> bytes, PackedChartLimits limits, bool gameplay)
    -> core::Result<PackedChartStatistics> {
    const auto budget = limits_detail::effectiveLimits(limits);
    if (bytes.size() < 96)
        return core::unexpected(
            fail("packed.header.invalid_size", "Packed chart is smaller than the fixed header"));
    if (bytes.size() > budget.maxPackedFileBytes)
        return core::unexpected(
            fail("packed.budget.file_bytes", "Packed chart exceeds the file byte budget"));
    ByteReader reader(bytes);
    auto magic = reader.readBytes(8);
    if (!magic || std::memcmp(magic->data(), "CXPK5\0\0\0", 8) != 0)
        return core::unexpected(
            fail("packed.header.invalid_magic", "Packed chart magic is invalid"));
    auto version = reader.readU16();
    auto header = reader.readU16();
    auto flags = reader.readU32();
    auto total = reader.readU32();
    auto dirOff = reader.readU32();
    auto count = reader.readU32();
    auto dirBytes = reader.readU32();
    if (!version || !header || !flags || !total || !dirOff || !count || !dirBytes)
        return core::unexpected(fail("packed.header.truncated", "Packed header is truncated"));
    // Spec 3.2 and 5.1: the directory size is a checked uint64 product before any reservation and
    // before the directory is read, so a wrapped count cannot size an allocation.
    const auto directoryBytesExpected = static_cast<std::uint64_t>(*count) * 32U;
    if (*version != 1 || *header != 96 || *flags != 1 || *dirOff != 96 || *total != bytes.size() ||
        directoryBytesExpected > std::numeric_limits<std::uint32_t>::max() ||
        *dirBytes != static_cast<std::uint32_t>(directoryBytesExpected) ||
        static_cast<std::uint64_t>(*dirOff) + *dirBytes > bytes.size())
        return core::unexpected(fail("packed.header.invalid", "Packed header fields are invalid"));
    auto semanticHash = reader.readBytes(32);
    if (!semanticHash)
        return core::unexpected(std::move(semanticHash.error()));
    auto entities = reader.readU32();
    auto requirements = reader.readU32();
    auto events = reader.readU32();
    auto decoded = reader.readU32();
    auto strings = reader.readU32();
    auto refs = reader.readU32();
    auto revision = reader.readU32();
    auto headerCrc = reader.readU32();
    if (!entities || !requirements || !events || !decoded || !strings || !refs || !revision ||
        !headerCrc)
        return core::unexpected(
            fail("packed.header.truncated", "Packed header counters are truncated"));
    if (*headerCrc != crc32(bytes.first(92)))
        return core::unexpected(fail("packed.header.crc", "Packed header CRC mismatch"));
    // Spec 5.1: only the implemented candidate revision may be read, and the Foundation profile
    // declares no schedule events. Neither declaration may be ignored as "nothing to do".
    if (gameplay ? (*revision != 2U && *revision != 3U && *revision != 4U) : (*revision != 1U))
        return core::unexpected(
            fail("packed.header.unsupported_revision", "Packed candidate revision is unsupported"));
    if (*events != 0U)
        return core::unexpected(
            fail("packed.header.events", "Foundation revision 1 declares no schedule events"));
    // Spec 3.2: the declared counters are pre-checked per field, so a rejection names the budget
    // that was exceeded instead of a single aggregate counter diagnostic. The counters are only
    // a pre-check; decode() re-counts the actual tables and compares them below.
    if (*entities > budget.maxPackedEntities)
        return core::unexpected(
            fail("packed.budget.entities", "Packed chart exceeds the entity budget"));
    if (*requirements > budget.maxPackedRequirements)
        return core::unexpected(
            fail("packed.budget.requirements", "Packed chart exceeds the requirement budget"));
    if (*strings > budget.maxPackedStrings)
        return core::unexpected(
            fail("packed.budget.strings", "Packed chart exceeds the string budget"));
    if (*refs > budget.maxPackedReferences)
        return core::unexpected(
            fail("packed.budget.references", "Packed chart exceeds the typed reference budget"));
    if (*decoded > budget.maxPackedDecodedBytes)
        return core::unexpected(
            fail("packed.budget.decoded_bytes", "Packed chart exceeds the decoded byte budget"));
    PackedChartStatistics stats{bytes.size(), *decoded, *strings, *refs, *entities, *requirements};
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    ranges.reserve(*count);
    std::set<std::string> sectionNames;
    std::uint64_t decodedSum = 0;
    for (std::uint32_t i = 0; i < *count; ++i) {
        auto code = reader.readBytes(4);
        auto codec = reader.readU8();
        auto fl = reader.readU8();
        auto res = reader.readU16();
        auto off = reader.readU32();
        auto enc = reader.readU32();
        auto dec = reader.readU32();
        auto rec = reader.readU32();
        auto crc = reader.readU32();
        auto res2 = reader.readU32();
        if (!code || !codec || !fl || !res || !off || !enc || !dec || !rec || !crc || !res2)
            return core::unexpected(
                fail("packed.directory.invalid", "Packed directory entry is truncated"));
        std::string name(reinterpret_cast<const char*>(code->data()), 4);
        if (*codec != 0 || *res != 0 || *res2 != 0 || *enc != *dec || *off > bytes.size() ||
            *enc > bytes.size() - *off)
            return core::unexpected(
                fail("packed.directory.invalid", "Packed directory entry is invalid"));
        if (!sectionNames.insert(name).second)
            return core::unexpected(
                fail("packed.directory.duplicate", "Packed section is duplicated"));
        if (auto role = classifySection(name, *fl, gameplay); !role)
            return core::unexpected(std::move(role.error()));
        // Spec 3.3: the per-section ceiling applies to every directory entry, including the
        // optional inspection sections whose payload a reader still has to read and CRC-check.
        // It precedes the section CRC so an over-budget section is refused before its payload is
        // touched.
        if (*dec > budget.maxPackedSectionBytes)
            return core::unexpected(
                fail("packed.budget.section_bytes", "Packed section exceeds the section budget"));
        if (crc32(bytes.subspan(*off, *enc)) != *crc)
            return core::unexpected(fail("packed.section.crc", "Packed section CRC mismatch"));
        ranges.emplace_back(*off, *enc);
        decodedSum += *dec;
    }
    std::sort(ranges.begin(), ranges.end());
    std::uint64_t expectedOffset = 96U + static_cast<std::uint64_t>(*dirBytes);
    for (const auto& range : ranges) {
        if (range.first != expectedOffset)
            return core::unexpected(
                fail("packed.directory.layout", "Packed section ranges overlap or contain gaps"));
        expectedOffset += range.second;
    }
    if (expectedOffset != bytes.size() || decodedSum != *decoded)
        return core::unexpected(
            fail("packed.directory.size", "Packed decoded byte count is inconsistent"));
    return stats;
}

auto inspect(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<PackedChartStatistics> {
    return inspectImpl(bytes, limits, false);
}

auto decode(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    return decodeImpl(bytes, limits, false, nullptr);
}

auto gameplay_detail::inspect(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<PackedChartStatistics> {
    return inspectImpl(bytes, limits, true);
}

auto gameplay_detail::decodeStatic(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<DecodedStatic> {
    DecodedStatic decoded;
    auto chart = decodeImpl(bytes, limits, true, &decoded);
    if (!chart) {
        return core::unexpected(std::move(chart.error()));
    }
    decoded.chart = std::move(*chart);
    return decoded;
}

} // namespace cuexis::chart::packed
