#include <cuexis/version.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::string twoDigits(std::uint32_t value) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << value;
    return output.str();
}

} // namespace

TEST_CASE("Generated version components are valid", "[core][version]") {
    REQUIRE(cuexis::version::year <= 99);
    REQUIRE(cuexis::version::month >= 1);
    REQUIRE(cuexis::version::month <= 12);
    REQUIRE(cuexis::version::day >= 1);
    REQUIRE(cuexis::version::day <= 31);
    REQUIRE(cuexis::version::hour <= 23);
    REQUIRE(cuexis::version::build >= 1);
}

TEST_CASE("Generated canonical version matches numeric components", "[core][version]") {
    const auto expected = twoDigits(cuexis::version::year) + "." +
                          twoDigits(cuexis::version::month) + "." +
                          twoDigits(cuexis::version::day) + "." + twoDigits(cuexis::version::hour) +
                          "-" + std::to_string(cuexis::version::build);

    REQUIRE(cuexis::version::canonical == expected);
}

TEST_CASE("Generated display version contains only an allowed suffix", "[core][version]") {
    const auto suffix = cuexis::version::suffix;
    const bool isAllowed = suffix.empty() || suffix == "dev" || suffix == "test" ||
                           suffix == "internal" || suffix.starts_with("exp.");
    REQUIRE(isAllowed);

    const auto expected = suffix.empty()
                              ? std::string(cuexis::version::canonical)
                              : std::string(cuexis::version::canonical) + "-" + std::string(suffix);
    REQUIRE(cuexis::version::display == expected);
}

TEST_CASE("Generated SDK API version is independent from the build identity", "[core][version]") {
    REQUIRE(cuexis::version::sdkApi == "0.3.0");
    REQUIRE(cuexis::version::sdkApi != cuexis::version::cmakeProject);
    REQUIRE(cuexis::version::sdkApi != cuexis::version::display);
}

TEST_CASE("Generated version matches CMake project version", "[core][version]") {
    const auto expected =
        std::to_string(cuexis::version::year) + "." + std::to_string(cuexis::version::month) + "." +
        std::to_string(cuexis::version::day) + "." + std::to_string(cuexis::version::hour);

    REQUIRE(cuexis::version::cmakeProject == expected);
}

TEST_CASE("Generated canonical version matches vcpkg manifest", "[core][version]") {
    REQUIRE(cuexis::version::manifest == cuexis::version::canonical);
}
