#include <cuexis/core/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Error retains stable code message and context", "[core][error]") {
    cuexis::core::Error error{"core.file.not_found", "The requested file was not found"};
    error.withContext("path", "chart.cuexis").withContext("operation", "open");

    REQUIRE(error.code() == "core.file.not_found");
    REQUIRE(error.message() == "The requested file was not found");
    REQUIRE(error.context().size() == 2);
    REQUIRE(error.context()[0].key == "path");
    REQUIRE(error.context()[0].value == "chart.cuexis");
    REQUIRE(error.context()[1].key == "operation");
    REQUIRE(error.context()[1].value == "open");
    REQUIRE(error.cause() == nullptr);
}

TEST_CASE("Error retains an optional cause", "[core][error]") {
    auto error = cuexis::core::Error{"core.asset.load_failed", "Failed to load asset"}.withCause(
        cuexis::core::Error{"core.file.read_failed", "Read failed"}.withContext("path",
                                                                                "texture.png"));

    REQUIRE(error.cause() != nullptr);
    REQUIRE(error.cause()->code() == "core.file.read_failed");
    REQUIRE(error.cause()->context().size() == 1);
    REQUIRE(error.cause()->context()[0].value == "texture.png");
}
