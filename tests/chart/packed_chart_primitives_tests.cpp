#include <cuexis/chart/packed_chart_primitives.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

auto readerFor(std::span<const std::byte> bytes) -> cuexis::chart::packed::ByteReader {
    return cuexis::chart::packed::ByteReader{bytes};
}

} // namespace

TEST_CASE("Packed primitives use little endian fixed-width integers", "[chart][packed][f3]") {
    cuexis::chart::packed::ByteWriter writer;
    writer.writeU16(0x1234U);
    writer.writeU32(0x12345678U);
    writer.writeU64(0x0123456789abcdefULL);

    auto reader = readerFor(writer.bytes());
    REQUIRE(*reader.readU16() == 0x1234U);
    REQUIRE(*reader.readU32() == 0x12345678U);
    REQUIRE(*reader.readU64() == 0x0123456789abcdefULL);
    CHECK(reader.empty());
}

TEST_CASE("Packed LEB128 and ZigZag round trip signed boundaries", "[chart][packed][f3]") {
    const std::array values = {std::int64_t{0}, std::int64_t{-1}, std::int64_t{1},
                               std::numeric_limits<std::int64_t>::min(),
                               std::numeric_limits<std::int64_t>::max()};
    cuexis::chart::packed::ByteWriter writer;
    for (const auto value : values) {
        writer.writeSignedZigZag(value);
    }
    auto reader = readerFor(writer.bytes());
    for (const auto value : values) {
        REQUIRE(*reader.readSignedZigZag() == value);
    }
    CHECK(reader.empty());
}

TEST_CASE("Packed codecs reject truncation, overflow, and non-minimal varints",
          "[chart][packed][f3]") {
    SECTION("truncated") {
        const std::array bytes = {std::byte{0x80}};
        auto reader = readerFor(bytes);
        const auto result = reader.readUnsignedLeb128();
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "packed.codec.truncated");
    }
    SECTION("non-minimal") {
        const std::array bytes = {std::byte{0x80}, std::byte{0x00}};
        auto reader = readerFor(bytes);
        const auto result = reader.readUnsignedLeb128();
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "packed.codec.non_minimal");
    }
    SECTION("overflow") {
        const std::array bytes = {
            std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
            std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0x02}};
        auto reader = readerFor(bytes);
        const auto result = reader.readUnsignedLeb128();
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "packed.codec.overflow");
    }
}

TEST_CASE("Packed CRC32 matches ISO-HDLC check vector", "[chart][packed][f3]") {
    constexpr std::array bytes = {std::byte{'1'}, std::byte{'2'}, std::byte{'3'},
                                  std::byte{'4'}, std::byte{'5'}, std::byte{'6'},
                                  std::byte{'7'}, std::byte{'8'}, std::byte{'9'}};
    CHECK(cuexis::chart::packed::crc32(bytes) == 0xcbf43926U);
}

TEST_CASE("Packed Rational Beat atoms are lossless and canonical", "[chart][packed][f3]") {
    const auto beat = *cuexis::chart::RationalBeat::create(-9, 6);
    cuexis::chart::packed::ByteWriter writer;
    REQUIRE(cuexis::chart::packed::writeRationalBeatAtom(writer, beat));
    auto reader = readerFor(writer.bytes());
    const auto decoded = reader.readUnsignedLeb128();
    REQUIRE(decoded);
    auto readerAgain = readerFor(writer.bytes());
    const auto roundTrip = cuexis::chart::packed::readRationalBeatAtom(readerAgain);
    REQUIRE(roundTrip);
    CHECK(*roundTrip == beat);
    CHECK(readerAgain.empty());

    cuexis::chart::packed::ByteWriter nonCanonical;
    nonCanonical.writeSignedZigZag(2);
    nonCanonical.writeUnsignedLeb128(4);
    auto nonCanonicalReader = readerFor(nonCanonical.bytes());
    const auto rejected = cuexis::chart::packed::readRationalBeatAtom(nonCanonicalReader);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "packed.beat.non_canonical");
}

TEST_CASE("Packed GridDelta atoms use checked exact accumulation", "[chart][packed][f3]") {
    const auto first = *cuexis::chart::RationalBeat::create(1, 4);
    const auto second = *cuexis::chart::RationalBeat::create(3, 4);
    cuexis::chart::packed::ByteWriter writer;
    std::int64_t previous{};
    REQUIRE(cuexis::chart::packed::writeGridDeltaAtom(writer, first, 4, previous));
    REQUIRE(cuexis::chart::packed::writeGridDeltaAtom(writer, second, 4, previous));

    auto reader = readerFor(writer.bytes());
    cuexis::chart::packed::GridDeltaDecoder decoder{4};
    REQUIRE(*decoder.readAtom(reader) == first);
    REQUIRE(*decoder.readAtom(reader) == second);
    CHECK(reader.empty());

    const auto offGrid = *cuexis::chart::RationalBeat::create(1, 3);
    REQUIRE_FALSE(cuexis::chart::packed::writeGridDeltaAtom(writer, offGrid, 4, previous));

    cuexis::chart::packed::ByteWriter overflowing;
    overflowing.writeSignedZigZag(std::numeric_limits<std::int64_t>::max());
    overflowing.writeSignedZigZag(1);
    auto overflowingReader = readerFor(overflowing.bytes());
    cuexis::chart::packed::GridDeltaDecoder overflowingDecoder{
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())};
    REQUIRE(overflowingDecoder.readAtom(overflowingReader));
    const auto rejected = overflowingDecoder.readAtom(overflowingReader);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "packed.codec.overflow");
}
