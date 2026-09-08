#include <cuexis/chart/packed_chart_primitives.hpp>

#include <cuexis/core/error.hpp>

#include <limits>
#include <numeric>
#include <string>
#include <utility>

namespace cuexis::chart::packed {
namespace {

[[nodiscard]] constexpr auto magnitude(std::int64_t value) noexcept -> std::uint64_t {
    if (value >= 0) {
        return static_cast<std::uint64_t>(value);
    }
    return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

[[nodiscard]] auto error(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

[[nodiscard]] auto checkedAdd(std::int64_t left, std::int64_t right) -> core::Result<std::int64_t> {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right)) {
        return core::unexpected(
            error("packed.codec.overflow", "Signed integer arithmetic overflowed"));
    }
    return left + right;
}

[[nodiscard]] auto checkedSubtract(std::int64_t left, std::int64_t right)
    -> core::Result<std::int64_t> {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left < minimum + right) || (right < 0 && left > maximum + right)) {
        return core::unexpected(
            error("packed.codec.overflow", "Signed integer arithmetic overflowed"));
    }
    return left - right;
}

[[nodiscard]] auto checkedMultiply(std::int64_t left, std::int64_t right)
    -> core::Result<std::int64_t> {
    if (left == 0 || right == 0) {
        return 0;
    }
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((left == -1 && right == minimum) || (right == -1 && left == minimum) ||
        (left > 0 && right > 0 && left > maximum / right) ||
        (left > 0 && right < 0 && right < minimum / left) ||
        (left < 0 && right > 0 && left < minimum / right) ||
        (left < 0 && right < 0 && left < maximum / right)) {
        return core::unexpected(
            error("packed.codec.overflow", "Signed integer arithmetic overflowed"));
    }
    return left * right;
}

[[nodiscard]] constexpr auto zigZagEncode(std::int64_t value) noexcept -> std::uint64_t {
    const auto absolute = magnitude(value);
    return value < 0 ? (absolute * 2U - 1U) : absolute * 2U;
}

[[nodiscard]] auto zigZagDecode(std::uint64_t value) -> core::Result<std::int64_t> {
    const auto negative = (value & 1U) != 0;
    const auto absolute = value >> 1U;
    constexpr auto negativeMagnitude =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if ((!negative &&
         absolute > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) ||
        (negative && absolute > negativeMagnitude)) {
        return core::unexpected(
            error("packed.codec.overflow", "ZigZag value is outside int64 range"));
    }
    if (negative && absolute == negativeMagnitude) {
        return std::numeric_limits<std::int64_t>::min();
    }
    const auto result = static_cast<std::int64_t>(absolute);
    return negative ? -(result + 1) : result;
}

[[nodiscard]] auto isCanonical(const RationalBeat& beat) -> bool {
    auto canonical = RationalBeat::create(beat.numerator(), beat.denominator());
    return canonical && *canonical == beat;
}

} // namespace

void ByteWriter::writeU8(std::uint8_t value) {
    bytes_.push_back(static_cast<std::byte>(value));
}

void ByteWriter::writeU16(std::uint16_t value) {
    for (unsigned shift = 0; shift < 16; shift += 8U) {
        writeU8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void ByteWriter::writeU32(std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8U) {
        writeU8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void ByteWriter::writeU64(std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8U) {
        writeU8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void ByteWriter::writeBytes(std::span<const std::byte> bytes) {
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
}

void ByteWriter::writeUnsignedLeb128(std::uint64_t value) {
    do {
        auto byte = static_cast<std::uint8_t>(value & 0x7fU);
        value >>= 7U;
        if (value != 0) {
            byte = static_cast<std::uint8_t>(byte | 0x80U);
        }
        writeU8(byte);
    } while (value != 0);
}

void ByteWriter::writeSignedZigZag(std::int64_t value) {
    writeUnsignedLeb128(zigZagEncode(value));
}

auto ByteReader::readU8() -> core::Result<std::uint8_t> {
    auto bytes = readBytes(1);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    return std::to_integer<std::uint8_t>((*bytes)[0]);
}

auto ByteReader::readU16() -> core::Result<std::uint16_t> {
    auto bytes = readBytes(2);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>((*bytes)[0]) |
        (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>((*bytes)[1])) << 8U));
}

auto ByteReader::readU32() -> core::Result<std::uint32_t> {
    auto bytes = readBytes(4);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    std::uint32_t value{};
    for (unsigned index = 0; index < 4; ++index) {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>((*bytes)[index]))
                 << (index * 8U);
    }
    return value;
}

auto ByteReader::readU64() -> core::Result<std::uint64_t> {
    auto bytes = readBytes(8);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    std::uint64_t value{};
    for (unsigned index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>((*bytes)[index]))
                 << (index * 8U);
    }
    return value;
}

