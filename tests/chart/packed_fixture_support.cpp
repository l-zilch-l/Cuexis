#include "packed_fixture_support.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::packed::test {
namespace {

constexpr std::size_t directoryOffset = 96U;
constexpr std::size_t directoryEntrySize = 32U;
constexpr std::size_t totalBytesField = 16U;
constexpr std::size_t directoryCountField = 24U;
constexpr std::size_t directoryBytesField = 28U;
constexpr std::size_t decodedBytesField = 76U;
constexpr std::size_t headerCrcField = 92U;

void appendU32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffU));
    }
}

} // namespace

auto readU8(std::span<const std::byte> bytes, std::size_t offset) -> std::uint8_t {
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

auto readU16(std::span<const std::byte> bytes, std::size_t offset) -> std::uint16_t {
    std::uint16_t value = 0;
    for (std::size_t index = 0; index < 2U; ++index) {
        value |= static_cast<std::uint16_t>(readU8(bytes, offset + index)) << (8U * index);
    }
    return value;
}

auto readU32(std::span<const std::byte> bytes, std::size_t offset) -> std::uint32_t {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
        value |= static_cast<std::uint32_t>(readU8(bytes, offset + index)) << (8U * index);
    }
    return value;
}

auto readU64(std::span<const std::byte> bytes, std::size_t offset) -> std::uint64_t {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8U; ++index) {
        value |= static_cast<std::uint64_t>(readU8(bytes, offset + index)) << (8U * index);
    }
    return value;
}

void writeU8(std::vector<std::byte>& bytes, std::size_t offset, std::uint8_t value) {
    bytes[offset] = static_cast<std::byte>(value);
}

void writeU16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    for (std::size_t index = 0; index < 2U; ++index) {
        writeU8(bytes, offset + index, static_cast<std::uint8_t>((value >> (8U * index)) & 0xffU));
    }
}

void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4U; ++index) {
        writeU8(bytes, offset + index, static_cast<std::uint8_t>((value >> (8U * index)) & 0xffU));
    }
}

void writeU64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t index = 0; index < 8U; ++index) {
        writeU8(bytes, offset + index, static_cast<std::uint8_t>((value >> (8U * index)) & 0xffU));
    }
}

auto findSection(std::span<const std::byte> bytes, std::string_view code)
    -> std::optional<SectionRef> {
    if (bytes.size() < headerSize) {
        return std::nullopt;
    }
    const auto count = readU32(bytes, directoryCountField);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto base = directoryOffset + directoryEntrySize * static_cast<std::size_t>(index);
        if (base + directoryEntrySize > bytes.size()) {
            return std::nullopt;
        }
        const std::string_view name{reinterpret_cast<const char*>(bytes.data() + base), 4U};
        if (name == code) {
            return SectionRef{index, readU32(bytes, base + 8U), readU32(bytes, base + 12U),
                              readU32(bytes, base + 20U)};
        }
    }
    return std::nullopt;
}

void refreshSectionCrc(std::vector<std::byte>& bytes, const SectionRef& section) {
    const auto crc = crc32(std::span<const std::byte>{bytes.data() + section.offset, section.size});
    writeU32(bytes, directoryOffset + directoryEntrySize * section.index + 24U, crc);
}

void refreshHeaderCrc(std::vector<std::byte>& bytes) {
    writeU32(bytes, headerCrcField, crc32(std::span<const std::byte>{bytes.data(), 92U}));
}

void refreshCrcs(std::vector<std::byte>& bytes, const SectionRef& section) {
    refreshSectionCrc(bytes, section);
    refreshHeaderCrc(bytes);
}

auto findRequirementRow(std::span<const std::byte> payload, std::uint32_t row)
    -> std::optional<RequirementRow> {
    ByteReader reader{payload};
    // REQ0 starts with the start/end Beat descriptors, both Rational (mode 0) in Foundation.
    if (!reader.readU8() || !reader.readU8()) {
        return std::nullopt;
    }
    for (std::uint32_t index = 0; index <= row; ++index) {
        if (!reader.readUnsignedLeb128() || !reader.readUnsignedLeb128()) {
            return std::nullopt;
        }
        const auto kindOffset = reader.position();
        if (!reader.readU8()) {
            return std::nullopt;
        }
        const auto intervalOffset = reader.position();
        auto interval = reader.readU8();
        if (!interval) {
            return std::nullopt;
        }
        if (!readRationalBeatAtom(reader)) {
            return std::nullopt;
        }
        if (*interval == 1U && !readRationalBeatAtom(reader)) {
            return std::nullopt;
        }
        const auto domainOffset = reader.position();
        if (!reader.readUnsignedLeb128()) {
            return std::nullopt;
        }
        const auto actionOffset = reader.position();
        if (!reader.readUnsignedLeb128()) {
            return std::nullopt;
        }
        const auto constraintOffset = reader.position();
        if (!reader.readUnsignedLeb128()) {
            return std::nullopt;
        }
        const auto effectOffset = reader.position();
        if (!reader.readUnsignedLeb128()) {
            return std::nullopt;
        }
        if (index == row) {
            return RequirementRow{kindOffset,   intervalOffset,   domainOffset,
                                  actionOffset, constraintOffset, effectOffset};
        }
    }
    return std::nullopt;
}

