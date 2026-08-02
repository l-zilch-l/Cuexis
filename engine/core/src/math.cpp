//  数学类型实现 — 内部使用 GLM 进行计算，仅对外暴露 Cuexis 自有类型
//  坐标约定：右手坐标系，+X 向右、+Y 向上、+Z 向后，列向量矩阵
//  composeTransform 按 Translation * Rotation * Scale 组合局部矩阵

#include <cuexis/core/math.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace cuexis::core {
namespace {

glm::mat4 toGlm(const Mat4& matrix) noexcept {
    glm::mat4 result{1.0F};
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            result[column][row] =
                matrix.element(static_cast<std::size_t>(row), static_cast<std::size_t>(column));
        }
    }
    return result;
}

Mat4 fromGlm(const glm::mat4& matrix) noexcept {
    Mat4 result{};
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            result.element(static_cast<std::size_t>(row), static_cast<std::size_t>(column)) =
                matrix[column][row];
        }
    }
    return result;
}

} // namespace

bool isFinite(const Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isFinite(const Quat& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
           std::isfinite(value.w);
}

bool isFinite(const Mat4& value) noexcept {
    return std::all_of(value.values.begin(), value.values.end(),
                       [](float element) { return std::isfinite(element); });
}

bool isNormalized(const Quat& value, float tolerance) noexcept {
    if (!isFinite(value) || !std::isfinite(tolerance) || tolerance < 0.0F) {
        return false;
    }
    const auto lengthSquared =
        value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w;
    return nearlyEqual(lengthSquared, 1.0F, tolerance);
}

Result<Quat> normalize(const Quat& value) noexcept {
    if (!isFinite(value)) {
        return unexpected(
            Error{"core.math.quaternion_non_finite", "Quaternion components must be finite"});
    }

    const auto lengthSquared =
        value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w;
    if (lengthSquared <= std::numeric_limits<float>::epsilon()) {
        return unexpected(
            Error{"core.math.quaternion_zero_length", "Quaternion length must be non-zero"});
    }

    const auto inverseLength = 1.0F / std::sqrt(lengthSquared);
    return Quat{value.x * inverseLength, value.y * inverseLength, value.z * inverseLength,
                value.w * inverseLength};
}

Mat4 makeTranslation(const Vec3& translation) noexcept {
    return fromGlm(
        glm::translate(glm::mat4{1.0F}, glm::vec3{translation.x, translation.y, translation.z}));
}

Mat4 makeScale(const Vec3& scale) noexcept {
    return fromGlm(glm::scale(glm::mat4{1.0F}, glm::vec3{scale.x, scale.y, scale.z}));
}

Result<Mat4> makeRotation(const Quat& rotation) noexcept {
    const auto normalized = normalize(rotation);
    if (!normalized) {
        return unexpected(normalized.error());
    }

    const glm::quat glmRotation{normalized->w, normalized->x, normalized->y, normalized->z};
    return fromGlm(glm::mat4_cast(glmRotation));
}

Result<Mat4> composeTransform(const Vec3& translation, const Quat& rotation,
                              const Vec3& scale) noexcept {
    if (!isFinite(translation) || !isFinite(scale)) {
        return unexpected(Error{"core.math.transform_non_finite",
                                "Transform translation and scale must be finite"});
    }

    const auto rotationMatrix = makeRotation(rotation);
    if (!rotationMatrix) {
        return unexpected(
            Error{"core.math.transform_invalid_rotation", "Transform rotation is invalid"}
                .withCause(rotationMatrix.error()));
    }

    return multiply(multiply(makeTranslation(translation), *rotationMatrix), makeScale(scale));
}

Mat4 multiply(const Mat4& left, const Mat4& right) noexcept {
    return fromGlm(toGlm(left) * toGlm(right));
}

Result<Mat4> inverse(const Mat4& matrix) noexcept {
    if (!isFinite(matrix)) {
        return unexpected(Error{"core.math.matrix_non_finite", "Matrix values must be finite"});
    }
    const auto glmMatrix = toGlm(matrix);
    const auto determinant = glm::determinant(glmMatrix);
    if (!std::isfinite(determinant) ||
        std::abs(determinant) <= std::numeric_limits<float>::epsilon()) {
        return unexpected(Error{"core.math.matrix_not_invertible", "Matrix is not invertible"});
    }
    const auto result = fromGlm(glm::inverse(glmMatrix));
    if (!isFinite(result)) {
        return unexpected(
            Error{"core.math.matrix_inverse_non_finite", "Matrix inverse is non-finite"});
    }
    return result;
}

Vec3 transformPoint(const Mat4& matrix, const Vec3& point) noexcept {
    const auto transformed = toGlm(matrix) * glm::vec4{point.x, point.y, point.z, 1.0F};
    return Vec3{transformed.x, transformed.y, transformed.z};
}

bool nearlyEqual(float left, float right, float tolerance) noexcept {
    if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(tolerance) ||
        tolerance < 0.0F) {
        return false;
    }
    return std::abs(left - right) <= tolerance;
}

