#pragma once

//  ChartDocument - document model for scheme A canonical charts
//  Kept separate from ChartRuntime: ChartDocument targets editing/saving/migration,
//  ChartRuntime targets playback
//  Types use stable string IDs rather than C++ class names; UUIDv7 is used for persistent IDs
//  Objects are expressed through Component composition (noted/element/decoration) instead of
//  parallel object containers
//  OpaqueJson: already-normalized JSON text; it produces no runtime behavior in v1

#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/math.hpp>

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart {

struct ChartId final {
    std::string value;
    auto operator<=>(const ChartId&) const = default;
};

struct ChartObjectId final {
    std::string value;
    auto operator<=>(const ChartObjectId&) const = default;
};

struct ChartTemplateId final {
    std::string value;
    auto operator<=>(const ChartTemplateId&) const = default;
};

struct AssetId final {
    std::string value;
    auto operator<=>(const AssetId&) const = default;
};

struct BehaviorId final {
    std::string value;
    auto operator<=>(const BehaviorId&) const = default;
};

// Opaque objects are normalized JSON text owned by Cuexis. They never affect v1 runtime behavior.
struct OpaqueJson final {
    std::string canonicalText{"{}"};
};

struct ChartMetadata final {
    OpaqueJson data{};
};

struct ChartTiming final {
    double offsetMs{};
    double defaultBpm{120.0};
};

struct TransformData final {
    core::Vec3 position{};
    core::Quat rotation{};
    core::Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct CameraData final {
    std::string type{"perspective"};
    double fovY{60.0};
    double nearPlane{0.1};
    double farPlane{1000.0};
    double pitch{0.0};
    double yaw{0.0};
    double roll{0.0};
    std::optional<TransformData> defaultTransform;
};

struct CameraComponentData final {
    std::string type{"perspective"};
    double fovY{60.0};
    double nearPlane{0.1};
    double farPlane{1000.0};
};

struct RenderableData final {
    AssetId mesh;
    AssetId material;
};

struct BehaviorReferenceData final {
    BehaviorId behavior;
};

struct NoteData final {
    // Template prototypes may omit beat; every concrete note object must provide it.
    std::optional<RationalBeat> beat;
};

enum class BehaviorProperty {
    TransformPositionX,
    TransformPositionY,
    TransformPositionZ,
    TransformRotation,
    TransformScale,
    CameraFovY,
};

enum class BehaviorEasing {
    Linear,
    InCubic,
    OutCubic,
    InOutCubic,
};

using BehaviorValue = std::variant<double, core::Vec3, core::Quat>;

struct BehaviorKey final {
    RationalBeat beat;
    BehaviorValue value{};
    std::optional<BehaviorEasing> easing;
};

struct BehaviorTrack final {
    BehaviorProperty property{};
    std::vector<BehaviorKey> keys;
};

struct BehaviorTracks final {
    std::vector<BehaviorTrack> items;
    // Transitional source text for 1A callers. Parsed v1 documents always populate items.
    std::string canonicalText{"[]"};

    BehaviorTracks() = default;
    BehaviorTracks(std::vector<BehaviorTrack> value) : items(std::move(value)) {}
    BehaviorTracks(OpaqueJson value) : canonicalText(std::move(value.canonicalText)) {}
    BehaviorTracks(std::string value) : canonicalText(std::move(value)) {}
};

struct ObjectComponents final {
    std::optional<TransformData> transform;
    std::optional<RenderableData> renderable;
    std::optional<BehaviorReferenceData> behavior;
    std::optional<NoteData> note;
    std::optional<CameraComponentData> camera;
    bool element{};
};

struct ChartTemplate final {
    ChartTemplateId id;
    std::optional<std::string> name;
    std::optional<ChartTemplateId> extends;
    ObjectComponents prototype;
    OpaqueJson extensions{};
};

struct ChartBehavior final {
    BehaviorId id;
    std::string type;
    std::uint32_t version{1};
    BehaviorTracks tracks;
};

struct ChartObject final {
    ChartObjectId id;
    std::optional<std::string> name;
    std::optional<ChartObjectId> parent;
    std::optional<ChartTemplateId> sourceTemplate;
    ObjectComponents components;
    OpaqueJson extensions{};
};

struct ChartDocument final {
    ChartId chartId;
    ChartMetadata metadata;
    ChartTiming timing;
    CameraData camera;
    std::vector<ChartTemplate> templates;
    std::vector<ChartBehavior> behaviors;
    std::vector<ChartObject> objects;
    OpaqueJson extensions{};
};

struct ChartDocumentResult final {
    std::optional<ChartDocument> document;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return document.has_value() && !diagnostics.hasErrors();
    }
};

} // namespace cuexis::chart
