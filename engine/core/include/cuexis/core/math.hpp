#pragma once

//  Cuexis 自有数学类型 — Vec3、Quat、Mat4（列向量，右手坐标系）
//  内部使用 GLM 实现，仅对 Cuexis 公共接口暴露自有类型
//  +X 向右，+Y 向上，+Z 向后；默认相机观察方向 -Z；世界单位 1 unit = 1 meter

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
    // 列主序存储：element(row, column) 即 values[column * 4 + row]
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

[[nodiscard]] Mat4 makePerspective(double fovYRadians, double aspectRatio, double nearPlane,
                                   double farPlane) noexcept;

} // namespace cuexis::core
