#pragma once

//  Cuexis-owned math types - Vec3, Quat, Mat4 (column vectors, right-handed)
//  Implemented internally with GLM; only the Cuexis-owned types are exposed on the public
//  interface
//  +X right, +Y up, +Z back; the default camera looks down -Z; world unit 1 unit = 1 meter

#include <cuexis/core/core_export.hpp>
#include <cuexis/core/result.hpp>

#include <array>
#include <cstddef>

namespace cuexis::core {

struct Vec3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    friend bool operator==(const Vec3&, const Vec3&) = default;
};

struct Quat {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{1.0F};

    friend bool operator==(const Quat&, const Quat&) = default;
};

struct Mat4 {
    // Column-major storage: element(row, column) is values[column * 4 + row]
    std::array<float, 16> values{
        1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
    };

    [[nodiscard]] constexpr float& element(std::size_t row, std::size_t column) noexcept {
        return values[column * 4 + row];
    }

    [[nodiscard]] constexpr const float& element(std::size_t row,
                                                 std::size_t column) const noexcept {
        return values[column * 4 + row];
    }

    friend bool operator==(const Mat4&, const Mat4&) = default;
};

[[nodiscard]] CUEXIS_CORE_API bool isFinite(const Vec3& value) noexcept;
[[nodiscard]] CUEXIS_CORE_API bool isFinite(const Quat& value) noexcept;
[[nodiscard]] CUEXIS_CORE_API bool isFinite(const Mat4& value) noexcept;
[[nodiscard]] CUEXIS_CORE_API bool isNormalized(const Quat& value,
                                                float tolerance = 1.0e-5F) noexcept;
[[nodiscard]] CUEXIS_CORE_API Result<Quat> normalize(const Quat& value) noexcept;
// Cubic Hermite progress clamps value to [0, 1] and uses endpoint slopes in normalized time.
[[nodiscard]] CUEXIS_CORE_API double hermiteProgress(double value, double startSlope,
                                                     double endSlope) noexcept;
// Linear interpolation uses the same float blend conversion as the animation samplers.
[[nodiscard]] CUEXIS_CORE_API Vec3 lerp(const Vec3& left, const Vec3& right, double t) noexcept;
// Shortest-path quaternion interpolation aligns hemispheres, clamps the dot product, and
// normalizes the result. The normalize error is returned unchanged.
[[nodiscard]] CUEXIS_CORE_API Result<Quat> slerp(const Quat& left, const Quat& right,
                                                 double t) noexcept;

[[nodiscard]] CUEXIS_CORE_API Mat4 makeTranslation(const Vec3& translation) noexcept;
[[nodiscard]] CUEXIS_CORE_API Mat4 makeScale(const Vec3& scale) noexcept;
[[nodiscard]] CUEXIS_CORE_API Result<Mat4> makeRotation(const Quat& rotation) noexcept;
[[nodiscard]] CUEXIS_CORE_API Result<Mat4>
composeTransform(const Vec3& translation, const Quat& rotation, const Vec3& scale) noexcept;
[[nodiscard]] CUEXIS_CORE_API Mat4 multiply(const Mat4& left, const Mat4& right) noexcept;
// Rejects matrices whose absolute determinant is no greater than float epsilon.
[[nodiscard]] CUEXIS_CORE_API Result<Mat4> inverse(const Mat4& matrix) noexcept;
// Treats point as an affine homogeneous point with w = 1 and returns xyz without a perspective
// divide.
[[nodiscard]] CUEXIS_CORE_API Vec3 transformPoint(const Mat4& matrix, const Vec3& point) noexcept;

// Uses an absolute tolerance. Vec3 and Mat4 comparisons apply it independently to each element.
[[nodiscard]] CUEXIS_CORE_API bool nearlyEqual(float left, float right,
                                               float tolerance = 1.0e-5F) noexcept;
[[nodiscard]] CUEXIS_CORE_API bool nearlyEqual(const Vec3& left, const Vec3& right,
                                               float tolerance = 1.0e-5F) noexcept;
[[nodiscard]] CUEXIS_CORE_API bool nearlyEqual(const Mat4& left, const Mat4& right,
                                               float tolerance = 1.0e-5F) noexcept;

[[nodiscard]] CUEXIS_CORE_API Result<Mat4>
makePerspective(double fovYRadians, double aspectRatio, double nearPlane, double farPlane) noexcept;

} // namespace cuexis::core
