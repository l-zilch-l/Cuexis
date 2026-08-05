#include <cuexis/chart/chart_migrator.hpp>

#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/value.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart {
namespace {

using Array = json::Value::Array;
using Object = json::Value::Object;

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string fieldPath = {}) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(fieldPath)});
}

[[nodiscard]] auto beatValue(const RationalBeat& beat) -> json::Value {
    Object result;
    result.emplace("denominator", json::Value{beat.denominator()});
    result.emplace("numerator", json::Value{beat.numerator()});
    return json::Value{std::move(result)};
}

[[nodiscard]] auto vec3Value(const core::Vec3& value) -> json::Value {
    Array result;
    result.emplace_back(static_cast<double>(value.x));
    result.emplace_back(static_cast<double>(value.y));
    result.emplace_back(static_cast<double>(value.z));
    return json::Value{std::move(result)};
}

[[nodiscard]] auto quatValue(const core::Quat& value) -> json::Value {
    Array result;
    result.emplace_back(static_cast<double>(value.x));
    result.emplace_back(static_cast<double>(value.y));
    result.emplace_back(static_cast<double>(value.z));
    result.emplace_back(static_cast<double>(value.w));
    return json::Value{std::move(result)};
}

[[nodiscard]] auto behaviorValue(const BehaviorValue& value) -> json::Value {
    return std::visit(
        [](const auto& item) -> json::Value {
            using Value = std::remove_cvref_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, double>) {
                return json::Value{item};
            } else if constexpr (std::is_same_v<Value, core::Vec3>) {
                return vec3Value(item);
            } else {
                return quatValue(item);
            }
        },
        value);
}

[[nodiscard]] auto referenceValue(std::string_view domain, std::string value) -> json::Value {
    Object result;
    result.emplace("domain", json::Value{std::string{domain}});
    result.emplace("id", json::Value{std::move(value)});
    return json::Value{std::move(result)};
}

[[nodiscard]] auto opaqueValue(const OpaqueJson& value) -> core::Result<json::Value> {
    return json::parse(value.canonicalText, {.maxBytes = value.canonicalText.size() + 1,
                                             .maxDepth = 64,
                                             .maxStringBytes = value.canonicalText.size() + 1});
}

[[nodiscard]] auto transformValue(const TransformData& transform) -> json::Value {
    Object result;
    result.emplace("position", vec3Value(transform.position));
    result.emplace("rotation", quatValue(transform.rotation));
    result.emplace("scale", vec3Value(transform.scale));
    result.emplace("version", json::Value{std::uint64_t{1}});
    return json::Value{std::move(result)};
}

[[nodiscard]] auto componentsValue(const ObjectComponents& components) -> json::Value {
    Object result;
    if (components.transform) {
        result.emplace("cuexis.transform", transformValue(*components.transform));
    }
    if (components.renderable) {
        Object renderable;
        renderable.emplace("material",
                           referenceValue("asset", components.renderable->material.value));
        renderable.emplace("mesh", referenceValue("asset", components.renderable->mesh.value));
        renderable.emplace("version", json::Value{std::uint64_t{1}});
        result.emplace("cuexis.renderable", json::Value{std::move(renderable)});
    }
    if (components.behavior) {
        Object behavior;
        behavior.emplace("behavior",
                         referenceValue("behavior", components.behavior->behavior.value));
        behavior.emplace("version", json::Value{std::uint64_t{1}});
        result.emplace("cuexis.behavior", json::Value{std::move(behavior)});
    }
    if (components.note) {
        Object note;
        if (components.note->beat) {
            note.emplace("beat", beatValue(*components.note->beat));
        }
        note.emplace("version", json::Value{std::uint64_t{1}});
        result.emplace("cuexis.note", json::Value{std::move(note)});
    }
    if (components.element) {
        Object element;
        element.emplace("version", json::Value{std::uint64_t{1}});
        result.emplace("cuexis.element", json::Value{std::move(element)});
    }
    if (components.camera) {
        Object camera;
        camera.emplace("far", json::Value{components.camera->farPlane});
        camera.emplace("fovY", json::Value{components.camera->fovY});
        camera.emplace("near", json::Value{components.camera->nearPlane});
        camera.emplace("type", json::Value{components.camera->type});
        camera.emplace("version", json::Value{std::uint64_t{1}});
        result.emplace("cuexis.camera", json::Value{std::move(camera)});
    }
    return json::Value{std::move(result)};
}

