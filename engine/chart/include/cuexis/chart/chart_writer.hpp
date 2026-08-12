#pragma once

#include <cuexis/chart/animation_template_document.hpp>
#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/result.hpp>

#include <string>

namespace cuexis::chart {

class ChartWriter final {
  public:
    [[nodiscard]] static auto write(const ChartDocument& document) -> core::Result<std::string>;

    [[nodiscard]] static auto writeV4(const ChartV4SourceDocument& document,
                                      const ChartLimits& limits = {}) -> core::Result<std::string>;

    [[nodiscard]] static auto writeAnimationTemplate(const AnimationTemplateDocument& document,
                                                     const ChartLimits& limits = {})
        -> core::Result<std::string>;
};

} // namespace cuexis::chart