auto ByteReader::readBytes(std::size_t count) -> core::Result<std::span<const std::byte>> {
    if (count > remaining()) {
        return core::unexpected(error("packed.codec.truncated", "Packed byte range is truncated"));
    }
    const auto result = bytes_.subspan(position_, count);
    position_ += count;
    return result;
}

auto ByteReader::readUnsignedLeb128(std::uint64_t maxValue) -> core::Result<std::uint64_t> {
    std::uint64_t value{};
    for (unsigned index = 0; index < 10; ++index) {
        auto byte = readU8();
        if (!byte) {
            return core::unexpected(std::move(byte.error()));
        }
        const auto payload = static_cast<std::uint64_t>(*byte & 0x7fU);
        if (index == 9 && ((*byte & 0x80U) != 0 || payload > 1U)) {
            return core::unexpected(
                error("packed.codec.overflow", "Unsigned LEB128 exceeds uint64 range"));
        }
        value |= payload << (index * 7U);
        if ((*byte & 0x80U) == 0) {
            if (index > 0 && payload == 0) {
                return core::unexpected(
                    error("packed.codec.non_minimal", "Unsigned LEB128 is not shortest"));
            }
            if (value > maxValue) {
                return core::unexpected(
                    error("packed.codec.overflow", "Unsigned LEB128 exceeds field range"));
            }
            return value;
        }
    }
    return core::unexpected(error("packed.codec.overflow", "Unsigned LEB128 is too long"));
}

auto ByteReader::readSignedZigZag() -> core::Result<std::int64_t> {
    auto encoded = readUnsignedLeb128();
    if (!encoded) {
        return core::unexpected(std::move(encoded.error()));
    }
    return zigZagDecode(*encoded);
}

auto crc32(std::span<const std::byte> bytes) noexcept -> std::uint32_t {
    std::uint32_t result = 0xffffffffU;
    for (const auto byte : bytes) {
        result ^= std::to_integer<std::uint8_t>(byte);
        for (unsigned bit = 0; bit < 8; ++bit) {
            result = (result & 1U) != 0 ? (result >> 1U) ^ 0xedb88320U : result >> 1U;
        }
    }
    return result ^ 0xffffffffU;
}

auto writeBeatDescriptor(ByteWriter& writer, BeatStreamDescriptor descriptor)
    -> core::Result<void> {
    if (descriptor.mode == BeatEncodingMode::Rational) {
        writer.writeU8(0);
        return {};
    }
    if (descriptor.mode != BeatEncodingMode::GridDelta || descriptor.denominator == 0 ||
        descriptor.denominator >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return core::unexpected(
            error("packed.beat.invalid_descriptor", "Beat stream descriptor is invalid"));
    }
    writer.writeU8(1);
    writer.writeUnsignedLeb128(descriptor.denominator);
    return {};
}

