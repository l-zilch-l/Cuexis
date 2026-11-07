//  JSON serialization: the owning Value tree is written straight to text.
//
//  The previous implementation converted the whole tree into a third-party DOM (allocating a
//  second copy of every node) and dumped that. The bytes are unchanged: the layout, the escaping
//  rules and the number formatting still come from the same third-party primitives, and the chart
//  canonical identity is the SHA-256 of this text, so any byte difference would be a format
//  change rather than an implementation detail. tests/json_support/serialize_tests.cpp pins the
//  bytes of a corpus of tricky values against the ones the reference implementation produced.
//
//  Escaping matches nlohmann::detail::serializer::dump with ensure_ascii = false: the seven short
//  escapes, \u00XX for the remaining C0 controls, and every other byte copied verbatim, which is
//  why valid UTF-8 passes through untouched. Malformed UTF-8 is rejected, as the reference path
//  rejected it (its strict error handler throws); a serializer that silently emitted invalid
//  UTF-8 would let a broken chart reach a canonical identity that no other reader can reproduce.

#include <cuexis/json/parse.hpp>

#include <nlohmann/detail/conversions/to_chars.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cuexis::json {
namespace {

// Matches the reference serializer's number_buffer size; a shortest-round-trip double needs
// fewer than 32 characters, so the buffer is never the limiting factor.
constexpr std::size_t kNumberBufferBytes = 64;

// Pretty output indents by two spaces per level and never ends with a newline.
constexpr unsigned int kPrettyIndentStep = 2;

constexpr std::string_view kHexDigits = "0123456789abcdef";

constexpr auto makeEscapeTable() noexcept -> std::array<bool, 256> {
    std::array<bool, 256> table{};
    for (std::size_t index = 0; index < 0x20; ++index) {
        table[index] = true;
    }
    table[0x22] = true; // quote
    table[0x5C] = true; // backslash
    for (std::size_t index = 0x80; index < table.size(); ++index) {
        table[index] = true; // multi-byte sequences need validation, never blind copying
    }
    return table;
}

constexpr auto kNeedsEscape = makeEscapeTable();

[[noreturn]] void throwInvalidUtf8(std::size_t index) {
    throw std::runtime_error{"invalid UTF-8 at byte index " + std::to_string(index)};
}

// Continuation byte count for a lead byte, or `kNotALeadByte` for a byte that cannot start a
// sequence. 0xC0 and 0xC1 are excluded because they can only encode overlong forms, and 0xF5
// through 0xFF are excluded because they are above U+10FFFF.
constexpr unsigned int kNotALeadByte = 0xFFU;

constexpr auto continuationCount(unsigned char byte) noexcept -> unsigned int {
    if (byte < 0xC2U) {
        return kNotALeadByte;
    }
    if (byte < 0xE0U) {
        return 1;
    }
    if (byte < 0xF0U) {
        return 2;
    }
    if (byte < 0xF5U) {
        return 3;
    }
    return kNotALeadByte;
}

struct WriterState final {
    std::string& out;
    std::array<char, kNumberBufferBytes> numberBuffer{};
};

void appendUtf8Sequence(WriterState& state, std::string_view text, std::size_t index,
                        unsigned int count) {
    const auto lead = static_cast<unsigned char>(text[index]);
    std::uint32_t codepoint =
        count == 1 ? lead & 0x1FU : (count == 2 ? lead & 0x0FU : lead & 0x07U);
    for (unsigned int offset = 1; offset <= count; ++offset) {
        const auto continuation = static_cast<unsigned char>(text[index + offset]);
        if ((continuation & 0xC0U) != 0x80U) {
            throwInvalidUtf8(index + offset);
        }
        codepoint = (codepoint << 6U) | (continuation & 0x3FU);
    }
    const bool overlong = (count == 1 && codepoint < 0x80U) || (count == 2 && codepoint < 0x800U) ||
                          (count == 3 && codepoint < 0x10000U);
    const bool surrogate = codepoint >= 0xD800U && codepoint <= 0xDFFFU;
    if (overlong || surrogate || codepoint > 0x10FFFFU) {
        throwInvalidUtf8(index);
    }
    state.out.append(text.data() + index, count + 1U);
}

void appendEscaped(WriterState& state, std::string_view text) {
    std::size_t index = 0;
    std::size_t runStart = 0;
    while (index < text.size()) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if (!kNeedsEscape[byte]) {
            ++index;
            continue;
        }
        // Flush the run of bytes that need no treatment.
        state.out.append(text.data() + runStart, index - runStart);
        if (byte >= 0x80U) {
            const auto count = continuationCount(byte);
            if (count == kNotALeadByte || index + count >= text.size()) {
                throwInvalidUtf8(index);
            }
            appendUtf8Sequence(state, text, index, count);
            index += count + 1U;
        } else {
            switch (byte) {
            case 0x08:
                state.out.append("\\b", 2);
                break;
            case 0x09:
                state.out.append("\\t", 2);
                break;
            case 0x0A:
                state.out.append("\\n", 2);
                break;
            case 0x0C:
                state.out.append("\\f", 2);
                break;
            case 0x0D:
                state.out.append("\\r", 2);
                break;
            case 0x22:
                state.out.append("\\\"", 2);
                break;
            case 0x5C:
                state.out.append("\\\\", 2);
                break;
            default:
                state.out.append("\\u00", 4);
                state.out.push_back(kHexDigits[byte >> 4U]);
                state.out.push_back(kHexDigits[byte & 0x0FU]);
                break;
            }
            ++index;
        }
        runStart = index;
    }
    state.out.append(text.data() + runStart, text.size() - runStart);
}

