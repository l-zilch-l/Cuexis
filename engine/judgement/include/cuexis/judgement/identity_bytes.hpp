#pragma once

//  The canonical identity byte writer, runtime side.
//
//  The identity encoding is a pure function of values: integers are big-endian, signed values are
//  zigzag, text is length-prefixed and every table is written in the order the caller stabilized.
//  It carries no author or graph type, so the running library and the compile side share exactly
//  one writer and therefore one byte stream. The header is standard-library only.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::judgement {

class IdentityByteWriter final {
  public:
    IdentityByteWriter() = default;
    explicit IdentityByteWriter(std::string_view signature) {
        writeText(signature);
    }

    void beginComponent(std::string_view name) {
        writeText(name);
    }

    void writeBool(bool value) {
        bytes_.push_back(value ? std::byte{0x01} : std::byte{0x00});
    }

    void writeUnsigned(std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            bytes_.push_back(std::byte{static_cast<unsigned char>((value >> shift) & 0xFFU)});
        }
    }

    void writeCount(std::size_t count) {
        writeUnsigned(static_cast<std::uint64_t>(count));
    }

    void writeSigned(std::int64_t value) {
        //  Zigzag: a negative value is mapped to an odd number and a non-negative one to an even
        //  number, so the signed domain has exactly one representation per value without relying on
        //  the width of a signed right shift.
        const std::uint64_t magnitude = value < 0 ? (~static_cast<std::uint64_t>(value) + 1U)
                                                  : static_cast<std::uint64_t>(value);
        writeUnsigned(value < 0 ? (magnitude << 1U) - 1U : (magnitude << 1U));
    }

    void writeText(std::string_view text) {
        writeCount(text.size());
        for (const char character : text) {
            bytes_.push_back(std::byte{static_cast<unsigned char>(character)});
        }
    }

    [[nodiscard]] auto take() -> std::vector<std::byte> {
        return std::move(bytes_);
    }

    void reserveAdditional(std::size_t additional) {
        if (additional > bytes_.max_size() - bytes_.size()) {
            throw std::length_error{"Identity bytes exceed vector capacity"};
        }
        bytes_.reserve(bytes_.size() + additional);
    }

    void append(std::span<const std::byte> bytes) {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

  private:
    std::vector<std::byte> bytes_;
};

//  Counts metadata without serializing a second large projection or provenance buffer.
class IdentitySizeWriter final {
  public:
    void beginComponent(std::string_view name) {
        writeText(name);
    }
    void writeBool(bool) {
        add(1);
    }
    void writeUnsigned(std::uint64_t) {
        add(8);
    }
    void writeCount(std::size_t) {
        add(8);
    }
    void writeText(std::string_view text) {
        add(8);
        add(text.size());
    }
    auto size() const noexcept -> std::size_t {
        return size_;
    }

  private:
    void add(std::size_t amount) {
        if (amount > std::numeric_limits<std::size_t>::max() - size_) {
            throw std::length_error{"Identity size overflow"};
        }
        size_ += amount;
    }
    std::size_t size_{};
};

} // namespace cuexis::judgement