auto readBeatDescriptor(ByteReader& reader) -> core::Result<BeatStreamDescriptor> {
    auto mode = reader.readU8();
    if (!mode) {
        return core::unexpected(std::move(mode.error()));
    }
    if (*mode == 0) {
        return BeatStreamDescriptor{BeatEncodingMode::Rational, 0};
    }
    if (*mode != 1) {
        return core::unexpected(
            error("packed.beat.invalid_descriptor", "Beat stream mode is unknown"));
    }
    auto denominator = reader.readUnsignedLeb128(
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
    if (!denominator || *denominator == 0) {
        if (!denominator) {
            return core::unexpected(std::move(denominator.error()));
        }
        return core::unexpected(
            error("packed.beat.invalid_denominator", "Beat grid denominator must be positive"));
    }
    return BeatStreamDescriptor{BeatEncodingMode::GridDelta, *denominator};
}

auto writeRationalBeatAtom(ByteWriter& writer, const RationalBeat& beat) -> core::Result<void> {
    if (!isCanonical(beat)) {
        return core::unexpected(
            error("packed.beat.non_canonical", "Rational Beat atom is not reduced"));
    }
    writer.writeSignedZigZag(beat.numerator());
    writer.writeUnsignedLeb128(static_cast<std::uint64_t>(beat.denominator()));
    return {};
}

auto readRationalBeatAtom(ByteReader& reader) -> core::Result<RationalBeat> {
    auto numerator = reader.readSignedZigZag();
    if (!numerator) {
        return core::unexpected(std::move(numerator.error()));
    }
    auto denominator = reader.readUnsignedLeb128(
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
    if (!denominator || *denominator == 0) {
        if (!denominator) {
            return core::unexpected(std::move(denominator.error()));
        }
        return core::unexpected(
            error("packed.beat.invalid_denominator", "Beat denominator must be positive"));
    }
    auto beat = RationalBeat::create(*numerator, static_cast<std::int64_t>(*denominator));
    if (!beat) {
        return core::unexpected(std::move(beat.error()));
    }
    if (beat->numerator() != *numerator ||
        beat->denominator() != static_cast<std::int64_t>(*denominator)) {
        return core::unexpected(
            error("packed.beat.non_canonical", "Rational Beat atom is not reduced"));
    }
    return *beat;
}

auto GridDeltaDecoder::readAtom(ByteReader& reader) -> core::Result<RationalBeat> {
    if (denominator_ == 0 ||
        denominator_ > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return core::unexpected(
            error("packed.beat.invalid_denominator", "Beat grid denominator must be positive"));
    }
    auto delta = reader.readSignedZigZag();
    if (!delta) {
        return core::unexpected(std::move(delta.error()));
    }
    auto scaled = checkedAdd(scaledNumerator_, *delta);
    if (!scaled) {
        return core::unexpected(std::move(scaled.error()));
    }
    auto beat = RationalBeat::create(*scaled, static_cast<std::int64_t>(denominator_));
    if (!beat) {
        return core::unexpected(std::move(beat.error()));
    }
    scaledNumerator_ = *scaled;
    return *beat;
}

auto writeGridDeltaAtom(ByteWriter& writer, const RationalBeat& beat, std::uint64_t denominator,
                        std::int64_t& previousScaledNumerator) -> core::Result<void> {
    if (denominator == 0 ||
        denominator > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return core::unexpected(
            error("packed.beat.invalid_denominator", "Beat grid denominator must be positive"));
    }
    if (!isCanonical(beat)) {
        return core::unexpected(
            error("packed.beat.non_canonical", "Rational Beat atom is not reduced"));
    }
    const auto divisor =
        std::gcd(magnitude(beat.numerator()), static_cast<std::uint64_t>(beat.denominator()));
    const auto reducedNumeratorMagnitude = magnitude(beat.numerator()) / divisor;
    const auto reducedDenominator = static_cast<std::uint64_t>(beat.denominator()) / divisor;
    if (denominator % reducedDenominator != 0) {
        return core::unexpected(
            error("packed.beat.not_on_grid", "Beat cannot be represented on grid"));
    }
    const auto factor = static_cast<std::int64_t>(denominator / reducedDenominator);
    auto reducedNumerator = static_cast<std::int64_t>(reducedNumeratorMagnitude);
    if (beat.numerator() < 0) {
        if (reducedNumeratorMagnitude == (std::uint64_t{1} << 63U)) {
            reducedNumerator = std::numeric_limits<std::int64_t>::min();
        } else {
            reducedNumerator = -reducedNumerator;
        }
    }
    auto scaled = checkedMultiply(reducedNumerator, factor);
    if (!scaled) {
        return core::unexpected(std::move(scaled.error()));
    }
    auto delta = checkedSubtract(*scaled, previousScaledNumerator);
    if (!delta) {
        return core::unexpected(std::move(delta.error()));
    }
    writer.writeSignedZigZag(*delta);
    previousScaledNumerator = *scaled;
    return {};
}

} // namespace cuexis::chart::packed
