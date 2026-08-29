//  CanonicalChartLoader 实现 — 方案 A 规范谱面 typed-reader
//  JSON parse → typed Reader 读取顶层/object/template/behavior/component 结构
//  → UUID 校验 → 语义校验 → 生成 ChartDocument
//  当前不调用 JSON Schema validator；结构权威是 typed Reader + 代码语义校验

#include <cuexis/chart/canonical_chart_loader.hpp>

#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/uuid.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include "canonical_chart_loader_internal.hpp"
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
namespace {

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

[[nodiscard]] auto parseComponents(const json::Value& value, std::string path,
                                   const ChartLimits& limits, core::Diagnostics& diagnostics,
                                   bool concreteObject) -> std::optional<ObjectComponents> {
    json::Reader reader{value, diagnostics, std::move(path)};
    const auto* object = reader.readObject();
    if (object == nullptr) {
        return std::nullopt;
    }
    if (object->empty()) {
        addError(diagnostics, "chart.components.empty", "Object components cannot be empty",
                 std::string{reader.fieldPath()});
    }
    constexpr std::array knownFields{
        std::string_view{"cuexis.transform"}, std::string_view{"cuexis.renderable"},
        std::string_view{"cuexis.behavior"},  std::string_view{"cuexis.note"},
        std::string_view{"cuexis.element"},   std::string_view{"cuexis.camera"}};
    reader.rejectUnknownFields(knownFields);

    ObjectComponents components;
    if (const auto component = reader.optionalField("cuexis.transform")) {
        constexpr std::array fields{std::string_view{"version"}, std::string_view{"position"},
                                    std::string_view{"rotation"}, std::string_view{"scale"}};
        component->rejectUnknownFields(fields);
        const auto positionReader = component->requiredField("position");
        const auto rotationReader = component->requiredField("rotation");
        const auto scaleReader = component->requiredField("scale");
        const bool versionValid = readComponentVersion(*component, diagnostics);
        const auto position =
            positionReader ? readVec3(*positionReader, diagnostics) : std::nullopt;
        const auto rotation =
            rotationReader ? readQuat(*rotationReader, diagnostics) : std::nullopt;
        const auto scale = scaleReader ? readVec3(*scaleReader, diagnostics) : std::nullopt;
        if (versionValid && position && rotation && scale) {
            components.transform = TransformData{*position, *rotation, *scale};
        }
    }

    if (const auto component = reader.optionalField("cuexis.renderable")) {
        constexpr std::array fields{std::string_view{"version"}, std::string_view{"mesh"},
                                    std::string_view{"material"}};
        component->rejectUnknownFields(fields);
        const auto meshReader = component->requiredField("mesh");
        const auto materialReader = component->requiredField("material");
        const bool versionValid = readComponentVersion(*component, diagnostics);
        const auto mesh =
            meshReader ? readReference(*meshReader, "asset", limits, diagnostics) : std::nullopt;
        const auto material = materialReader
                                  ? readReference(*materialReader, "asset", limits, diagnostics)
                                  : std::nullopt;
        if (versionValid && mesh && material) {
            components.renderable = RenderableData{AssetId{*mesh}, AssetId{*material}};
        }
    }

    if (const auto component = reader.optionalField("cuexis.behavior")) {
        constexpr std::array fields{std::string_view{"version"}, std::string_view{"behavior"}};
        component->rejectUnknownFields(fields);
        const auto behaviorReader = component->requiredField("behavior");
        const bool versionValid = readComponentVersion(*component, diagnostics);
        const auto behavior = behaviorReader
                                  ? readReference(*behaviorReader, "behavior", limits, diagnostics)
                                  : std::nullopt;
        if (versionValid && behavior) {
            components.behavior = BehaviorReferenceData{BehaviorId{*behavior}};
        }
    }

    if (const auto component = reader.optionalField("cuexis.note")) {
        constexpr std::array fields{std::string_view{"version"}, std::string_view{"beat"}};
        component->rejectUnknownFields(fields);
        const bool versionValid = readComponentVersion(*component, diagnostics);
        const auto beatReader = component->optionalField("beat");
        const auto beat = beatReader ? readBeat(*beatReader, limits, diagnostics) : std::nullopt;
        if (concreteObject && !beatReader) {
            addError(diagnostics, "chart.note.beat_missing", "Concrete note object requires a beat",
                     json::appendFieldPath(component->fieldPath(), "beat"));
        }
        if (versionValid && (!concreteObject || beat)) {
            components.note = NoteData{beat};
        }
    }

    if (const auto component = reader.optionalField("cuexis.element")) {
        constexpr std::array fields{std::string_view{"version"}};
        component->rejectUnknownFields(fields);
        if (readComponentVersion(*component, diagnostics)) {
            components.element = true;
        }
    }

    if (const auto component = reader.optionalField("cuexis.camera")) {
        constexpr std::array fields{std::string_view{"version"}, std::string_view{"type"},
                                    std::string_view{"fovY"}, std::string_view{"near"},
                                    std::string_view{"far"}};
        component->rejectUnknownFields(fields);
        const bool versionValid = readComponentVersion(*component, diagnostics);
        const auto typeReader = component->requiredField("type");
        const auto fovReader = component->requiredField("fovY");
        const auto nearReader = component->requiredField("near");
        const auto farReader = component->requiredField("far");
        if (versionValid) {
            CameraComponentData cameraData;
            bool cameraValid = true;
            if (typeReader) {
                const auto type = typeReader->readString();
                if (type && *type == "perspective") {
                    cameraData.type = *type;
                } else if (type) {
                    addError(diagnostics, "chart.camera.unsupported_type",
                             "Unsupported camera projection type; expected 'perspective'",
                             std::string{typeReader->fieldPath()});
                    cameraValid = false;
                } else {
                    cameraValid = false;
                }
            } else {
                cameraValid = false;
            }
            if (fovReader) {
                const auto fov = fovReader->readNumber();
                if (fov && std::isfinite(*fov) && *fov > 0.0 && *fov < 179.0) {
                    cameraData.fovY = *fov;
                } else if (fov) {
                    addError(diagnostics, "chart.camera.invalid_fov",
                             "Camera FOV must be finite and in (0, 179) degrees",
                             std::string{fovReader->fieldPath()});
                    cameraValid = false;
                } else {
                    cameraValid = false;
                }
            } else {
                cameraValid = false;
            }
            if (nearReader) {
                const auto nearPlane = nearReader->readNumber();
                if (nearPlane && std::isfinite(*nearPlane) && *nearPlane > 0.0) {
                    cameraData.nearPlane = *nearPlane;
                } else if (nearPlane) {
                    addError(diagnostics, "chart.camera.invalid_near",
                             "Camera near plane must be finite and positive",
                             std::string{nearReader->fieldPath()});
                    cameraValid = false;
                } else {
                    cameraValid = false;
                }
            } else {
                cameraValid = false;
            }
            if (farReader) {
                const auto farPlane = farReader->readNumber();
                if (farPlane && std::isfinite(*farPlane) && *farPlane > 0.0) {
                    cameraData.farPlane = *farPlane;
                } else if (farPlane) {
                    addError(diagnostics, "chart.camera.invalid_far",
                             "Camera far plane must be finite and positive",
                             std::string{farReader->fieldPath()});
                    cameraValid = false;
                } else {
                    cameraValid = false;
                }
            } else {
                cameraValid = false;
            }
            if (cameraData.nearPlane >= cameraData.farPlane) {
                addError(diagnostics, "chart.camera.near_exceeds_far",
                         "Camera near plane must be less than far plane",
                         std::string{component->fieldPath()});
                cameraValid = false;
            }
            if (cameraValid) {
                components.camera = std::move(cameraData);
            }
        }
    }

    return components;
}

[[nodiscard]] auto parseBehaviors(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics) -> std::vector<ChartBehavior> {
    std::vector<ChartBehavior> result;
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return result;
    }
    if (values->size() > limits.maxBehaviors) {
        addError(diagnostics, "chart.limit.behaviors", "Behavior count exceeds configured limit",
                 std::string{reader.fieldPath()});
        return result;
    }
    std::set<std::string> ids;
    std::size_t totalKeys = 0;
    for (std::size_t index = 0; index < values->size(); ++index) {
        if (diagnostics.limitReached()) {
            break;
        }
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array knownFields{std::string_view{"id"}, std::string_view{"type"},
                                         std::string_view{"version"}, std::string_view{"tracks"}};
        item->rejectUnknownFields(knownFields);
        const auto idReader = item->requiredField("id");
        const auto typeReader = item->requiredField("type");
        const auto versionReader = item->requiredField("version");
        const auto tracksReader = item->requiredField("tracks");
        if (!idReader || !typeReader || !versionReader || !tracksReader) {
            continue;
        }
        const auto id = readIdentifier(*idReader, limits, diagnostics, "Behavior ID");
        const auto type = typeReader->readString();
        const bool versionValid = readSupportedComponentVersion(*versionReader, diagnostics);
        const auto* tracks = tracksReader->readArray();
        if (!id || !type || !versionValid || tracks == nullptr) {
            continue;
        }
        if (!ids.insert(*id).second) {
            addError(diagnostics, "chart.behavior.id_duplicate", "Behavior ID must be unique",
                     std::string{idReader->fieldPath()});
            continue;
        }
        if (*type != "behavior.transform.keyframe") {
            addError(diagnostics, "chart.behavior.type_unsupported",
                     "Behavior type is unsupported for this chart version",
                     std::string{typeReader->fieldPath()});
            continue;
        }
        if (tracks->size() > limits.maxTracksPerBehavior) {
            addError(diagnostics, "chart.limit.behavior_tracks",
                     "Behavior track count exceeds configured limit",
                     std::string{tracksReader->fieldPath()});
            continue;
        }

        std::vector<BehaviorTrack> typedTracks;
        typedTracks.reserve(tracks->size());
        std::set<BehaviorProperty> properties;
        bool behaviorValid = true;
        for (std::size_t trackIndex = 0; trackIndex < tracks->size(); ++trackIndex) {
            const auto trackReader = tracksReader->element(trackIndex);
            if (!trackReader || trackReader->readObject() == nullptr) {
                behaviorValid = false;
                continue;
            }
            constexpr std::array trackFields{std::string_view{"property"},
                                             std::string_view{"keys"}};
            trackReader->rejectUnknownFields(trackFields);
            const auto propertyReader = trackReader->requiredField("property");
            const auto keysReader = trackReader->requiredField("keys");
            if (!propertyReader || !keysReader) {
                behaviorValid = false;
                continue;
            }
            const auto property = readBehaviorProperty(*propertyReader, diagnostics);
            const auto* keys = keysReader->readArray();
            if (!property || keys == nullptr) {
                behaviorValid = false;
                continue;
            }
            if (!properties.insert(*property).second) {
                addError(diagnostics, "chart.behavior.property_duplicate",
                         "A Behavior may write each v1 property at most once",
                         std::string{propertyReader->fieldPath()});
                behaviorValid = false;
            }
            if (keys->empty()) {
                addError(diagnostics, "chart.behavior.keys_empty",
                         "A Behavior Track must contain at least one key",
                         std::string{keysReader->fieldPath()});
                behaviorValid = false;
                continue;
            }
            if (keys->size() > limits.maxKeysPerTrack ||
                keys->size() > limits.maxTotalBehaviorKeys ||
                totalKeys > limits.maxTotalBehaviorKeys - keys->size()) {
                addError(diagnostics, "chart.limit.behavior_keys",
                         "Behavior key count exceeds configured limit",
                         std::string{keysReader->fieldPath()});
                behaviorValid = false;
                continue;
            }
            totalKeys += keys->size();

            BehaviorTrack typedTrack{.property = *property, .keys = {}};
            typedTrack.keys.reserve(keys->size());
            bool trackValid = true;
            for (std::size_t keyIndex = 0; keyIndex < keys->size(); ++keyIndex) {
                const auto keyReader = keysReader->element(keyIndex);
                if (!keyReader || keyReader->readObject() == nullptr) {
                    trackValid = false;
                    continue;
                }
                constexpr std::array keyFields{std::string_view{"beat"}, std::string_view{"value"},
                                               std::string_view{"easing"}};
                keyReader->rejectUnknownFields(keyFields);
                const auto beatReader = keyReader->requiredField("beat");
                const auto valueReader = keyReader->requiredField("value");
                if (!beatReader || !valueReader) {
                    trackValid = false;
                    continue;
                }
                const auto beat = readBeat(*beatReader, limits, diagnostics);
                const auto value = readBehaviorValue(*valueReader, *property, diagnostics);
                std::optional<BehaviorEasing> easing;
                if (const auto easingReader = keyReader->optionalField("easing")) {
                    if (keyIndex == 0) {
                        addError(diagnostics, "chart.behavior.first_key_easing",
                                 "The first key must omit easing",
                                 std::string{easingReader->fieldPath()});
                        trackValid = false;
                    }
                    easing = readBehaviorEasing(*easingReader, diagnostics);
                }
                if (!beat || !value ||
                    (keyIndex > 0 && keyReader->optionalField("easing") && !easing)) {
                    trackValid = false;
                    continue;
                }
                typedTrack.keys.push_back(
                    BehaviorKey{.beat = *beat, .value = *value, .easing = easing});
            }
            if (trackValid) {
                typedTracks.push_back(std::move(typedTrack));
            } else {
                behaviorValid = false;
            }
        }
        if (behaviorValid) {
            BehaviorTracks behaviorTracks{std::move(typedTracks)};
            behaviorTracks.canonicalText =
                opaqueJson(tracksReader->value(), diagnostics, tracksReader->fieldPath())
                    .canonicalText;
            result.push_back(ChartBehavior{.id = BehaviorId{*id},
                                           .type = std::string{*type},
                                           .version = 1,
                                           .tracks = std::move(behaviorTracks),
                                           .events = {},
                                           .stepEvents = {}});
        }
    }
    return result;
}

