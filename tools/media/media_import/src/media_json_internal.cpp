#include "media_json_internal.hpp"

#include "media_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::media_import::detail {
namespace {

[[nodiscard]] auto isJsonSpace(char character) noexcept -> bool {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

[[nodiscard]] auto invalid(const std::string& errorCode, std::string message) -> core::Error {
    return mediaError(errorCode, std::move(message));
}

[[nodiscard]] auto peek(std::string_view text, std::size_t offset) noexcept -> char {
    return offset < text.size() ? text[offset] : '\0';
}

[[nodiscard]] auto consume(std::string_view text, std::size_t& offset, char expected) noexcept
    -> bool {
    if (offset >= text.size() || text[offset] != expected) {
        return false;
    }
    ++offset;
    return true;
}

void skipSpace(std::string_view text, std::size_t& offset) noexcept {
    while (offset < text.size() && isJsonSpace(text[offset])) {
        ++offset;
    }
}

[[nodiscard]] auto readString(std::string_view text, std::size_t& offset, const std::string& code)
    -> core::Result<std::string> {
    if (!consume(text, offset, '"')) {
        return core::unexpected(invalid(code, "document expects a quoted string"));
    }
    std::string out;
    while (offset < text.size()) {
        const char character = text[offset++];
        if (character == '"') {
            return out;
        }
        if (character != '\\') {
            if (static_cast<unsigned char>(character) < 0x20) {
                return core::unexpected(
                    invalid(code, "string holds an unescaped control character"));
            }
            out.push_back(character);
            continue;
        }
        if (offset >= text.size()) {
            return core::unexpected(invalid(code, "string ends inside an escape"));
        }
        const char escape = text[offset++];
        switch (escape) {
        case '"':
            out.push_back('"');
            break;
        case '\\':
            out.push_back('\\');
            break;
        case '/':
            out.push_back('/');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'b':
            out.push_back('\b');
            break;
        case 'f':
            out.push_back('\f');
            break;
        case 'u': {
            if (offset + 4 > text.size()) {
                return core::unexpected(invalid(code, "string has a short \\u escape"));
            }
            if (text[offset] != '0' || text[offset + 1] != '0') {
                return core::unexpected(
                    invalid(code, "only the canonical \\u00XX escape is accepted"));
            }
            const auto hexValue = [](char value) -> int {
                if (value >= '0' && value <= '9') {
                    return value - '0';
                }
                if (value >= 'a' && value <= 'f') {
                    return value - 'a' + 10;
                }
                if (value >= 'A' && value <= 'F') {
                    return value - 'A' + 10;
                }
                return -1;
            };
            const int high = hexValue(text[offset + 2]);
            const int low = hexValue(text[offset + 3]);
            if (high < 0 || low < 0) {
                return core::unexpected(invalid(code, "string has a malformed \\u escape"));
            }
            out.push_back(static_cast<char>((high << 4) | low));
            offset += 4;
            break;
        }
        default:
            return core::unexpected(invalid(code, "string has an unknown escape"));
        }
    }
    return core::unexpected(invalid(code, "string is not terminated"));
}

[[nodiscard]] auto readNumber(std::string_view text, std::size_t& offset, const std::string& code)
    -> core::Result<std::string> {
    const std::size_t begin = offset;
    while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9') {
        ++offset;
    }
    if (offset == begin) {
        return core::unexpected(invalid(code, "value must be a string or an unsigned integer"));
    }
    const std::string_view digits = text.substr(begin, offset - begin);
    if (digits.size() > 1 && digits.front() == '0') {
        return core::unexpected(invalid(code, "integer has a leading zero"));
    }
    if (digits.size() > 20) {
        return core::unexpected(invalid(code, "integer is out of range"));
    }
    if (digits.size() == 20 && digits > std::string_view{"18446744073709551615"}) {
        return core::unexpected(invalid(code, "integer is out of range"));
    }
    return std::string{digits};
}

} // namespace

void appendJsonString(std::string& out, std::string_view value) {
    constexpr std::string_view hexDigits = "0123456789abcdef";
    out.push_back('"');
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (character) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (byte < 0x20) {
                out += "\\u00";
                out.push_back(hexDigits[byte >> 4]);
                out.push_back(hexDigits[byte & 0x0FU]);
            } else {
                out.push_back(character);
            }
            break;
        }
    }
    out.push_back('"');
}

