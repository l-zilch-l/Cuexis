// Byte-level regression coverage for json::serialize.
//
// The chart canonical text is the byte identity of a chart (its SHA-256 is the
// chart identity), so the serializer is not allowed to drift by a single byte.
// `serialize_golden.hpp` records the bytes produced by the reference
// implementation, so a refactor of the serializer has to reproduce them for
// every tricky value: doubles in every magnitude, control characters, escaped
// keys, empty containers, deep nesting and UTF-8 pass-through.

#include "serialize_golden.hpp"

#include <cuexis/json/parse.hpp>
#include <cuexis/json/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

constexpr cuexis::json::ParseLimits kLimits{64 * 1024, 32, 4096};

auto encodeBase64(std::string_view text) -> std::string {
    static constexpr std::string_view kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((text.size() + 2) / 3) * 4);
    std::size_t index = 0;
    while (index + 3 <= text.size()) {
        const auto triple =
            (static_cast<std::uint32_t>(static_cast<unsigned char>(text[index])) << 16U) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 1])) << 8U) |
            static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 2]));
        out.push_back(kAlphabet[(triple >> 18U) & 0x3FU]);
        out.push_back(kAlphabet[(triple >> 12U) & 0x3FU]);
        out.push_back(kAlphabet[(triple >> 6U) & 0x3FU]);
        out.push_back(kAlphabet[triple & 0x3FU]);
        index += 3;
    }
    const auto remaining = text.size() - index;
    if (remaining == 1) {
        const auto byte = static_cast<std::uint32_t>(static_cast<unsigned char>(text[index]));
        out.push_back(kAlphabet[(byte >> 2U) & 0x3FU]);
        out.push_back(kAlphabet[(byte << 4U) & 0x3FU]);
        out.append("==");
    } else if (remaining == 2) {
        const auto pair =
            (static_cast<std::uint32_t>(static_cast<unsigned char>(text[index])) << 8U) |
            static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 1]));
        out.push_back(kAlphabet[(pair >> 10U) & 0x3FU]);
        out.push_back(kAlphabet[(pair >> 4U) & 0x3FU]);
        out.push_back(kAlphabet[(pair << 2U) & 0x3FU]);
        out.push_back('=');
    }
    return out;
}

} // namespace

TEST_CASE("json::serialize reproduces the recorded golden bytes", "[json][serialize]") {
    REQUIRE(cuexis_json_golden::kCaseCount > 0);
    for (std::size_t index = 0; index < cuexis_json_golden::kCaseCount; ++index) {
        const auto& golden = cuexis_json_golden::kCases[index];
        INFO("corpus case: " << golden.name);
        REQUIRE_FALSE(golden.compact.empty());
        REQUIRE_FALSE(golden.pretty.empty());

        const auto parsed = cuexis::json::parse(golden.source, kLimits);
        REQUIRE(parsed.has_value());

        const auto compact =
            cuexis::json::serialize(*parsed, cuexis::json::SerializeStyle::Compact);
        REQUIRE(compact.has_value());
        CHECK(*compact == golden.compact);

        const auto pretty = cuexis::json::serialize(*parsed, cuexis::json::SerializeStyle::Pretty);
        REQUIRE(pretty.has_value());
        CHECK(*pretty == golden.pretty);

        // Serialized text must be a fixed point: re-parsing either style and
        // serializing again has to reproduce the same bytes. (Value equality is
        // deliberately not asserted here: the DOM distinguishes the signed
        // integer 0 from the unsigned integer 0 because `-0` and `0` are
        // different literals, while both print as `0`.)
        const auto reparsedCompact = cuexis::json::parse(*compact, kLimits);
        REQUIRE(reparsedCompact.has_value());
        const auto againCompact =
            cuexis::json::serialize(*reparsedCompact, cuexis::json::SerializeStyle::Compact);
        REQUIRE(againCompact.has_value());
        CHECK(*againCompact == *compact);

        const auto reparsedPretty = cuexis::json::parse(*pretty, kLimits);
        REQUIRE(reparsedPretty.has_value());
        const auto againPretty =
            cuexis::json::serialize(*reparsedPretty, cuexis::json::SerializeStyle::Pretty);
        REQUIRE(againPretty.has_value());
        CHECK(*againPretty == *pretty);
    }
}

TEST_CASE("json::serialize pretty output is indented by two spaces per level",
          "[json][serialize]") {
    const auto parsed = cuexis::json::parse(R"({"a":{"b":[1,{}]}})", kLimits);
    REQUIRE(parsed.has_value());
    const auto pretty = cuexis::json::serialize(*parsed, cuexis::json::SerializeStyle::Pretty);
    REQUIRE(pretty.has_value());
    CHECK(*pretty == "{\n"
                     "  \"a\": {\n"
                     "    \"b\": [\n"
                     "      1,\n"
                     "      {}\n"
                     "    ]\n"
                     "  }\n"
                     "}");
}

// Hidden unless explicitly requested: prints the bytes this binary produces so
// that gen_serialize_golden.py can freeze a new golden header after an
// intentional, reviewed change to the serializer.
TEST_CASE("json::serialize golden capture", "[.serialize-capture]") {
    for (std::size_t index = 0; index < cuexis_json_golden::kCaseCount; ++index) {
        const auto& golden = cuexis_json_golden::kCases[index];
        const auto parsed = cuexis::json::parse(golden.source, kLimits);
        REQUIRE(parsed.has_value());
        const auto compact =
            cuexis::json::serialize(*parsed, cuexis::json::SerializeStyle::Compact);
        REQUIRE(compact.has_value());
        const auto pretty = cuexis::json::serialize(*parsed, cuexis::json::SerializeStyle::Pretty);
        REQUIRE(pretty.has_value());
        const auto line = std::string{"CUE|"} + std::string{golden.name} + "|" +
                          encodeBase64(*compact) + "|" + encodeBase64(*pretty);
        std::printf("%s\n", line.c_str());
    }
    std::fflush(stdout);
}
