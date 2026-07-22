#include <cuexis/json/parse.hpp>

#include <catch2/catch_test_macros.hpp>

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
