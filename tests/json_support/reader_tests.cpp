#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("JSON Reader reports deterministic field paths", "[json][reader]") {
    const auto parsed = cuexis::json::parse(R"({"count":"two","extra":true})",
                                            cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE(parsed.has_value());

    cuexis::core::Diagnostics diagnostics;
    const cuexis::json::Reader root{*parsed, diagnostics};

    const auto count = root.requiredField("count");
    REQUIRE(count.has_value());
    REQUIRE_FALSE(count->readUInt64().has_value());
    REQUIRE_FALSE(root.requiredField("id").has_value());

    constexpr std::array<std::string_view, 1> knownFields{"count"};
    root.rejectUnknownFields(knownFields);
    diagnostics.sortDeterministically();

    REQUIRE(diagnostics.size() == 3);
    REQUIRE(diagnostics.items()[0].fieldPath() == "$/count");
    REQUIRE(diagnostics.items()[1].fieldPath() == "$/extra");
    REQUIRE(diagnostics.items()[2].fieldPath() == "$/id");
}

TEST_CASE("JSON Reader escapes object keys in field paths", "[json][reader]") {
    const auto parsed =
        cuexis::json::parse(R"({"a/b~c":1})", cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE(parsed.has_value());

    cuexis::core::Diagnostics diagnostics;
    const cuexis::json::Reader root{*parsed, diagnostics};
    const auto field = root.requiredField("a/b~c");

    REQUIRE(field.has_value());
    REQUIRE(field->fieldPath() == "$/a~1b~0c");
}

TEST_CASE("JSON Reader accepts safe signed and unsigned integer conversions", "[json][reader]") {
    const auto parsed = cuexis::json::parse(R"({"positive":5,"negative":-3})",
                                            cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE(parsed.has_value());

    cuexis::core::Diagnostics diagnostics;
    const cuexis::json::Reader root{*parsed, diagnostics};

    REQUIRE(root.requiredField("positive")->readInt64() == 5);
    REQUIRE(root.requiredField("negative")->readUInt64() == std::nullopt);
    REQUIRE(diagnostics.hasErrors());
}

TEST_CASE("JSON Reader stops unknown-field reporting at the diagnostic limit", "[json][reader]") {
    const auto parsed = cuexis::json::parse(R"({"d":4,"c":3,"b":2,"a":1})",
                                            cuexis::json::ParseLimits{1024, 8, 1024});
    REQUIRE(parsed.has_value());

    cuexis::core::Diagnostics diagnostics{
        2, cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error,
                                    "chart.diagnostics.limit_exceeded", "Limit reached", "$"}};
    const cuexis::json::Reader root{*parsed, diagnostics};
    constexpr std::array<std::string_view, 0> knownFields{};

    root.rejectUnknownFields(knownFields);

    REQUIRE(diagnostics.limitReached());
    REQUIRE(diagnostics.size() == 2);
    REQUIRE(diagnostics.items()[0].fieldPath() == "$/a");
    REQUIRE(diagnostics.items()[1].code() == "chart.diagnostics.limit_exceeded");
}
