#include <cuexis/core/uuid.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Core UUID validation enforces canonical version and RFC variant", "[core][uuid]") {
    CHECK(cuexis::core::isUuidV7("019b0000-0000-7abc-8def-000000000001"));
    CHECK_FALSE(cuexis::core::isUuidV7("019B0000-0000-7ABC-8DEF-000000000001"));
    CHECK_FALSE(cuexis::core::isUuidV7("019b0000-0000-7abc-7def-000000000001"));

    const auto generated =
        cuexis::core::uuidV5("6ba7b810-9dad-11d1-80b4-00c04fd430c8", "www.widgets.com");
    REQUIRE(generated.has_value());
    CHECK(*generated == "21f7f8de-8051-5b89-8680-0195ef798b6a");
    CHECK(cuexis::core::isUuidV5(*generated));

    const auto invalid = cuexis::core::uuidV5("not-a-uuid", "name");
    REQUIRE_FALSE(invalid.has_value());
    CHECK(invalid.error().code() == "core.uuid.invalid_namespace");
}
