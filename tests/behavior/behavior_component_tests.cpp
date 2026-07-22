#include <cuexis/behavior/behavior_component.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BehaviorComponent stores a stable Runtime behavior index", "[behavior]") {
    const cuexis::behavior::RuntimeBehaviorIndex empty;
    const cuexis::behavior::RuntimeBehaviorIndex valid{.value = 7};
    const cuexis::behavior::BehaviorComponent component{.behavior = valid};

    CHECK_FALSE(empty.valid());
    CHECK(component.behavior.valid());
    CHECK(component.behavior.value == 7);
}
