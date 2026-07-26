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

[[nodiscard]] auto readVersion(const json::Reader& reader, core::Diagnostics& diagnostics,
                               std::int64_t supported = 1) -> bool {
    const auto version = reader.readInt64();
    if (!version) {
        return false;
    }
    if (*version != supported) {
        addError(diagnostics, "chart.version.unsupported", "Chart format version is unsupported",
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
    if (property == BehaviorProperty::TransformScale) {
        const auto value = readVec3(reader, diagnostics);
        if (!value) {
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
    if (property != BehaviorProperty::CameraFovY &&
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
    return BehaviorValue{*value};
}

[[nodiscard]] auto readComponentVersion(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> bool {
    const auto versionReader = reader.requiredField("version");
    return versionReader && readVersion(*versionReader, diagnostics);
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
            if (typeReader) {
                const auto type = typeReader->readString();
                if (type && *type == "perspective") {
                    cameraData.type = *type;
                }
            }
            if (fovReader) {
                const auto fov = fovReader->readNumber();
                if (fov && std::isfinite(*fov) && *fov > 0.0 && *fov < 179.0) {
                    cameraData.fovY = *fov;
                }
            }
            if (nearReader) {
                const auto nearPlane = nearReader->readNumber();
                if (nearPlane && std::isfinite(*nearPlane) && *nearPlane > 0.0) {
                    cameraData.nearPlane = *nearPlane;
                }
            }
            if (farReader) {
                const auto farPlane = farReader->readNumber();
                if (farPlane && std::isfinite(*farPlane) && *farPlane > 0.0) {
                    cameraData.farPlane = *farPlane;
                }
            }
            components.camera = std::move(cameraData);
        }
    }

    return components;
}

[[nodiscard]] auto splitPatchPath(std::string_view path) -> std::vector<std::string> {
    std::vector<std::string> segments;
    if (path.empty() || path.front() != '/') {
        return segments;
    }
    std::size_t begin = 1;
    while (begin <= path.size()) {
        const auto end = path.find('/', begin);
        const auto encoded =
            path.substr(begin, end == std::string_view::npos ? path.size() - begin : end - begin);
        std::string decoded;
        for (std::size_t index = 0; index < encoded.size(); ++index) {
            if (encoded[index] == '~' && index + 1 < encoded.size()) {
                if (encoded[index + 1] == '0') {
                    decoded.push_back('~');
                    ++index;
                    continue;
                }
                if (encoded[index + 1] == '1') {
                    decoded.push_back('/');
                    ++index;
                    continue;
                }
            }
            decoded.push_back(encoded[index]);
        }
        segments.push_back(std::move(decoded));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return segments;
}

[[nodiscard]] auto isPatchableComponent(std::string_view component) noexcept -> bool {
    return component == "cuexis.transform" || component == "cuexis.renderable" ||
           component == "cuexis.behavior" || component == "cuexis.note" ||
           component == "cuexis.element" || component == "cuexis.camera";
}

[[nodiscard]] auto isPatchableField(std::string_view component, std::string_view field) noexcept
    -> bool {
    if (component == "cuexis.transform") {
        return field == "position" || field == "rotation" || field == "scale";
    }
    if (component == "cuexis.renderable") {
        return field == "mesh" || field == "material";
    }
    if (component == "cuexis.behavior") {
        return field == "behavior";
    }
    if (component == "cuexis.note") {
        return field == "beat";
    }
    if (component == "cuexis.camera") {
        return field == "fovY" || field == "near" || field == "far" || field == "type";
    }
    return false;
}

void applyPatches(json::Value& components, const json::Value::Array& patches,
                  std::string_view patchesPath, const ChartLimits& limits,
                  core::Diagnostics& diagnostics) {
    if (patches.size() > limits.maxPatchesPerTemplate) {
        addError(diagnostics, "chart.limit.patches", "Patch count exceeds the configured limit",
                 std::string{patchesPath});
        return;
    }
    auto* componentsObject = components.object();
    if (componentsObject == nullptr) {
        addError(diagnostics, "chart.patch.target_invalid", "Patch target components are invalid",
                 std::string{patchesPath});
        return;
    }

    for (std::size_t index = 0; index < patches.size(); ++index) {
        if (diagnostics.limitReached()) {
            break;
        }
        const std::string patchPath = json::appendIndexPath(patchesPath, index);
        json::Reader patchReader{patches[index], diagnostics, patchPath};
        constexpr std::array knownFields{std::string_view{"op"}, std::string_view{"path"},
                                         std::string_view{"value"}};
        if (patchReader.readObject() == nullptr) {
            continue;
        }
        patchReader.rejectUnknownFields(knownFields);
        const auto opReader = patchReader.requiredField("op");
        const auto pathReader = patchReader.requiredField("path");
        if (!opReader || !pathReader) {
            continue;
        }
        const auto operation = opReader->readString();
        const auto targetPath = pathReader->readString();
        if (!operation || !targetPath) {
            continue;
        }
        if (*operation != "add" && *operation != "remove" && *operation != "replace") {
            addError(diagnostics, "chart.patch.operation_unsupported",
                     "Only add, remove, and replace patch operations are supported",
                     std::string{opReader->fieldPath()});
            continue;
        }

        const auto segments = splitPatchPath(*targetPath);
        if ((segments.size() != 2 && segments.size() != 3) || segments[0] != "components" ||
            !isPatchableComponent(segments[1]) ||
            (segments.size() == 3 && !isPatchableField(segments[1], segments[2]))) {
            addError(diagnostics, "chart.patch.path_unsupported",
                     "Patch path is outside the fixed chart v1 component schema",
                     std::string{pathReader->fieldPath()});
            continue;
        }

        const auto valueReader = patchReader.optionalField("value");
        if (*operation == "remove" && valueReader) {
            addError(diagnostics, "chart.patch.remove_has_value",
                     "Remove patch must not contain a value",
                     std::string{valueReader->fieldPath()});
            continue;
        }
        if (*operation != "remove" && !valueReader) {
            addError(diagnostics, "chart.patch.value_missing", "Patch operation requires a value",
                     json::appendFieldPath(patchReader.fieldPath(), "value"));
            continue;
        }

        json::Value::Object* targetObject = componentsObject;
        std::string key = segments[1];
        if (segments.size() == 3) {
            const auto component = componentsObject->find(segments[1]);
            if (component == componentsObject->end() || component->second.object() == nullptr) {
                addError(diagnostics, "chart.patch.target_missing",
                         "Patch component target does not exist",
                         std::string{pathReader->fieldPath()});
                continue;
            }
            targetObject = component->second.object();
            key = segments[2];
        }

        const auto existing = targetObject->find(key);
        if (*operation == "remove") {
            if (existing == targetObject->end()) {
                addError(diagnostics, "chart.patch.target_missing",
                         "Remove patch target does not exist",
                         std::string{pathReader->fieldPath()});
                continue;
            }
            targetObject->erase(existing);
        } else if (*operation == "replace") {
            if (existing == targetObject->end()) {
                addError(diagnostics, "chart.patch.target_missing",
                         "Replace patch target does not exist",
                         std::string{pathReader->fieldPath()});
                continue;
            }
            existing->second = valueReader->value();
        } else {
            targetObject->insert_or_assign(std::move(key), valueReader->value());
        }
    }
}

struct RawTemplate final {
    ChartTemplateId id;
    std::optional<std::string> name;
    std::optional<ChartTemplateId> extends;
    std::optional<json::Value> prototype;
    json::Value::Array patches;
    OpaqueJson extensions;
    std::string path;
    enum class State { Unvisited, Visiting, Resolved, Failed } state{State::Unvisited};
    std::optional<json::Value> expanded;
};

[[nodiscard]] auto resolveTemplate(const std::string& id,
                                   std::map<std::string, RawTemplate>& templates,
                                   const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<json::Value> {
    std::vector<std::string> chain;
    std::string current = id;
    std::optional<json::Value> components;
    bool failed = false;

    while (true) {
        auto& item = templates.at(current);
        if (item.state == RawTemplate::State::Resolved) {
            components = item.expanded;
            break;
        }
        if (item.state == RawTemplate::State::Failed) {
            failed = true;
            break;
        }
        if (item.state == RawTemplate::State::Visiting) {
            addError(diagnostics, "chart.template.inheritance_cycle",
                     "Template inheritance contains a cycle",
                     json::appendFieldPath(item.path, "extends"));
            failed = true;
            break;
        }

        item.state = RawTemplate::State::Visiting;
        chain.push_back(current);
        if (!item.extends) {
            if (item.prototype) {
                components = *item.prototype;
            } else {
                failed = true;
            }
            break;
        }

        const auto parent = templates.find(item.extends->value);
        if (parent == templates.end()) {
            addError(diagnostics, "chart.template.parent_missing",
                     "Template extends a missing template",
                     json::appendFieldPath(item.path, "extends"));
            failed = true;
            break;
        }
        current = parent->first;
    }

    if (failed || !components) {
        for (const auto& templateId : chain) {
            templates.at(templateId).state = RawTemplate::State::Failed;
        }
        return std::nullopt;
    }

    while (!chain.empty()) {
        auto& item = templates.at(chain.back());
        if (item.extends) {
            applyPatches(*components, item.patches, json::appendFieldPath(item.path, "patch"),
                         limits, diagnostics);
        }
        item.expanded = *components;
        item.state = RawTemplate::State::Resolved;
        chain.pop_back();
    }
    return templates.at(id).expanded;
}

[[nodiscard]] auto parseTemplates(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics)
    -> std::pair<std::vector<ChartTemplate>, std::map<std::string, json::Value>> {
    std::vector<ChartTemplate> result;
    std::map<std::string, json::Value> expandedById;
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return {std::move(result), std::move(expandedById)};
    }
    if (values->size() > limits.maxTemplates) {
        addError(diagnostics, "chart.limit.templates", "Template count exceeds configured limit",
                 std::string{reader.fieldPath()});
        return {std::move(result), std::move(expandedById)};
    }

    std::map<std::string, RawTemplate> templates;
    for (std::size_t index = 0; index < values->size(); ++index) {
        if (diagnostics.limitReached()) {
            break;
        }
        const auto itemReader = reader.element(index);
        if (!itemReader || itemReader->readObject() == nullptr) {
            continue;
        }
        constexpr std::array knownFields{
            std::string_view{"id"},      std::string_view{"name"},
            std::string_view{"extends"}, std::string_view{"prototype"},
            std::string_view{"patch"},   std::string_view{"extensions"}};
        itemReader->rejectUnknownFields(knownFields);
        const auto idReader = itemReader->requiredField("id");
        const auto extendsReader = itemReader->requiredField("extends");
        const auto extensionsReader = itemReader->requiredField("extensions");
        if (!idReader || !extendsReader || !extensionsReader) {
            continue;
        }
        const auto id = readIdentifier(*idReader, limits, diagnostics, "Template ID");
        if (!id) {
            continue;
        }
        if (!isChartEntityUuid(*id)) {
            addError(diagnostics, "chart.uuid.invalid_entity_id",
                     "Template ID must be a canonical UUIDv7 or imported UUIDv5",
                     std::string{idReader->fieldPath()});
            continue;
        }
        if (templates.contains(*id)) {
            addError(diagnostics, "chart.template.id_duplicate", "Template ID must be unique",
                     std::string{idReader->fieldPath()});
            continue;
        }

        RawTemplate raw{.id = ChartTemplateId{*id},
                        .name = readNullableName(*itemReader, limits, diagnostics),
                        .extends = std::nullopt,
                        .prototype = std::nullopt,
                        .patches = {},
                        .extensions = {},
                        .path = std::string{itemReader->fieldPath()},
                        .state = RawTemplate::State::Unvisited,
                        .expanded = std::nullopt};
        if (extendsReader->value().isNull()) {
            const auto prototypeReader = itemReader->requiredField("prototype");
            if (itemReader->optionalField("patch")) {
                addError(diagnostics, "chart.template.shape_invalid",
                         "Root template must not contain patch",
                         json::appendFieldPath(itemReader->fieldPath(), "patch"));
            }
            if (prototypeReader) {
                constexpr std::array prototypeFields{std::string_view{"components"}};
                prototypeReader->rejectUnknownFields(prototypeFields);
                const auto componentsReader = prototypeReader->requiredField("components");
                if (componentsReader) {
                    raw.prototype = componentsReader->value();
                }
            }
        } else {
            const auto parent = readReference(*extendsReader, "template", limits, diagnostics);
            if (parent) {
                raw.extends = ChartTemplateId{*parent};
            }
            if (itemReader->optionalField("prototype")) {
                addError(diagnostics, "chart.template.shape_invalid",
                         "Derived template must not contain prototype",
                         json::appendFieldPath(itemReader->fieldPath(), "prototype"));
            }
            const auto patchReader = itemReader->requiredField("patch");
            if (patchReader) {
                if (const auto* patches = patchReader->readArray()) {
                    raw.patches = *patches;
                }
            }
        }
        if (extensionsReader->readObject() != nullptr) {
            raw.extensions =
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
        templates.emplace(*id, std::move(raw));
    }

    for (auto& [id, item] : templates) {
        if (diagnostics.limitReached()) {
            break;
        }
        auto expanded = resolveTemplate(id, templates, limits, diagnostics);
        if (!expanded) {
            continue;
        }
        const auto typed =
            parseComponents(*expanded, json::appendFieldPath(item.path, "prototype/components"),
                            limits, diagnostics, false);
        if (!typed) {
            continue;
        }
        expandedById.emplace(id, *expanded);
        result.push_back(ChartTemplate{item.id, item.name, item.extends, *typed, item.extensions});
    }
    return {std::move(result), std::move(expandedById)};
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
        const bool versionValid = readVersion(*versionReader, diagnostics);
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
                     "Behavior type is unsupported in stage 1A",
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
            result.push_back(
                ChartBehavior{BehaviorId{*id}, std::string{*type}, 1, std::move(behaviorTracks)});
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
                        applyPatches(expandedComponents, *overrides, overridesReader->fieldPath(),
                                     limits, diagnostics);
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

[[nodiscard]] auto parseTiming(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<ChartTiming> {
    constexpr std::array knownFields{std::string_view{"offsetMs"}, std::string_view{"defaultBpm"},
                                     std::string_view{"bpmChanges"}, std::string_view{"stops"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(knownFields);
    const auto offsetReader = reader.requiredField("offsetMs");
    const auto bpmReader = reader.requiredField("defaultBpm");
    const auto changesReader = reader.requiredField("bpmChanges");
    const auto stopsReader = reader.requiredField("stops");
    if (!offsetReader || !bpmReader || !changesReader || !stopsReader) {
        return std::nullopt;
    }
    const auto offset = offsetReader->readNumber();
    const auto bpm = bpmReader->readNumber();
    const auto* changes = changesReader->readArray();
    const auto* stops = stopsReader->readArray();
    if (!offset || !bpm || changes == nullptr || stops == nullptr) {
        return std::nullopt;
    }
    if (!std::isfinite(*offset)) {
        addError(diagnostics, "chart.timing.non_finite", "Offset must be finite",
                 std::string{offsetReader->fieldPath()});
    }
    if (!std::isfinite(*bpm) || *bpm <= 0.0) {
        addError(diagnostics, "chart.timing.invalid_bpm", "Default BPM must be finite and positive",
                 std::string{bpmReader->fieldPath()});
    }
    if (!changes->empty()) {
        addError(diagnostics, "chart.timing.bpm_changes_unsupported",
                 "BPM changes are unsupported in stage 1A",
                 std::string{changesReader->fieldPath()});
    }
    if (!stops->empty()) {
        addError(diagnostics, "chart.timing.stops_unsupported", "Stops are unsupported in stage 1A",
                 std::string{stopsReader->fieldPath()});
    }
    if (!std::isfinite(*offset) || !std::isfinite(*bpm) || *bpm <= 0.0 || !changes->empty() ||
        !stops->empty()) {
        return std::nullopt;
    }
    return ChartTiming{*offset, *bpm};
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

    json::Reader root{*parsed, diagnostics};
    constexpr std::array knownFields{
        std::string_view{"format"},    std::string_view{"version"},
        std::string_view{"chartId"},   std::string_view{"metadata"},
        std::string_view{"timing"},    std::string_view{"camera"},
        std::string_view{"templates"}, std::string_view{"behaviors"},
        std::string_view{"objects"},   std::string_view{"requiredExtensions"},
        std::string_view{"extensions"}};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    root.rejectUnknownFields(knownFields);

    const auto formatReader = root.requiredField("format");
    const auto versionReader = root.requiredField("version");
    const auto chartIdReader = root.requiredField("chartId");
    const auto metadataReader = root.requiredField("metadata");
    const auto timingReader = root.requiredField("timing");
    const auto cameraReader = root.optionalField("camera");
    const auto templatesReader = root.requiredField("templates");
    const auto behaviorsReader = root.requiredField("behaviors");
    const auto objectsReader = root.requiredField("objects");
    const auto requiredExtensionsReader = root.requiredField("requiredExtensions");
    const auto extensionsReader = root.requiredField("extensions");

    std::optional<ChartId> chartId;
    if (formatReader) {
        const auto format = formatReader->readString();
        if (format && *format != "cuexis.chart") {
            addError(diagnostics, "chart.format.invalid", "Expected canonical cuexis.chart format",
                     std::string{formatReader->fieldPath()});
        }
    }
    if (versionReader) {
        static_cast<void>(readVersion(*versionReader, diagnostics));
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
    const auto timing = timingReader ? parseTiming(*timingReader, diagnostics) : std::nullopt;
    const auto camera = cameraReader ? readCamera(*cameraReader, diagnostics) : CameraData{};
    auto [templates, expandedTemplates] =
        templatesReader
            ? parseTemplates(*templatesReader, limits, diagnostics)
            : std::pair<std::vector<ChartTemplate>, std::map<std::string, json::Value>>{};
    auto behaviors = behaviorsReader ? parseBehaviors(*behaviorsReader, limits, diagnostics)
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
                         "Required extension has no registered stage 1A handler",
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

    if (!chartId || !timing || diagnostics.hasErrors()) {
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
                           std::move(extensions)};
    auto semantic = ChartCompiler::compile(document, limits);
    diagnostics.append(std::move(semantic.diagnostics));
    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    return ChartDocumentResult{std::move(document), std::move(diagnostics)};
}

} // namespace cuexis::chart
