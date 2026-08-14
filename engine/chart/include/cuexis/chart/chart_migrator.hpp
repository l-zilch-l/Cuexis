#pragma once

// Explicit canonical Chart v1/v2 to v3 and v3 to v4 migration. Source data is never modified.

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::chart {

struct ChartMigrationFieldCounts final {
    std::size_t animationClips{};
    std::size_t animationTemplateImports{};
    std::size_t behaviors{};
    std::size_t objects{};
    std::size_t parameters{};
};

struct ChartMigrationDiagnosticRecord final {
    std::string code;
    std::string fieldPath;
    std::string severity;
    std::string message;
};

struct ChartMigrationReport final {
    std::uint32_t sourceVersion{};
    std::uint32_t targetVersion{3};
    std::size_t convertedBehaviors{};
    std::size_t generatedEvents{};
    std::size_t rewrittenBindings{};
    std::size_t expandedTemplateObjects{};
    std::vector<std::string> unboundBehaviorIds;
    std::optional<std::string> sourceCanonicalIdentity;
    std::optional<std::string> targetCanonicalIdentity;
    std::vector<std::string> discardedFields;
    std::size_t generatedClips{};
    std::size_t generatedBindings{};
    std::size_t generatedParameters{};
    std::optional<ChartMigrationFieldCounts> fieldCounts;
    std::vector<ChartMigrationDiagnosticRecord> warnings;
    std::vector<ChartMigrationDiagnosticRecord> diagnostics;
};

struct ChartMigrationArtifact final {
    ChartDocument document;
    ChartMigrationReport report;
    std::string chartJson;
    std::string reportJson;
    std::optional<ChartV4SourceDocument> v4Document;
};

struct ChartMigrationResult final {
    std::optional<ChartMigrationArtifact> artifact;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return artifact.has_value() && !diagnostics.hasErrors();
    }
};

class ChartMigrator final {
  public:
    [[nodiscard]] static auto migrateToV3(std::string_view sourceJson,
                                          const ChartLimits& limits = {}) -> ChartMigrationResult;

    [[nodiscard]] static auto migrateToV4(std::string_view sourceJson,
                                          const ChartLimits& limits = {}) -> ChartMigrationResult;
};

} // namespace cuexis::chart