auto findConstraintRow(std::span<const std::byte> payload, std::uint32_t row)
    -> std::optional<ConstraintRow> {
    ByteReader reader{payload};
    for (std::uint32_t index = 0; index <= row; ++index) {
        const auto kindOffset = reader.position();
        if (!reader.readU8()) {
            return std::nullopt;
        }
        const auto laneOffset = reader.position();
        if (!reader.readUnsignedLeb128()) {
            return std::nullopt;
        }
        if (index == row) {
            return ConstraintRow{kindOffset, laneOffset};
        }
    }
    return std::nullopt;
}

auto findReferenceRow(std::span<const std::byte> payload, std::uint8_t kind)
    -> std::optional<std::size_t> {
    ByteReader reader{payload};
    std::size_t row = 0;
    while (!reader.empty()) {
        auto rowKind = reader.readU8();
        if (!rowKind || !reader.readUnsignedLeb128()) {
            return std::nullopt;
        }
        if (*rowKind == kind) {
            return row;
        }
        ++row;
    }
    return std::nullopt;
}

auto appendSection(std::span<const std::byte> input, std::string_view code,
                   std::uint8_t sectionFlags, std::uint32_t records,
                   std::span<const std::byte> payload) -> std::vector<std::byte> {
    const auto directoryCount = readU32(input, directoryCountField);
    const auto directoryBytes = readU32(input, directoryBytesField);
    const auto totalBytes = readU32(input, totalBytesField);
    const auto decodedBytes = readU32(input, decodedBytesField);
    const auto payloadSize = static_cast<std::uint32_t>(payload.size());
    std::vector<std::byte> output;
    output.reserve(totalBytes + directoryEntrySize + payload.size());
    output.insert(output.end(), input.begin(),
                  input.begin() + static_cast<std::ptrdiff_t>(headerSize));
    // Existing entries keep their payloads but every payload moves by one directory entry.
    for (std::uint32_t index = 0; index < directoryCount; ++index) {
        const auto base = directoryOffset + directoryEntrySize * static_cast<std::size_t>(index);
        output.insert(output.end(), input.begin() + static_cast<std::ptrdiff_t>(base),
                      input.begin() + static_cast<std::ptrdiff_t>(base + directoryEntrySize));
        writeU32(output, output.size() - directoryEntrySize + 8U,
                 readU32(input, base + 8U) + static_cast<std::uint32_t>(directoryEntrySize));
    }
    for (const auto character : code) {
        output.push_back(static_cast<std::byte>(character));
    }
    output.push_back(std::byte{0}); // codec: uncompressed
    output.push_back(static_cast<std::byte>(sectionFlags));
    output.push_back(std::byte{0}); // reserved
    output.push_back(std::byte{0});
    appendU32(output, totalBytes + static_cast<std::uint32_t>(directoryEntrySize));
    appendU32(output, payloadSize);
    appendU32(output, payloadSize);
    appendU32(output, records);
    appendU32(output, crc32(payload));
    appendU32(output, 0U); // reserved2
    output.insert(output.end(),
                  input.begin() + static_cast<std::ptrdiff_t>(headerSize + directoryBytes),
                  input.end());
    output.insert(output.end(), payload.begin(), payload.end());

    writeU32(output, totalBytesField,
             totalBytes + static_cast<std::uint32_t>(directoryEntrySize) + payloadSize);
    writeU32(output, directoryCountField, directoryCount + 1U);
    writeU32(output, directoryBytesField,
             directoryBytes + static_cast<std::uint32_t>(directoryEntrySize));
    writeU32(output, decodedBytesField, decodedBytes + payloadSize);
    refreshHeaderCrc(output);
    return output;
}