[[nodiscard]] auto parseEventBehaviors(const json::Reader& reader, const ChartLimits& limits,
                                       core::Diagnostics& diagnostics)
    -> std::vector<ChartBehavior> {
    std::vector<ChartBehavior> result;
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return result;
    }
    if (values->size() > limits.maxBehaviors) {
        addError(diagnostics, "chart.limit.behaviors", "Behavior count exceeds configured limit",
                 std::string{reader.fieldPath()});
        return result;
    }
    std::set<std::string> ids;
    std::size_t totalEvents = 0;
    for (std::size_t index = 0; index < values->size(); ++index) {
        if (diagnostics.limitReached()) {
            break;
        }
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array knownFields{std::string_view{"id"}, std::string_view{"type"},
                                         std::string_view{"version"}, std::string_view{"events"},
                                         std::string_view{"stepEvents"}};
        item->rejectUnknownFields(knownFields);
        const auto idReader = item->requiredField("id");
        const auto typeReader = item->requiredField("type");
        const auto versionReader = item->requiredField("version");
        const auto eventsReader = item->requiredField("events");
        const auto stepEventsReader = item->requiredField("stepEvents");
        if (!idReader || !typeReader || !versionReader || !eventsReader || !stepEventsReader) {
            continue;
        }
        const auto id = readIdentifier(*idReader, limits, diagnostics, "Behavior ID");
        const auto type = typeReader->readString();
        const bool versionValid = readSupportedComponentVersion(*versionReader, diagnostics);
        const auto* events = eventsReader->readArray();
        const auto* stepEvents = stepEventsReader->readArray();
        if (!id || !type || !versionValid || events == nullptr || stepEvents == nullptr) {
            continue;
        }
        if (!ids.insert(*id).second) {
            addError(diagnostics, "chart.behavior.id_duplicate", "Behavior ID must be unique",
                     std::string{idReader->fieldPath()});
            continue;
        }
        if (*type != "behavior.event") {
            addError(diagnostics, "chart.behavior.type_unsupported",
                     "Chart v3 requires behavior.event version 1",
                     std::string{typeReader->fieldPath()});
            continue;
        }
        const auto eventCount = events->size() + stepEvents->size();
        if (eventCount == 0) {
            addError(diagnostics, "chart.behavior.events_empty",
                     "behavior.event must contain at least one event",
                     std::string{item->fieldPath()});
            continue;
        }
        if (eventCount > limits.maxEventsPerBehavior ||
            eventCount > limits.maxTotalBehaviorEvents ||
            totalEvents > limits.maxTotalBehaviorEvents - eventCount) {
            addError(diagnostics, "chart.limit.behavior_events",
                     "Behavior Event count exceeds configured limit",
                     std::string{item->fieldPath()});
            continue;
        }
        totalEvents += eventCount;

        std::vector<BehaviorEvent> typedEvents;
        typedEvents.reserve(events->size());
        bool behaviorValid = true;
        for (std::size_t eventIndex = 0; eventIndex < events->size(); ++eventIndex) {
            const auto eventReader = eventsReader->element(eventIndex);
            if (!eventReader || eventReader->readObject() == nullptr) {
                behaviorValid = false;
                continue;
            }
            constexpr std::array eventFields{
                std::string_view{"property"},      std::string_view{"startBeat"},
                std::string_view{"durationBeats"}, std::string_view{"startValue"},
                std::string_view{"endValue"},      std::string_view{"startSlope"},
                std::string_view{"endSlope"},      std::string_view{"groupId"}};
            eventReader->rejectUnknownFields(eventFields);
            const auto propertyReader = eventReader->requiredField("property");
            const auto startBeatReader = eventReader->requiredField("startBeat");
            const auto durationReader = eventReader->requiredField("durationBeats");
            const auto startValueReader = eventReader->requiredField("startValue");
            const auto endValueReader = eventReader->requiredField("endValue");
            const auto startSlopeReader = eventReader->requiredField("startSlope");
            const auto endSlopeReader = eventReader->requiredField("endSlope");
            if (!propertyReader || !startBeatReader || !durationReader || !startValueReader ||
                !endValueReader || !startSlopeReader || !endSlopeReader) {
                behaviorValid = false;
                continue;
            }
            const auto property = readEventProperty(*propertyReader, diagnostics);
            const auto startBeat = readBeat(*startBeatReader, limits, diagnostics);
            const auto duration = readBeat(*durationReader, limits, diagnostics);
            const auto startSlope = startSlopeReader->readNumber();
            const auto endSlope = endSlopeReader->readNumber();
            const auto startValue =
                property ? readBehaviorValue(*startValueReader, *property, diagnostics)
                         : std::nullopt;
            const auto endValue = property
                                      ? readBehaviorValue(*endValueReader, *property, diagnostics)
                                      : std::nullopt;
            std::optional<std::string> groupId;
            bool groupValid = true;
            if (const auto groupReader = eventReader->optionalField("groupId")) {
                groupId = readGroupId(*groupReader, limits, diagnostics);
                groupValid = groupId.has_value();
            }
            if (!property || !startBeat || !duration || !startValue || !endValue || !startSlope ||
                !endSlope || !groupValid) {
                behaviorValid = false;
                continue;
            }
            if (duration->numerator() < 0) {
                addError(diagnostics, "chart.behavior.duration_negative",
                         "Behavior Event duration must be non-negative",
                         std::string{durationReader->fieldPath()});
                behaviorValid = false;
                continue;
            }
            if (!std::isfinite(*startSlope) || !std::isfinite(*endSlope) || *startSlope < 0.0 ||
                *endSlope < 0.0 || *startSlope + *endSlope > 3.0) {
                addError(diagnostics, "chart.behavior.slope_invalid",
                         "Behavior Event slopes must be finite, non-negative, and sum to at most 3",
                         std::string{eventReader->fieldPath()});
                behaviorValid = false;
                continue;
            }
            if (duration->numerator() == 0 &&
                (*startValue != *endValue || *startSlope != 0.0 || *endSlope != 0.0)) {
                addError(diagnostics, "chart.behavior.zero_duration_invalid",
                         "Zero-duration Behavior Events require equal values and zero slopes",
                         std::string{eventReader->fieldPath()});
                behaviorValid = false;
                continue;
            }
            typedEvents.push_back(BehaviorEvent{*property, *startBeat, *duration, *startValue,
                                                *endValue, *startSlope, *endSlope,
                                                std::move(groupId)});
        }

        std::vector<BehaviorStepEvent> typedStepEvents;
        typedStepEvents.reserve(stepEvents->size());
        for (std::size_t eventIndex = 0; eventIndex < stepEvents->size(); ++eventIndex) {
            const auto eventReader = stepEventsReader->element(eventIndex);
            if (!eventReader || eventReader->readObject() == nullptr) {
                behaviorValid = false;
                continue;
            }
            constexpr std::array eventFields{std::string_view{"property"}, std::string_view{"beat"},
                                             std::string_view{"value"},
                                             std::string_view{"groupId"}};
            eventReader->rejectUnknownFields(eventFields);
            const auto propertyReader = eventReader->requiredField("property");
            const auto beatReader = eventReader->requiredField("beat");
            const auto valueReader = eventReader->requiredField("value");
            if (!propertyReader || !beatReader || !valueReader) {
                behaviorValid = false;
                continue;
            }
            const auto property = readStepProperty(*propertyReader, diagnostics);
            const auto beat = readBeat(*beatReader, limits, diagnostics);
            std::optional<BehaviorStepValue> value;
            if (property == BehaviorStepProperty::RenderVisible) {
                if (const auto visible = valueReader->readBoolean()) {
                    value = BehaviorStepValue{*visible};
                }
            } else if (property == BehaviorStepProperty::RenderMaterial) {
                if (const auto material =
                        readReference(*valueReader, "asset", limits, diagnostics)) {
                    value = BehaviorStepValue{AssetId{*material}};
                }
            }
            std::optional<std::string> groupId;
            bool groupValid = true;
            if (const auto groupReader = eventReader->optionalField("groupId")) {
                groupId = readGroupId(*groupReader, limits, diagnostics);
                groupValid = groupId.has_value();
            }
            if (!property || !beat || !value || !groupValid) {
                behaviorValid = false;
                continue;
            }
            typedStepEvents.push_back(
                BehaviorStepEvent{*property, *beat, std::move(*value), std::move(groupId)});
        }
        if (behaviorValid) {
            result.push_back(ChartBehavior{.id = BehaviorId{*id},
                                           .type = std::string{*type},
                                           .version = 1,
                                           .tracks = {},
                                           .events = std::move(typedEvents),
                                           .stepEvents = std::move(typedStepEvents)});
        }
    }
    return result;
}

