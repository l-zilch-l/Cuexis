#include "chart_v4_reader_internal.hpp"

#include <cuexis/core/math.hpp>
#include <cuexis_internal/portable_path.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart::detail {
namespace {

[[nodiscard]] auto isAsciiAlphaNumeric(char character) noexcept -> bool {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}

using cuexis::core::detail::foldAscii;
using cuexis::core::detail::isWindowsReservedSegment;

[[nodiscard]] auto knownAnimationProperties() noexcept -> const std::array<std::string_view, 9>& {
    static constexpr std::array values{
        std::string_view{"material.opacity"},     std::string_view{"material.tint"},
        std::string_view{"render.material"},      std::string_view{"render.visible"},
        std::string_view{"transform.position.x"}, std::string_view{"transform.position.y"},
        std::string_view{"transform.position.z"}, std::string_view{"transform.rotation"},
        std::string_view{"transform.scale"}};
    return values;
}

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

} // namespace

void addV4Error(core::Diagnostics& diagnostics, std::string code, std::string message,
                std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(path)});
}

auto readPortableStableId(const json::Reader& reader, const ChartLimits& limits,
                          core::Diagnostics& diagnostics, std::string_view purpose)
    -> std::optional<std::string> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (value->empty() || value->size() > limits.maxIdentifierBytes ||
        !isAsciiAlphaNumeric(value->front()) ||
        !std::ranges::all_of(value->substr(1), [](char character) {
            return isAsciiAlphaNumeric(character) || character == '.' || character == '_' ||
                   character == '-';
        })) {
        addV4Error(diagnostics, "chart.identifier.invalid", std::string{purpose} + " is invalid",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return std::string{*value};
}

auto readV4Rational(const json::Reader& reader, const ChartLimits& limits,
                    core::Diagnostics& diagnostics, bool requirePositive, bool allowNegative)
    -> std::optional<RationalBeat> {
    constexpr std::array fields{std::string_view{"numerator"}, std::string_view{"denominator"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto numeratorReader = reader.requiredField("numerator");
    const auto denominatorReader = reader.requiredField("denominator");
    auto numerator = std::optional<std::int64_t>{};
    auto denominator = std::optional<std::int64_t>{};
    if (numeratorReader) {
        numerator = numeratorReader->readInt64();
    }
    if (denominatorReader) {
        denominator = denominatorReader->readInt64();
    }
    if (!numerator || !denominator) {
        return std::nullopt;
    }
    const auto numeratorValue = *numerator;
    const auto denominatorValue = *denominator;
    if ((!allowNegative && numeratorValue < 0) || (requirePositive && numeratorValue <= 0) ||
        denominatorValue <= 0 || denominatorValue > limits.maxBeatDenominator ||
        numeratorValue < -limits.maxBeatNumeratorMagnitude ||
        numeratorValue > limits.maxBeatNumeratorMagnitude) {
        addV4Error(diagnostics, "chart.beat.out_of_range", "Rational beat is outside the limit",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    auto value = RationalBeat::create(numeratorValue, denominatorValue);
    if (!value) {
        addV4Error(diagnostics, std::string{value.error().code()},
                   std::string{value.error().message()}, std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return *value;
}

auto readRequiredExtensions(const json::Reader& reader, const ChartLimits& limits,
                            core::Diagnostics& diagnostics) -> std::vector<RequiredExtension> {
    std::vector<RequiredExtension> result;
    const auto* items = reader.readArray();
    if (items == nullptr) {
        return result;
    }
    if (items->size() > limits.maxExtensions) {
        addV4Error(diagnostics, "chart.extension.limit", "Required extension count exceeds limit",
                   std::string{reader.fieldPath()});
        return result;
    }
    std::set<std::string, std::less<>> ids;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{std::string_view{"id"}, std::string_view{"version"}};
        item->rejectUnknownFields(fields);
        const auto idReader = item->requiredField("id");
        const auto versionReader = item->requiredField("version");
        const auto id =
            idReader ? readPortableStableId(*idReader, limits, diagnostics, "Required extension ID")
                     : std::nullopt;
        auto version = std::optional<std::uint64_t>{};
        if (versionReader) {
            version = versionReader->readUInt64();
        }
        auto versionValue = std::uint64_t{};
        auto versionValid = false;
        if (version) {
            versionValue = *version;
            versionValid =
                versionValue > 0 && versionValue <= std::numeric_limits<std::uint32_t>::max();
            if (!versionValid) {
                addV4Error(diagnostics, "chart.version.unsupported",
                           "Required extension version is invalid",
                           std::string{versionReader->fieldPath()});
            }
        }
        if (!id || !versionValid) {
            continue;
        }
        if (!ids.emplace(*id).second) {
            addV4Error(diagnostics, "chart.extension.duplicate",
                       "Required extension ID is duplicated", std::string{item->fieldPath()});
            continue;
        }
        result.push_back(RequiredExtension{*id, static_cast<std::uint32_t>(versionValue)});
    }
    std::ranges::sort(result, {}, &RequiredExtension::id);
    return result;
}

auto readBlendMode(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<AnimationBlendMode> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "override") {
        return AnimationBlendMode::Override;
    }
    if (*value == "additive") {
        return AnimationBlendMode::Additive;
    }
    addV4Error(diagnostics, "chart.animation.clip_invalid", "Animation blend mode is unsupported",
               std::string{reader.fieldPath()});
    return std::nullopt;
}

auto readFillMode(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<AnimationFillMode> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "none") {
        return AnimationFillMode::None;
    }
    if (*value == "hold") {
        return AnimationFillMode::Hold;
    }
    addV4Error(diagnostics, "chart.animation.clip_invalid", "Animation fill mode is unsupported",
               std::string{reader.fieldPath()});
    return std::nullopt;
}

auto readIterations(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<AnimationIterations> {
    if (const auto* text = reader.value().string(); text != nullptr) {
        if (*text == "infinite") {
            return AnimationIterations{true, 1};
        }
        addV4Error(diagnostics, "chart.animation.clip_invalid",
                   "Animation iterations must be 1..65535 or infinite",
                   std::string{reader.fieldPath()});
        return std::nullopt;
    }
    if (const auto value = reader.readUInt64()) {
        if (*value == 0 || *value > 65535) {
            addV4Error(diagnostics, "chart.animation.clip_invalid",
                       "Animation iteration count must be within 1..65535",
                       std::string{reader.fieldPath()});
            return std::nullopt;
        }
        return AnimationIterations{false, static_cast<std::uint16_t>(*value)};
    }
    return std::nullopt;
}

auto readAnimationClip(const json::Reader& reader, const ChartLimits& limits,
                       core::Diagnostics& diagnostics, bool requireId, std::string fallbackId)
    -> std::optional<AnimationClip> {
    constexpr std::array fields{std::string_view{"id"}, std::string_view{"version"},
                                std::string_view{"durationBeats"}, std::string_view{"tracks"},
                                std::string_view{"stepTracks"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    std::optional<std::string> id{std::move(fallbackId)};
    if (requireId) {
        const auto idReader = reader.requiredField("id");
        id = idReader ? readPortableStableId(*idReader, limits, diagnostics, "Animation clip ID")
                      : std::nullopt;
    }
    const auto versionReader = reader.requiredField("version");
    const auto durationReader = reader.requiredField("durationBeats");
    const auto tracksReader = reader.requiredField("tracks");
    const auto stepTracksReader = reader.requiredField("stepTracks");
    auto version = std::optional<std::int64_t>{};
    if (versionReader) {
        version = versionReader->readInt64();
    }
    const auto duration = durationReader
                              ? readV4Rational(*durationReader, limits, diagnostics, true, false)
                              : std::nullopt;
    if (version && *version != 1) {
        addV4Error(diagnostics, "chart.version.unsupported",
                   "Animation clip version is unsupported",
                   std::string{versionReader->fieldPath()});
    }
    if (!id || !version || *version != 1 || !duration || !tracksReader || !stepTracksReader) {
        return std::nullopt;
    }
    const auto* tracks = tracksReader->readArray();
    const auto* stepTracks = stepTracksReader->readArray();
    if (tracks == nullptr || stepTracks == nullptr) {
        return std::nullopt;
    }
    if (tracks->empty() && stepTracks->empty()) {
        addV4Error(diagnostics, "chart.animation.clip_invalid",
                   "Animation clip must contain a continuous or step track",
                   std::string{reader.fieldPath()});
    }
    if (tracks->size() > limits.maxAnimationTracksPerClip ||
        stepTracks->size() > limits.maxAnimationTracksPerClip - tracks->size()) {
        addV4Error(diagnostics, "chart.animation.clip_invalid",
                   "Animation track count exceeds the configured limit",
                   std::string{reader.fieldPath()});
    }

    std::vector<AnimationTrack> parsedTracks;
    std::set<AnimationProperty> continuousProperties;
    for (std::size_t trackIndex = 0; trackIndex < tracks->size(); ++trackIndex) {
        const auto trackReader = tracksReader->element(trackIndex);
        if (!trackReader || trackReader->readObject() == nullptr) {
            continue;
        }
        constexpr std::array trackFields{std::string_view{"property"},
                                         std::string_view{"segments"}};
        trackReader->rejectUnknownFields(trackFields);
        const auto propertyReader = trackReader->requiredField("property");
        const auto segmentsReader = trackReader->requiredField("segments");
        const auto propertyText = propertyReader ? propertyReader->readString() : std::nullopt;
        const auto property = propertyText ? readAnimationProperty(*propertyText) : std::nullopt;
        if (propertyText && !property) {
            addV4Error(diagnostics, "chart.animation.clip_invalid",
                       "Animation property is unsupported",
                       std::string{propertyReader->fieldPath()});
        }
        if (!property || !segmentsReader) {
            continue;
        }
        if (!continuousProperties.emplace(*property).second) {
            addV4Error(diagnostics, "chart.animation.track_conflict",
                       "Animation clip contains duplicate property tracks",
                       std::string{propertyReader->fieldPath()});
        }
        const auto* segments = segmentsReader->readArray();
        if (segments == nullptr) {
            continue;
        }
        if (segments->empty() || segments->size() > limits.maxAnimationSegmentsOrStepsPerTrack) {
            addV4Error(diagnostics, "chart.animation.clip_invalid",
                       "Animation segment count is invalid",
                       std::string{segmentsReader->fieldPath()});
        }
        std::vector<AnimationSegment> parsedSegments;
        for (std::size_t segmentIndex = 0; segmentIndex < segments->size(); ++segmentIndex) {
            const auto segmentReader = segmentsReader->element(segmentIndex);
            if (!segmentReader || segmentReader->readObject() == nullptr) {
                continue;
            }
            constexpr std::array segmentFields{
                std::string_view{"startBeat"},  std::string_view{"durationBeats"},
                std::string_view{"startValue"}, std::string_view{"endValue"},
                std::string_view{"startSlope"}, std::string_view{"endSlope"}};
            segmentReader->rejectUnknownFields(segmentFields);
            const auto startReader = segmentReader->requiredField("startBeat");
            const auto lengthReader = segmentReader->requiredField("durationBeats");
            const auto startValueReader = segmentReader->requiredField("startValue");
            const auto endValueReader = segmentReader->requiredField("endValue");
            const auto startSlopeReader = segmentReader->requiredField("startSlope");
            const auto endSlopeReader = segmentReader->requiredField("endSlope");
            const auto start = startReader
                                   ? readV4Rational(*startReader, limits, diagnostics, false, false)
                                   : std::nullopt;
            const auto length =
                lengthReader ? readV4Rational(*lengthReader, limits, diagnostics, false, false)
                             : std::nullopt;
            const auto startValue =
                startValueReader ? readAnimationValue(*startValueReader, *property, diagnostics)
                                 : std::nullopt;
            const auto endValue = endValueReader
                                      ? readAnimationValue(*endValueReader, *property, diagnostics)
                                      : std::nullopt;
            auto startSlope = std::optional<double>{};
            auto endSlope = std::optional<double>{};
            if (startSlopeReader) {
                startSlope = readFiniteNumber(*startSlopeReader, diagnostics);
            }
            if (endSlopeReader) {
                endSlope = readFiniteNumber(*endSlopeReader, diagnostics);
            }
            if (startSlope && endSlope &&
                (*startSlope < 0.0 || *endSlope < 0.0 || *startSlope + *endSlope > 3.0)) {
                addV4Error(diagnostics, "chart.animation.clip_invalid",
                           "Animation slopes must be non-negative and sum to at most 3",
                           std::string{segmentReader->fieldPath()});
            }
            if (!start || !length || !startValue || !endValue || !startSlope || !endSlope) {
                continue;
            }
            if (length->numerator() == 0 &&
                (*startValue != *endValue || *startSlope != 0.0 || *endSlope != 0.0)) {
                addV4Error(diagnostics, "chart.animation.clip_invalid",
                           "Zero-duration animation segments require equal values and zero slopes",
                           std::string{segmentReader->fieldPath()});
            }
            const auto end = addRationalBeats(*start, *length);
            if (!end || *end > *duration) {
                addV4Error(diagnostics, "chart.animation.clip_invalid",
                           "Animation segment exceeds clip duration",
                           std::string{segmentReader->fieldPath()});
            }
            parsedSegments.push_back(AnimationSegment{*start, *length, *startValue, *endValue,
                                                      *startSlope, *endSlope,
                                                      std::string{segmentReader->fieldPath()}});
        }
        std::ranges::sort(parsedSegments, {}, &AnimationSegment::startBeat);
        for (std::size_t index = 1; index < parsedSegments.size(); ++index) {
            const auto previousEnd = addRationalBeats(parsedSegments[index - 1].startBeat,
                                                      parsedSegments[index - 1].durationBeats);
            if (!previousEnd ||
                parsedSegments[index].startBeat == parsedSegments[index - 1].startBeat ||
                parsedSegments[index].startBeat < *previousEnd) {
                addV4Error(diagnostics, "chart.animation.track_conflict",
                           "Animation segments overlap or share an invalid start beat",
                           parsedSegments[index].fieldPath);
            }
        }
        parsedTracks.push_back(AnimationTrack{*property, std::move(parsedSegments),
                                              std::string{trackReader->fieldPath()}});
    }

    std::vector<AnimationStepTrack> parsedStepTracks;
    std::set<AnimationStepProperty> discreteProperties;
    for (std::size_t trackIndex = 0; trackIndex < stepTracks->size(); ++trackIndex) {
        const auto trackReader = stepTracksReader->element(trackIndex);
        if (!trackReader || trackReader->readObject() == nullptr) {
            continue;
        }
        constexpr std::array trackFields{std::string_view{"property"}, std::string_view{"steps"}};
        trackReader->rejectUnknownFields(trackFields);
        const auto propertyReader = trackReader->requiredField("property");
        const auto stepsReader = trackReader->requiredField("steps");
        const auto propertyText = propertyReader ? propertyReader->readString() : std::nullopt;
        std::optional<AnimationStepProperty> property;
        if (propertyText && *propertyText == "render.visible") {
            property = AnimationStepProperty::RenderVisible;
        } else if (propertyText && *propertyText == "render.material") {
            property = AnimationStepProperty::RenderMaterial;
        } else if (propertyText) {
            addV4Error(diagnostics, "chart.animation.clip_invalid",
                       "Animation step property is unsupported",
                       std::string{propertyReader->fieldPath()});
        }
        if (!property || !stepsReader) {
            continue;
        }
        if (!discreteProperties.emplace(*property).second) {
            addV4Error(diagnostics, "chart.animation.track_conflict",
                       "Animation clip contains duplicate step tracks",
                       std::string{propertyReader->fieldPath()});
        }
        const auto* steps = stepsReader->readArray();
        if (steps == nullptr) {
            continue;
        }
        if (steps->empty() || steps->size() > limits.maxAnimationSegmentsOrStepsPerTrack) {
            addV4Error(diagnostics, "chart.animation.clip_invalid",
                       "Animation step count is invalid", std::string{stepsReader->fieldPath()});
        }
        std::vector<AnimationStep> parsedSteps;
        for (std::size_t stepIndex = 0; stepIndex < steps->size(); ++stepIndex) {
            const auto stepReader = stepsReader->element(stepIndex);
            if (!stepReader || stepReader->readObject() == nullptr) {
                continue;
            }
            constexpr std::array stepFields{std::string_view{"beat"}, std::string_view{"value"}};
            stepReader->rejectUnknownFields(stepFields);
            const auto beatReader = stepReader->requiredField("beat");
            const auto valueReader = stepReader->requiredField("value");
            const auto beat = beatReader
                                  ? readV4Rational(*beatReader, limits, diagnostics, false, false)
                                  : std::nullopt;
            std::optional<AnimationStepValue> value;
            if (valueReader && *property == AnimationStepProperty::RenderVisible) {
                if (const auto visible = valueReader->readBoolean()) {
                    value = AnimationStepValue{*visible};
                }
            } else if (valueReader) {
                if (const auto asset = readAssetReference(*valueReader, limits, diagnostics)) {
                    value = AnimationStepValue{*asset};
                }
            }
            if (!beat || !value) {
                continue;
            }
            if (*beat > *duration) {
                addV4Error(diagnostics, "chart.animation.clip_invalid",
                           "Animation step exceeds clip duration",
                           std::string{stepReader->fieldPath()});
            }
            parsedSteps.push_back(
                AnimationStep{*beat, *value, std::string{stepReader->fieldPath()}});
        }
        std::ranges::sort(parsedSteps, {}, &AnimationStep::beat);
        for (std::size_t index = 1; index < parsedSteps.size(); ++index) {
            if (parsedSteps[index - 1].beat == parsedSteps[index].beat) {
                addV4Error(diagnostics, "chart.animation.track_conflict",
                           "Animation steps share the same beat", parsedSteps[index].fieldPath);
            }
        }
        parsedStepTracks.push_back(AnimationStepTrack{*property, std::move(parsedSteps),
                                                      std::string{trackReader->fieldPath()}});
    }

    std::ranges::sort(parsedTracks, {}, &AnimationTrack::property);
    std::ranges::sort(parsedStepTracks, {}, &AnimationStepTrack::property);

    return AnimationClip{*id, *duration, std::move(parsedTracks), std::move(parsedStepTracks),
                         std::string{reader.fieldPath()}};
}

auto readAnimatorComponent(const json::Reader& reader, const ChartLimits& limits,
                           core::Diagnostics& diagnostics, std::vector<ParameterUse>& parameterUses)
    -> std::optional<AnimatorComponent> {
    constexpr std::array fields{std::string_view{"version"}, std::string_view{"templateBindings"},
                                std::string_view{"layers"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto versionReader = reader.requiredField("version");
    const auto bindingsReader = reader.requiredField("templateBindings");
    const auto layersReader = reader.requiredField("layers");
    auto version = std::optional<std::int64_t>{};
    if (versionReader) {
        version = versionReader->readInt64();
    }
    if (version && *version != 1) {
        addV4Error(diagnostics, "chart.version.unsupported", "Animator version is unsupported",
                   std::string{versionReader->fieldPath()});
    }
    if (!version || *version != 1 || !bindingsReader || !layersReader) {
        return std::nullopt;
    }
    const auto* bindings = bindingsReader->readArray();
    const auto* layers = layersReader->readArray();
    if (bindings == nullptr || layers == nullptr) {
        return std::nullopt;
    }
    if (bindings->size() > limits.maxTemplateBindingsPerAnimator ||
        layers->size() > limits.maxAnimationLayersPerAnimator) {
        addV4Error(diagnostics, "chart.animation.generated_limit",
                   "Animator record count exceeds the configured limit",
                   std::string{reader.fieldPath()});
    }

    std::vector<TemplateBinding> parsedBindings;
    std::set<std::string, std::less<>> bindingIds;
    for (std::size_t index = 0; index < bindings->size(); ++index) {
        const auto item = bindingsReader->element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array bindingFields{
            std::string_view{"bindingId"}, std::string_view{"template"},
            std::string_view{"startBeat"}, std::string_view{"durationScale"},
            std::string_view{"weight"},    std::string_view{"priority"}};
        item->rejectUnknownFields(bindingFields);
        const auto idReader = item->requiredField("bindingId");
        const auto templateReader = item->requiredField("template");
        const auto startReader = item->requiredField("startBeat");
        const auto durationReader = item->requiredField("durationScale");
        const auto weightReader = item->requiredField("weight");
        const auto priorityReader = item->requiredField("priority");
        const auto id = idReader
                            ? readPortableStableId(*idReader, limits, diagnostics, "Binding ID")
                            : std::nullopt;
        std::optional<std::string> templateId;
        if (templateReader && templateReader->readObject() != nullptr) {
            constexpr std::array referenceFields{std::string_view{"domain"},
                                                 std::string_view{"id"}};
            templateReader->rejectUnknownFields(referenceFields);
            const auto domainReader = templateReader->requiredField("domain");
            const auto referenceIdReader = templateReader->requiredField("id");
            const auto domain = domainReader ? domainReader->readString() : std::nullopt;
            templateId = referenceIdReader
                             ? readPortableStableId(*referenceIdReader, limits, diagnostics,
                                                    "Animation template reference")
                             : std::nullopt;
            if (domain && *domain != "animation-template") {
                addV4Error(diagnostics, "chart.animation.template_reference_missing",
                           "Template binding must use the animation-template domain",
                           std::string{domainReader->fieldPath()});
            }
        }
        const auto start = startReader
                               ? readV4Rational(*startReader, limits, diagnostics, false, true)
                               : std::nullopt;
        const auto duration =
            durationReader ? readRationalSource(*durationReader, limits, diagnostics, parameterUses)
                           : std::nullopt;
        const auto weight =
            weightReader ? readWeightSource(*weightReader, limits, diagnostics, parameterUses)
                         : std::nullopt;
        auto priority = std::optional<std::int64_t>{};
        if (priorityReader) {
            priority = priorityReader->readInt64();
        }
        if (!id || !templateId || !start || !duration || !weight || !priority) {
            continue;
        }
        if (!bindingIds.emplace(*id).second) {
            addV4Error(diagnostics, "chart.animation.template_binding_conflict",
                       "Template binding ID is duplicated", std::string{idReader->fieldPath()});
        }
        parsedBindings.push_back(TemplateBinding{*id, *templateId, *start, *duration, *weight,
                                                 *priority, std::string{item->fieldPath()}});
    }

    std::vector<AnimationLayer> parsedLayers;
    std::set<std::string, std::less<>> layerIds;
    for (std::size_t layerIndex = 0; layerIndex < layers->size(); ++layerIndex) {
        const auto layerReader = layersReader->element(layerIndex);
        if (!layerReader || layerReader->readObject() == nullptr) {
            continue;
        }
        constexpr std::array layerFields{
            std::string_view{"layerId"}, std::string_view{"priority"}, std::string_view{"weight"},
            std::string_view{"propertyMask"}, std::string_view{"blendGroups"}};
        layerReader->rejectUnknownFields(layerFields);
        const auto idReader = layerReader->requiredField("layerId");
        const auto priorityReader = layerReader->requiredField("priority");
        const auto weightReader = layerReader->requiredField("weight");
        const auto maskReader = layerReader->requiredField("propertyMask");
        const auto groupsReader = layerReader->requiredField("blendGroups");
        const auto id = idReader ? readPortableStableId(*idReader, limits, diagnostics, "Layer ID")
                                 : std::nullopt;
        auto priority = std::optional<std::int64_t>{};
        if (priorityReader) {
            priority = priorityReader->readInt64();
        }
        const auto weight =
            weightReader ? readWeightSource(*weightReader, limits, diagnostics, parameterUses)
                         : std::nullopt;
        const auto mask =
            maskReader ? readPropertyMask(*maskReader, limits, diagnostics) : std::nullopt;
        if (!id || !priority || !weight || !mask || !groupsReader) {
            continue;
        }
        if (!layerIds.emplace(*id).second) {
            addV4Error(diagnostics, "chart.animation.mask_conflict", "Layer ID is duplicated",
                       std::string{idReader->fieldPath()});
        }
        const auto* groups = groupsReader->readArray();
        if (groups == nullptr) {
            continue;
        }
        if (groups->size() > limits.maxBlendGroupsPerLayer) {
            addV4Error(diagnostics, "chart.animation.generated_limit",
                       "Blend group count exceeds the configured limit",
                       std::string{groupsReader->fieldPath()});
        }
        std::vector<BlendGroup> parsedGroups;
        std::set<std::string, std::less<>> groupIds;
        for (std::size_t groupIndex = 0; groupIndex < groups->size(); ++groupIndex) {
            const auto groupReader = groupsReader->element(groupIndex);
            if (!groupReader || groupReader->readObject() == nullptr) {
                continue;
            }
            constexpr std::array groupFields{std::string_view{"groupId"}, std::string_view{"mode"},
                                             std::string_view{"weight"},
                                             std::string_view{"instances"}};
            groupReader->rejectUnknownFields(groupFields);
            const auto groupIdReader = groupReader->requiredField("groupId");
            const auto modeReader = groupReader->requiredField("mode");
            const auto groupWeightReader = groupReader->requiredField("weight");
            const auto instancesReader = groupReader->requiredField("instances");
            const auto groupId = groupIdReader ? readPortableStableId(*groupIdReader, limits,
                                                                      diagnostics, "Blend group ID")
                                               : std::nullopt;
            auto mode = std::optional<AnimationBlendMode>{};
            if (modeReader) {
                mode = readBlendMode(*modeReader, diagnostics);
            }
            const auto groupWeight =
                groupWeightReader
                    ? readWeightSource(*groupWeightReader, limits, diagnostics, parameterUses)
                    : std::nullopt;
            if (!groupId || !mode || !groupWeight || !instancesReader) {
                continue;
            }
            if (!groupIds.emplace(*groupId).second) {
                addV4Error(diagnostics, "chart.animation.track_conflict",
                           "Blend group ID is duplicated", std::string{groupIdReader->fieldPath()});
            }
            const auto* instances = instancesReader->readArray();
            if (instances == nullptr) {
                continue;
            }
            if (instances->size() > limits.maxClipInstancesPerBlendGroup) {
                addV4Error(diagnostics, "chart.animation.generated_limit",
                           "Clip instance count exceeds the configured limit",
                           std::string{instancesReader->fieldPath()});
            }
            std::vector<ClipInstance> parsedInstances;
            std::set<std::string, std::less<>> instanceIds;
            for (std::size_t instanceIndex = 0; instanceIndex < instances->size();
                 ++instanceIndex) {
                const auto instanceReader = instancesReader->element(instanceIndex);
                if (!instanceReader || instanceReader->readObject() == nullptr) {
                    continue;
                }
                constexpr std::array instanceFields{
                    std::string_view{"instanceId"},  std::string_view{"clip"},
                    std::string_view{"startBeat"},   std::string_view{"iterations"},
                    std::string_view{"fillMode"},    std::string_view{"weight"},
                    std::string_view{"propertyMask"}};
                instanceReader->rejectUnknownFields(instanceFields);
                const auto instanceIdReader = instanceReader->requiredField("instanceId");
                const auto clipReader = instanceReader->requiredField("clip");
                const auto instanceStartReader = instanceReader->requiredField("startBeat");
                const auto iterationsReader = instanceReader->requiredField("iterations");
                const auto fillReader = instanceReader->requiredField("fillMode");
                const auto instanceWeightReader = instanceReader->requiredField("weight");
                const auto instanceMaskReader = instanceReader->requiredField("propertyMask");
                const auto instanceId = instanceIdReader
                                            ? readPortableStableId(*instanceIdReader, limits,
                                                                   diagnostics, "Clip instance ID")
                                            : std::nullopt;
                std::optional<std::string> clipId;
                if (clipReader && clipReader->readObject() != nullptr) {
                    constexpr std::array referenceFields{std::string_view{"domain"},
                                                         std::string_view{"id"}};
                    clipReader->rejectUnknownFields(referenceFields);
                    const auto domainReader = clipReader->requiredField("domain");
                    const auto referenceIdReader = clipReader->requiredField("id");
                    const auto domain = domainReader ? domainReader->readString() : std::nullopt;
                    clipId = referenceIdReader
                                 ? readPortableStableId(*referenceIdReader, limits, diagnostics,
                                                        "Animation reference")
                                 : std::nullopt;
                    if (domain && *domain != "animation") {
                        addV4Error(diagnostics, "chart.animation.reference_missing",
                                   "Clip instance must use the animation domain",
                                   std::string{domainReader->fieldPath()});
                    }
                }
                const auto instanceStart =
                    instanceStartReader
                        ? readV4Rational(*instanceStartReader, limits, diagnostics, false, true)
                        : std::nullopt;
                auto iterations = std::optional<AnimationIterations>{};
                auto fill = std::optional<AnimationFillMode>{};
                if (iterationsReader) {
                    iterations = readIterations(*iterationsReader, diagnostics);
                }
                if (fillReader) {
                    fill = readFillMode(*fillReader, diagnostics);
                }
                const auto instanceWeight = instanceWeightReader
                                                ? readWeightSource(*instanceWeightReader, limits,
                                                                   diagnostics, parameterUses)
                                                : std::nullopt;
                const auto instanceMask =
                    instanceMaskReader ? readPropertyMask(*instanceMaskReader, limits, diagnostics)
                                       : std::nullopt;
                if (!instanceId || !clipId || !instanceStart || !iterations || !fill ||
                    !instanceWeight || !instanceMask) {
                    continue;
                }
                auto instance =
                    ClipInstance{*instanceId,    *clipId,
                                 *instanceStart, *iterations,
                                 *fill,          *instanceWeight,
                                 *instanceMask,  std::string{instanceReader->fieldPath()}};
                if (instance.iterations.infinite && instance.fillMode != AnimationFillMode::None) {
                    addV4Error(diagnostics, "chart.animation.clip_invalid",
                               "Infinite clip instance must use fillMode none",
                               std::string{fillReader->fieldPath()});
                }
                if (!instanceIds.emplace(*instanceId).second) {
                    addV4Error(diagnostics, "chart.animation.track_conflict",
                               "Clip instance ID is duplicated",
                               std::string{instanceIdReader->fieldPath()});
                }
                parsedInstances.push_back(std::move(instance));
            }
            std::ranges::sort(parsedInstances, {}, &ClipInstance::instanceId);
            parsedGroups.push_back(BlendGroup{*groupId, *mode, *groupWeight,
                                              std::move(parsedInstances),
                                              std::string{groupReader->fieldPath()}});
        }
        std::ranges::sort(parsedGroups, {}, &BlendGroup::groupId);
        parsedLayers.push_back(AnimationLayer{*id, *priority, *weight, *mask,
                                              std::move(parsedGroups),
                                              std::string{layerReader->fieldPath()}});
    }
    for (std::size_t left = 0; left < parsedLayers.size(); ++left) {
        for (std::size_t right = left + 1; right < parsedLayers.size(); ++right) {
            if (parsedLayers[left].priority == parsedLayers[right].priority &&
                masksOverlap(parsedLayers[left].propertyMask, parsedLayers[right].propertyMask)) {
                addV4Error(diagnostics, "chart.animation.mask_conflict",
                           "Layers with the same priority have overlapping masks",
                           parsedLayers[right].fieldPath);
            }
        }
    }
    std::ranges::sort(parsedBindings, {}, &TemplateBinding::bindingId);
    std::ranges::sort(parsedLayers, {}, &AnimationLayer::layerId);
    return AnimatorComponent{std::move(parsedBindings), std::move(parsedLayers)};
}

auto isPortableProjectPath(std::string_view path) noexcept -> bool {
    if (path.empty() || path.size() > 4096 || path.front() == '/' || path.back() == '/' ||
        path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos ||
        path.find("//") != std::string_view::npos) {
        return false;
    }
    std::size_t segmentStart = 0;
    while (segmentStart < path.size()) {
        const auto separator = path.find('/', segmentStart);
        const auto segment = path.substr(segmentStart, separator - segmentStart);
        if (segment.empty() || segment == "." || segment == ".." || segment.back() == ' ' ||
            segment.back() == '.' || isWindowsReservedSegment(segment)) {
            return false;
        }
        if (!std::ranges::all_of(segment, [](char character) {
                return isAsciiAlphaNumeric(character) || character == '.' || character == '_' ||
                       character == '-';
            })) {
            return false;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        segmentStart = separator + 1;
    }
    return true;
}

auto isCxtProjectPath(std::string_view path) noexcept -> bool {
    return isPortableProjectPath(path) && path.ends_with(".cxt");
}

auto portableProjectPathCaseKey(std::string_view path) -> std::string {
    return foldAscii(path);
}

} // namespace cuexis::chart::detail