void renameSection(std::vector<std::byte>& bytes, const SectionRef& section,
                   std::string_view code) {
    const auto base = directoryOffset + directoryEntrySize * section.index;
    for (std::size_t index = 0; index < 4U; ++index) {
        writeU8(bytes, base + index, static_cast<std::uint8_t>(code[index]));
    }
}

auto replaceSection(std::span<const std::byte> input, std::size_t sectionIndex,
                    std::span<const std::byte> payload) -> std::vector<std::byte> {
    const auto directoryCount = readU32(input, directoryCountField);
    const auto directoryBytes = readU32(input, directoryBytesField);
    const auto totalBytes = readU32(input, totalBytesField);
    const auto decodedBytes = readU32(input, decodedBytesField);
    const auto base = directoryOffset + directoryEntrySize * sectionIndex;
    if (sectionIndex >= directoryCount) {
        return {};
    }
    const auto previousSize = readU32(input, base + 12U);
    const auto payloadSize = static_cast<std::uint32_t>(payload.size());
    const auto newTotal = totalBytes - previousSize + payloadSize;
    const auto newDecoded = decodedBytes - previousSize + payloadSize;

    std::vector<std::byte> output;
    output.reserve(newTotal);
    output.insert(output.end(), input.begin(),
                  input.begin() + static_cast<std::ptrdiff_t>(headerSize));
    std::uint32_t offset = static_cast<std::uint32_t>(headerSize + directoryBytes);
    for (std::uint32_t index = 0; index < directoryCount; ++index) {
        const auto source = directoryOffset + directoryEntrySize * static_cast<std::size_t>(index);
        const auto target = headerSize + directoryEntrySize * static_cast<std::size_t>(index);
        output.insert(output.end(), input.begin() + static_cast<std::ptrdiff_t>(source),
                      input.begin() + static_cast<std::ptrdiff_t>(source + directoryEntrySize));
        writeU32(output, target + 8U, offset);
        if (index == sectionIndex) {
            writeU32(output, target + 12U, payloadSize);
            writeU32(output, target + 16U, payloadSize);
            writeU32(output, target + 24U, crc32(payload));
        }
        offset += readU32(output, target + 12U);
    }
    for (std::uint32_t index = 0; index < directoryCount; ++index) {
        const auto source = directoryOffset + directoryEntrySize * static_cast<std::size_t>(index);
        const auto sourceOffset = readU32(input, source + 8U);
        const auto sourceSize = readU32(input, source + 12U);
        if (index == sectionIndex) {
            output.insert(output.end(), payload.begin(), payload.end());
            continue;
        }
        output.insert(output.end(), input.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
                      input.begin() + static_cast<std::ptrdiff_t>(sourceOffset + sourceSize));
    }
    writeU32(output, totalBytesField, newTotal);
    writeU32(output, decodedBytesField, newDecoded);
    refreshHeaderCrc(output);
    return output;
}

auto readStrings(std::span<const std::byte> payload) -> std::vector<std::string> {
    ByteReader reader{payload};
    auto count = reader.readU32();
    auto dataBytes = reader.readU32();
    if (!count || !dataBytes) {
        return {};
    }
    std::vector<std::uint32_t> offsets;
    offsets.reserve(*count + 1U);
    for (std::uint32_t index = 0; index < *count + 1U; ++index) {
        auto value = reader.readU32();
        if (!value) {
            return {};
        }
        offsets.push_back(*value);
    }
    auto data = reader.readBytes(*dataBytes);
    if (!data) {
        return {};
    }
    std::vector<std::string> values;
    values.reserve(*count);
    for (std::uint32_t index = 0; index < *count; ++index) {
        values.emplace_back(reinterpret_cast<const char*>(data->data() + offsets[index]),
                            offsets[index + 1U] - offsets[index]);
    }
    return values;
}

auto buildStrings(const std::vector<std::string>& values) -> std::vector<std::byte> {
    std::vector<std::byte> output;
    appendU32(output, static_cast<std::uint32_t>(values.size()));
    std::uint32_t dataBytes = 0;
    for (const auto& value : values) {
        dataBytes += static_cast<std::uint32_t>(value.size());
    }
    appendU32(output, dataBytes);
    std::uint32_t offset = 0;
    appendU32(output, offset);
    for (const auto& value : values) {
        offset += static_cast<std::uint32_t>(value.size());
        appendU32(output, offset);
    }
    for (const auto& value : values) {
        for (const auto character : value) {
            output.push_back(static_cast<std::byte>(character));
        }
    }
    return output;
}

