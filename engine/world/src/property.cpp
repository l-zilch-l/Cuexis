#include <cuexis/world/property.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace cuexis::world {
namespace {

using EntityValue = std::underlying_type_t<entt::entity>;

[[nodiscard]] auto entityValue(entt::entity entity) noexcept -> EntityValue {
    return entt::to_integral(entity);
}

[[nodiscard]] auto propertyMask(PropertyId property) noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(property));
}

[[nodiscard]] auto isTransformProperty(PropertyId property) noexcept -> bool {
    return property != PropertyId::CameraFovY;
}

[[nodiscard]] auto entityError(std::string code, std::string message, entt::entity entity)
    -> core::Error {
    return core::Error{std::move(code), std::move(message)}.withContext(
        "entity", std::to_string(static_cast<std::uint64_t>(entityValue(entity))));
}

} // namespace

PropertyWriteBuffer::PropertyWriteBuffer(std::size_t maxWrites) : maxWrites_(maxWrites) {
    writes_.reserve(maxWrites_);
}

auto PropertyWriteBuffer::push(PropertyWrite write) -> core::Result<void> {
    if (write.entity == entt::null) {
        return core::unexpected(
            core::Error{"world.property.null_entity", "Property writes require a valid entity"});
    }
    if (writes_.size() >= maxWrites_) {
        return core::unexpected(core::Error{"world.property.write_limit",
                                            "The per-frame PropertyWrite limit was reached"}
                                    .withContext("limit", std::to_string(maxWrites_)));
    }
    writes_.push_back(std::move(write));
    return {};
}

void PropertyWriteBuffer::clear() noexcept {
    writes_.clear();
}

auto PropertyWriteBuffer::writes() const noexcept -> std::span<const PropertyWrite> {
    return writes_;
}

auto PropertyWriteBuffer::size() const noexcept -> std::size_t {
    return writes_.size();
}

auto PropertyWriteBuffer::maxWrites() const noexcept -> std::size_t {
    return maxWrites_;
}

auto TransformPropertyResolver::capture(const World& world)
    -> core::Result<TransformPropertyResolver> {
    return world.withRegistry(
        [](const entt::registry& registry) -> core::Result<TransformPropertyResolver> {
            TransformPropertyResolver resolver;
            const auto view = registry.view<const TransformComponent>();
            for (const entt::entity entity : view) {
                const auto& transform = view.get<const TransformComponent>(entity);
                resolver.entries_.push_back(Entry{.entity = entity,
                                                  .baseline = transform,
                                                  .candidate = transform,
                                                  .previous = transform});
            }
            std::sort(resolver.entries_.begin(), resolver.entries_.end(),
                      [](const Entry& left, const Entry& right) {
                          return entityValue(left.entity) < entityValue(right.entity);
                      });
            return resolver;
        });
}