[[nodiscard]] auto parseObjects(const json::Reader& reader,
                                const std::map<std::string, json::Value>& templates,
                                const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::vector<ChartObject> {
    std::vector<ChartObject> result;
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return result;
    }
    if (values->size() > limits.maxObjects) {
        addError(diagnostics, "chart.limit.objects", "Object count exceeds configured limit",
                 std::string{reader.fieldPath()});
        return result;
    }
    std::set<std::string> ids;
    for (std::size_t index = 0; index < values->size(); ++index) {
        if (diagnostics.limitReached()) {
            break;
        }
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array knownFields{
            std::string_view{"id"},        std::string_view{"name"},
            std::string_view{"parent"},    std::string_view{"components"},
            std::string_view{"template"},  std::string_view{"overrides"},
            std::string_view{"extensions"}};
        item->rejectUnknownFields(knownFields);
        const auto idReader = item->requiredField("id");
        const auto parentReader = item->requiredField("parent");
        const auto extensionsReader = item->requiredField("extensions");
        if (!idReader || !parentReader || !extensionsReader) {
            continue;
        }
        const auto id = readIdentifier(*idReader, limits, diagnostics, "Object ID");
        if (!id) {
            continue;
        }
        if (!isChartEntityUuid(*id)) {
            addError(diagnostics, "chart.uuid.invalid_entity_id",
                     "Object ID must be a canonical UUIDv7 or imported UUIDv5",
                     std::string{idReader->fieldPath()});
            continue;
        }
        if (!ids.insert(*id).second) {
            addError(diagnostics, "chart.object.id_duplicate", "Object ID must be unique",
                     std::string{idReader->fieldPath()});
            continue;
        }

        std::optional<ChartObjectId> parent;
        if (!parentReader->value().isNull()) {
            if (const auto parentId = readReference(*parentReader, "object", limits, diagnostics)) {
                parent = ChartObjectId{*parentId};
            }
        }

        const auto componentsReader = item->optionalField("components");
        const auto templateReader = item->optionalField("template");
        const auto overridesReader = item->optionalField("overrides");
        json::Value expandedComponents;
        std::optional<ChartTemplateId> sourceTemplate;
        bool shapeValid = true;
        if (componentsReader) {
            if (templateReader || overridesReader) {
                addError(diagnostics, "chart.object.shape_invalid",
                         "Direct object cannot contain template or overrides",
                         std::string{item->fieldPath()});
                shapeValid = false;
            }
            expandedComponents = componentsReader->value();
        } else {
            if (!templateReader || !overridesReader) {
                addError(diagnostics, "chart.object.shape_invalid",
                         "Template object requires template and overrides",
                         std::string{item->fieldPath()});
                shapeValid = false;
            } else {
                const auto templateId =
                    readReference(*templateReader, "template", limits, diagnostics);
                const auto* overrides = overridesReader->readArray();
                if (templateId && overrides != nullptr) {
                    const auto found = templates.find(*templateId);
                    if (found == templates.end()) {
                        addError(diagnostics, "chart.reference.template_missing",
                                 "Object refers to a missing or invalid template",
                                 std::string{templateReader->fieldPath()});
                        shapeValid = false;
                    } else {
                        sourceTemplate = ChartTemplateId{*templateId};
                        expandedComponents = found->second;
                        detail::applyPatches(expandedComponents, *overrides,
                                             overridesReader->fieldPath(), limits, diagnostics);
                    }
                } else {
                    shapeValid = false;
                }
            }
        }

        OpaqueJson extensions;
        if (extensionsReader->readObject() != nullptr) {
            extensions =
                opaqueJson(extensionsReader->value(), diagnostics, extensionsReader->fieldPath());
            for (const auto& [extensionId, ignored] : *extensionsReader->value().object()) {
                if (diagnostics.limitReached()) {
                    break;
                }
                static_cast<void>(ignored);
                addWarning(diagnostics, "chart.extension.optional_unknown",
                           "Unknown optional extension is preserved without runtime behavior",
                           json::appendFieldPath(extensionsReader->fieldPath(), extensionId));
            }
        }
        if (!shapeValid) {
            continue;
        }
        const auto components = parseComponents(
            expandedComponents, json::appendFieldPath(item->fieldPath(), "components"), limits,
            diagnostics, true);
        if (!components) {
            continue;
        }
        result.push_back(ChartObject{ChartObjectId{*id},
                                     readNullableName(*item, limits, diagnostics), parent,
                                     sourceTemplate, *components, std::move(extensions)});
    }
    return result;
}

