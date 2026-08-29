#include <cuexis/core/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
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

TEST_CASE("Quaternion normalization rejects finite input with non-finite squared length",
          "[core][math][cm-04]") {
    constexpr auto large = std::numeric_limits<float>::max();
    const cuexis::core::Quat value{large, large, large, large};
    const auto lengthSquared =
        value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w;
    REQUIRE(cuexis::core::isFinite(value));
    REQUIRE_FALSE(std::isfinite(lengthSquared));

    const auto normalized = cuexis::core::normalize(value);
    REQUIRE_FALSE(normalized.has_value());
    CHECK(normalized.error().code() == "core.math.quaternion_not_representable");
}

// ZYX-to-Quat remains outside core until its axis and composition contract is frozen.

TEST_CASE("Hermite progress clamps input and preserves endpoint slopes", "[core][math][api]") {
    CHECK(cuexis::core::hermiteProgress(-1.0, 0.2, 0.4) == Catch::Approx(0.0));
    CHECK(cuexis::core::hermiteProgress(2.0, 0.2, 0.4) == Catch::Approx(1.0));
    CHECK(cuexis::core::hermiteProgress(0.5, 0.2, 0.4) == Catch::Approx(0.475));
}

TEST_CASE("Vec3 lerp preserves endpoints and midpoint", "[core][math][api]") {
    const cuexis::core::Vec3 left{1.0F, 2.0F, 3.0F};
    const cuexis::core::Vec3 right{5.0F, 6.0F, 7.0F};
    CHECK(cuexis::core::lerp(left, right, 0.0) == left);
    CHECK(cuexis::core::lerp(left, right, 1.0) == right);
    CHECK(cuexis::core::nearlyEqual(cuexis::core::lerp(left, right, 0.5),
                                    cuexis::core::Vec3{3.0F, 4.0F, 5.0F}));
}

TEST_CASE("Quaternion slerp follows shortest path and returns normalized results",
          "[core][math][api]") {
    const cuexis::core::Quat identity{0.0F, 0.0F, 0.0F, 1.0F};
    const cuexis::core::Quat opposite{0.0F, 0.0F, 0.0F, -1.0F};
    const auto same = cuexis::core::slerp(identity, identity, 0.5);
    const auto oppositeHemisphere = cuexis::core::slerp(identity, opposite, 0.5);

    REQUIRE(same.has_value());
    REQUIRE(oppositeHemisphere.has_value());
    CHECK(*same == identity);
    CHECK(*oppositeHemisphere == identity);
    CHECK(cuexis::core::isNormalized(*same));
    CHECK(cuexis::core::isNormalized(*oppositeHemisphere));

    constexpr float halfTurn = 0.7071067811865475F;
    const cuexis::core::Quat quarterTurn{0.0F, 0.0F, halfTurn, halfTurn};
    const auto midpoint = cuexis::core::slerp(identity, quarterTurn, 0.5);
    REQUIRE(midpoint.has_value());
    CHECK(cuexis::core::isNormalized(*midpoint));
    CHECK(midpoint->z > 0.0F);
    CHECK(midpoint->w > 0.0F);
}

TEST_CASE("Matrix inverse and nearlyEqual use absolute tolerances", "[core][math][contract]") {
    const auto invertible =
        cuexis::core::inverse(cuexis::core::makeScale(cuexis::core::Vec3{1.0e-4F, 1.0F, 1.0F}));
    REQUIRE(invertible.has_value());
    CHECK(cuexis::core::nearlyEqual(invertible->element(0, 0), 1.0e4F, 1.0F));

    const auto belowThreshold =
        cuexis::core::inverse(cuexis::core::makeScale(cuexis::core::Vec3{1.0e-8F, 1.0F, 1.0F}));
    REQUIRE_FALSE(belowThreshold.has_value());
    CHECK(belowThreshold.error().code() == "core.math.matrix_not_invertible");

    CHECK(cuexis::core::nearlyEqual(1.0e6F, 1.0e6F + 0.5F, 0.5F));
    CHECK_FALSE(cuexis::core::nearlyEqual(1.0e6F, 1.0e6F + 1.0F, 0.5F));
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

TEST_CASE("Perspective matrix maps the view frustum to OpenGL canonical NDC", "[core][math]") {
    constexpr double nearPlane = 0.1;
    constexpr double farPlane = 1000.0;
    const auto projection =
        cuexis::core::makePerspective(std::numbers::pi / 3.0, 16.0 / 9.0, nearPlane, farPlane);
    REQUIRE(projection.has_value());

    const auto projectedDepth = [&projection](double viewZ) {
        const double clipZ = static_cast<double>(projection->element(2, 2)) * viewZ +
                             static_cast<double>(projection->element(2, 3));
        const double clipW = static_cast<double>(projection->element(3, 2)) * viewZ +
                             static_cast<double>(projection->element(3, 3));
        return clipZ / clipW;
    };

    CHECK(projectedDepth(-nearPlane) == Catch::Approx(-1.0).margin(1.0e-6));
    CHECK(projectedDepth(-farPlane) == Catch::Approx(1.0).margin(1.0e-6));
}
