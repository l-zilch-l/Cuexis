#pragma once

#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/animation_template_document.hpp>
#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
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
