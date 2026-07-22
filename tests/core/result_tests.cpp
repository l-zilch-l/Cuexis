#include <cuexis/core/result.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

cuexis::core::Result<int> makeValue(bool succeed) {
    if (!succeed) {
        return cuexis::core::unexpected(
            cuexis::core::Error{"core.test.rejected", "The test value was rejected"});
    }
    return 42;
}

} // namespace

TEST_CASE("Result carries a successful value", "[core][result]") {
    const auto result = makeValue(true);

    REQUIRE(result.has_value());
    REQUIRE(*result == 42);
}

TEST_CASE("Result carries a project Error", "[core][result]") {
    const auto result = makeValue(false);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == "core.test.rejected");
}

TEST_CASE("Result supports void success", "[core][result]") {
    const cuexis::core::Result<void> result{};

    REQUIRE(result.has_value());
}
