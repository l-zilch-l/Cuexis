#pragma once

// The chart v4 resolver applies the resolved parameters, then builds the animation program and
// validates it. The units share the readers and the resolved value helpers, so this header names
// them once.

#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/json/value.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::chart {

using PropertySet = std::set<std::string, std::less<>>;

struct ImportedTemplate final {
    AnimationTemplateDocument document;
    CanonicalContentIdentity identity;
    std::string source;
    std::string importFieldPath;
};

struct LayerPathKey final {
    std::string objectId;
    AnimationRecordIdentity layerIdentity;

    auto operator<=>(const LayerPathKey&) const = default;
};

struct GroupPathKey final {
    std::string objectId;
    AnimationRecordIdentity layerIdentity;
    AnimationRecordIdentity groupIdentity;

    auto operator<=>(const GroupPathKey&) const = default;
};

struct InstancePathKey final {
    std::string objectId;
    AnimationRecordIdentity layerIdentity;
    AnimationRecordIdentity groupIdentity;
    AnimationRecordIdentity instanceIdentity;

    auto operator<=>(const InstancePathKey&) const = default;
};

struct ProgramDiagnosticScope final {
    std::string fieldPath;
    std::optional<GeneratedAnimationIdentity> generatedIdentity;
};

struct ProgramPaths final {
    std::map<AnimationRecordIdentity, ProgramDiagnosticScope, std::less<>> clips;
    std::map<LayerPathKey, ProgramDiagnosticScope, std::less<>> layers;
    std::map<GroupPathKey, ProgramDiagnosticScope, std::less<>> groups;
    std::map<InstancePathKey, ProgramDiagnosticScope, std::less<>> instances;
};

struct ProgramBuild final {
    AnimationProgramInput program;
    ProgramPaths paths;
    std::size_t generatedRecords{};
};

void addResolverError(core::Diagnostics& diagnostics, std::string code, std::string message,
                      std::string path);

void addResolverError(core::Diagnostics& diagnostics, const core::Error& error, std::string path);

void appendTemplateDiagnostics(core::Diagnostics& destination, core::Diagnostics source,
                               std::string_view path, std::string_view importId);

[[nodiscard]] auto parameterJsonValue(const ChartParameterValue& value) -> json::Value;

[[nodiscard]] auto replaceAtFieldPath(json::Value& root, std::string_view fieldPath,
                                      const json::Value& replacement) -> bool;

[[nodiscard]] auto makeConcreteChart(json::Value source, const ChartLimits& limits,
                                     core::Diagnostics& diagnostics)
    -> std::optional<ChartDocument>;

[[nodiscard]] auto concreteAnimators(const json::Value& source, const ChartLimits& limits,
                                     core::Diagnostics& diagnostics)
    -> std::map<std::string, AnimatorComponent, std::less<>>;

void validateRequiredExtensions(const std::vector<RequiredExtension>& required,
                                const std::map<std::string, std::uint32_t, std::less<>>& supported,
                                core::Diagnostics& diagnostics, std::string path,
                                std::optional<std::string_view> source = {},
                                std::optional<std::string_view> templateId = {},
                                std::optional<std::string_view> importId = {});

[[nodiscard]] auto
loadImports(const ChartV4SourceDocument& source, std::span<const ProjectDocument> projectDocuments,
            const std::map<std::string, std::uint32_t, std::less<>>& supportedExtensions,
            const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::map<std::string, ImportedTemplate, std::less<>>;

[[nodiscard]] auto expandMask(const PropertyMask& mask) -> PropertySet;

[[nodiscard]] auto clipProperties(const AnimationClip& clip) -> PropertySet;

[[nodiscard]] auto propertyMask(const AnimationClip& clip) -> PropertyMask;

[[nodiscard]] auto intersects(const PropertySet& left, const PropertySet& right) -> bool;

[[nodiscard]] auto intersection(const PropertySet& left, const PropertySet& right) -> PropertySet;

[[nodiscard]] auto isSubset(const PropertySet& subset, const PropertySet& superset) -> bool;

[[nodiscard]] auto isDiscrete(const PropertySet& properties) -> bool;

[[nodiscard]] auto additiveAllowed(const PropertySet& properties) -> bool;

[[nodiscard]] auto scaleIsPositive(const AnimationClip& clip) -> bool;

void addScopedError(core::Diagnostics& diagnostics, std::string code, std::string message,
                    const ProgramDiagnosticScope* scope, std::string fallbackPath);

[[nodiscard]] auto preferGeneratedScope(const ProgramDiagnosticScope* preferred,
                                        const ProgramDiagnosticScope* alternate)
    -> const ProgramDiagnosticScope*;

[[nodiscard]] auto
buildProgram(const ChartV4SourceDocument& source,
             const std::map<std::string, AnimatorComponent, std::less<>>& animators,
             const std::map<std::string, ImportedTemplate, std::less<>>& imports,
             const std::map<std::string, ChartParameterValue, std::less<>>& parameters,
             const ChartLimits& limits, core::Diagnostics& diagnostics) -> ProgramBuild;

} // namespace cuexis::chart
