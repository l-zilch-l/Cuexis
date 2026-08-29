#pragma once

#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/json/reader.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cuexis::chart::detail {

[[nodiscard]] auto readAnimationClip(const json::Reader& reader, const ChartLimits& limits,
                                     core::Diagnostics& diagnostics, bool requireId,
                                     std::string fallbackId = {}) -> std::optional<AnimationClip>;

[[nodiscard]] auto readAnimatorComponent(const json::Reader& reader, const ChartLimits& limits,
                                         core::Diagnostics& diagnostics,
                                         std::vector<ParameterUse>& parameterUses)
    -> std::optional<AnimatorComponent>;

[[nodiscard]] auto readBlendMode(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<AnimationBlendMode>;
[[nodiscard]] auto readFillMode(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<AnimationFillMode>;
[[nodiscard]] auto readIterations(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<AnimationIterations>;

} // namespace cuexis::chart::detail
