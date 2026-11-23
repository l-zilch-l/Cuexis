#pragma once

// Candidate CXT v2 reader and finite expander. This API is deliberately
// separate from the frozen CXT v1 animation-template loader.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cuexis::chart {

using CxtV2Value = std::variant<std::int64_t, RationalBeat>;

struct CxtV2ParameterBinding final {
    std::string id;
    CxtV2Value value;
};

struct CxtV2Invocation final {
    ChartId chartId;
    std::string bindingId;
    std::string moduleId;
    std::string exportId;
    RationalBeat startBeat{RationalBeat::zero()};
    std::optional<CanonicalEntityIdentity> parent;
    std::vector<CxtV2ParameterBinding> parameters;
    std::vector<CxtV2ParameterBinding> slotBindings;
};

struct CxtV2ExpansionCounts final {
    std::size_t entityCount{};
    std::size_t requirementCount{};
    std::size_t eventCount{};
};

struct CxtV2ExpansionResult final {
    std::optional<CanonicalSemanticChart> chart;
    CxtV2ExpansionCounts counts;
    core::Diagnostics diagnostics;
};

class CxtV2Loader final {
  public:
    [[nodiscard]] static auto expand(std::string_view jsonText, const CxtV2Invocation& invocation,
                                     const ChartLimits& limits = {}) -> CxtV2ExpansionResult;
};

} // namespace cuexis::chart
