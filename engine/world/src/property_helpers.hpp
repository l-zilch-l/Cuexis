// Internal value-conversion and validation helpers for the property resolvers.
// Declared inline so both translation units can use them without changing linkage.
#pragma once

#include <cuexis/world/property.hpp>
#include <cuexis_internal/allocation_observer.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/world/resolver_access.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cuexis::world::property_detail {

using EntityValue = std::underlying_type_t<entt::entity>;

[[nodiscard]] inline auto entityValue(entt::entity entity) noexcept -> EntityValue {
    return entt::to_integral(entity);
}

[[nodiscard]] inline auto layerBit(PropertyLayer layer) noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(layer));
}

[[nodiscard]] inline auto transformBit(PropertyId property) noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(property));
}

[[nodiscard]] inline auto isTransformProperty(PropertyId property) noexcept -> bool {
    return property == PropertyId::TransformPositionX ||
           property == PropertyId::TransformPositionY ||
           property == PropertyId::TransformPositionZ ||
           property == PropertyId::TransformRotation || property == PropertyId::TransformScale;
}

[[nodiscard]] inline auto isCameraProperty(PropertyId property) noexcept -> bool {
    return property == PropertyId::CameraFovY;
}

[[nodiscard]] inline auto isAppearanceProperty(PropertyId property) noexcept -> bool {
    return property == PropertyId::RenderVisible || property == PropertyId::RenderMaterial ||
           property == PropertyId::MaterialOpacity || property == PropertyId::MaterialTint;
}

[[nodiscard]] inline auto entityError(std::string code, std::string message, entt::entity entity)
    -> core::Error {
    return core::Error{std::move(code), std::move(message)}.withContext(
        "entity", std::to_string(static_cast<std::uint64_t>(entityValue(entity))));
}

[[nodiscard]] inline auto transformFromBaseline(const TransformComponent& transform,
                                                PropertyId property) -> PropertyValue {
    switch (property) {
    case PropertyId::TransformPositionX:
        return PropertyValue{static_cast<double>(transform.position.x)};
    case PropertyId::TransformPositionY:
        return PropertyValue{static_cast<double>(transform.position.y)};
    case PropertyId::TransformPositionZ:
        return PropertyValue{static_cast<double>(transform.position.z)};
    case PropertyId::TransformRotation:
        return PropertyValue{transform.rotation};
    case PropertyId::TransformScale:
        return PropertyValue{transform.scale};
    default:
        return PropertyValue{};
    }
}

inline void resetStoredValue(PropertyValue& value) noexcept {
    if (auto* text = std::get_if<std::string>(&value)) {
        text->clear();
        return;
    }
    value = {};
}

inline void copyStoredValue(PropertyValue& dest, const PropertyValue& src) {
    if (auto* destText = std::get_if<std::string>(&dest)) {
        if (const auto* srcText = std::get_if<std::string>(&src)) {
            destText->assign(srcText->data(), srcText->size());
            return;
        }
    }
    dest = src;
}

[[nodiscard]] inline auto parsedPropertyValue(entt::entity entity, bool hasTransform, bool present,
                                              PropertyId property, const PropertyWriteValue& value)
    -> core::Result<PropertyValue>;

[[nodiscard]] inline auto
storeParsedPropertyValue(PropertyValue& dest, entt::entity entity, bool hasTransform, bool present,
                         PropertyId property, const PropertyWriteValue& value)
    -> core::Result<void> {
    if (property == PropertyId::RenderMaterial) {
        const auto* material = std::get_if<std::string_view>(&value);
        if (material == nullptr || material->empty()) {
            return core::unexpected(
                core::Error{"runtime.appearance.value_invalid",
                            "render.material requires a non-empty Material AssetId"});
        }
        if (!present) {
            return core::unexpected(core::Error{"runtime.appearance.binding_missing",
                                                "Appearance target has no Renderable component"});
        }
        if (auto* text = std::get_if<std::string>(&dest)) {
            text->assign(material->data(), material->size());
        } else {
            dest = std::string{*material};
        }
        return {};
    }
    auto parsed = parsedPropertyValue(entity, hasTransform, present, property, value);
    if (!parsed) {
        return core::unexpected(std::move(parsed.error()));
    }
    dest = std::move(*parsed);
    return {};
}