auto TransformPropertyResolver::prepare(std::span<const PropertyWrite> writes)
    -> core::Result<void> {
    prepared_ = false;
    committed_ = false;
    for (const auto index : touchedEntries_) {
        entries_[index].candidate = entries_[index].baseline;
        entries_[index].seenMask = 0;
    }
    touchedEntries_.clear();

    for (const auto& write : writes) {
        if (!isTransformProperty(write.property)) {
            continue;
        }
        const auto entry =
            std::lower_bound(entries_.begin(), entries_.end(), write.entity,
                             [](const Entry& candidate, entt::entity entity) {
                                 return entityValue(candidate.entity) < entityValue(entity);
                             });
        if (entry == entries_.end() || entry->entity != write.entity) {
            return core::unexpected(entityError("world.property.transform_missing",
                                                "Transform property target has no baseline",
                                                write.entity));
        }
        const auto entryIndex = static_cast<std::size_t>(entry - entries_.begin());
        if (entry->seenMask == 0U) {
            touchedEntries_.push_back(entryIndex);
        }
        const auto mask = propertyMask(write.property);
        if ((entry->seenMask & mask) != 0U) {
            return core::unexpected(entityError("world.property.write_conflict",
                                                "A Transform property was written more than once",
                                                write.entity));
        }
        entry->seenMask = static_cast<std::uint8_t>(entry->seenMask | mask);

        switch (write.property) {
        case PropertyId::TransformPositionX:
        case PropertyId::TransformPositionY:
        case PropertyId::TransformPositionZ: {
            const auto* value = std::get_if<double>(&write.value);
            constexpr auto floatMax = static_cast<double>(std::numeric_limits<float>::max());
            if (value == nullptr || !std::isfinite(*value) || *value < -floatMax ||
                *value > floatMax) {
                return core::unexpected(
                    entityError("world.property.value_invalid",
                                "Transform position property requires a finite float-range scalar",
                                write.entity));
            }
            auto& position = entry->candidate.position;
            if (write.property == PropertyId::TransformPositionX) {
                position.x = static_cast<float>(*value);
            } else if (write.property == PropertyId::TransformPositionY) {
                position.y = static_cast<float>(*value);
            } else {
                position.z = static_cast<float>(*value);
            }
            break;
        }
        case PropertyId::TransformRotation: {
            const auto* value = std::get_if<core::Quat>(&write.value);
            if (value == nullptr || !core::isNormalized(*value)) {
                return core::unexpected(entityError(
                    "world.property.value_invalid",
                    "Transform rotation property requires a normalized quaternion", write.entity));
            }
            entry->candidate.rotation = *value;
            break;
        }
        case PropertyId::TransformScale: {
            const auto* value = std::get_if<core::Vec3>(&write.value);
            if (value == nullptr || !core::isFinite(*value)) {
                return core::unexpected(
                    entityError("world.property.value_invalid",
                                "Transform scale property requires a finite Vec3", write.entity));
            }
            entry->candidate.scale = *value;
            break;
        }
        case PropertyId::CameraFovY:
            break;
        }
    }

    for (const auto index : touchedEntries_) {
        const auto& entry = entries_[index];
        const auto matrix = core::composeTransform(entry.candidate.position,
                                                   entry.candidate.rotation, entry.candidate.scale);
        if (!matrix || !core::isFinite(*matrix)) {
            return core::unexpected(entityError(
                "world.property.transform_invalid",
                "Resolved Transform did not produce a finite local matrix", entry.entity));
        }
    }
    prepared_ = true;
    return {};
}

auto TransformPropertyResolver::commit(World& world) -> core::Result<void> {
    if (!prepared_) {
        return core::unexpected(core::Error{"world.property.not_prepared",
                                            "Transform resolver commit requires prepare"});
    }
    auto result = world.withRegistry([&](entt::registry& registry) -> core::Result<void> {
        for (const auto index : touchedEntries_) {
            const auto& entry = entries_[index];
            if (!registry.valid(entry.entity) ||
                !registry.all_of<TransformComponent>(entry.entity)) {
                return core::unexpected(entityError(
                    "world.property.transform_missing",
                    "A captured Transform baseline is no longer available", entry.entity));
            }
        }
        for (const auto index : touchedEntries_) {
            auto& entry = entries_[index];
            entry.previous = registry.get<TransformComponent>(entry.entity);
            registry.replace<TransformComponent>(entry.entity, entry.candidate);
        }
        return {};
    });
    if (result) {
        committed_ = true;
    }
    return result;
}

void TransformPropertyResolver::rollback(World& world) noexcept {
    if (!committed_) {
        return;
    }
    world.withRegistry([&](entt::registry& registry) {
        for (const auto index : touchedEntries_) {
            const auto& entry = entries_[index];
            if (registry.valid(entry.entity) && registry.all_of<TransformComponent>(entry.entity)) {
                registry.replace<TransformComponent>(entry.entity, entry.previous);
            }
        }
    });
    committed_ = false;
}

auto TransformPropertyResolver::baselineCount() const noexcept -> std::size_t {
    return entries_.size();
}

} // namespace cuexis::world
