#include "player_options.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Candidate Player entry is explicit and disabled in production", "[player][candidate]") {
    std::array arguments{std::string{"player"}, std::string{"--cxc"},
                         std::string{"nonexistent.cxc"}, std::string{"--candidate-entry"},
                         std::string{"compiled/chart.packed"}};
    std::array<char*, 5> pointers{};
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        pointers[i] = arguments[i].data();
    }
    const auto result = cuexis::player::parsePlayerOptions(5, pointers.data());
#if defined(CUEXIS_EXPERIMENTAL_BUILD)
    REQUIRE(result);
    CHECK(result->cxcPath == "nonexistent.cxc");
    CHECK(result->candidateEntry == "compiled/chart.packed");
#else
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == "player.candidate.disabled");
#endif
}

TEST_CASE("Player rejects the removed practice guide option", "[player][candidate]") {
    std::array arguments{std::string{"player"}, std::string{"--gameplay-guide"},
                         std::string{"guide.txt"}};
    std::array<char*, 3> pointers{};
    for (std::size_t i = 0; i < arguments.size(); ++i)
        pointers[i] = arguments[i].data();
    const auto result = cuexis::player::parsePlayerOptions(3, pointers.data());
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == "player.arguments.unknown");
}

TEST_CASE("Separate Gameplay content requires explicit execution mode", "[player][candidate]") {
    std::array arguments{std::string{"player"},
                         std::string{"--cxc"},
                         std::string{"scene.cxc"},
                         std::string{"--candidate-entry"},
                         std::string{"compiled/chart.v5.json"},
                         std::string{"--gameplay-packed"},
                         std::string{"gameplay.packed"}};
    std::array<char*, 7> pointers{};
    for (std::size_t i = 0; i < arguments.size(); ++i)
        pointers[i] = arguments[i].data();
    REQUIRE_FALSE(cuexis::player::parsePlayerOptions(7, pointers.data()));
}
