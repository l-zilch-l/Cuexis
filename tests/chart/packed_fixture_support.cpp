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

} // namespace cuexis::chart::packed::test
