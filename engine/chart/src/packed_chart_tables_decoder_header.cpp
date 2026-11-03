#include "packed_chart_tables_decoder_internal.hpp"

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
#include <vector>

namespace cuexis::chart::packed::decoder_detail {

auto Decoder::readHeader() -> core::Result<void> {
    ByteReader header(bytes);
    (void)header.readBytes(8);
    (void)header.readU16();
    (void)header.readU16();
    (void)header.readU32();
    (void)header.readU32();
    (void)header.readU32();
    auto directoryCount = header.readU32();
    auto directoryBytes = header.readU32();
    if (!directoryCount || !directoryBytes)
        return core::unexpected(
            fail("packed.directory.truncated", "Packed directory is truncated"));
    auto semanticHash = header.readBytes(32);
    if (!semanticHash)
        return core::unexpected(std::move(semanticHash.error()));
    auto entityCount = header.readU32();
    auto requirementCount = header.readU32();
    auto eventCount = header.readU32();
    auto decodedBytes = header.readU32();
    auto stringCount = header.readU32();
    auto refCount = header.readU32();
    auto revision = header.readU32();
    auto headerCrc = header.readU32();
    if (!entityCount || !requirementCount || !eventCount || !decodedBytes || !stringCount ||
        !refCount || !revision || !headerCrc ||
        (gameplay ? (*revision != 2U && *revision != 3U && *revision != 4U) : (*revision != 1U)))
        return core::unexpected(
            fail("packed.header.invalid", "Packed header counters are invalid"));
    directories.reserve(*directoryCount);
    std::set<std::string> seen;
    for (std::uint32_t index = 0; index < *directoryCount; ++index) {
        auto codeBytes = header.readBytes(4);
        auto codec = header.readU8();
        auto flags = header.readU8();
        auto reserved = header.readU16();
        auto offset = header.readU32();
        auto encoded = header.readU32();
        auto decoded = header.readU32();
        auto records = header.readU32();
        auto sectionCrc = header.readU32();
        auto reserved2 = header.readU32();
        if (!codeBytes || !codec || !flags || !reserved || !offset || !encoded || !decoded ||
            !records || !sectionCrc || !reserved2)
            return core::unexpected(
                fail("packed.directory.truncated", "Packed directory is truncated"));
        std::string code(reinterpret_cast<const char*>(codeBytes->data()), 4);
        if (!seen.insert(code).second)
            return core::unexpected(
                fail("packed.directory.duplicate", "Packed section is duplicated"));
        if (*codec != 0 || *reserved != 0 || *reserved2 != 0 || *encoded != *decoded ||
            *offset > bytes.size() || *encoded > bytes.size() - *offset)
            return core::unexpected(
                fail("packed.directory.invalid", "Packed directory entry is invalid"));
        // Spec 5.3: the same registry decision as inspect(). Foundation semantic sections are
        // parsed, the registered inspection section and unknown inspection sections are ignored,
        // and later-contract sections are refused.
        auto role = classifySection(code, *flags, gameplay);
        if (!role)
            return core::unexpected(std::move(role.error()));
        auto data = bytes.subspan(*offset, *encoded);
        if (crc32(data) != *sectionCrc)
            return core::unexpected(fail("packed.section.crc", "Packed section CRC mismatch"));
        directories.push_back(Directory{std::move(code), data, *records, *role});
    }
    for (const auto required :
         {"META", "TIME", "STR0", "REF0", "IDN0", "ARCH", "ENT0", "CNS0", "REQ0"})
        if (!section(required))
            return core::unexpected(
                fail("packed.directory.required", "Packed required section is missing"));
    if (gameplay) {
        for (const auto required : {"GPH0", "GPR0", "GPD0", "GRC0"}) {
            if (!section(required)) {
                return core::unexpected(
                    fail("packed.directory.required", "Gameplay required section is missing"));
            }
        }
        if (section("STR0")->records != *stringCount || section("REF0")->records != *refCount ||
            section("ENT0")->records != *entityCount || section("IDN0")->records != *entityCount ||
            section("META")->records != 1U || section("TIME")->records != 1U ||
            section("GPH0")->records != 1U || section("GPR0")->records != *requirementCount ||
            section("REQ0")->records != 0U || section("CNS0")->records != 0U ||
            section("REQ0")->data.size() != 2U || !section("CNS0")->data.empty()) {
            return core::unexpected(
                fail("packed.header.invalid", "Revision-2 section counters are inconsistent"));
        }
    }

    this->semanticHash = *semanticHash;
    this->entityCount = *entityCount;
    this->requirementCount = *requirementCount;
    this->stringCount = *stringCount;
    this->refCount = *refCount;
    this->revision = *revision;
    return {};
}

} // namespace cuexis::chart::packed::decoder_detail