[[nodiscard]] auto propertyName(BehaviorProperty property) -> std::string_view {
    switch (property) {
    case BehaviorProperty::TransformPositionX:
        return "transform.position.x";
    case BehaviorProperty::TransformPositionY:
        return "transform.position.y";
    case BehaviorProperty::TransformPositionZ:
        return "transform.position.z";
    case BehaviorProperty::TransformRotation:
        return "transform.rotation";
    case BehaviorProperty::TransformScale:
        return "transform.scale";
    case BehaviorProperty::CameraFovY:
        return "camera.fovY";
    case BehaviorProperty::MaterialOpacity:
        return "material.opacity";
    case BehaviorProperty::MaterialTint:
        return "material.tint";
    }
    return "";
}

[[nodiscard]] auto eventValue(const BehaviorEvent& event) -> json::Value {
    Object result;
    result.emplace("durationBeats", beatValue(event.durationBeats));
    result.emplace("endSlope", json::Value{event.endSlope});
    result.emplace("endValue", behaviorValue(event.endValue));
    if (event.groupId) {
        result.emplace("groupId", json::Value{*event.groupId});
    }
    result.emplace("property", json::Value{std::string{propertyName(event.property)}});
    result.emplace("startBeat", beatValue(event.startBeat));
    result.emplace("startSlope", json::Value{event.startSlope});
    result.emplace("startValue", behaviorValue(event.startValue));
    return json::Value{std::move(result)};
}

[[nodiscard]] auto serializeDocument(const ChartDocument& document) -> core::Result<std::string> {
    Object root;
    if (document.audio) {
        Object audio;
        audio.emplace("mainMusic", referenceValue("asset", document.audio->mainMusic.value));
        audio.emplace("version", json::Value{std::uint64_t{1}});
        root.emplace("audio", json::Value{std::move(audio)});
    }

    Array behaviors;
    for (const auto& behavior : document.behaviors) {
        Object item;
        Array events;
        for (const auto& event : behavior.events) {
            events.push_back(eventValue(event));
        }
        item.emplace("events", json::Value{std::move(events)});
        item.emplace("id", json::Value{behavior.id.value});
        item.emplace("stepEvents", json::Value{Array{}});
        item.emplace("type", json::Value{behavior.type});
        item.emplace("version", json::Value{std::uint64_t{behavior.version}});
        behaviors.emplace_back(json::Value{std::move(item)});
    }
    root.emplace("behaviors", json::Value{std::move(behaviors)});

    Object camera;
    if (document.camera.defaultTransform) {
        Object transform;
        transform.emplace("position", vec3Value(document.camera.defaultTransform->position));
        camera.emplace("defaultTransform", json::Value{std::move(transform)});
    }
    camera.emplace("far", json::Value{document.camera.farPlane});
    camera.emplace("fovY", json::Value{document.camera.fovY});
    camera.emplace("near", json::Value{document.camera.nearPlane});
    camera.emplace("pitch", json::Value{document.camera.pitch});
    camera.emplace("roll", json::Value{document.camera.roll});
    camera.emplace("type", json::Value{document.camera.type});
    camera.emplace("yaw", json::Value{document.camera.yaw});
    root.emplace("camera", json::Value{std::move(camera)});
    root.emplace("chartId", json::Value{document.chartId.value});

    auto extensions = opaqueValue(document.extensions);
    auto metadata = opaqueValue(document.metadata.data);
    if (!extensions || !metadata) {
        return core::unexpected(
            core::Error{"chart.migration.opaque_json_invalid",
                        "Chart metadata or extensions could not be serialized"});
    }
    root.emplace("extensions", std::move(*extensions));
    root.emplace("format", json::Value{"cuexis.chart"});
    root.emplace("metadata", std::move(*metadata));

    Array objects;
    for (const auto& object : document.objects) {
        Object item;
        item.emplace("components", componentsValue(object.components));
        auto objectExtensions = opaqueValue(object.extensions);
        if (!objectExtensions) {
            return core::unexpected(std::move(objectExtensions.error()));
        }
        item.emplace("extensions", std::move(*objectExtensions));
        item.emplace("id", json::Value{object.id.value});
        if (object.name) {
            item.emplace("name", json::Value{*object.name});
        }
        item.emplace("parent", object.parent ? referenceValue("object", object.parent->value)
                                             : json::Value{nullptr});
        objects.emplace_back(json::Value{std::move(item)});
    }
    root.emplace("objects", json::Value{std::move(objects)});
    root.emplace("requiredExtensions", json::Value{Array{}});
    root.emplace("templates", json::Value{Array{}});

    Object timing;
    timing.emplace("defaultBpm", json::Value{document.timing.defaultBpm});
    timing.emplace("offsetMs", json::Value{document.timing.offsetMs});
    timing.emplace("stops", json::Value{Array{}});
    timing.emplace("tempoEvents", json::Value{Array{}});
    root.emplace("timing", json::Value{std::move(timing)});
    root.emplace("version", json::Value{std::uint64_t{3}});

    auto serialized = json::serialize(json::Value{std::move(root)}, json::SerializeStyle::Pretty);
    if (serialized) {
        serialized->push_back('\n');
    }
    return serialized;
}

