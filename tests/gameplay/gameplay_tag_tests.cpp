#include <cuexis/gameplay/tags.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

static_assert(std::is_empty_v<cuexis::gameplay::NoteTag>);
static_assert(std::is_empty_v<cuexis::gameplay::ElementTag>);
static_assert(!std::is_same_v<cuexis::gameplay::NoteTag, cuexis::gameplay::ElementTag>);

TEST_CASE("Gameplay tags carry no behavior or state", "[gameplay]") {
    CHECK(std::is_empty_v<cuexis::gameplay::NoteTag>);
    CHECK(std::is_empty_v<cuexis::gameplay::ElementTag>);
}
