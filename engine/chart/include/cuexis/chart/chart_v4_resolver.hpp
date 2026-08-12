#pragma once

#include <cuexis/chart/animation_template_document.hpp>
#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace cuexis::chart {

struct CanonicalContentIdentity final {
    std::array<std::uint8_t, 32> sha256{};

    [[nodiscard]] auto hex() const -> std::string;
    auto operator<=>(const CanonicalContentIdentity&) const = default;
};

struct ChartParameterInput final {
    std::string id;
    ChartParameterType type{ChartParameterType::Number};
    ChartParameterValue value{};
};

struct ResolvedChartParameter final {
    std::string id;
    ChartParameterType type{ChartParameterType::Number};
    ChartParameterValue value{};
};

struct ProjectDocument final {
    std::string path;
    std::string utf8Text;
};

enum class GeneratedRecordKind {
    Clip,
    Layer,
    BlendGroup,
    ClipInstance,
};

struct GeneratedAnimationIdentity final {
    std::string objectId;
    std::string bindingId;
    std::string templateId;
    GeneratedRecordKind recordKind{GeneratedRecordKind::Clip};

    auto operator<=>(const GeneratedAnimationIdentity&) const = default;
};

using AnimationRecordIdentity = std::variant<std::string, GeneratedAnimationIdentity>;

struct AnimationProgramClip final {
    AnimationRecordIdentity identity;
    AnimationClip clip;
};

struct ResolvedClipInstance final {
    AnimationRecordIdentity identity;
    AnimationRecordIdentity clipIdentity;
    RationalBeat startBeat;
    RationalBeat durationScale;
    AnimationIterations iterations;
    AnimationFillMode fillMode{AnimationFillMode::None};
    double weight{1.0};
    PropertyMask propertyMask;
};

struct ResolvedBlendGroup final {
    AnimationRecordIdentity identity;
    AnimationBlendMode mode{AnimationBlendMode::Override};
    double weight{1.0};
    std::vector<ResolvedClipInstance> instances;
};

struct ResolvedAnimationLayer final {
    AnimationRecordIdentity identity;
    std::int64_t priority{};
    double weight{1.0};
    PropertyMask propertyMask;
    std::vector<ResolvedBlendGroup> blendGroups;
};

struct ObjectAnimationProgram final {
    ChartObjectId objectId;
    std::vector<ResolvedAnimationLayer> layers;
};

struct AnimationProgramInput final {
    std::vector<AnimationProgramClip> clips;
    std::vector<ObjectAnimationProgram> objects;
};

struct ResolvedChartDocument final {
    ChartDocument chart;
    std::vector<ResolvedChartParameter> parameters;
};

struct CxtIdentityComponent final {
    std::string importId;
    CanonicalContentIdentity identity;
};

enum class ChartResourceUse {
    MainMusic,
    RenderableMesh,
    RenderableMaterial,
    BehaviorMaterial,
    AnimationMaterial,
};

struct ChartResourceRequirement final {
    AssetId assetId;
    std::vector<ChartResourceUse> uses;
};

struct ChartV4ResolvedArtifact final {
    ResolvedChartDocument document;
    AnimationProgramInput animationProgram;
    CanonicalContentIdentity chartIdentity;
    std::vector<CxtIdentityComponent> cxtIdentities;
    CanonicalContentIdentity parameterIdentity;
    std::vector<ChartResourceRequirement> resourceRequirements;
    std::vector<std::string> capabilityRequirements;
};

struct ChartV4ResolveResult final {
    std::optional<ChartV4ResolvedArtifact> artifact;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return artifact.has_value() && !diagnostics.hasErrors();
    }
};

class ChartV4Resolver final {
  public:
    [[nodiscard]] static auto resolve(const ChartV4SourceDocument& source,
                                      std::span<const ChartParameterInput> parameters = {},
                                      std::span<const ProjectDocument> projectDocuments = {},
                                      std::span<const RequiredExtension> supportedExtensions = {},
                                      const ChartLimits& limits = {}) -> ChartV4ResolveResult;
};

} // namespace cuexis::chart