[[nodiscard]] auto serializeReport(const ChartMigrationReport& report)
    -> core::Result<std::string> {
    Object root;
    root.emplace("convertedBehaviors", json::Value{std::uint64_t{report.convertedBehaviors}});
    root.emplace("expandedTemplateObjects",
                 json::Value{std::uint64_t{report.expandedTemplateObjects}});
    root.emplace("generatedEvents", json::Value{std::uint64_t{report.generatedEvents}});
    root.emplace("rewrittenBindings", json::Value{std::uint64_t{report.rewrittenBindings}});
    root.emplace("sourceVersion", json::Value{std::uint64_t{report.sourceVersion}});
    root.emplace("targetVersion", json::Value{std::uint64_t{report.targetVersion}});
    Array unbound;
    for (const auto& id : report.unboundBehaviorIds) {
        unbound.emplace_back(id);
    }
    root.emplace("unboundBehaviorIds", json::Value{std::move(unbound)});
    auto serialized = json::serialize(json::Value{std::move(root)}, json::SerializeStyle::Pretty);
    if (serialized) {
        serialized->push_back('\n');
    }
    return serialized;
}

[[nodiscard]] auto negateBeat(const RationalBeat& value) -> core::Result<RationalBeat> {
    if (value.numerator() == std::numeric_limits<std::int64_t>::min()) {
        return core::unexpected(
            core::Error{"chart.beat.out_of_range", "Beat subtraction overflowed"});
    }
    return RationalBeat::create(-value.numerator(), value.denominator());
}

[[nodiscard]] auto subtractBeat(const RationalBeat& right, const RationalBeat& left)
    -> core::Result<RationalBeat> {
    auto negative = negateBeat(left);
    if (!negative) {
        return core::unexpected(std::move(negative.error()));
    }
    return addRationalBeats(right, *negative);
}

[[nodiscard]] auto withinBeatLimits(const RationalBeat& value, const ChartLimits& limits) noexcept
    -> bool {
    const auto numerator = value.numerator();
    const auto magnitude = numerator < 0 ? static_cast<std::uint64_t>(-(numerator + 1)) + 1U
                                         : static_cast<std::uint64_t>(numerator);
    return magnitude <= static_cast<std::uint64_t>(limits.maxBeatNumeratorMagnitude) &&
           value.denominator() <= limits.maxBeatDenominator;
}

