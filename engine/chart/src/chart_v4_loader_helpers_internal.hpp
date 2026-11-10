#pragma once

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/json/reader.hpp>
#include <cuexis/json/value.hpp>

#include <optional>
#include <vector>

// Chart v4 reading is split across units: this header publishes the steps that assembling a
// ChartV4SourceDocument needs, so the parameter, projection and animator readers can be defined
// once instead of being duplicated per unit.
namespace cuexis::chart::loader_internal {

[[nodiscard]] auto readParameters(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics)
    -> std::vector<ChartParameterDeclaration>;

[[nodiscard]] auto readImports(const json::Reader& reader, const ChartLimits& limits,
                               core::Diagnostics& diagnostics)
    -> std::vector<AnimationTemplateImport>;

[[nodiscard]] auto makeLegacyProjection(json::Value source, const ChartLimits& limits,
                                        core::Diagnostics& diagnostics,
                                        std::vector<ParameterUse>& parameterUses)
    -> std::optional<ChartDocument>;

[[nodiscard]] auto readAnimators(const json::Reader& root, const ChartLimits& limits,
                                 core::Diagnostics& diagnostics,
                                 std::vector<ParameterUse>& parameterUses)
    -> std::vector<AnimatorSource>;

void validateParameterUses(const std::vector<ChartParameterDeclaration>& parameters,
                           const std::vector<ParameterUse>& uses, core::Diagnostics& diagnostics);

void validateAnimationReferences(const std::vector<AnimationTemplateImport>& imports,
                                 const std::vector<AnimationClip>& clips,
                                 const std::vector<AnimatorSource>& animators,
                                 core::Diagnostics& diagnostics);

} // namespace cuexis::chart::loader_internal
