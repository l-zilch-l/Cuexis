#pragma once

// Explicit canonical Chart v1/v2 to v3 migration. Source data is never modified.

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::chart {

struct ChartMigrationReport final {
    std::uint32_t sourceVersion{};
    std::uint32_t targetVersion{3};
    std::size_t convertedBehaviors{};
    std::size_t generatedEvents{};
    std::size_t rewrittenBindings{};
    std::size_t expandedTemplateObjects{};
    std::vector<std::string> unboundBehaviorIds;
};

struct ChartMigrationArtifact final {
    ChartDocument document;
    ChartMigrationReport report;
    std::string chartJson;
    std::string reportJson;
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
};

} // namespace cuexis::chart