[[nodiscard]] auto midpointValue(const BehaviorValue& left, const BehaviorValue& right)
    -> core::Result<BehaviorValue> {
    if (const auto* leftScalar = std::get_if<double>(&left)) {
        const auto* rightScalar = std::get_if<double>(&right);
        if (rightScalar == nullptr) {
            return core::unexpected(
                core::Error{"chart.migration.value_type_mismatch", "Behavior key types differ"});
        }
        return BehaviorValue{*leftScalar + (*rightScalar - *leftScalar) * 0.5};
    }
    if (const auto* leftVector = std::get_if<core::Vec3>(&left)) {
        const auto* rightVector = std::get_if<core::Vec3>(&right);
        if (rightVector == nullptr) {
            return core::unexpected(
                core::Error{"chart.migration.value_type_mismatch", "Behavior key types differ"});
        }
        return BehaviorValue{core::Vec3{(leftVector->x + rightVector->x) * 0.5F,
                                        (leftVector->y + rightVector->y) * 0.5F,
                                        (leftVector->z + rightVector->z) * 0.5F}};
    }
    const auto* leftRotation = std::get_if<core::Quat>(&left);
    const auto* rightRotation = std::get_if<core::Quat>(&right);
    if (leftRotation == nullptr || rightRotation == nullptr) {
        return core::unexpected(
            core::Error{"chart.migration.value_type_mismatch", "Behavior key types differ"});
    }
    auto target = *rightRotation;
    const double dot = static_cast<double>(leftRotation->x) * target.x +
                       static_cast<double>(leftRotation->y) * target.y +
                       static_cast<double>(leftRotation->z) * target.z +
                       static_cast<double>(leftRotation->w) * target.w;
    if (dot < 0.0) {
        target = core::Quat{-target.x, -target.y, -target.z, -target.w};
    }
    auto midpoint =
        core::normalize(core::Quat{leftRotation->x + target.x, leftRotation->y + target.y,
                                   leftRotation->z + target.z, leftRotation->w + target.w});
    if (!midpoint) {
        return core::unexpected(core::Error{"chart.migration.quaternion_midpoint_invalid",
                                            "Quaternion midpoint could not be normalized"});
    }
    return BehaviorValue{*midpoint};
}

[[nodiscard]] auto rewriteBaseline(ObjectComponents& components, BehaviorProperty property,
                                   const BehaviorValue& value) -> bool {
    switch (property) {
    case BehaviorProperty::TransformPositionX:
    case BehaviorProperty::TransformPositionY:
    case BehaviorProperty::TransformPositionZ: {
        auto* scalar = std::get_if<double>(&value);
        if (!components.transform || scalar == nullptr) {
            return false;
        }
        auto& position = components.transform->position;
        if (property == BehaviorProperty::TransformPositionX) {
            position.x = static_cast<float>(*scalar);
        } else if (property == BehaviorProperty::TransformPositionY) {
            position.y = static_cast<float>(*scalar);
        } else {
            position.z = static_cast<float>(*scalar);
        }
        return true;
    }
    case BehaviorProperty::TransformRotation: {
        const auto* rotation = std::get_if<core::Quat>(&value);
        if (!components.transform || rotation == nullptr) {
            return false;
        }
        components.transform->rotation = *rotation;
        return true;
    }
    case BehaviorProperty::TransformScale: {
        const auto* scale = std::get_if<core::Vec3>(&value);
        if (!components.transform || scale == nullptr) {
            return false;
        }
        components.transform->scale = *scale;
        return true;
    }
    case BehaviorProperty::CameraFovY: {
        const auto* fov = std::get_if<double>(&value);
        if (!components.camera || fov == nullptr) {
            return false;
        }
        components.camera->fovY = *fov;
        return true;
    }
    case BehaviorProperty::MaterialOpacity:
    case BehaviorProperty::MaterialTint:
        return false;
    }
    return false;
}