bool nearlyEqual(const Vec3& left, const Vec3& right, float tolerance) noexcept {
    return nearlyEqual(left.x, right.x, tolerance) && nearlyEqual(left.y, right.y, tolerance) &&
           nearlyEqual(left.z, right.z, tolerance);
}

bool nearlyEqual(const Mat4& left, const Mat4& right, float tolerance) noexcept {
    for (std::size_t index = 0; index < left.values.size(); ++index) {
        if (!nearlyEqual(left.values[index], right.values[index], tolerance)) {
            return false;
        }
    }
    return true;
}

Result<Mat4> makePerspective(double fovYRadians, double aspectRatio, double nearPlane,
                             double farPlane) noexcept {
    if (!std::isfinite(fovYRadians) || !std::isfinite(aspectRatio) || !std::isfinite(nearPlane) ||
        !std::isfinite(farPlane)) {
        return unexpected(core::Error{"core.math.perspective_non_finite",
                                      "Perspective parameters must be finite"});
    }
    if (fovYRadians <= 0.0 || fovYRadians >= std::numbers::pi) {
        return unexpected(core::Error{"core.math.perspective_fov_invalid",
                                      "Perspective vertical FOV must be in (0, pi) radians"});
    }
    if (aspectRatio <= 0.0) {
        return unexpected(core::Error{"core.math.perspective_aspect_invalid",
                                      "Perspective aspect ratio must be positive"});
    }
    if (nearPlane <= 0.0 || farPlane <= nearPlane) {
        return unexpected(core::Error{"core.math.perspective_planes_invalid",
                                      "Perspective planes must satisfy 0 < near < far"});
    }

    const double tanHalfFov = std::tan(fovYRadians * 0.5);
    const double xScale = 1.0 / (tanHalfFov * aspectRatio);
    const double yScale = 1.0 / tanHalfFov;
    const double depthScale = (farPlane + nearPlane) / (nearPlane - farPlane);
    const double depthTranslation = (2.0 * farPlane * nearPlane) / (nearPlane - farPlane);
    const auto isFloatRepresentable = [](double value) noexcept {
        constexpr double floatMaximum = static_cast<double>(std::numeric_limits<float>::max());
        if (!std::isfinite(value) || std::abs(value) > floatMaximum) {
            return false;
        }
        const float converted = static_cast<float>(value);
        return value == 0.0 || converted != 0.0F;
    };
    if (!isFloatRepresentable(xScale) || !isFloatRepresentable(yScale) ||
        !isFloatRepresentable(depthScale) || !isFloatRepresentable(depthTranslation)) {
        return unexpected(
            core::Error{"core.math.perspective_not_representable",
                        "Perspective matrix cannot be represented with finite non-zero floats"});
    }

    const float tanHalfFovFloat = static_cast<float>(tanHalfFov);
    Mat4 result{};
    result.values.fill(0.0F);
    result.element(0, 0) = 1.0F / (tanHalfFovFloat * static_cast<float>(aspectRatio));
    result.element(1, 1) = 1.0F / tanHalfFovFloat;
    result.element(2, 2) = static_cast<float>(depthScale);
    result.element(2, 3) = -1.0F;
    result.element(3, 2) = static_cast<float>(depthTranslation);
    result.element(3, 3) = 0.0F;
    if (result.element(0, 0) == 0.0F || result.element(1, 1) == 0.0F ||
        result.element(2, 2) == 0.0F || result.element(3, 2) == 0.0F) {
        return unexpected(
            core::Error{"core.math.perspective_not_representable",
                        "Perspective matrix cannot be represented with finite non-zero floats"});
    }
    if (!isFinite(result)) {
        return unexpected(core::Error{"core.math.perspective_result_non_finite",
                                      "Perspective matrix is non-finite"});
    }
    return result;
}

} // namespace cuexis::core
