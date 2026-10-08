#include <cuexis/json/parse.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <stdexcept>

namespace {
class TypedEventFixture final : public cuexis::json::ISaxEventSink {
  public:
    std::int64_t signedValue{};
    std::uint64_t unsignedValue{};
    std::string ownedText;
    std::size_t values{}, starts{}, ends{};
    bool rejectUnknown{}, throwOnValue{};
    auto onEvent(const cuexis::json::SaxEvent& event) -> cuexis::core::Result<void> override {
        using enum cuexis::json::SaxEventKind;
        switch (event.kind) {
        case Key:
            if (rejectUnknown && event.text == "unknown")
                return cuexis::core::unexpected(
                    cuexis::core::Error{"fixture.unknown_field", "Unknown typed field"});
            break;
        case SignedInteger:
            signedValue = event.signedInteger;
            ++values;
            break;
        case UnsignedInteger:
            unsignedValue = event.unsignedInteger;
            ++values;
            break;
        case String:
            ownedText = event.text;
            ++values;
            break;
        case ObjectStart:
        case ArrayStart:
            ++starts;
            break;
        case ObjectEnd:
        case ArrayEnd:
            ++ends;
            break;
        default:
            ++values;
            break;
        }
        if (throwOnValue && values)
            throw std::runtime_error{"typed construction failed"};
        return {};
    }
};
} // namespace

TEST_CASE("SAX events deliver exact typed integers and borrowed text without a Value tree",
          "[json][parse][bounded][events]") {
    TypedEventFixture fixture;
    auto result = cuexis::json::parseEvents(
        R"({"i":-9223372036854775808,"u":18446744073709551615,"s":"owned","a":[]})",
        {{1024, 3, 16}, 5, 4}, fixture);
    REQUIRE(result);
    CHECK(fixture.signedValue == std::numeric_limits<std::int64_t>::min());
    CHECK(fixture.unsignedValue == std::numeric_limits<std::uint64_t>::max());
    CHECK(fixture.ownedText == "owned");
    CHECK(fixture.values == 3);
    CHECK(fixture.starts == 2);
    CHECK(fixture.ends == 2);
}

TEST_CASE("SAX handler rejects unknown fields before payload and exceptions stay in JSON boundary",
          "[json][parse][bounded][events]") {
    TypedEventFixture fixture;
    fixture.rejectUnknown = true;
    auto unknown =
        cuexis::json::parseEvents(R"({"unknown":[invalid])", {{1024, 3, 16}, 8, 4}, fixture);
    REQUIRE_FALSE(unknown);
    CHECK(unknown.error().code() == "fixture.unknown_field");
    CHECK(fixture.values == 0);
    CHECK(fixture.starts == 1);
    TypedEventFixture throwing;
    throwing.throwOnValue = true;
    auto failed = cuexis::json::parseEvents("[1]", {{1024, 3, 16}, 8, 4}, throwing);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code() == "json.parse.failed");
}

TEST_CASE("SAX event mode shares duplicate and container admission with bounded Value mode",
          "[json][parse][bounded][events]") {
    const cuexis::json::ValueParseLimits limits{{1024, 3, 16}, 8, 2};
    for (const auto text : {R"({"a":1,"a":[invalid])", "[1,2,[invalid]", "[[[[invalid]"}) {
        TypedEventFixture fixture;
        auto events = cuexis::json::parseEvents(text, limits, fixture);
        auto tree = cuexis::json::parseBounded(text, limits);
        REQUIRE_FALSE(events);
        REQUIRE_FALSE(tree);
        CHECK(events.error().code() == tree.error().code());
    }
    TypedEventFixture fixture;
    auto malformed = cuexis::json::parseEvents("{} true", limits, fixture);
    REQUIRE_FALSE(malformed);
    CHECK(malformed.error().code() == "json.parse.syntax_error");
}

