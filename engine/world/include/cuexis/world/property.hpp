#pragma once

// Backend-neutral runtime property writes and transactional Transform resolution.

#include <cuexis/core/math.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/world/components.hpp>

#include <entt/entity/entity.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cuexis::world {

class World;

inline constexpr std::size_t maxPropertyWritesPerFrame = 600000;

enum class PropertyId : std::uint8_t {
    TransformPositionX,
    TransformPositionY,
    TransformPositionZ,
    TransformRotation,
    TransformScale,
    CameraFovY,
    RenderVisible,
    RenderMaterial,
    MaterialOpacity,
    MaterialTint,
};

using PropertyValue = std::variant<double, core::Vec3, core::Quat, bool, std::string>;
using PropertyWriteValue = std::variant<double, core::Vec3, core::Quat, bool, std::string_view>;

struct PropertyWrite final {
    entt::entity entity{entt::null};
    PropertyId property{};
    PropertyWriteValue value{};
};

class PropertyWriteBuffer final {
  public:
    explicit PropertyWriteBuffer(std::size_t maxWrites = maxPropertyWritesPerFrame);

    [[nodiscard]] auto push(PropertyWrite write) -> core::Result<void>;
    void clear() noexcept;

    [[nodiscard]] auto writes() const noexcept -> std::span<const PropertyWrite>;
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto maxWrites() const noexcept -> std::size_t;

  private:
    std::size_t maxWrites_{};
    std::vector<PropertyWrite> writes_;
};

class TransformPropertyResolver final {
  public:
    TransformPropertyResolver() = default;

    [[nodiscard]] static auto capture(const World& world)
        -> core::Result<TransformPropertyResolver>;

    [[nodiscard]] auto prepare(std::span<const PropertyWrite> writes) -> core::Result<void>;
    [[nodiscard]] auto commit(World& world) -> core::Result<void>;
    void rollback(World& world) noexcept;

    [[nodiscard]] auto resolvedValue(entt::entity entity, PropertyId property) const noexcept
        -> std::optional<PropertyValue>;
    [[nodiscard]] auto baselineCount() const noexcept -> std::size_t;

  private:
    struct Entry final {
        entt::entity entity{entt::null};
        TransformComponent baseline{};
        TransformComponent candidate{};
        TransformComponent previous{};
        std::uint8_t seenMask{};
    };

    std::vector<Entry> entries_;
    std::vector<std::size_t> touchedEntries_;
    bool prepared_{};
    bool committed_{};
};

} // namespace cuexis::world
