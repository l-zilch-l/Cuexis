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

TEST_CASE("Player practice guide is explicit and requires candidate Gameplay",
          "[player][candidate]") {
    std::array arguments{std::string{"player"}, std::string{"--gameplay-guide"},
                         std::string{"guide.txt"}};
    std::array<char*, 3> pointers{};
    for (std::size_t i = 0; i < arguments.size(); ++i)
        pointers[i] = arguments[i].data();
    const auto result = cuexis::player::parsePlayerOptions(3, pointers.data());
    REQUIRE_FALSE(result);
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    CHECK(result.error().code() == "player.arguments.unknown");
#else
    CHECK(result.error().code() == "player.candidate.disabled");
#endif
}