TEST_CASE("Bounded SAX preserves exact integer extremes without a JSON DOM",
          "[json][parse][bounded]") {
    const auto parsed = cuexis::json::parseBounded(
        R"({"i":-9223372036854775808,"u":18446744073709551615,"a":[9007199254740993,1.25]})",
        {{1024, 3, 16}, 6, 3});
    REQUIRE(parsed);
    CHECK(*parsed->find("i")->signedInteger() == std::numeric_limits<std::int64_t>::min());
    CHECK(*parsed->find("u")->unsignedInteger() == std::numeric_limits<std::uint64_t>::max());
    const auto& array = *parsed->find("a")->array();
    CHECK(*array[0].unsignedInteger() == UINT64_C(9007199254740993));
    CHECK(*array[1].number() == 1.25);
    const auto ordinary = cuexis::json::parse(
        R"({"i":-9223372036854775808,"u":18446744073709551615,"a":[9007199254740993,1.25]})",
        {1024, 3, 16});
    REQUIRE(ordinary);
    CHECK(*parsed == *ordinary);
}

TEST_CASE("Bounded SAX rejects before reading duplicate or overflowing child payloads",
          "[json][parse][bounded]") {
    const cuexis::json::ValueParseLimits limits{{1024, 3, 8}, 4, 2};
    // Invalid syntax in a rejected child's payload proves the callback stopped earlier.
    for (const auto text : {R"({"a":1,"a":[invalid])", R"({"a":{},"a":[invalid])"}) {
        auto rejected = cuexis::json::parseBounded(text, limits);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "json.parse.duplicate_key");
    }
    auto array = cuexis::json::parseBounded("[1,2,[invalid]", limits);
    REQUIRE_FALSE(array);
    CHECK(array.error().code() == "json.parse.element_limit");
    auto object = cuexis::json::parseBounded(R"({"a":1,"b":2,"c":[invalid])", limits);
    REQUIRE_FALSE(object);
    CHECK(object.error().code() == "json.parse.element_limit");
    auto count = cuexis::json::parseBounded("[[1,2],3]", limits);
    REQUIRE_FALSE(count);
    CHECK(count.error().code() == "json.parse.value_limit");
    auto depth = cuexis::json::parseBounded("[[[[invalid]", limits);
    REQUIRE_FALSE(depth);
    CHECK(depth.error().code() == "json.parse.depth_limit");
    REQUIRE(cuexis::json::parseBounded("[[1],2]", limits));
    REQUIRE(cuexis::json::parseBounded(R"({"a":[],"b":{}})", limits));
}

TEST_CASE("Bounded SAX retains text limits and rejects malformed input atomically",
          "[json][parse][bounded]") {
    const cuexis::json::ValueParseLimits limits{{64, 3, 3}, 8, 3};
    auto decoded = cuexis::json::parseBounded(R"({"\u4F60":"\u4F60"})", limits);
    REQUIRE(decoded);
    for (const auto text : {R"({"abcd":0})", R"({"v":"abcd"})"}) {
        auto result = cuexis::json::parseBounded(text, limits);
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "json.parse.string_limit");
    }
    for (const auto text : {"", "[", "[1,]", "{} true", "18446744073709551616 trailing"}) {
        auto result = cuexis::json::parseBounded(text, limits);
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "json.parse.syntax_error");
    }
    auto size = cuexis::json::parseBounded(std::string(65, ' '), limits);
    REQUIRE_FALSE(size);
    CHECK(size.error().code() == "json.parse.size_limit");
    auto zero = limits;
    zero.maxValues = 0;
    auto result = cuexis::json::parseBounded("null", zero);
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == "json.parse.invalid_limits");
}

TEST_CASE("JSON parse preserves value categories and stable object order", "[json][parse]") {
    const auto result = cuexis::json::parse(R"({"z":null,"a":[-2,3,1.5,true,"text"]})",
                                            cuexis::json::ParseLimits{1024, 8, 1024});

    REQUIRE(result.has_value());
    const auto* object = result->object();
    REQUIRE(object != nullptr);
    REQUIRE(object->begin()->first == "a");

    const auto* array = result->find("a")->array();
    REQUIRE(array != nullptr);
    REQUIRE(*(*array)[0].signedInteger() == -2);
    REQUIRE(*(*array)[1].unsignedInteger() == 3);
    REQUIRE(*(*array)[2].number() == 1.5);
    REQUIRE(*(*array)[3].boolean());
    REQUIRE(*(*array)[4].string() == "text");
}