void appendIndent(WriterState& state, unsigned int indent) {
    state.out.append(indent, ' ');
}

void dumpValue(WriterState& state, const Value& value, unsigned int indentStep,
               unsigned int indent);

void dumpObject(WriterState& state, const Value::Object& object, unsigned int indentStep,
                unsigned int indent) {
    if (object.empty()) {
        state.out.append("{}", 2);
        return;
    }
    const auto childIndent = indent + indentStep;
    const bool pretty = indentStep != 0;
    state.out.push_back('{');
    if (pretty) {
        state.out.push_back('\n');
    }
    auto iterator = object.begin();
    const auto last = std::prev(object.end());
    for (; iterator != last; ++iterator) {
        if (pretty) {
            appendIndent(state, childIndent);
        }
        state.out.push_back('"');
        appendEscaped(state, iterator->first);
        state.out.append(pretty ? "\": " : "\":", pretty ? 3 : 2);
        dumpValue(state, iterator->second, indentStep, childIndent);
        state.out.push_back(',');
        if (pretty) {
            state.out.push_back('\n');
        }
    }
    if (pretty) {
        appendIndent(state, childIndent);
    }
    state.out.push_back('"');
    appendEscaped(state, iterator->first);
    state.out.append(pretty ? "\": " : "\":", pretty ? 3 : 2);
    dumpValue(state, iterator->second, indentStep, childIndent);
    if (pretty) {
        state.out.push_back('\n');
        appendIndent(state, indent);
    }
    state.out.push_back('}');
}

void dumpArray(WriterState& state, const Value::Array& array, unsigned int indentStep,
               unsigned int indent) {
    if (array.empty()) {
        state.out.append("[]", 2);
        return;
    }
    const auto childIndent = indent + indentStep;
    const bool pretty = indentStep != 0;
    state.out.push_back('[');
    if (pretty) {
        state.out.push_back('\n');
    }
    auto iterator = array.begin();
    const auto last = std::prev(array.end());
    for (; iterator != last; ++iterator) {
        if (pretty) {
            appendIndent(state, childIndent);
        }
        dumpValue(state, *iterator, indentStep, childIndent);
        state.out.push_back(',');
        if (pretty) {
            state.out.push_back('\n');
        }
    }
    if (pretty) {
        appendIndent(state, childIndent);
    }
    dumpValue(state, *iterator, indentStep, childIndent);
    if (pretty) {
        state.out.push_back('\n');
        appendIndent(state, indent);
    }
    state.out.push_back(']');
}

void appendInteger(WriterState& state, std::int64_t value) {
    char text[24];
    const auto result = std::to_chars(text, text + sizeof(text), value);
    state.out.append(text, static_cast<std::size_t>(result.ptr - text));
}

void appendUnsigned(WriterState& state, std::uint64_t value) {
    char text[24];
    const auto result = std::to_chars(text, text + sizeof(text), value);
    state.out.append(text, static_cast<std::size_t>(result.ptr - text));
}

void appendNumber(WriterState& state, double value) {
    if (!std::isfinite(value)) {
        // JSON has no representation for these; the reference serializer writes null.
        state.out.append("null", 4);
        return;
    }
    auto* begin = state.numberBuffer.data();
    auto* end = ::nlohmann::detail::to_chars(begin, begin + state.numberBuffer.size(), value);
    state.out.append(begin, static_cast<std::size_t>(end - begin));
}

void dumpValue(WriterState& state, const Value& value, unsigned int indentStep,
               unsigned int indent) {
    switch (value.type()) {
    case ValueType::Null:
        state.out.append("null", 4);
        return;
    case ValueType::Boolean:
        if (*value.boolean()) {
            state.out.append("true", 4);
        } else {
            state.out.append("false", 5);
        }
        return;
    case ValueType::SignedInteger:
        appendInteger(state, *value.signedInteger());
        return;
    case ValueType::UnsignedInteger:
        appendUnsigned(state, *value.unsignedInteger());
        return;
    case ValueType::Number:
        appendNumber(state, *value.number());
        return;
    case ValueType::String:
        state.out.push_back('"');
        appendEscaped(state, *value.string());
        state.out.push_back('"');
        return;
    case ValueType::Array:
        dumpArray(state, *value.array(), indentStep, indent);
        return;
    case ValueType::Object:
        dumpObject(state, *value.object(), indentStep, indent);
        return;
    }
}

} // namespace

core::Result<std::string> serialize(const Value& value, SerializeStyle style) {
    try {
        std::string text;
        WriterState state{text, {}};
        dumpValue(state, value, style == SerializeStyle::Pretty ? kPrettyIndentStep : 0U, 0U);
        return text;
    } catch (const std::exception& exception) {
        return core::unexpected(
            core::Error{"json.serialize.failed", "JSON serialization failed"}.withContext(
                "exception", exception.what()));
    }
}

} // namespace cuexis::json
