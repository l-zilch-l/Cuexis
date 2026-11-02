#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/uuid.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include "canonical_chart_loader_internal.hpp"
#include "canonical_chart_loader_units_internal.hpp"
#include "canonical_chart_template_internal.hpp"
#include "diagnostic_limit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart {

using core::DiagnosticSeverity;

void addDiagnostic(core::Diagnostics& diagnostics, DiagnosticSeverity severity, std::string code,
                   std::string message, std::string path) {
    diagnostics.add(
        core::Diagnostic{severity, std::move(code), std::move(message), std::move(path)});
}

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Error, std::move(code), std::move(message),
                  std::move(path));
}

void addWarning(core::Diagnostics& diagnostics, std::string code, std::string message,
                std::string path) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Warning, std::move(code), std::move(message),
                  std::move(path));
}

void addError(core::Diagnostics& diagnostics, const core::Error& error, std::string path) {
    auto diagnostic = core::Diagnostic{DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(path)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto isChartEntityUuid(std::string_view value) noexcept -> bool {
    return isUuidV7(value) || isUuidV5(value);
}

[[nodiscard]] auto isExtensionId(std::string_view value) noexcept -> bool {
    const auto isAsciiAlphaNumeric = [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9');
    };
    if (value.empty() || !isAsciiAlphaNumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [&](char character) {
        return isAsciiAlphaNumeric(character) || character == '.' || character == '_' ||
               character == '-';
    });
}

[[nodiscard]] auto opaqueJson(const json::Value& value, core::Diagnostics& diagnostics,
                              std::string_view path) -> OpaqueJson {
    auto serialized = json::serialize(value);
    if (!serialized) {
        addError(diagnostics, serialized.error(), std::string{path});
        return {};
    }
    return OpaqueJson{std::move(*serialized)};
}

[[nodiscard]] auto readIdentifier(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics, std::string_view purpose)
    -> std::optional<std::string> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (value->empty() || value->size() > limits.maxIdentifierBytes) {
        addError(diagnostics, "chart.identifier.invalid_length",
                 std::string{purpose} + " has an invalid length", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return std::string{*value};
}

[[nodiscard]] auto readVersion(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<std::uint32_t> {
    const auto version = reader.readInt64();
    if (!version) {
        return std::nullopt;
    }
    if (*version != 1 && *version != 2 && *version != 3) {
        addError(diagnostics, "chart.version.unsupported", "Chart format version is unsupported",
                 std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*version);
}

[[nodiscard]] auto readSupportedComponentVersion(const json::Reader& reader,
                                                 core::Diagnostics& diagnostics) -> bool {
    const auto version = reader.readInt64();
    if (!version) {
        return false;
    }
    if (*version != 1) {
        addError(diagnostics, "chart.version.unsupported", "Component version is unsupported",
                 std::string{reader.fieldPath()});
        return false;
    }
    return true;
}

[[nodiscard]] auto readFloat(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<float> {
    const auto value = reader.readNumber();
    if (!value) {
        return std::nullopt;
    }
    constexpr double floatMax = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(*value) || *value < -floatMax || *value > floatMax) {
        addError(diagnostics, "chart.number.out_of_range",
                 "Chart number must be finite and representable as float",
                 std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return static_cast<float>(*value);
}

[[nodiscard]] auto readVec3(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<core::Vec3> {
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return std::nullopt;
    }
    if (values->size() != 3) {
        addError(diagnostics, "chart.vector.size", "Vector must contain exactly three values",
                 std::string{reader.fieldPath()});
        return std::nullopt;
    }
    std::array<float, 3> result{};
    bool valid = true;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto element = reader.element(index);
        if (!element) {
            valid = false;
            continue;
        }
        const auto value = readFloat(*element, diagnostics);
        if (!value) {
            valid = false;
            continue;
        }
        result[index] = *value;
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
        addError(diagnostics, "chart.quaternion.size",
                 "Quaternion must contain exactly four values", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    std::array<float, 4> result{};
    bool valid = true;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto element = reader.element(index);
        if (!element) {
            valid = false;
            continue;
        }
        const auto value = readFloat(*element, diagnostics);
        if (!value) {
            valid = false;
            continue;
        }
        result[index] = *value;
    }
    if (!valid) {
        return std::nullopt;
    }
    const core::Quat quaternion{result[0], result[1], result[2], result[3]};
    if (!core::isNormalized(quaternion)) {
        addError(diagnostics, "chart.transform.rotation_not_normalized",
                 "Transform quaternion must be normalized", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return quaternion;
}

[[nodiscard]] auto readNullableName(const json::Reader& objectReader, const ChartLimits& limits,
                                    core::Diagnostics& diagnostics) -> std::optional<std::string> {
    const auto nameReader = objectReader.optionalField("name");
    if (!nameReader || nameReader->value().isNull()) {
        return std::nullopt;
    }
    const auto name = nameReader->readString();
    if (!name) {
        return std::nullopt;
    }
    if (name->size() > limits.maxIdentifierBytes) {
        addError(diagnostics, "chart.name.too_long", "Chart display name exceeds the limit",
                 std::string{nameReader->fieldPath()});
        return std::nullopt;
    }
    return std::string{*name};
}

[[nodiscard]] auto readReference(const json::Reader& reader, std::string_view expectedDomain,
                                 const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<std::string> {
    constexpr std::array knownFields{std::string_view{"domain"}, std::string_view{"id"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(knownFields);
    const auto domainReader = reader.requiredField("domain");
    const auto idReader = reader.requiredField("id");
    if (!domainReader || !idReader) {
        return std::nullopt;
    }
    const auto domain = domainReader->readString();
    const auto id = readIdentifier(*idReader, limits, diagnostics, "Reference ID");
    if (!domain || !id) {
        return std::nullopt;
    }
    if (*domain == "external-chart") {
        addError(diagnostics, "chart.reference.external_unsupported",
                 "Cross-chart references are unsupported in chart v1",
                 std::string{domainReader->fieldPath()});
        return std::nullopt;
    }
    if (*domain != expectedDomain) {
        addError(diagnostics, "chart.reference.domain_invalid",
                 "Reference domain is invalid for this field",
                 std::string{domainReader->fieldPath()});
        return std::nullopt;
    }
    if ((expectedDomain == "object" || expectedDomain == "template") && !isChartEntityUuid(*id)) {
        addError(diagnostics, "chart.uuid.invalid_entity_id",
                 "Object and template references require a canonical UUIDv7 or UUIDv5",
                 std::string{idReader->fieldPath()});
        return std::nullopt;
    }
    return id;
}

[[nodiscard]] auto readChartAudio(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics) -> std::optional<ChartAudioData> {
    constexpr std::array knownFields{std::string_view{"version"}, std::string_view{"mainMusic"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(knownFields);
    const auto versionReader = reader.requiredField("version");
    const auto mainMusicReader = reader.requiredField("mainMusic");
    if (!versionReader || !mainMusicReader) {
        return std::nullopt;
    }
    const auto version = versionReader->readInt64();
    const auto mainMusic = readReference(*mainMusicReader, "asset", limits, diagnostics);
    if (version && *version != 1) {
        addError(diagnostics, "chart.audio.version_unsupported",
                 "Chart audio block version is unsupported",
                 std::string{versionReader->fieldPath()});
    }
    if (!version || *version != 1 || !mainMusic) {
        return std::nullopt;
    }
    return ChartAudioData{1, AssetId{std::move(*mainMusic)}};
}

[[nodiscard]] auto readBeat(const json::Reader& reader, const ChartLimits& limits,
                            core::Diagnostics& diagnostics) -> std::optional<RationalBeat> {
    constexpr std::array knownFields{std::string_view{"numerator"},
                                     std::string_view{"denominator"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(knownFields);
    const auto numeratorReader = reader.requiredField("numerator");
    const auto denominatorReader = reader.requiredField("denominator");
    if (!numeratorReader || !denominatorReader) {
        return std::nullopt;
    }
    const auto numerator = numeratorReader->readInt64();
    const auto denominator = denominatorReader->readInt64();
    if (!numerator || !denominator) {
        return std::nullopt;
    }
    auto beat = RationalBeat::create(*numerator, *denominator);
    if (!beat) {
        addError(diagnostics, beat.error(), std::string{reader.fieldPath()});
        return std::nullopt;
    }
    const auto magnitude = beat->numerator() < 0
                               ? static_cast<std::uint64_t>(-(beat->numerator() + 1)) + 1U
                               : static_cast<std::uint64_t>(beat->numerator());
    if (limits.maxBeatNumeratorMagnitude < 0 || limits.maxBeatDenominator <= 0 ||
        magnitude > static_cast<std::uint64_t>(limits.maxBeatNumeratorMagnitude) ||
        beat->denominator() > limits.maxBeatDenominator) {
        addError(diagnostics, "chart.beat.limit_exceeded", "Beat exceeds configured limits",
                 std::string{reader.fieldPath()});
        return std::nullopt;
    }
    if (beat->numerator() != *numerator || beat->denominator() != *denominator) {
        addWarning(diagnostics, "chart.beat.normalized",
                   "Beat was normalized to its canonical rational representation",
                   std::string{reader.fieldPath()});
    }
    return *beat;
}

[[nodiscard]] auto readBehaviorProperty(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<BehaviorProperty> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "transform.position.x") {
        return BehaviorProperty::TransformPositionX;
    }
    if (*value == "transform.position.y") {
        return BehaviorProperty::TransformPositionY;
    }
    if (*value == "transform.position.z") {
        return BehaviorProperty::TransformPositionZ;
    }
    if (*value == "transform.rotation") {
        return BehaviorProperty::TransformRotation;
    }
    if (*value == "transform.scale") {
        return BehaviorProperty::TransformScale;
    }
    if (*value == "camera.fovY") {
        return BehaviorProperty::CameraFovY;
    }
    addError(diagnostics, "chart.behavior.property_invalid",
             "Behavior property is not supported by keyframe version 1",
             std::string{reader.fieldPath()});
    return std::nullopt;
}

[[nodiscard]] auto readBehaviorEasing(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<BehaviorEasing> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "linear") {
        return BehaviorEasing::Linear;
    }
    if (*value == "in_cubic") {
        return BehaviorEasing::InCubic;
    }
    if (*value == "out_cubic") {
        return BehaviorEasing::OutCubic;
    }
    if (*value == "in_out_cubic") {
        return BehaviorEasing::InOutCubic;
    }
    addError(diagnostics, "chart.behavior.easing_invalid",
             "Behavior easing is not supported by keyframe version 1",
             std::string{reader.fieldPath()});
    return std::nullopt;
}

[[nodiscard]] auto readBehaviorValue(const json::Reader& reader, BehaviorProperty property,
                                     core::Diagnostics& diagnostics)
    -> std::optional<BehaviorValue> {
    if (property == BehaviorProperty::TransformRotation) {
        const auto value = readQuat(reader, diagnostics);
        if (!value) {
            return std::nullopt;
        }
        return BehaviorValue{*value};
    }
    if (property == BehaviorProperty::TransformScale ||
        property == BehaviorProperty::MaterialTint) {
        const auto value = readVec3(reader, diagnostics);
        if (!value) {
            return std::nullopt;
        }
        if (property == BehaviorProperty::MaterialTint &&
            (value->x < 0.0F || value->x > 1.0F || value->y < 0.0F || value->y > 1.0F ||
             value->z < 0.0F || value->z > 1.0F)) {
            addError(diagnostics, "chart.behavior.material_tint_out_of_range",
                     "material.tint components must be in [0, 1]", std::string{reader.fieldPath()});
            return std::nullopt;
        }
        return BehaviorValue{*value};
    }

    const auto value = reader.readNumber();
    if (!value || !std::isfinite(*value)) {
        if (value && !std::isfinite(*value)) {
            addError(diagnostics, "chart.behavior.value_non_finite",
                     "Behavior key value must be finite", std::string{reader.fieldPath()});
        }
        return std::nullopt;
    }
    if (property != BehaviorProperty::CameraFovY && property != BehaviorProperty::MaterialOpacity &&
        (*value < -static_cast<double>(std::numeric_limits<float>::max()) ||
         *value > static_cast<double>(std::numeric_limits<float>::max()))) {
        addError(diagnostics, "chart.behavior.value_out_of_range",
                 "Transform scalar value must be representable as a float",
                 std::string{reader.fieldPath()});
        return std::nullopt;
    }
    if (property == BehaviorProperty::CameraFovY && (*value <= 0.0 || *value >= 179.0)) {
        addError(diagnostics, "chart.behavior.camera_fov_out_of_range",
                 "camera.fovY must be strictly between 0 and 179 degrees",
                 std::string{reader.fieldPath()});
        return std::nullopt;
    }
    if (property == BehaviorProperty::MaterialOpacity && (*value < 0.0 || *value > 1.0)) {
        addError(diagnostics, "chart.behavior.material_opacity_out_of_range",
                 "material.opacity must be in [0, 1]", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return BehaviorValue{*value};
}

[[nodiscard]] auto readEventProperty(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<BehaviorProperty> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "transform.position.x") {
        return BehaviorProperty::TransformPositionX;
    }
    if (*value == "transform.position.y") {
        return BehaviorProperty::TransformPositionY;
    }
    if (*value == "transform.position.z") {
        return BehaviorProperty::TransformPositionZ;
    }
    if (*value == "transform.rotation") {
        return BehaviorProperty::TransformRotation;
    }
    if (*value == "transform.scale") {
        return BehaviorProperty::TransformScale;
    }
    if (*value == "camera.fovY") {
        return BehaviorProperty::CameraFovY;
    }
    if (*value == "material.opacity") {
        return BehaviorProperty::MaterialOpacity;
    }
    if (*value == "material.tint") {
        return BehaviorProperty::MaterialTint;
    }
    addError(diagnostics, "chart.behavior.property_invalid",
             "Behavior Event property is unsupported", std::string{reader.fieldPath()});
    return std::nullopt;
}

[[nodiscard]] auto readStepProperty(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<BehaviorStepProperty> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "render.visible") {
        return BehaviorStepProperty::RenderVisible;
    }
    if (*value == "render.material") {
        return BehaviorStepProperty::RenderMaterial;
    }
    addError(diagnostics, "chart.behavior.step_property_invalid",
             "Step Event property is unsupported", std::string{reader.fieldPath()});
    return std::nullopt;
}

[[nodiscard]] auto readGroupId(const json::Reader& reader, const ChartLimits& limits,
                               core::Diagnostics& diagnostics) -> std::optional<std::string> {
    const auto value = readIdentifier(reader, limits, diagnostics, "Behavior group ID");
    if (!value) {
        return std::nullopt;
    }
    if (!isExtensionId(*value)) {
        addError(diagnostics, "chart.behavior.group_id_invalid",
                 "Behavior group ID contains unsupported characters",
                 std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] auto readComponentVersion(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> bool {
    const auto versionReader = reader.requiredField("version");
    return versionReader && readSupportedComponentVersion(*versionReader, diagnostics);
}

} // namespace cuexis::chart
