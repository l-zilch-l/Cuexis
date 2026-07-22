#include <cuexis/core/math.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>

TEST_CASE("Mat4 defaults to column-major identity", "[core][math]") {
    const cuexis::core::Mat4 identity;

    REQUIRE(identity.element(0, 0) == 1.0F);
    REQUIRE(identity.element(1, 1) == 1.0F);
    REQUIRE(identity.element(2, 2) == 1.0F);
    REQUIRE(identity.element(3, 3) == 1.0F);
    REQUIRE(identity.element(0, 3) == 0.0F);
}

TEST_CASE("Quaternion normalization rejects invalid values", "[core][math]") {
    const auto zero = cuexis::core::normalize(cuexis::core::Quat{0.0F, 0.0F, 0.0F, 0.0F});
    const auto nonFinite = cuexis::core::normalize(
        cuexis::core::Quat{0.0F, 0.0F, 0.0F, std::numeric_limits<float>::infinity()});

    REQUIRE_FALSE(zero.has_value());
    REQUIRE(zero.error().code() == "core.math.quaternion_zero_length");
    REQUIRE_FALSE(nonFinite.has_value());
    REQUIRE(nonFinite.error().code() == "core.math.quaternion_non_finite");

    const auto normalized = cuexis::core::normalize(cuexis::core::Quat{0.0F, 0.0F, 0.0F, 2.0F});
    REQUIRE(normalized.has_value());
    REQUIRE(cuexis::core::isNormalized(*normalized));
    REQUIRE(normalized->w == 1.0F);
}

TEST_CASE("Transform composition follows translation rotation scale order", "[core][math]") {
    const auto transform = cuexis::core::composeTransform(
        cuexis::core::Vec3{10.0F, 20.0F, 30.0F}, cuexis::core::Quat{0.0F, 0.0F, 0.0F, 1.0F},
        cuexis::core::Vec3{2.0F, 3.0F, 4.0F});

    REQUIRE(transform.has_value());
    REQUIRE(cuexis::core::nearlyEqual(
        cuexis::core::transformPoint(*transform, cuexis::core::Vec3{1.0F, 1.0F, 1.0F}),
        cuexis::core::Vec3{12.0F, 23.0F, 34.0F}));
}

TEST_CASE("Parent world transform multiplies child local transform", "[core][math]") {
    const auto parent = cuexis::core::makeTranslation(cuexis::core::Vec3{5.0F, 0.0F, 0.0F});
    const auto child = cuexis::core::makeTranslation(cuexis::core::Vec3{0.0F, 7.0F, 0.0F});
    const auto world = cuexis::core::multiply(parent, child);

    REQUIRE(cuexis::core::nearlyEqual(cuexis::core::transformPoint(world, cuexis::core::Vec3{}),
                                      cuexis::core::Vec3{5.0F, 7.0F, 0.0F}));
}
