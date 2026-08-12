#pragma once

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/math.hpp>

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cuexis::chart {

enum class ChartParameterType {
    Number,
    Rational,
    Weight,
};

struct ParameterReference final {
    std::string id;
    auto operator<=>(const ParameterReference&) const = default;
};

using NumberSource = std::variant<double, ParameterReference>;
using RationalSource = std::variant<RationalBeat, ParameterReference>;
using WeightSource = std::variant<double, ParameterReference>;
using ChartParameterValue = std::variant<double, RationalBeat>;

struct ChartParameterConstraints final {
    std::optional<ChartParameterValue> minimum;
    std::optional<ChartParameterValue> exclusiveMinimum;
    std::optional<ChartParameterValue> maximum;
    std::optional<ChartParameterValue> exclusiveMaximum;
};

struct ChartParameterDeclaration final {
    std::string id;
    ChartParameterType type{ChartParameterType::Number};
    std::optional<ChartParameterValue> defaultValue;
    ChartParameterConstraints constraints;
    std::string fieldPath;
};

struct ParameterUse final {
    std::string id;
    ChartParameterType expectedType{ChartParameterType::Number};
    std::string fieldPath;
};

struct RequiredExtension final {
    std::string id;
    std::uint32_t version{1};
};

struct AnimationTemplateImport final {
    std::string id;
    std::string source;
    std::string fieldPath;
};

enum class AnimationProperty {
    TransformPositionX,
    TransformPositionY,
    TransformPositionZ,
    TransformRotation,
    TransformScale,
    MaterialOpacity,
    MaterialTint,
};

enum class AnimationStepProperty {
    RenderVisible,
    RenderMaterial,
};

enum class AnimationBlendMode {
    Override,
    Additive,
};

enum class AnimationFillMode {
    None,
    Hold,
};

using AnimationValue = std::variant<double, core::Vec3, core::Quat>;
using AnimationStepValue = std::variant<bool, AssetId>;

struct AnimationSegment final {
    RationalBeat startBeat;
    RationalBeat durationBeats;
    AnimationValue startValue;
    AnimationValue endValue;
    double startSlope{};
    double endSlope{};
    std::string fieldPath;
};

struct AnimationTrack final {
    AnimationProperty property{AnimationProperty::TransformPositionX};
    std::vector<AnimationSegment> segments;
    std::string fieldPath;
};

struct AnimationStep final {
    RationalBeat beat;
    AnimationStepValue value;
    std::string fieldPath;
};

struct AnimationStepTrack final {
    AnimationStepProperty property{AnimationStepProperty::RenderVisible};
    std::vector<AnimationStep> steps;
    std::string fieldPath;
};

struct AnimationClip final {
    std::string id;
    RationalBeat durationBeats;
    std::vector<AnimationTrack> tracks;
    std::vector<AnimationStepTrack> stepTracks;
    std::string fieldPath;
};

struct PropertyMask final {
    std::vector<std::string> properties;
    std::vector<std::string> prefixes;
};

struct AnimationIterations final {
    bool infinite{};
    std::uint16_t count{1};
};

struct TemplateBinding final {
    std::string bindingId;
    std::string templateId;
    RationalBeat startBeat;
    RationalSource durationScale;
    WeightSource weight;
    std::int64_t priority{};
    std::string fieldPath;
};

struct ClipInstance final {
    std::string instanceId;
    std::string clipId;
    RationalBeat startBeat;
    AnimationIterations iterations;
    AnimationFillMode fillMode{AnimationFillMode::None};
    WeightSource weight;
    PropertyMask propertyMask;
    std::string fieldPath;
};

struct BlendGroup final {
    std::string groupId;
    AnimationBlendMode mode{AnimationBlendMode::Override};
    WeightSource weight;
    std::vector<ClipInstance> instances;
    std::string fieldPath;
};

struct AnimationLayer final {
    std::string layerId;
    std::int64_t priority{};
    WeightSource weight;
    PropertyMask propertyMask;
    std::vector<BlendGroup> blendGroups;
    std::string fieldPath;
};

struct AnimatorComponent final {
    std::vector<TemplateBinding> templateBindings;
    std::vector<AnimationLayer> layers;
};

enum class AnimatorOwnerKind {
    Object,
    Template,
};

struct AnimatorSource final {
    AnimatorOwnerKind ownerKind{AnimatorOwnerKind::Object};
    std::string ownerId;
    AnimatorComponent component;
    std::string fieldPath;
};

struct ChartV4SourceDocument final {
    ChartId chartId;
    ChartDocument legacyProjection;
    OpaqueJson canonicalSource;
    std::vector<ChartParameterDeclaration> parameters;
    std::vector<ParameterUse> parameterUses;
    std::vector<AnimationTemplateImport> animationTemplateImports;
    std::vector<AnimationClip> animationClips;
    std::vector<AnimatorSource> animators;
    std::vector<RequiredExtension> requiredExtensions;
    OpaqueJson extensions;
};

struct ChartV4SourceResult final {
    std::optional<ChartV4SourceDocument> document;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return document.has_value() && !diagnostics.hasErrors();
    }
};

} // namespace cuexis::chart
