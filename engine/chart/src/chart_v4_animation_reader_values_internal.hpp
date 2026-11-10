#pragma once

#include <cuexis/chart/chart_v4_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/json/reader.hpp>

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::chart::detail {

// The animation value readers. The clip reader and the animator component reader both validate
// values through these, so they live in one unit behind this header instead of being duplicated.
[[nodiscard]] auto readFiniteNumber(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<double>;

[[nodiscard]] auto readVec3(const json::Reader& reader, core::Diagnostics& diagnostics,
                            bool unitRange) -> std::optional<core::Vec3>;

[[nodiscard]] auto readQuat(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<core::Quat>;

[[nodiscard]] auto readParameterReference(const json::Reader& reader, const ChartLimits& limits,
                                          core::Diagnostics& diagnostics,
                                          ChartParameterType expectedType,
                                          std::vector<ParameterUse>* uses)
    -> std::optional<ParameterReference>;

[[nodiscard]] auto readWeightSource(const json::Reader& reader, const ChartLimits& limits,
                                    core::Diagnostics& diagnostics, std::vector<ParameterUse>& uses)
    -> std::optional<WeightSource>;

[[nodiscard]] auto readRationalSource(const json::Reader& reader, const ChartLimits& limits,
                                      core::Diagnostics& diagnostics,
                                      std::vector<ParameterUse>& uses)
    -> std::optional<RationalSource>;

[[nodiscard]] auto readAnimationProperty(std::string_view property)
    -> std::optional<AnimationProperty>;

[[nodiscard]] auto readAnimationValue(const json::Reader& reader, AnimationProperty property,
                                      core::Diagnostics& diagnostics)
    -> std::optional<AnimationValue>;

[[nodiscard]] auto readAssetReference(const json::Reader& reader, const ChartLimits& limits,
                                      core::Diagnostics& diagnostics) -> std::optional<AssetId>;

[[nodiscard]] auto readPropertyMask(const json::Reader& reader, const ChartLimits& limits,
                                    core::Diagnostics& diagnostics) -> std::optional<PropertyMask>;

[[nodiscard]] auto maskProperties(const PropertyMask& mask) -> std::set<std::string, std::less<>>;

[[nodiscard]] auto masksOverlap(const PropertyMask& left, const PropertyMask& right) -> bool;

} // namespace cuexis::chart::detail