void appendInterval(std::vector<BehaviorEvent>& events, BehaviorProperty property,
                    const BehaviorKey& left, const BehaviorKey& right, const ChartLimits& limits,
                    core::Diagnostics& diagnostics, std::string_view fieldPath) {
    auto duration = subtractBeat(right.beat, left.beat);
    if (!duration || duration->numerator() <= 0 || !withinBeatLimits(*duration, limits)) {
        addError(diagnostics, "chart.migration.duration_out_of_range",
                 "Behavior interval duration cannot be represented in the v3 Beat budget",
                 std::string{fieldPath});
        return;
    }
    const auto easing = right.easing.value_or(BehaviorEasing::Linear);
    if (easing != BehaviorEasing::InOutCubic) {
        double startSlope = 1.0;
        double endSlope = 1.0;
        if (easing == BehaviorEasing::InCubic) {
            startSlope = 0.0;
            endSlope = 3.0;
        } else if (easing == BehaviorEasing::OutCubic) {
            startSlope = 3.0;
            endSlope = 0.0;
        }
        events.push_back(BehaviorEvent{property, left.beat, *duration, left.value, right.value,
                                       startSlope, endSlope, std::nullopt});
        return;
    }

    auto midpointBeat = rationalBeatMidpoint(left.beat, right.beat);
    if (!midpointBeat || !withinBeatLimits(*midpointBeat, limits)) {
        addError(diagnostics, "chart.migration.midpoint_out_of_range",
                 "in_out_cubic midpoint cannot be represented in the v3 Beat budget",
                 std::string{fieldPath});
        return;
    }
    auto firstDuration = subtractBeat(*midpointBeat, left.beat);
    auto secondDuration = subtractBeat(right.beat, *midpointBeat);
    auto midpoint = midpointValue(left.value, right.value);
    if (!firstDuration || !secondDuration || !midpoint ||
        !withinBeatLimits(*firstDuration, limits) || !withinBeatLimits(*secondDuration, limits)) {
        addError(diagnostics, "chart.migration.midpoint_invalid",
                 "in_out_cubic midpoint value or duration could not be represented",
                 std::string{fieldPath});
        return;
    }
    events.push_back(BehaviorEvent{property, left.beat, *firstDuration, left.value, *midpoint, 0.0,
                                   3.0, std::nullopt});
    events.push_back(BehaviorEvent{property, *midpointBeat, *secondDuration, *midpoint, right.value,
                                   3.0, 0.0, std::nullopt});
}

} // namespace

