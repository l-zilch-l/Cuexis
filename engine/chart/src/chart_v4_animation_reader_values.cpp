#include "chart_v4_animation_reader_values_internal.hpp"

#include "chart_v4_common_reader_internal.hpp"

#include <cuexis/core/math.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::detail {

namespace {

[[nodiscard]] auto knownAnimationProperties() noexcept -> const std::array<std::string_view, 9>& {
    static constexpr std::array values{
        std::string_view{"material.opacity"},     std::string_view{"material.tint"},
        std::string_view{"render.material"},      std::string_view{"render.visible"},
        std::string_view{"transform.position.x"}, std::string_view{"transform.position.y"},
        std::string_view{"transform.position.z"}, std::string_view{"transform.rotation"},
        std::string_view{"transform.scale"}};
    return values;
}
} // namespace

[[nodiscard]] auto readFiniteNumber(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<double> {
    const auto value = reader.readNumber();
    if (!value) {
        return std::nullopt;
    }
    if (!std::isfinite(*value)) {
        addV4Error(diagnostics, "chart.number.out_of_range", "Number must be finite",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] auto readVec3(const json::Reader& reader, core::Diagnostics& diagnostics,
                            bool unitRange) -> std::optional<core::Vec3> {
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return std::nullopt;
    }
    if (values->size() != 3) {
        addV4Error(diagnostics, "chart.vector.size", "Vector must contain exactly three values",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    std::array<float, 3> result{};
    bool valid = true;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto element = reader.element(index);
        const auto value = element ? readFiniteNumber(*element, diagnostics) : std::nullopt;
        if (!value || *value < -static_cast<double>(std::numeric_limits<float>::max()) ||
            *value > static_cast<double>(std::numeric_limits<float>::max()) ||
            (unitRange && (*value < 0.0 || *value > 1.0))) {
            if (value && unitRange) {
                addV4Error(diagnostics, "chart.animation.clip_invalid",
                           "Vector component is outside the allowed range",
                           std::string{element->fieldPath()});
            }
            valid = false;
            continue;
        }
        result[index] = static_cast<float>(*value);
    }
    if (!valid) {
        return std::nullopt;
    }
    return core::Vec3{result[0], result[1], result[2]};
}

[[nodiscard]] auto readQuat(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<core::Quat> {
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return std::nullopt;
    }
    if (values->size() != 4) {
        addV4Error(diagnostics, "chart.quaternion.size",
                   "Quaternion must contain exactly four values", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    std::array<float, 4> result{};
    bool valid = true;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto element = reader.element(index);
        const auto value = element ? readFiniteNumber(*element, diagnostics) : std::nullopt;
        if (!value || *value < -static_cast<double>(std::numeric_limits<float>::max()) ||
            *value > static_cast<double>(std::numeric_limits<float>::max())) {
            valid = false;
            continue;
        }
        result[index] = static_cast<float>(*value);
    }
    if (!valid) {
        return std::nullopt;
    }
    const core::Quat quaternion{result[0], result[1], result[2], result[3]};
    if (!core::isNormalized(quaternion)) {
        addV4Error(diagnostics, "chart.transform.rotation_not_normalized",
                   "Quaternion must be normalized", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return quaternion;
}

[[nodiscard]] auto readParameterReference(const json::Reader& reader, const ChartLimits& limits,
                                          core::Diagnostics& diagnostics,
                                          ChartParameterType expectedType,
                                          std::vector<ParameterUse>* uses)
    -> std::optional<ParameterReference> {
    constexpr std::array fields{std::string_view{"parameter"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto parameterReader = reader.requiredField("parameter");
    if (!parameterReader || parameterReader->readObject() == nullptr) {
        return std::nullopt;
    }
    constexpr std::array referenceFields{std::string_view{"domain"}, std::string_view{"id"}};
    parameterReader->rejectUnknownFields(referenceFields);
    const auto domainReader = parameterReader->requiredField("domain");
    const auto idReader = parameterReader->requiredField("id");
    const auto domain = domainReader ? domainReader->readString() : std::nullopt;
    const auto id =
        idReader ? readPortableStableId(*idReader, limits, diagnostics, "Chart parameter reference")
                 : std::nullopt;
    if (!domain || !id) {
        return std::nullopt;
    }
    if (*domain != "chart-parameter") {
        addV4Error(diagnostics, "chart.parameter.use_not_allowed",
                   "Parameter reference domain must be chart-parameter",
                   std::string{domainReader->fieldPath()});
        return std::nullopt;
    }
    if (uses != nullptr) {
        uses->push_back(ParameterUse{*id, expectedType, std::string{reader.fieldPath()}});
    }
    return ParameterReference{*id};
}

[[nodiscard]] auto readWeightSource(const json::Reader& reader, const ChartLimits& limits,
                                    core::Diagnostics& diagnostics, std::vector<ParameterUse>& uses)
    -> std::optional<WeightSource> {
    if (reader.value().object() != nullptr) {
        const auto reference =
            readParameterReference(reader, limits, diagnostics, ChartParameterType::Weight, &uses);
        if (!reference) {
            return std::nullopt;
        }
        return WeightSource{*reference};
    }
    const auto value = readFiniteNumber(reader, diagnostics);
    if (!value) {
        return std::nullopt;
    }
    if (*value < 0.0 || *value > 1.0) {
        addV4Error(diagnostics, "chart.parameter.out_of_range", "Weight must be within [0, 1]",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return WeightSource{*value};
}

[[nodiscard]] auto readRationalSource(const json::Reader& reader, const ChartLimits& limits,
                                      core::Diagnostics& diagnostics,
                                      std::vector<ParameterUse>& uses)
    -> std::optional<RationalSource> {
    if (reader.value().object() != nullptr && reader.value().find("parameter") != nullptr) {
        const auto reference = readParameterReference(reader, limits, diagnostics,
                                                      ChartParameterType::Rational, &uses);
        if (!reference) {
            return std::nullopt;
        }
        return RationalSource{*reference};
    }
    const auto value = readV4Rational(reader, limits, diagnostics, true, false);
    if (!value) {
        return std::nullopt;
    }
    return RationalSource{*value};
}

[[nodiscard]] auto readAnimationProperty(std::string_view property)
    -> std::optional<AnimationProperty> {
    if (property == "transform.position.x") {
        return AnimationProperty::TransformPositionX;
    }
    if (property == "transform.position.y") {
        return AnimationProperty::TransformPositionY;
    }
    if (property == "transform.position.z") {
        return AnimationProperty::TransformPositionZ;
    }
    if (property == "transform.rotation") {
        return AnimationProperty::TransformRotation;
    }
    if (property == "transform.scale") {
        return AnimationProperty::TransformScale;
    }
    if (property == "material.opacity") {
        return AnimationProperty::MaterialOpacity;
    }
    if (property == "material.tint") {
        return AnimationProperty::MaterialTint;
    }
    return std::nullopt;
}

[[nodiscard]] auto readAnimationValue(const json::Reader& reader, AnimationProperty property,
                                      core::Diagnostics& diagnostics)
    -> std::optional<AnimationValue> {
    switch (property) {
    case AnimationProperty::TransformPositionX:
    case AnimationProperty::TransformPositionY:
    case AnimationProperty::TransformPositionZ: {
        const auto value = readFiniteNumber(reader, diagnostics);
        return value ? std::optional<AnimationValue>{AnimationValue{*value}} : std::nullopt;
    }
    case AnimationProperty::TransformRotation: {
        const auto value = readQuat(reader, diagnostics);
        return value ? std::optional<AnimationValue>{AnimationValue{*value}} : std::nullopt;
    }
    case AnimationProperty::TransformScale: {
        const auto value = readVec3(reader, diagnostics, false);
        return value ? std::optional<AnimationValue>{AnimationValue{*value}} : std::nullopt;
    }
    case AnimationProperty::MaterialOpacity: {
        const auto value = readFiniteNumber(reader, diagnostics);
        if (value && (*value < 0.0 || *value > 1.0)) {
            addV4Error(diagnostics, "chart.animation.clip_invalid", "Opacity must be within [0, 1]",
                       std::string{reader.fieldPath()});
            return std::nullopt;
        }
        return value ? std::optional<AnimationValue>{AnimationValue{*value}} : std::nullopt;
    }
    case AnimationProperty::MaterialTint: {
        const auto value = readVec3(reader, diagnostics, true);
        return value ? std::optional<AnimationValue>{AnimationValue{*value}} : std::nullopt;
    }
    }
    return std::nullopt;
}

[[nodiscard]] auto readAssetReference(const json::Reader& reader, const ChartLimits& limits,
                                      core::Diagnostics& diagnostics) -> std::optional<AssetId> {
    constexpr std::array fields{std::string_view{"domain"}, std::string_view{"id"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto domainReader = reader.requiredField("domain");
    const auto idReader = reader.requiredField("id");
    const auto domain = domainReader ? domainReader->readString() : std::nullopt;
    const auto id =
        idReader ? readPortableStableId(*idReader, limits, diagnostics, "Asset ID") : std::nullopt;
    if (!domain || !id) {
        return std::nullopt;
    }
    if (*domain != "asset") {
        addV4Error(diagnostics, "chart.animation.clip_invalid",
                   "Animation asset reference must use the asset domain",
                   std::string{domainReader->fieldPath()});
        return std::nullopt;
    }
    return AssetId{*id};
}

[[nodiscard]] auto readPropertyMask(const json::Reader& reader, const ChartLimits& limits,
                                    core::Diagnostics& diagnostics) -> std::optional<PropertyMask> {
    constexpr std::array fields{std::string_view{"properties"}, std::string_view{"prefixes"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto propertiesReader = reader.requiredField("properties");
    const auto prefixesReader = reader.requiredField("prefixes");
    if (!propertiesReader || !prefixesReader) {
        return std::nullopt;
    }
    const auto* properties = propertiesReader->readArray();
    const auto* prefixes = prefixesReader->readArray();
    if (properties == nullptr || prefixes == nullptr) {
        return std::nullopt;
    }
    if (properties->size() > limits.maxAnimationMaskEntries ||
        prefixes->size() > limits.maxAnimationMaskEntries - properties->size()) {
        addV4Error(diagnostics, "chart.animation.mask_conflict", "Property mask exceeds the limit",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    if (properties->empty() && prefixes->empty()) {
        addV4Error(diagnostics, "chart.animation.mask_conflict",
                   "Property mask must select at least one known property",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    PropertyMask result;
    std::set<std::string, std::less<>> seen;
    bool valid = true;
    for (std::size_t index = 0; index < properties->size(); ++index) {
        const auto element = propertiesReader->element(index);
        const auto value = element ? element->readString() : std::nullopt;
        if (!value || value->empty() || value->size() > limits.maxIdentifierBytes) {
            addV4Error(diagnostics, "chart.animation.mask_conflict",
                       "Property mask property must be a bounded non-empty value",
                       element ? std::string{element->fieldPath()}
                               : std::string{propertiesReader->fieldPath()});
            valid = false;
            continue;
        }
        if (std::ranges::find(knownAnimationProperties(), *value) ==
            knownAnimationProperties().end()) {
            addV4Error(diagnostics, "chart.animation.mask_conflict",
                       "Property mask contains an unknown property",
                       std::string{element->fieldPath()});
            valid = false;
            continue;
        }
        if (!seen.emplace(*value).second) {
            addV4Error(diagnostics, "chart.animation.mask_conflict",
                       "Property mask contains a duplicate property",
                       std::string{element->fieldPath()});
            valid = false;
            continue;
        }
        result.properties.emplace_back(*value);
    }
    for (std::size_t index = 0; index < prefixes->size(); ++index) {
        const auto element = prefixesReader->element(index);
        const auto value = element ? element->readString() : std::nullopt;
        if (!value || value->empty() || value->back() != '.' ||
            value->size() > limits.maxIdentifierBytes) {
            addV4Error(diagnostics, "chart.animation.mask_conflict",
                       "Property mask prefix must be a bounded value ending in '.'",
                       element ? std::string{element->fieldPath()}
                               : std::string{prefixesReader->fieldPath()});
            valid = false;
            continue;
        }
        if (!std::ranges::any_of(knownAnimationProperties(), [value](std::string_view property) {
                return property.starts_with(*value);
            })) {
            addV4Error(diagnostics, "chart.animation.mask_conflict",
                       "Property mask prefix does not match a known property",
                       std::string{element->fieldPath()});
            valid = false;
            continue;
        }
        if (!seen.emplace(*value).second) {
            addV4Error(diagnostics, "chart.animation.mask_conflict",
                       "Property mask contains a duplicate prefix",
                       std::string{element->fieldPath()});
            valid = false;
            continue;
        }
        result.prefixes.emplace_back(*value);
    }
    for (const auto& property : result.properties) {
        for (const auto& prefix : result.prefixes) {
            if (property.starts_with(prefix)) {
                addV4Error(diagnostics, "chart.animation.mask_conflict",
                           "Property mask property overlaps a prefix",
                           std::string{reader.fieldPath()});
                valid = false;
            }
        }
    }
    for (std::size_t left = 0; left < result.prefixes.size(); ++left) {
        for (std::size_t right = left + 1; right < result.prefixes.size(); ++right) {
            if (result.prefixes[left].starts_with(result.prefixes[right]) ||
                result.prefixes[right].starts_with(result.prefixes[left])) {
                addV4Error(diagnostics, "chart.animation.mask_conflict",
                           "Property mask prefixes overlap", std::string{reader.fieldPath()});
                valid = false;
            }
        }
    }
    if (!valid) {
        return std::nullopt;
    }
    std::ranges::sort(result.properties);
    std::ranges::sort(result.prefixes);
    return result;
}

[[nodiscard]] auto maskProperties(const PropertyMask& mask) -> std::set<std::string, std::less<>> {
    std::set<std::string, std::less<>> result{mask.properties.begin(), mask.properties.end()};
    for (const auto& prefix : mask.prefixes) {
        for (const auto property : knownAnimationProperties()) {
            if (property.starts_with(prefix)) {
                result.emplace(property);
            }
        }
    }
    return result;
}

[[nodiscard]] auto masksOverlap(const PropertyMask& left, const PropertyMask& right) -> bool {
    const auto leftProperties = maskProperties(left);
    const auto rightProperties = maskProperties(right);
    std::vector<std::string> overlap;
    std::ranges::set_intersection(leftProperties, rightProperties, std::back_inserter(overlap));
    return !overlap.empty();
}

} // namespace cuexis::chart::detail