TEST_CASE("JSON parse rejects duplicate keys and configured limits", "[json][parse]") {
    const auto duplicate =
        cuexis::json::parse(R"({"id":1,"id":2})", cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE_FALSE(duplicate.has_value());
    REQUIRE(duplicate.error().code() == "json.parse.duplicate_key");

    const auto tooLarge =
        cuexis::json::parse(R"({"value":1})", cuexis::json::ParseLimits{4, 8, 1024});
    REQUIRE_FALSE(tooLarge.has_value());
    REQUIRE(tooLarge.error().code() == "json.parse.size_limit");

    const auto tooDeep =
        cuexis::json::parse(R"({"a":{"b":1}})", cuexis::json::ParseLimits{1024, 1, 1024});
    REQUIRE_FALSE(tooDeep.has_value());
    REQUIRE(tooDeep.error().code() == "json.parse.depth_limit");

    const auto invalidStringLimit =
        cuexis::json::parse("null", cuexis::json::ParseLimits{1024, 8, 0});
    REQUIRE_FALSE(invalidStringLimit.has_value());
    REQUIRE(invalidStringLimit.error().code() == "json.parse.invalid_limits");
}

TEST_CASE("JSON parse limits decoded UTF-8 bytes in object keys and string values",
          "[json][parse][limits]") {
    const auto boundary =
        cuexis::json::parse(R"({"\u4F60":"\u4F60"})", cuexis::json::ParseLimits{1024, 8, 3});
    REQUIRE(boundary.has_value());

    const auto oversizedValue =
        cuexis::json::parse(R"({"v":"\u4F60"})", cuexis::json::ParseLimits{1024, 8, 2});
    REQUIRE_FALSE(oversizedValue.has_value());
    REQUIRE(oversizedValue.error().code() == "json.parse.string_limit");
    REQUIRE(oversizedValue.error().context().size() == 3);
    CHECK(oversizedValue.error().context()[0].key == "string_kind");
    CHECK(oversizedValue.error().context()[0].value == "string_value");
    CHECK(oversizedValue.error().context()[1].key == "actual_bytes");
    CHECK(oversizedValue.error().context()[1].value == "3");
    CHECK(oversizedValue.error().context()[2].key == "max_string_bytes");
    CHECK(oversizedValue.error().context()[2].value == "2");

    const auto oversizedKey =
        cuexis::json::parse(R"({"\u4F60":1})", cuexis::json::ParseLimits{1024, 8, 2});
    REQUIRE_FALSE(oversizedKey.has_value());
    REQUIRE(oversizedKey.error().code() == "json.parse.string_limit");
    REQUIRE(oversizedKey.error().context().size() == 3);
    CHECK(oversizedKey.error().context()[0].key == "string_kind");
    CHECK(oversizedKey.error().context()[0].value == "object_key");
    CHECK(oversizedKey.error().context()[1].key == "actual_bytes");
    CHECK(oversizedKey.error().context()[1].value == "3");
    CHECK(oversizedKey.error().context()[2].key == "max_string_bytes");
    CHECK(oversizedKey.error().context()[2].value == "2");
}

TEST_CASE("JSON serialization is deterministic and round trips", "[json][parse]") {
    const auto parsed =
        cuexis::json::parse(R"({"z":1,"a":2})", cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE(parsed.has_value());

    const auto serialized = cuexis::json::serialize(*parsed);
    REQUIRE(serialized.has_value());
    REQUIRE(*serialized == R"({"a":2,"z":1})");

    const auto reparsed =
        cuexis::json::parse(*serialized, cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE(reparsed.has_value());
    REQUIRE(*reparsed == *parsed);
}
