#pragma once

// Backend-neutral runtime property writes and layered PropertyResolver.
// Transform commit writes World components. Camera and Appearance values are resolved here
// and committed by Runtime, so World does not depend on render headers.

#include <cuexis/core/math.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/world/components.hpp>

#include <entt/entity/entity.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cuexis::world {

class World;

inline constexpr std::size_t maxPropertyWritesPerFrame = 600000;
inline constexpr std::size_t propertyCount = 10;

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

enum class PropertyLayer : std::uint8_t {
    Initial = 0,
    Behavior = 1,
    Animation = 2,
    HostOverride = 3,
    StudioPreviewOverride = 4,
};

enum class OverrideKind : std::uint8_t {
    Host,
    StudioPreview,
};

enum class OverrideLifetimeKind : std::uint8_t {
    UntilReleased,
    RemainingFrames,
    UntilChartTimeMs,
};

using PropertyValue = std::variant<double, core::Vec3, core::Quat, bool, std::string>;
using PropertyWriteValue = std::variant<double, core::Vec3, core::Quat, bool, std::string_view>;

struct PropertyWrite final {
    entt::entity entity{entt::null};
    PropertyId property{};
    PropertyWriteValue value{};
};

struct OverrideLifetime final {
    OverrideLifetimeKind kind{OverrideLifetimeKind::UntilReleased};
    std::uint32_t remainingFrames{};
    double untilChartTimeMs{};
};

struct OverrideWrite final {
    entt::entity entity{entt::null};
    PropertyId property{};
    PropertyValue value{};
};

struct OverrideTokenId final {
    std::uint64_t value{};

    auto operator<=>(const OverrideTokenId&) const = default;
};

struct OverrideToken final {
    OverrideTokenId id{};
    OverrideKind kind{OverrideKind::Host};
    std::string ownerId;
    std::int64_t priority{};
    std::uint16_t propertyMask{0xFFFF};
    OverrideLifetime lifetime{};
    std::vector<OverrideWrite> writes;
};

struct PropertyConflict final {
    entt::entity entity{entt::null};
    PropertyId property{};
    PropertyLayer layer{PropertyLayer::HostOverride};
    std::int64_t priority{};
};

[[nodiscard]] constexpr auto propertyIndex(PropertyId property) noexcept -> std::size_t {
    return static_cast<std::size_t>(property);
}

[[nodiscard]] constexpr auto propertyBit(PropertyId property) noexcept -> std::uint16_t {
    return static_cast<std::uint16_t>(1U << static_cast<std::uint8_t>(property));
}

[[nodiscard]] auto owningValue(const PropertyWriteValue& value) -> PropertyValue;
[[nodiscard]] auto writeValue(const PropertyValue& value) -> PropertyWriteValue;

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
    std::deque<std::string> ownedStrings_;
    std::vector<PropertyWrite> writes_;
};

class PropertyResolver final {
  public:
    PropertyResolver() = default;

    [[nodiscard]] static auto capture(const World& world) -> core::Result<PropertyResolver>;

    [[nodiscard]] auto registerBaseline(entt::entity entity, PropertyId property,
                                        PropertyValue value) -> core::Result<void>;
    // Replaces a captured Initial baseline, increments baseRevision, and leaves later layers
    // to re-evaluate from that new baseline. The property must already have a baseline.
    [[nodiscard]] auto applyBaseProperty(entt::entity entity, PropertyId property,
                                         PropertyValue value) -> core::Result<void>;

    void beginFrame();
    [[nodiscard]] auto applyLayer(std::span<const PropertyWrite> writes, PropertyLayer layer,
                                  bool duplicateIsError) -> core::Result<void>;
    [[nodiscard]] auto applyOverrides(std::span<const OverrideToken> tokens, PropertyLayer layer)
        -> core::Result<void>;
    [[nodiscard]] auto prepare(std::span<const PropertyWrite> writes) -> core::Result<void>;
    [[nodiscard]] auto finalize() -> core::Result<void>;
    [[nodiscard]] auto commit(World& world) -> core::Result<void>;
    void rollback(World& world) noexcept;

    [[nodiscard]] auto resolvedValue(entt::entity entity, PropertyId property) const noexcept
        -> std::optional<PropertyValue>;
    [[nodiscard]] auto baselineValue(entt::entity entity, PropertyId property) const noexcept
        -> std::optional<PropertyValue>;
    [[nodiscard]] auto layerValue(entt::entity entity, PropertyId property,
                                  PropertyLayer layer) const noexcept
        -> std::optional<PropertyValue>;
    [[nodiscard]] auto sourceLayer(entt::entity entity, PropertyId property) const noexcept
        -> std::optional<PropertyLayer>;
    [[nodiscard]] auto hadConflict(entt::entity entity, PropertyId property) const noexcept -> bool;
    [[nodiscard]] auto conflicts() const noexcept -> std::span<const PropertyConflict>;
    [[nodiscard]] auto baselineCount() const noexcept -> std::size_t;
    [[nodiscard]] auto baseRevision() const noexcept -> std::uint64_t;

  private:
    struct PropertyState final {
        bool present{};
        PropertyValue baseline{};
        PropertyValue candidate{};
        PropertyValue behavior{};
        PropertyValue animation{};
        PropertyValue host{};
        PropertyValue preview{};
        PropertyLayer source{PropertyLayer::Initial};
        std::uint8_t seenLayers{};
        bool conflict{};
    };

    struct Entry final {
        entt::entity entity{entt::null};
        bool hasTransform{};
        TransformComponent transformBaseline{};
        TransformComponent transformCandidate{};
        TransformComponent transformPrevious{};
        std::uint8_t transformSeenMask{};
        std::array<PropertyState, propertyCount> properties{};
    };

    [[nodiscard]] auto findEntry(entt::entity entity) noexcept -> Entry*;
    [[nodiscard]] auto findEntry(entt::entity entity) const noexcept -> const Entry*;
    [[nodiscard]] auto ensureEntry(entt::entity entity) -> Entry&;
    void resetEntry(Entry& entry) noexcept;
    void markTouched(std::size_t entryIndex);
    void collectCommitEntries();
    [[nodiscard]] auto applyWrite(const PropertyWrite& write, PropertyLayer layer,
                                  bool duplicateIsError) -> core::Result<void>;
    [[nodiscard]] auto validateAndStore(Entry& entry, PropertyId property,
                                        const PropertyWriteValue& value, PropertyLayer layer)
        -> core::Result<void>;

    std::vector<Entry> entries_;
    std::vector<std::size_t> touchedEntries_;
    std::vector<std::size_t> committedEntries_;
    std::vector<std::size_t> thisCommit_;
    std::vector<PropertyConflict> conflicts_;
    std::uint64_t baseRevision_{};
    bool prepared_{};
    bool committed_{};
};

using TransformPropertyResolver = PropertyResolver;

} // namespace cuexis::world
