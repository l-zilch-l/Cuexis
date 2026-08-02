#include <cuexis/core/math.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <numbers>

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

TEST_CASE("Perspective matrix validates its complete public input contract", "[core][math]") {
    const auto valid =
        cuexis::core::makePerspective(std::numbers::pi / 3.0, 16.0 / 9.0, 0.1, 1000.0);
    REQUIRE(valid.has_value());
    CHECK(cuexis::core::isFinite(*valid));

    const auto nonFinite =
        cuexis::core::makePerspective(std::numeric_limits<double>::infinity(), 1.0, 0.1, 1000.0);
    REQUIRE_FALSE(nonFinite.has_value());
    CHECK(nonFinite.error().code() == "core.math.perspective_non_finite");

    const auto badFov = cuexis::core::makePerspective(std::numbers::pi, 1.0, 0.1, 1000.0);
    REQUIRE_FALSE(badFov.has_value());
    CHECK(badFov.error().code() == "core.math.perspective_fov_invalid");

    const auto badAspect = cuexis::core::makePerspective(1.0, 0.0, 0.1, 1000.0);
    REQUIRE_FALSE(badAspect.has_value());
    CHECK(badAspect.error().code() == "core.math.perspective_aspect_invalid");

    const auto badPlanes = cuexis::core::makePerspective(1.0, 1.0, 1.0, 1.0);
    REQUIRE_FALSE(badPlanes.has_value());
    CHECK(badPlanes.error().code() == "core.math.perspective_planes_invalid");

    const auto fovUnderflow =
        cuexis::core::makePerspective(std::numeric_limits<double>::denorm_min(), 1.0, 0.1, 1000.0);
    REQUIRE_FALSE(fovUnderflow.has_value());
    CHECK(fovUnderflow.error().code() == "core.math.perspective_not_representable");

    const auto aspectUnderflow = cuexis::core::makePerspective(
        std::numbers::pi / 2.0, std::numeric_limits<double>::max(), 0.1, 1000.0);
    REQUIRE_FALSE(aspectUnderflow.has_value());
    CHECK(aspectUnderflow.error().code() == "core.math.perspective_not_representable");

    const auto depthUnderflow = cuexis::core::makePerspective(
        std::numbers::pi / 2.0, 1.0, std::numeric_limits<double>::denorm_min(), 1.0);
    REQUIRE_FALSE(depthUnderflow.has_value());
    CHECK(depthUnderflow.error().code() == "core.math.perspective_not_representable");
}