auto JsonObjectReader::parse(std::string_view text, std::string errorCode)
    -> core::Result<JsonObjectReader> {
    JsonObjectReader reader;
    reader.errorCode_ = std::move(errorCode);
    std::size_t offset = 0;
    skipSpace(text, offset);
    if (!consume(text, offset, '{')) {
        return core::unexpected(invalid(reader.errorCode_, "document must be a JSON object"));
    }
    skipSpace(text, offset);
    if (peek(text, offset) == '}') {
        ++offset;
    } else {
        while (true) {
            auto key = readString(text, offset, reader.errorCode_);
            if (!key) {
                return core::unexpected(std::move(key.error()));
            }
            skipSpace(text, offset);
            if (!consume(text, offset, ':')) {
                return core::unexpected(
                    invalid(reader.errorCode_, "field is missing its ':' separator"));
            }
            skipSpace(text, offset);
            JsonField field;
            if (peek(text, offset) == '"') {
                auto value = readString(text, offset, reader.errorCode_);
                if (!value) {
                    return core::unexpected(std::move(value.error()));
                }
                field.value = std::move(*value);
            } else {
                auto number = readNumber(text, offset, reader.errorCode_);
                if (!number) {
                    return core::unexpected(std::move(number.error()));
                }
                field.value = std::move(*number);
                field.number = true;
            }
            if (reader.find(*key) != nullptr) {
                return core::unexpected(
                    invalid(reader.errorCode_, "document repeats the key '" + *key + "'"));
            }
            reader.fields_.emplace_back(std::move(*key), std::move(field));
            skipSpace(text, offset);
            const char next = peek(text, offset);
            if (next == ',') {
                ++offset;
                skipSpace(text, offset);
                continue;
            }
            if (next == '}') {
                ++offset;
                break;
            }
            return core::unexpected(
                invalid(reader.errorCode_, "document has a malformed separator"));
        }
    }
    skipSpace(text, offset);
    if (offset != text.size()) {
        return core::unexpected(invalid(reader.errorCode_, "document has trailing content"));
    }
    return reader;
}

auto JsonObjectReader::find(std::string_view key) const noexcept -> const JsonField* {
    for (const auto& entry : fields_) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

auto JsonObjectReader::take(std::string_view key) noexcept -> std::optional<JsonField> {
    for (auto iterator = fields_.begin(); iterator != fields_.end(); ++iterator) {
        if (iterator->first == key) {
            JsonField value = std::move(iterator->second);
            fields_.erase(iterator);
            return value;
        }
    }
    return std::nullopt;
}

auto JsonObjectReader::takeString(std::string_view key) -> core::Result<std::string> {
    auto field = take(key);
    if (!field.has_value()) {
        return core::unexpected(
            invalid(errorCode_, "document is missing '" + std::string{key} + "'"));
    }
    if (field->number) {
        return core::unexpected(
            invalid(errorCode_, "field '" + std::string{key} + "' must be a string"));
    }
    return std::move(field->value);
}

auto JsonObjectReader::takeNumber(std::string_view key) -> core::Result<std::uint64_t> {
    auto field = take(key);
    if (!field.has_value()) {
        return core::unexpected(
            invalid(errorCode_, "document is missing '" + std::string{key} + "'"));
    }
    if (!field->number) {
        return core::unexpected(
            invalid(errorCode_, "field '" + std::string{key} + "' must be an unsigned integer"));
    }
    std::uint64_t result = 0;
    for (const char character : field->value) {
        result = result * 10U + static_cast<std::uint64_t>(character - '0');
    }
    return result;
}

auto JsonObjectReader::takeOptionalString(std::string_view key, std::string fallback)
    -> core::Result<std::string> {
    if (find(key) == nullptr) {
        return fallback;
    }
    return takeString(key);
}

auto JsonObjectReader::remaining() const noexcept -> std::size_t {
    return fields_.size();
}

auto JsonObjectReader::firstRemainingKey() const noexcept -> std::string_view {
    return fields_.empty() ? std::string_view{} : std::string_view{fields_.front().first};
}

auto isLowerHex(std::string_view value, std::size_t length) noexcept -> bool {
    if (value.size() != length) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

auto isPortableAssetId(std::string_view value, std::size_t maxBytes) noexcept -> bool {
    if (value.empty() || value.size() > maxBytes) {
        return false;
    }
    const auto isAlphaNumeric = [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9');
    };
    if (!isAlphaNumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [&](char character) {
        return isAlphaNumeric(character) || character == '.' || character == '_' ||
               character == '/' || character == '-';
    });
}

auto isPrintableAscii(std::string_view value) noexcept -> bool {
    return std::all_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte >= 0x20 && byte <= 0x7E;
    });
}

} // namespace cuexis::media_import::detail
