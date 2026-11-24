#pragma once

// Primitive codecs for the candidate Packed Chart wire format.

#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace cuexis::chart::packed {

class ByteWriter final {
  public:
    void writeU8(std::uint8_t value);
    void writeU16(std::uint16_t value);
    void writeU32(std::uint32_t value);
    void writeU64(std::uint64_t value);
    void writeBytes(std::span<const std::byte> bytes);
    void writeUnsignedLeb128(std::uint64_t value);
    void writeSignedZigZag(std::int64_t value);

    [[nodiscard]] auto bytes() const noexcept -> std::span<const std::byte> {
        return bytes_;
    }
    [[nodiscard]] auto takeBytes() && noexcept -> std::vector<std::byte> {
        return std::move(bytes_);
    }

  private:
    std::vector<std::byte> bytes_;
};

class ByteReader final {
  public:
    explicit ByteReader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] auto position() const noexcept -> std::size_t {
        return position_;
    }
    [[nodiscard]] auto remaining() const noexcept -> std::size_t {
        return bytes_.size() - position_;
    }
    [[nodiscard]] auto empty() const noexcept -> bool {
        return position_ == bytes_.size();
    }

    [[nodiscard]] auto readU8() -> core::Result<std::uint8_t>;
    [[nodiscard]] auto readU16() -> core::Result<std::uint16_t>;
    [[nodiscard]] auto readU32() -> core::Result<std::uint32_t>;
    [[nodiscard]] auto readU64() -> core::Result<std::uint64_t>;
    [[nodiscard]] auto readBytes(std::size_t count) -> core::Result<std::span<const std::byte>>;
    [[nodiscard]] auto
    readUnsignedLeb128(std::uint64_t maxValue = std::numeric_limits<std::uint64_t>::max())
        -> core::Result<std::uint64_t>;
    [[nodiscard]] auto readSignedZigZag() -> core::Result<std::int64_t>;

  private:
    std::span<const std::byte> bytes_;
    std::size_t position_{};
};

using PackedByteWriter = ByteWriter;
using PackedByteReader = ByteReader;

[[nodiscard]] auto crc32(std::span<const std::byte> bytes) noexcept -> std::uint32_t;

enum class BeatEncodingMode : std::uint8_t { Rational = 0, GridDelta = 1 };

struct BeatStreamDescriptor final {
    BeatEncodingMode mode{BeatEncodingMode::Rational};
    std::uint64_t denominator{};
};

[[nodiscard]] auto writeBeatDescriptor(ByteWriter& writer, BeatStreamDescriptor descriptor)
    -> core::Result<void>;
[[nodiscard]] auto readBeatDescriptor(ByteReader& reader) -> core::Result<BeatStreamDescriptor>;

[[nodiscard]] auto writeRationalBeatAtom(ByteWriter& writer, const RationalBeat& beat)
    -> core::Result<void>;
[[nodiscard]] auto readRationalBeatAtom(ByteReader& reader) -> core::Result<RationalBeat>;

class GridDeltaDecoder final {
  public:
    explicit GridDeltaDecoder(std::uint64_t denominator) noexcept : denominator_(denominator) {}

    [[nodiscard]] auto readAtom(ByteReader& reader) -> core::Result<RationalBeat>;
    [[nodiscard]] auto scaledNumerator() const noexcept -> std::int64_t {
        return scaledNumerator_;
    }

  private:
    std::uint64_t denominator_;
    std::int64_t scaledNumerator_{};
};

[[nodiscard]] auto writeGridDeltaAtom(ByteWriter& writer, const RationalBeat& beat,
                                      std::uint64_t denominator,
                                      std::int64_t& previousScaledNumerator) -> core::Result<void>;

} // namespace cuexis::chart::packed