[[nodiscard]] inline auto parsedPropertyValue(entt::entity entity, bool hasTransform, bool present,
                                              PropertyId property, const PropertyWriteValue& value)
    -> core::Result<PropertyValue> {
    switch (property) {
    case PropertyId::TransformPositionX:
    case PropertyId::TransformPositionY:
    case PropertyId::TransformPositionZ: {
        const auto* scalar = std::get_if<double>(&value);
        constexpr auto floatMax = static_cast<double>(std::numeric_limits<float>::max());
        if (scalar == nullptr || !std::isfinite(*scalar) || *scalar < -floatMax ||
            *scalar > floatMax) {
            return core::unexpected(entityError(
                "world.property.value_invalid",
                "Transform position property requires a finite float-range scalar", entity));
        }
        if (!hasTransform) {
            return core::unexpected(entityError("world.property.transform_missing",
                                                "Transform property target has no baseline",
                                                entity));
        }
        return PropertyValue{*scalar};
    }
    case PropertyId::TransformRotation: {
        const auto* rotation = std::get_if<core::Quat>(&value);
        if (rotation == nullptr || !core::isNormalized(*rotation)) {
            return core::unexpected(entityError(
                "world.property.value_invalid",
                "Transform rotation property requires a normalized quaternion", entity));
        }
        if (!hasTransform) {
            return core::unexpected(entityError("world.property.transform_missing",
                                                "Transform property target has no baseline",
                                                entity));
        }
        return PropertyValue{*rotation};
    }
    case PropertyId::TransformScale: {
        const auto* scale = std::get_if<core::Vec3>(&value);
        if (scale == nullptr || !core::isFinite(*scale)) {
            return core::unexpected(entityError("world.property.value_invalid",
                                                "Transform scale property requires a finite Vec3",
                                                entity));
        }
        if (!hasTransform) {
            return core::unexpected(entityError("world.property.transform_missing",
                                                "Transform property target has no baseline",
                                                entity));
        }
        return PropertyValue{*scale};
    }
    case PropertyId::CameraFovY: {
        const auto* fov = std::get_if<double>(&value);
        if (fov == nullptr || !std::isfinite(*fov) || *fov <= 0.0 || *fov >= 179.0) {
            return core::unexpected(core::Error{"runtime.camera.fov_out_of_range",
                                                "camera.fovY must be between 0 and 179 degrees"});
        }
        if (!present) {
            return core::unexpected(core::Error{"runtime.camera.binding_missing",
                                                "camera.fovY target has no CameraComponent"});
        }
        return PropertyValue{*fov};
    }
    case PropertyId::RenderVisible: {
        const auto* visible = std::get_if<bool>(&value);
        if (visible == nullptr) {
            return core::unexpected(core::Error{"runtime.appearance.value_invalid",
                                                "render.visible requires a Boolean value"});
        }
        if (!present) {
            return core::unexpected(core::Error{"runtime.appearance.binding_missing",
                                                "Appearance target has no Renderable component"});
        }
        return PropertyValue{*visible};
    }
    case PropertyId::RenderMaterial: {
        const auto* material = std::get_if<std::string_view>(&value);
        if (material == nullptr || material->empty()) {
            return core::unexpected(
                core::Error{"runtime.appearance.value_invalid",
                            "render.material requires a non-empty Material AssetId"});
        }
        if (!present) {
            return core::unexpected(core::Error{"runtime.appearance.binding_missing",
                                                "Appearance target has no Renderable component"});
        }
        return PropertyValue{std::string{*material}};
    }
    case PropertyId::MaterialOpacity: {
        const auto* opacity = std::get_if<double>(&value);
        if (opacity == nullptr || !std::isfinite(*opacity) || *opacity < 0.0 || *opacity > 1.0) {
            return core::unexpected(core::Error{"runtime.appearance.value_invalid",
                                                "material.opacity must be in [0, 1]"});
        }
        if (!present) {
            return core::unexpected(core::Error{"runtime.appearance.binding_missing",
                                                "Appearance target has no Renderable component"});
        }
        return PropertyValue{*opacity};
    }
    case PropertyId::MaterialTint: {
        const auto* tint = std::get_if<core::Vec3>(&value);
        if (tint == nullptr || !core::isFinite(*tint) || tint->x < 0.0F || tint->x > 1.0F ||
            tint->y < 0.0F || tint->y > 1.0F || tint->z < 0.0F || tint->z > 1.0F) {
            return core::unexpected(
                core::Error{"runtime.appearance.value_invalid",
                            "material.tint components must be finite and in [0, 1]"});
        }
        if (!present) {
            return core::unexpected(core::Error{"runtime.appearance.binding_missing",
                                                "Appearance target has no Renderable component"});
        }
        return PropertyValue{*tint};
    }
    }
    return core::unexpected(entityError("world.property.value_invalid",
                                        "Property value type is not supported", entity));
}

inline void assignTransform(TransformComponent& transform, PropertyId property,
                            const PropertyValue& value) noexcept {
    switch (property) {
    case PropertyId::TransformPositionX:
        transform.position.x = static_cast<float>(std::get<double>(value));
        break;
    case PropertyId::TransformPositionY:
        transform.position.y = static_cast<float>(std::get<double>(value));
        break;
    case PropertyId::TransformPositionZ:
        transform.position.z = static_cast<float>(std::get<double>(value));
        break;
    case PropertyId::TransformRotation:
        transform.rotation = std::get<core::Quat>(value);
        break;
    case PropertyId::TransformScale:
        transform.scale = std::get<core::Vec3>(value);
        break;
    default:
        break;
    }
}

} // namespace cuexis::world::property_detail