auto ChartMigrator::migrateToV3(std::string_view sourceJson, const ChartLimits& limits)
    -> ChartMigrationResult {
    ChartMigrationResult result;
    auto loaded = ChartLoader::load(sourceJson, limits);
    const bool loadedValid = loaded.hasValue();
    result.diagnostics.append(std::move(loaded.diagnostics));
    if (!loadedValid) {
        result.diagnostics.sortDeterministically();
        return result;
    }
    if (loaded.document->version != 1 && loaded.document->version != 2) {
        addError(result.diagnostics, "chart.migration.source_version_unsupported",
                 "Only canonical Chart v1 and v2 can be migrated to v3", "$/version");
        return result;
    }
    auto sourceRuntime = ChartCompiler::compile(*loaded.document, limits);
    result.diagnostics.append(std::move(sourceRuntime.diagnostics));
    if (!sourceRuntime.hasValue()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    ChartDocument migrated = *loaded.document;
    ChartMigrationReport report{.sourceVersion = migrated.version};
    migrated.version = 3;
    migrated.timing.tempoEvents.clear();
    migrated.timing.stops.clear();
    report.expandedTemplateObjects = static_cast<std::size_t>(
        std::count_if(migrated.objects.begin(), migrated.objects.end(),
                      [](const auto& object) { return object.sourceTemplate.has_value(); }));
    migrated.templates.clear();
    for (auto& object : migrated.objects) {
        object.sourceTemplate.reset();
    }
    std::sort(migrated.objects.begin(), migrated.objects.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });
    std::sort(migrated.behaviors.begin(), migrated.behaviors.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });

    std::vector<std::string> emptyBehaviorIds;

    for (std::size_t behaviorIndex = 0; behaviorIndex < migrated.behaviors.size();
         ++behaviorIndex) {
        auto& behavior = migrated.behaviors[behaviorIndex];
        std::vector<ChartObject*> bindings;
        for (auto& object : migrated.objects) {
            if (object.components.behavior && object.components.behavior->behavior == behavior.id) {
                bindings.push_back(&object);
            }
        }
        if (bindings.empty()) {
            report.unboundBehaviorIds.push_back(behavior.id.value);
        }

        std::sort(
            behavior.tracks.items.begin(), behavior.tracks.items.end(),
            [](const auto& left, const auto& right) { return left.property < right.property; });
        std::vector<BehaviorEvent> events;
        for (std::size_t trackIndex = 0; trackIndex < behavior.tracks.items.size(); ++trackIndex) {
            auto track = behavior.tracks.items[trackIndex];
            std::sort(track.keys.begin(), track.keys.end(),
                      [](const auto& left, const auto& right) { return left.beat < right.beat; });
            if (track.keys.empty()) {
                continue;
            }
            for (auto* object : bindings) {
                if (!rewriteBaseline(object->components, track.property,
                                     track.keys.front().value)) {
                    addError(result.diagnostics, "chart.migration.baseline_target_invalid",
                             "Behavior first key cannot be written to its binding baseline",
                             "$/behaviors/" + std::to_string(behaviorIndex) + "/tracks/" +
                                 std::to_string(trackIndex));
                } else {
                    ++report.rewrittenBindings;
                }
            }
            for (std::size_t keyIndex = 1; keyIndex < track.keys.size(); ++keyIndex) {
                appendInterval(events, track.property, track.keys[keyIndex - 1],
                               track.keys[keyIndex], limits, result.diagnostics,
                               "$/behaviors/" + std::to_string(behaviorIndex) + "/tracks/" +
                                   std::to_string(trackIndex) + "/keys/" +
                                   std::to_string(keyIndex));
            }
        }
        behavior.type = "behavior.event";
        behavior.version = 1;
        behavior.tracks = {};
        behavior.events = std::move(events);
        behavior.stepEvents.clear();
        report.generatedEvents += behavior.events.size();
        if (behavior.events.empty()) {
            emptyBehaviorIds.push_back(behavior.id.value);
            for (auto* object : bindings) {
                object->components.behavior.reset();
            }
        }
        ++report.convertedBehaviors;
    }
    migrated.behaviors.erase(std::remove_if(migrated.behaviors.begin(), migrated.behaviors.end(),
                                            [&emptyBehaviorIds](const auto& behavior) {
                                                return std::binary_search(emptyBehaviorIds.begin(),
                                                                          emptyBehaviorIds.end(),
                                                                          behavior.id.value);
                                            }),
                             migrated.behaviors.end());
    std::sort(report.unboundBehaviorIds.begin(), report.unboundBehaviorIds.end());
    if (result.diagnostics.hasErrors()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    auto compiled = ChartCompiler::compile(migrated, limits);
    result.diagnostics.append(std::move(compiled.diagnostics));
    if (!compiled.hasValue()) {
        result.diagnostics.sortDeterministically();
        return result;
    }
    auto chartJson = serializeDocument(migrated);
    auto reportJson = serializeReport(report);
    if (!chartJson || !reportJson) {
        addError(result.diagnostics, "chart.migration.serialize_failed",
                 "Migrated Chart or report serialization failed");
        return result;
    }
    result.artifact.emplace(ChartMigrationArtifact{.document = std::move(migrated),
                                                   .report = std::move(report),
                                                   .chartJson = std::move(*chartJson),
                                                   .reportJson = std::move(*reportJson)});
    result.diagnostics.sortDeterministically();
    return result;
}

} // namespace cuexis::chart