auto readReferences(std::span<const std::byte> payload) -> std::vector<ReferenceRow> {
    ByteReader reader{payload};
    std::vector<ReferenceRow> rows;
    while (!reader.empty()) {
        auto kind = reader.readU8();
        auto index = reader.readUnsignedLeb128();
        if (!kind || !index) {
            return {};
        }
        rows.push_back(ReferenceRow{*kind, static_cast<std::uint32_t>(*index)});
    }
    return rows;
}

auto buildReferences(const std::vector<ReferenceRow>& rows) -> std::vector<std::byte> {
    ByteWriter writer;
    for (const auto& row : rows) {
        writer.writeU8(row.kind);
        writer.writeUnsignedLeb128(row.stringIndex);
    }
    return std::move(writer).takeBytes();
}

auto requirementRowRanges(std::span<const std::byte> payload)
    -> std::vector<std::pair<std::size_t, std::size_t>> {
    ByteReader reader{payload};
    if (!reader.readU8() || !reader.readU8()) {
        return {};
    }
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    while (!reader.empty()) {
        const auto begin = reader.position();
        if (!reader.readUnsignedLeb128() || !reader.readUnsignedLeb128()) {
            return {};
        }
        if (!reader.readU8()) {
            return {};
        }
        auto interval = reader.readU8();
        if (!interval) {
            return {};
        }
        if (!readRationalBeatAtom(reader)) {
            return {};
        }
        if (*interval == 1U && !readRationalBeatAtom(reader)) {
            return {};
        }
        if (!reader.readUnsignedLeb128() || !reader.readUnsignedLeb128() ||
            !reader.readUnsignedLeb128() || !reader.readUnsignedLeb128()) {
            return {};
        }
        ranges.emplace_back(begin, reader.position());
    }
    return ranges;
}

auto constraintRowRanges(std::span<const std::byte> payload)
    -> std::vector<std::pair<std::size_t, std::size_t>> {
    ByteReader reader{payload};
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    while (!reader.empty()) {
        const auto begin = reader.position();
        if (!reader.readU8() || !reader.readUnsignedLeb128()) {
            return {};
        }
        ranges.emplace_back(begin, reader.position());
    }
    return ranges;
}

auto archetypeRowRanges(std::span<const std::byte> payload)
    -> std::vector<std::pair<std::size_t, std::size_t>> {
    ByteReader reader{payload};
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    while (!reader.empty()) {
        const auto begin = reader.position();
        if (!reader.readU64()) {
            return {};
        }
        auto payloadBytes = reader.readU32();
        if (!payloadBytes || !reader.readBytes(*payloadBytes)) {
            return {};
        }
        ranges.emplace_back(begin, reader.position());
    }
    return ranges;
}

auto explicitIdentityRecordRanges(std::span<const std::byte> payload)
    -> std::vector<std::pair<std::size_t, std::size_t>> {
    ByteReader reader{payload};
    auto scopeCount = reader.readU32();
    auto pathCount = reader.readU32();
    auto identityCount = reader.readU32();
    if (!scopeCount || !pathCount || !identityCount || *scopeCount != 0U || *pathCount != 0U) {
        return {};
    }
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    ranges.reserve(*identityCount);
    for (std::uint32_t index = 0; index < *identityCount; ++index) {
        const auto begin = reader.position();
        auto tag = reader.readU8();
        if (!tag || *tag != 0U || !reader.readBytes(16)) {
            return {};
        }
        ranges.emplace_back(begin, reader.position());
    }
    if (!reader.empty()) {
        return {};
    }
    return ranges;
}

auto reorderRows(std::span<const std::byte> payload,
                 const std::vector<std::pair<std::size_t, std::size_t>>& rows,
                 const std::vector<std::size_t>& order, std::size_t prefixBytes)
    -> std::vector<std::byte> {
    std::vector<std::byte> output;
    if (prefixBytes > payload.size()) {
        return output;
    }
    output.insert(output.end(), payload.begin(),
                  payload.begin() + static_cast<std::ptrdiff_t>(prefixBytes));
    for (const auto index : order) {
        if (index >= rows.size() || rows[index].second > payload.size()) {
            return {};
        }
        output.insert(output.end(),
                      payload.begin() + static_cast<std::ptrdiff_t>(rows[index].first),
                      payload.begin() + static_cast<std::ptrdiff_t>(rows[index].second));
    }
    return output;
}

} // namespace cuexis::chart::packed::test
