#include <cuexis/chart/uuid.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("UUIDv7 validation enforces canonical text version and RFC variant", "[chart][uuid]") {
    CHECK(cuexis::chart::isUuidV7("019b0000-0000-7abc-8def-000000000001"));
    CHECK_FALSE(cuexis::chart::isUuidV7("019b0000-0000-5abc-8def-000000000001"));
    CHECK_FALSE(cuexis::chart::isUuidV7("019b0000-0000-7abc-7def-000000000001"));
    CHECK_FALSE(cuexis::chart::isUuidV7("019B0000-0000-7ABC-8DEF-000000000001"));
    CHECK_FALSE(cuexis::chart::isUuidV7("not-a-uuid"));
}

TEST_CASE("UUIDv5 matches the RFC 4122 DNS namespace golden vector", "[chart][uuid]") {
    const auto generated =
        cuexis::chart::uuidV5("6ba7b810-9dad-11d1-80b4-00c04fd430c8", "www.widgets.com");
    REQUIRE(generated.has_value());
    CHECK(*generated == "21f7f8de-8051-5b89-8680-0195ef798b6a");
    CHECK((*generated)[14] == '5');
    CHECK((*generated)[19] == '8');
    CHECK(cuexis::chart::isUuidV5(*generated));
    CHECK_FALSE(cuexis::chart::isUuidV7(*generated));
}

TEST_CASE("UUIDv5 rejects malformed and non-RFC namespaces", "[chart][uuid]") {
    const auto malformed = cuexis::chart::uuidV5("not-a-uuid", "name");
    REQUIRE_FALSE(malformed.has_value());
    CHECK(malformed.error().code() == "chart.uuid.invalid_namespace");

    const auto wrongVariant = cuexis::chart::uuidV5("6ba7b810-9dad-11d1-70b4-00c04fd430c8", "name");
    REQUIRE_FALSE(wrongVariant.has_value());
    CHECK(wrongVariant.error().code() == "chart.uuid.invalid_namespace");
}
