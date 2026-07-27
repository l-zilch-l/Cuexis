#pragma once

//  Cuexis-owned math types - Vec3, Quat, Mat4 (column vectors, right-handed)
//  Implemented internally with GLM; only the Cuexis-owned types are exposed on the public
//  interface
//  +X right, +Y up, +Z back; the default camera looks down -Z; world unit 1 unit = 1 meter

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

[[nodiscard]] bool isFinite(const Vec3& value) noexcept;
[[nodiscard]] bool isFinite(const Quat& value) noexcept;
[[nodiscard]] bool isFinite(const Mat4& value) noexcept;
[[nodiscard]] bool isNormalized(const Quat& value, float tolerance = 1.0e-5F) noexcept;
[[nodiscard]] Result<Quat> normalize(const Quat& value) noexcept;

[[nodiscard]] Mat4 makeTranslation(const Vec3& translation) noexcept;
[[nodiscard]] Mat4 makeScale(const Vec3& scale) noexcept;
[[nodiscard]] Result<Mat4> makeRotation(const Quat& rotation) noexcept;
[[nodiscard]] Result<Mat4> composeTransform(const Vec3& translation, const Quat& rotation,
                                            const Vec3& scale) noexcept;
[[nodiscard]] Mat4 multiply(const Mat4& left, const Mat4& right) noexcept;
[[nodiscard]] Result<Mat4> inverse(const Mat4& matrix) noexcept;
[[nodiscard]] Vec3 transformPoint(const Mat4& matrix, const Vec3& point) noexcept;

[[nodiscard]] bool nearlyEqual(float left, float right, float tolerance = 1.0e-5F) noexcept;
[[nodiscard]] bool nearlyEqual(const Vec3& left, const Vec3& right,
                               float tolerance = 1.0e-5F) noexcept;
[[nodiscard]] bool nearlyEqual(const Mat4& left, const Mat4& right,
                               float tolerance = 1.0e-5F) noexcept;

[[nodiscard]] Result<Mat4> makePerspective(double fovYRadians, double aspectRatio, double nearPlane,
                                           double farPlane) noexcept;

} // namespace cuexis::core
