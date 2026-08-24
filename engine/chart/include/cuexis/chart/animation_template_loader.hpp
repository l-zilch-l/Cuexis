#pragma once

#include <cuexis/chart/animation_template_document.hpp>
#include <cuexis/chart/limits.hpp>

#include <string_view>

namespace cuexis::chart {

class AnimationTemplateLoader final {
  public:
    [[nodiscard]] static auto load(std::string_view jsonText, const ChartLimits& limits = {})
        -> AnimationTemplateResult;
};

} // namespace cuexis::chart