[[nodiscard]] auto parseTiming(const json::Reader& reader, std::uint32_t formatVersion,
                               const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<ChartTiming> {
    constexpr std::array legacyFields{std::string_view{"offsetMs"}, std::string_view{"defaultBpm"},
                                      std::string_view{"bpmChanges"}, std::string_view{"stops"}};
    constexpr std::array v3Fields{std::string_view{"offsetMs"}, std::string_view{"defaultBpm"},
                                  std::string_view{"tempoEvents"}, std::string_view{"stops"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(formatVersion == 3 ? std::span{v3Fields} : std::span{legacyFields});
    const auto offsetReader = reader.requiredField("offsetMs");
    const auto bpmReader = reader.requiredField("defaultBpm");
    const auto eventsReader =
        reader.requiredField(formatVersion == 3 ? "tempoEvents" : "bpmChanges");
    const auto stopsReader = reader.requiredField("stops");
    if (!offsetReader || !bpmReader || !eventsReader || !stopsReader) {
        return std::nullopt;
    }
    const auto offset = offsetReader->readNumber();
    const auto bpm = bpmReader->readNumber();
    const auto* events = eventsReader->readArray();
    const auto* stops = stopsReader->readArray();
    if (!offset || !bpm || events == nullptr || stops == nullptr) {
        return std::nullopt;
    }
    if (!std::isfinite(*offset)) {
        addError(diagnostics, "chart.timing.non_finite", "Offset must be finite",
                 std::string{offsetReader->fieldPath()});
    }
    const bool bpmValid = formatVersion == 3 ? *bpm >= 1.0 && *bpm <= 65536.0 : *bpm > 0.0;
    if (!std::isfinite(*bpm) || !bpmValid) {
        addError(diagnostics, "chart.timing.invalid_bpm",
                 formatVersion == 3 ? "Default BPM must be in [1, 65536]"
                                    : "Default BPM must be positive",
                 std::string{bpmReader->fieldPath()});
    }
    if (formatVersion != 3 && !events->empty()) {
        addError(diagnostics, "chart.timing.bpm_changes_unsupported",
                 "BPM changes are unsupported before chart v3",
                 std::string{eventsReader->fieldPath()});
    }
    if (formatVersion != 3 && !stops->empty()) {
        addError(diagnostics, "chart.timing.stops_unsupported",
                 "Stops are unsupported before chart v3", std::string{stopsReader->fieldPath()});
    }
    if (!std::isfinite(*offset) || !std::isfinite(*bpm) || !bpmValid ||
        (formatVersion != 3 && (!events->empty() || !stops->empty()))) {
        return std::nullopt;
    }

    ChartTiming timing{.offsetMs = *offset, .defaultBpm = *bpm, .tempoEvents = {}, .stops = {}};
    if (formatVersion != 3) {
        return timing;
    }
    if (events->size() > limits.maxTempoEvents) {
        addError(diagnostics, "chart.limit.tempo_events",
                 "Tempo Event count exceeds configured limit",
                 std::string{eventsReader->fieldPath()});
        return std::nullopt;
    }
    if (stops->size() > limits.maxStops) {
        addError(diagnostics, "chart.limit.stops", "Stop count exceeds configured limit",
                 std::string{stopsReader->fieldPath()});
        return std::nullopt;
    }
    bool valid = true;
    timing.tempoEvents.reserve(events->size());
    for (std::size_t index = 0; index < events->size(); ++index) {
        const auto item = eventsReader->element(index);
        if (!item || item->readObject() == nullptr) {
            valid = false;
            continue;
        }
        constexpr std::array fields{
            std::string_view{"startBeat"},  std::string_view{"durationBeats"},
            std::string_view{"startBpm"},   std::string_view{"endBpm"},
            std::string_view{"startSlope"}, std::string_view{"endSlope"}};
        item->rejectUnknownFields(fields);
        const auto startReader = item->requiredField("startBeat");
        const auto durationReader = item->requiredField("durationBeats");
        const auto startBpmReader = item->requiredField("startBpm");
        const auto endBpmReader = item->requiredField("endBpm");
        const auto startSlopeReader = item->requiredField("startSlope");
        const auto endSlopeReader = item->requiredField("endSlope");
        if (!startReader || !durationReader || !startBpmReader || !endBpmReader ||
            !startSlopeReader || !endSlopeReader) {
            valid = false;
            continue;
        }
        const auto start = readBeat(*startReader, limits, diagnostics);
        const auto duration = readBeat(*durationReader, limits, diagnostics);
        const auto startBpm = startBpmReader->readNumber();
        const auto endBpm = endBpmReader->readNumber();
        const auto startSlope = startSlopeReader->readNumber();
        const auto endSlope = endSlopeReader->readNumber();
        if (!start || !duration || !startBpm || !endBpm || !startSlope || !endSlope) {
            valid = false;
            continue;
        }
        if (duration->numerator() < 0) {
            addError(diagnostics, "chart.timing.duration_negative",
                     "Tempo Event duration must be non-negative",
                     std::string{durationReader->fieldPath()});
            valid = false;
            continue;
        }
        if (!std::isfinite(*startBpm) || !std::isfinite(*endBpm) || *startBpm < 1.0 ||
            *startBpm > 65536.0 || *endBpm < 1.0 || *endBpm > 65536.0) {
            addError(diagnostics, "chart.timing.invalid_bpm",
                     "Tempo Event BPM must be in [1, 65536]", std::string{item->fieldPath()});
            valid = false;
            continue;
        }
        if (!std::isfinite(*startSlope) || !std::isfinite(*endSlope) || *startSlope < 0.0 ||
            *endSlope < 0.0 || *startSlope + *endSlope > 3.0) {
            addError(diagnostics, "chart.timing.invalid_slope",
                     "Tempo Event slopes must be finite, non-negative, and sum to at most 3",
                     std::string{item->fieldPath()});
            valid = false;
            continue;
        }
        if (duration->numerator() == 0 &&
            (*startBpm != *endBpm || *startSlope != 0.0 || *endSlope != 0.0)) {
            addError(diagnostics, "chart.timing.zero_duration_invalid",
                     "Zero-duration Tempo Events require equal BPM and zero slopes",
                     std::string{item->fieldPath()});
            valid = false;
            continue;
        }
        timing.tempoEvents.push_back(
            TempoEvent{*start, *duration, *startBpm, *endBpm, *startSlope, *endSlope});
    }
    timing.stops.reserve(stops->size());
    for (std::size_t index = 0; index < stops->size(); ++index) {
        const auto item = stopsReader->element(index);
        if (!item || item->readObject() == nullptr) {
            valid = false;
            continue;
        }
        constexpr std::array fields{std::string_view{"beat"}, std::string_view{"durationMs"}};
        item->rejectUnknownFields(fields);
        const auto beatReader = item->requiredField("beat");
        const auto durationReader = item->requiredField("durationMs");
        if (!beatReader || !durationReader) {
            valid = false;
            continue;
        }
        const auto beat = readBeat(*beatReader, limits, diagnostics);
        const auto duration = durationReader->readNumber();
        if (!beat || !duration) {
            valid = false;
            continue;
        }
        if (!std::isfinite(*duration) || *duration <= 0.0) {
            addError(diagnostics, "chart.timing.stop_duration_invalid",
                     "Stop duration must be finite and positive",
                     std::string{durationReader->fieldPath()});
            valid = false;
            continue;
        }
        timing.stops.push_back(TimingStop{*beat, *duration});
    }
    return valid ? std::optional<ChartTiming>{std::move(timing)} : std::nullopt;
}

[[nodiscard]] auto readCamera(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> CameraData {
    constexpr std::array knownFields{
        std::string_view{"type"},  std::string_view{"fovY"},
        std::string_view{"near"},  std::string_view{"far"},
        std::string_view{"pitch"}, std::string_view{"yaw"},
        std::string_view{"roll"},  std::string_view{"defaultTransform"}};
    if (reader.readObject() == nullptr) {
        return CameraData{};
    }
    reader.rejectUnknownFields(knownFields);

    const auto typeReader = reader.requiredField("type");
    const auto fovReader = reader.requiredField("fovY");
    const auto nearReader = reader.requiredField("near");
    const auto farReader = reader.requiredField("far");

    CameraData camera;

    if (typeReader) {
        const auto type = typeReader->readString();
        if (type && *type == "perspective") {
            camera.type = *type;
        } else if (type) {
            addError(diagnostics, "chart.camera.unsupported_type",
                     "Unsupported camera projection type; expected 'perspective'",
                     std::string{typeReader->fieldPath()});
        }
    }

    if (fovReader) {
        const auto fov = fovReader->readNumber();
        if (fov && std::isfinite(*fov) && *fov > 0.0 && *fov < 179.0) {
            camera.fovY = *fov;
        } else {
            addError(diagnostics, "chart.camera.invalid_fov",
                     "Camera FOV must be finite and in (0, 179) degrees",
                     std::string{fovReader->fieldPath()});
        }
    }

    if (nearReader) {
        const auto nearPlane = nearReader->readNumber();
        if (nearPlane && std::isfinite(*nearPlane) && *nearPlane > 0.0) {
            camera.nearPlane = *nearPlane;
        } else {
            addError(diagnostics, "chart.camera.invalid_near",
                     "Camera near plane must be finite and positive",
                     std::string{nearReader->fieldPath()});
        }
    }

    if (farReader) {
        const auto farPlane = farReader->readNumber();
        if (farPlane && std::isfinite(*farPlane) && *farPlane > 0.0) {
            camera.farPlane = *farPlane;
        } else {
            addError(diagnostics, "chart.camera.invalid_far",
                     "Camera far plane must be finite and positive",
                     std::string{farReader->fieldPath()});
        }
    }

    if (camera.nearPlane >= camera.farPlane) {
        addError(diagnostics, "chart.camera.near_exceeds_far",
                 "Camera near plane must be less than far plane", std::string{reader.fieldPath()});
    }

    const auto pitchReader = reader.optionalField("pitch");
    if (pitchReader) {
        const auto pitch = pitchReader->readNumber();
        if (pitch && std::isfinite(*pitch)) {
            camera.pitch = *pitch;
        }
    }
    const auto yawReader = reader.optionalField("yaw");
    if (yawReader) {
        const auto yaw = yawReader->readNumber();
        if (yaw && std::isfinite(*yaw)) {
            camera.yaw = *yaw;
        }
    }
    const auto rollReader = reader.optionalField("roll");
    if (rollReader) {
        const auto roll = rollReader->readNumber();
        if (roll && std::isfinite(*roll)) {
            camera.roll = *roll;
        }
    }

    const auto transformReader = reader.optionalField("defaultTransform");
    if (transformReader) {
        constexpr std::array transformFields{std::string_view{"position"}};
        if (transformReader->readObject() != nullptr) {
            transformReader->rejectUnknownFields(transformFields);
            const auto posReader = transformReader->requiredField("position");
            const auto position = posReader ? readVec3(*posReader, diagnostics) : std::nullopt;
            if (position) {
                camera.defaultTransform =
                    TransformData{*position, {0.0F, 0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F}};
            }
        }
    }

    return camera;
}

} // namespace

auto detail::loadCanonicalValue(json::Value parsed, const ChartLimits& limits)
    -> ChartDocumentResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    json::Reader root{parsed, diagnostics};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    const auto versionReader = root.requiredField("version");
    const auto formatVersion =
        versionReader ? readVersion(*versionReader, diagnostics) : std::nullopt;
    constexpr std::array knownFieldsV1{
        std::string_view{"format"},    std::string_view{"version"},
        std::string_view{"chartId"},   std::string_view{"metadata"},
        std::string_view{"timing"},    std::string_view{"camera"},
        std::string_view{"templates"}, std::string_view{"behaviors"},
        std::string_view{"objects"},   std::string_view{"requiredExtensions"},
        std::string_view{"extensions"}};
    constexpr std::array knownFieldsV2{
        std::string_view{"format"},     std::string_view{"version"},
        std::string_view{"chartId"},    std::string_view{"metadata"},
        std::string_view{"timing"},     std::string_view{"camera"},
        std::string_view{"templates"},  std::string_view{"behaviors"},
        std::string_view{"objects"},    std::string_view{"requiredExtensions"},
        std::string_view{"extensions"}, std::string_view{"audio"}};
    if (formatVersion && (*formatVersion == 2 || *formatVersion == 3)) {
        root.rejectUnknownFields(knownFieldsV2);
    } else {
        root.rejectUnknownFields(knownFieldsV1);
    }

    const auto formatReader = root.requiredField("format");
    const auto chartIdReader = root.requiredField("chartId");
    const auto metadataReader = root.requiredField("metadata");
    const auto timingReader = root.requiredField("timing");
    const auto cameraReader = root.optionalField("camera");
    const auto templatesReader = root.requiredField("templates");
    const auto behaviorsReader = root.requiredField("behaviors");
    const auto objectsReader = root.requiredField("objects");
    const auto requiredExtensionsReader = root.requiredField("requiredExtensions");
    const auto extensionsReader = root.requiredField("extensions");
    const auto audioReader = formatVersion && (*formatVersion == 2 || *formatVersion == 3)
                                 ? root.optionalField("audio")
                                 : std::nullopt;

    std::optional<ChartId> chartId;
    if (formatReader) {
        const auto format = formatReader->readString();
        if (format && *format != "cuexis.chart") {
            addError(diagnostics, "chart.format.invalid", "Expected canonical cuexis.chart format",
                     std::string{formatReader->fieldPath()});
        }
    }
    if (chartIdReader) {
        const auto id = readIdentifier(*chartIdReader, limits, diagnostics, "Chart ID");
        if (id && isUuidV7(*id)) {
            chartId = ChartId{*id};
        } else if (id) {
            addError(diagnostics, "chart.uuid.invalid_v7", "Chart ID must be a UUIDv7",
                     std::string{chartIdReader->fieldPath()});
        }
    }

    ChartMetadata metadata;
    if (metadataReader && metadataReader->readObject() != nullptr) {
        if (metadataReader->value().object()->size() > limits.maxMetadataMembers) {
            addError(diagnostics, "chart.limit.metadata",
                     "Metadata member count exceeds configured limit",
                     std::string{metadataReader->fieldPath()});
        }
        metadata.data =
            opaqueJson(metadataReader->value(), diagnostics, metadataReader->fieldPath());
    }
    const auto timing = timingReader && formatVersion
                            ? parseTiming(*timingReader, *formatVersion, limits, diagnostics)
                            : std::nullopt;
    const auto audio = audioReader ? readChartAudio(*audioReader, limits, diagnostics)
                                   : std::optional<ChartAudioData>{};
    const auto camera = cameraReader ? readCamera(*cameraReader, diagnostics) : CameraData{};
    const detail::TemplateParserCallbacks templateCallbacks{.readIdentifier = readIdentifier,
                                                            .readReference = readReference,
                                                            .readNullableName = readNullableName,
                                                            .opaqueJson = opaqueJson,
                                                            .parseComponents = parseComponents};
    auto [templates, expandedTemplates] =
        templatesReader
            ? detail::parseTemplates(*templatesReader, limits, diagnostics, templateCallbacks)
            : std::pair<std::vector<ChartTemplate>, std::map<std::string, json::Value>>{};
    auto behaviors =
        behaviorsReader && formatVersion
            ? (*formatVersion == 3 ? parseEventBehaviors(*behaviorsReader, limits, diagnostics)
                                   : parseBehaviors(*behaviorsReader, limits, diagnostics))
            : std::vector<ChartBehavior>{};
    auto objects = objectsReader
                       ? parseObjects(*objectsReader, expandedTemplates, limits, diagnostics)
                       : std::vector<ChartObject>{};

    if (requiredExtensionsReader) {
        if (const auto* required = requiredExtensionsReader->readArray()) {
            if (required->size() > limits.maxExtensions) {
                addError(diagnostics, "chart.limit.required_extensions",
                         "Required extension count exceeds configured limit",
                         std::string{requiredExtensionsReader->fieldPath()});
            }
            for (std::size_t index = 0; index < required->size(); ++index) {
                if (diagnostics.limitReached()) {
                    break;
                }
                const auto itemReader = requiredExtensionsReader->element(index);
                if (!itemReader || itemReader->readObject() == nullptr) {
                    continue;
                }
                constexpr std::array requiredExtensionFields{std::string_view{"id"},
                                                             std::string_view{"version"}};
                itemReader->rejectUnknownFields(requiredExtensionFields);
                const auto idReader = itemReader->requiredField("id");
                const auto extensionVersionReader = itemReader->requiredField("version");
                if (!idReader || !extensionVersionReader) {
                    continue;
                }
                const auto extensionId =
                    readIdentifier(*idReader, limits, diagnostics, "Required extension ID");
                const auto extensionVersion = extensionVersionReader->readInt64();
                bool valid = extensionId.has_value() && isExtensionId(*extensionId);
                if (extensionId && !valid) {
                    addError(diagnostics, "chart.extension.id_invalid",
                             "Required extension ID contains unsupported characters",
                             std::string{idReader->fieldPath()});
                }
                if (extensionVersion && *extensionVersion < 1) {
                    addError(diagnostics, "chart.extension.version_invalid",
                             "Required extension version must be positive",
                             std::string{extensionVersionReader->fieldPath()});
                    valid = false;
                } else if (!extensionVersion) {
                    valid = false;
                }
                if (!valid) {
                    continue;
                }
                addError(diagnostics, "chart.extension.required_unsupported",
                         "Required extension has no registered handler",
                         json::appendIndexPath(requiredExtensionsReader->fieldPath(), index));
            }
        }
    }

    OpaqueJson extensions;
    if (extensionsReader && extensionsReader->readObject() != nullptr) {
        if (extensionsReader->value().object()->size() > limits.maxExtensions) {
            addError(diagnostics, "chart.limit.extensions",
                     "Extension count exceeds configured limit",
                     std::string{extensionsReader->fieldPath()});
        }
        extensions =
            opaqueJson(extensionsReader->value(), diagnostics, extensionsReader->fieldPath());
        for (const auto& [extensionId, ignored] : *extensionsReader->value().object()) {
            if (diagnostics.limitReached()) {
                break;
            }
            static_cast<void>(ignored);
            addWarning(diagnostics, "chart.extension.optional_unknown",
                       "Unknown optional extension is preserved without runtime behavior",
                       json::appendFieldPath(extensionsReader->fieldPath(), extensionId));
        }
    }

    if (!chartId || !timing || !formatVersion || diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }

    ChartDocument document{*chartId,
                           std::move(metadata),
                           *timing,
                           camera,
                           std::move(templates),
                           std::move(behaviors),
                           std::move(objects),
                           std::move(extensions),
                           *formatVersion,
                           audio};
    auto semantic = ChartCompiler::compile(document, limits);
    diagnostics.append(std::move(semantic.diagnostics));
    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    return ChartDocumentResult{std::move(document), std::move(diagnostics)};
}

auto CanonicalChartLoader::load(std::string_view jsonText, const ChartLimits& limits)
    -> ChartDocumentResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addError(diagnostics, parsed.error(), "$");
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }

    return detail::loadCanonicalValue(std::move(*parsed), limits);
}

} // namespace cuexis::chart
