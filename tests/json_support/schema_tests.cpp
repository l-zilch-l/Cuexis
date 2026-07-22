#include <cuexis/json/parse.hpp>
#include <cuexis/json/schema.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("JSON Schema violations become field-path diagnostics", "[json][schema]") {
    const auto schema = cuexis::json::parse(
        R"({"type":"object","properties":{"version":{"type":"integer","minimum":1}},"required":["version"]})",
        cuexis::json::ParseLimits{4096, 16, 4096});
    const auto instance =
        cuexis::json::parse(R"({"version":0})", cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE(schema.has_value());
    REQUIRE(instance.has_value());

    cuexis::core::Diagnostics diagnostics;
    const auto result = cuexis::json::validateAgainstSchema(*instance, *schema, diagnostics);

    REQUIRE(result.has_value());
    REQUIRE(diagnostics.hasErrors());
    REQUIRE(diagnostics.items()[0].code() == "json.schema.validation_failed");
    REQUIRE(diagnostics.items()[0].fieldPath() == "$/version");
}

TEST_CASE("Invalid JSON Schema returns an operational error", "[json][schema]") {
    const auto schema = cuexis::json::parse("5", cuexis::json::ParseLimits{1024, 8, 1024});
    const auto instance = cuexis::json::parse(R"({})", cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE(schema.has_value());
    REQUIRE(instance.has_value());

    cuexis::core::Diagnostics diagnostics;
    const auto result = cuexis::json::validateAgainstSchema(*instance, *schema, diagnostics);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == "json.schema.invalid");
}
