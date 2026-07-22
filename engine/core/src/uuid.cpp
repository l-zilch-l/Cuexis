//  UUID 工具实现 — 包括 UUIDv5（基于 SHA-1 的确定性命名 UUID）和 UUIDv7 格式检查
//  SHA-1 仅用于标准化命名标识生成，不用于安全用途
//  parseUuid 解析 36 字符标准 UUID 字符串；sha1 实现完整 SHA-1 哈希

#include <cuexis/core/uuid.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cuexis::core {
namespace {

using UuidBytes = std::array<std::uint8_t, 16>;

[[nodiscard]] constexpr auto hexValue(char character) noexcept -> int {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    return -1;
}

[[nodiscard]] auto parseUuid(std::string_view text) noexcept -> std::optional<UuidBytes> {
    if (text.size() != 36 || text[8] != '-' || text[13] != '-' || text[18] != '-' ||
        text[23] != '-') {
        return std::nullopt;
    }

    UuidBytes bytes{};
    std::size_t byteIndex = 0;
    for (std::size_t index = 0; index < text.size();) {
        if (text[index] == '-') {
            ++index;
            continue;
        }
        if (index + 1 >= text.size() || byteIndex >= bytes.size()) {
            return std::nullopt;
        }
        const int high = hexValue(text[index]);
        const int low = hexValue(text[index + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        bytes[byteIndex++] = static_cast<std::uint8_t>((high << 4) | low);
        index += 2;
    }
    if (byteIndex != bytes.size()) {
        return std::nullopt;
    }
    return bytes;
}

[[nodiscard]] constexpr auto hasRfcVariant(const UuidBytes& bytes) noexcept -> bool {
    return (bytes[8] & 0xC0U) == 0x80U;
}

[[nodiscard]] auto sha1(const std::vector<std::uint8_t>& input) -> std::array<std::uint8_t, 20> {
    std::vector<std::uint8_t> message = input;
    const std::uint64_t bitLength = static_cast<std::uint64_t>(message.size()) * 8U;
    message.push_back(0x80U);
    while ((message.size() % 64U) != 56U) {
        message.push_back(0U);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>((bitLength >> shift) & 0xFFU));
    }

    std::uint32_t hash0 = 0x67452301U;
    std::uint32_t hash1 = 0xEFCDAB89U;
    std::uint32_t hash2 = 0x98BADCFEU;
    std::uint32_t hash3 = 0x10325476U;
    std::uint32_t hash4 = 0xC3D2E1F0U;

    for (std::size_t offset = 0; offset < message.size(); offset += 64U) {
        std::array<std::uint32_t, 80> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const std::size_t base = offset + (index * 4U);
            words[index] = (static_cast<std::uint32_t>(message[base]) << 24U) |
                           (static_cast<std::uint32_t>(message[base + 1]) << 16U) |
                           (static_cast<std::uint32_t>(message[base + 2]) << 8U) |
                           static_cast<std::uint32_t>(message[base + 3]);
        }
        for (std::size_t index = 16; index < words.size(); ++index) {
            words[index] = std::rotl(
                words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
        }

        std::uint32_t a = hash0;
        std::uint32_t b = hash1;
        std::uint32_t c = hash2;
        std::uint32_t d = hash3;
        std::uint32_t e = hash4;
        for (std::size_t index = 0; index < words.size(); ++index) {
            std::uint32_t function{};
            std::uint32_t constant{};
            if (index < 20) {
                function = (b & c) | ((~b) & d);
                constant = 0x5A827999U;
            } else if (index < 40) {
                function = b ^ c ^ d;
                constant = 0x6ED9EBA1U;
            } else if (index < 60) {
                function = (b & c) | (b & d) | (c & d);
                constant = 0x8F1BBCDCU;
            } else {
                function = b ^ c ^ d;
                constant = 0xCA62C1D6U;
            }

            const std::uint32_t temporary =
                std::rotl(a, 5) + function + e + constant + words[index];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = temporary;
        }

        hash0 += a;
        hash1 += b;
        hash2 += c;
        hash3 += d;
        hash4 += e;
    }

    const std::array<std::uint32_t, 5> words{hash0, hash1, hash2, hash3, hash4};
    std::array<std::uint8_t, 20> result{};
    for (std::size_t index = 0; index < words.size(); ++index) {
        result[index * 4U] = static_cast<std::uint8_t>((words[index] >> 24U) & 0xFFU);
        result[index * 4U + 1U] = static_cast<std::uint8_t>((words[index] >> 16U) & 0xFFU);
        result[index * 4U + 2U] = static_cast<std::uint8_t>((words[index] >> 8U) & 0xFFU);
        result[index * 4U + 3U] = static_cast<std::uint8_t>(words[index] & 0xFFU);
    }
    return result;
}

[[nodiscard]] auto formatUuid(const UuidBytes& bytes) -> std::string {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(36);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10) {
            text.push_back('-');
        }
        text.push_back(digits[bytes[index] >> 4U]);
        text.push_back(digits[bytes[index] & 0x0FU]);
    }
    return text;
}

} // namespace

auto isUuidV7(std::string_view text) noexcept -> bool {
    const auto bytes = parseUuid(text);
    return bytes.has_value() && ((*bytes)[6] >> 4U) == 7U && hasRfcVariant(*bytes);
}

auto isUuidV5(std::string_view text) noexcept -> bool {
    const auto bytes = parseUuid(text);
    return bytes.has_value() && ((*bytes)[6] >> 4U) == 5U && hasRfcVariant(*bytes);
}

auto uuidV5(std::string_view namespaceUuid, std::string_view name) -> Result<std::string> {
    const auto namespaceBytes = parseUuid(namespaceUuid);
    if (!namespaceBytes || !hasRfcVariant(*namespaceBytes)) {
        return core::unexpected(
            core::Error{"core.uuid.invalid_namespace", "UUIDv5 namespace must be an RFC UUID"}
                .withContext("namespace", std::string{namespaceUuid}));
    }
    constexpr std::size_t maxNameBytes = 1024U * 1024U;
    if (name.size() > maxNameBytes) {
        return core::unexpected(
            core::Error{"core.uuid.name_too_long", "UUIDv5 name exceeds the supported limit"}
                .withContext("length", std::to_string(name.size())));
    }

    std::vector<std::uint8_t> input;
    input.reserve(namespaceBytes->size() + name.size());
    input.insert(input.end(), namespaceBytes->begin(), namespaceBytes->end());
    for (const char character : name) {
        input.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(character)));
    }

    const auto digest = sha1(input);
    UuidBytes result{};
    std::copy_n(digest.begin(), result.size(), result.begin());
    result[6] = static_cast<std::uint8_t>((result[6] & 0x0FU) | 0x50U);
    result[8] = static_cast<std::uint8_t>((result[8] & 0x3FU) | 0x80U);
    return formatUuid(result);
}

} // namespace cuexis::core
