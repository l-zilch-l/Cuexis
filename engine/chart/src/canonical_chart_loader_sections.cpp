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

} // namespace cuexis::chart
