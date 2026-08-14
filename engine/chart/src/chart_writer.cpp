#include <cuexis/chart/chart_writer.hpp>

#include <cuexis/core/error.hpp>
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

[[nodiscard]] auto behaviorPropertyName(BehaviorProperty property) -> std::string_view {
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

[[nodiscard]] auto behaviorStepPropertyName(BehaviorStepProperty property) -> std::string_view {
    switch (property) {
    case BehaviorStepProperty::RenderVisible:
        return "render.visible";
    case BehaviorStepProperty::RenderMaterial:
        return "render.material";
    }
    return "";
}

[[nodiscard]] auto animationPropertyName(AnimationProperty property) -> std::string_view {
    switch (property) {
    case AnimationProperty::TransformPositionX:
        return "transform.position.x";
    case AnimationProperty::TransformPositionY:
        return "transform.position.y";
    case AnimationProperty::TransformPositionZ:
        return "transform.position.z";
    case AnimationProperty::TransformRotation:
        return "transform.rotation";
    case AnimationProperty::TransformScale:
        return "transform.scale";
    case AnimationProperty::MaterialOpacity:
        return "material.opacity";
    case AnimationProperty::MaterialTint:
        return "material.tint";
    }
    return "";
}

[[nodiscard]] auto animationStepPropertyName(AnimationStepProperty property) -> std::string_view {
    switch (property) {
    case AnimationStepProperty::RenderVisible:
        return "render.visible";
    case AnimationStepProperty::RenderMaterial:
        return "render.material";
    }
    return "";
}

template <typename Value> [[nodiscard]] auto continuousValue(const Value& value) -> json::Value {
    return std::visit(
        [](const auto& item) -> json::Value {
            using Item = std::remove_cvref_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, double>) {
                return json::Value{item};
            } else if constexpr (std::is_same_v<Item, core::Vec3>) {
                return vec3Value(item);
            } else {
                return quatValue(item);
            }
        },
        value);
}

template <typename Value> [[nodiscard]] auto stepValue(const Value& value) -> json::Value {
    return std::visit(
        [](const auto& item) -> json::Value {
            using Item = std::remove_cvref_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, bool>) {
                return json::Value{item};
            } else {
                return referenceValue("asset", item.value);
            }
        },
        value);
}

[[nodiscard]] auto behaviorEventValue(const BehaviorEvent& event) -> json::Value {
    Object result;
    result.emplace("durationBeats", beatValue(event.durationBeats));
    result.emplace("endSlope", json::Value{event.endSlope});
    result.emplace("endValue", continuousValue(event.endValue));
    if (event.groupId) {
        result.emplace("groupId", json::Value{*event.groupId});
    }
    result.emplace("property", json::Value{std::string{behaviorPropertyName(event.property)}});
    result.emplace("startBeat", beatValue(event.startBeat));
    result.emplace("startSlope", json::Value{event.startSlope});
    result.emplace("startValue", continuousValue(event.startValue));
    return json::Value{std::move(result)};
}

[[nodiscard]] auto behaviorStepEventValue(const BehaviorStepEvent& event) -> json::Value {
    Object result;
    result.emplace("beat", beatValue(event.beat));
    if (event.groupId) {
        result.emplace("groupId", json::Value{*event.groupId});
    }
    result.emplace("property", json::Value{std::string{behaviorStepPropertyName(event.property)}});
    result.emplace("value", stepValue(event.value));
    return json::Value{std::move(result)};
}

[[nodiscard]] auto animationClipValue(const AnimationClip& clip, bool includeId) -> json::Value {
    Object result;
    result.emplace("durationBeats", beatValue(clip.durationBeats));
    if (includeId) {
        result.emplace("id", json::Value{clip.id});
    }

    Array stepTracks;
    for (const auto& track : clip.stepTracks) {
        Object trackValue;
        trackValue.emplace("property",
                           json::Value{std::string{animationStepPropertyName(track.property)}});
        Array steps;
        for (const auto& step : track.steps) {
            Object stepObject;
            stepObject.emplace("beat", beatValue(step.beat));
            stepObject.emplace("value", stepValue(step.value));
            steps.emplace_back(json::Value{std::move(stepObject)});
        }
        trackValue.emplace("steps", json::Value{std::move(steps)});
        stepTracks.emplace_back(json::Value{std::move(trackValue)});
    }
    result.emplace("stepTracks", json::Value{std::move(stepTracks)});

    Array tracks;
    for (const auto& track : clip.tracks) {
        Object trackValue;
        trackValue.emplace("property",
                           json::Value{std::string{animationPropertyName(track.property)}});
        Array segments;
        for (const auto& segment : track.segments) {
            Object segmentValue;
            segmentValue.emplace("durationBeats", beatValue(segment.durationBeats));
            segmentValue.emplace("endSlope", json::Value{segment.endSlope});
            segmentValue.emplace("endValue", continuousValue(segment.endValue));
            segmentValue.emplace("startBeat", beatValue(segment.startBeat));
            segmentValue.emplace("startSlope", json::Value{segment.startSlope});
            segmentValue.emplace("startValue", continuousValue(segment.startValue));
            segments.emplace_back(json::Value{std::move(segmentValue)});
        }
        trackValue.emplace("segments", json::Value{std::move(segments)});
        tracks.emplace_back(json::Value{std::move(trackValue)});
    }
    result.emplace("tracks", json::Value{std::move(tracks)});
    result.emplace("version", json::Value{std::uint64_t{1}});
    return json::Value{std::move(result)};
}

[[nodiscard]] auto requiredExtensionsValue(const std::vector<RequiredExtension>& extensions)
    -> json::Value {
    Array result;
    for (const auto& extension : extensions) {
        Object item;
        item.emplace("id", json::Value{extension.id});
        item.emplace("version", json::Value{std::uint64_t{extension.version}});
        result.emplace_back(json::Value{std::move(item)});
    }
    return json::Value{std::move(result)};
}

[[nodiscard]] auto integerValue(const json::Value& value) -> std::optional<std::int64_t> {
    if (const auto* signedValue = value.signedInteger()) {
        return *signedValue;
    }
    const auto* unsignedValue = value.unsignedInteger();
    if (unsignedValue == nullptr ||
        *unsignedValue > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(*unsignedValue);
}

[[nodiscard]] auto normalizeRational(json::Value& value) -> core::Result<void> {
    auto* object = value.object();
    if (object == nullptr || object->size() != 2 || !object->contains("numerator") ||
        !object->contains("denominator")) {
        return {};
    }
    const auto numerator = integerValue(object->at("numerator"));
    const auto denominator = integerValue(object->at("denominator"));
    if (!numerator || !denominator) {
        return core::unexpected(
            core::Error{"chart.writer.rational_invalid", "Rational value is not integral"});
    }
    auto rational = RationalBeat::create(*numerator, *denominator);
    if (!rational) {
        return core::unexpected(std::move(rational.error()));
    }
    object->insert_or_assign("denominator", json::Value{rational->denominator()});
    object->insert_or_assign("numerator", json::Value{rational->numerator()});
    return {};
}

[[nodiscard]] auto rationalSortKey(const json::Value& value) -> std::optional<RationalBeat> {
    const auto* object = value.object();
    if (object == nullptr) {
        return std::nullopt;
    }
    const auto numeratorValue = value.find("numerator");
    const auto denominatorValue = value.find("denominator");
    if (numeratorValue == nullptr || denominatorValue == nullptr) {
        return std::nullopt;
    }
    const auto numerator = integerValue(*numeratorValue);
    const auto denominator = integerValue(*denominatorValue);
    if (!numerator || !denominator) {
        return std::nullopt;
    }
    auto result = RationalBeat::create(*numerator, *denominator);
    return result ? std::optional<RationalBeat>{*result} : std::nullopt;
}

template <typename ComparePrimary>
[[nodiscard]] auto sortWithCanonicalTieBreak(json::Value& value, ComparePrimary comparePrimary)
    -> core::Result<void> {
    auto* array = value.array();
    if (array == nullptr) {
        return {};
    }

    std::ranges::sort(*array, [&](const json::Value& left, const json::Value& right) {
        return comparePrimary(left, right) < 0;
    });
    for (std::size_t begin = 0; begin < array->size();) {
        std::size_t end = begin + 1;
        while (end < array->size() && comparePrimary((*array)[begin], (*array)[end]) == 0) {
            ++end;
        }
        if (end - begin > 1) {
            std::vector<std::string> keys;
            keys.reserve(end - begin);
            for (std::size_t index = begin; index < end; ++index) {
                auto serialized = json::serialize((*array)[index]);
                if (!serialized) {
                    return core::unexpected(
                        core::Error{"chart.writer.tie_break_failed",
                                    "Canonical array tie-break serialization failed"}
                            .withCause(std::move(serialized.error())));
                }
                keys.push_back(std::move(*serialized));
            }

            std::vector<std::pair<std::string, json::Value>> decorated;
            decorated.reserve(end - begin);
            for (std::size_t index = begin; index < end; ++index) {
                decorated.emplace_back(std::move(keys[index - begin]), std::move((*array)[index]));
            }
            std::ranges::sort(decorated, {}, &std::pair<std::string, json::Value>::first);
            for (std::size_t index = begin; index < end; ++index) {
                (*array)[index] = std::move(decorated[index - begin].second);
            }
        }
        begin = end;
    }
    return {};
}

[[nodiscard]] auto sortByStringKey(json::Value& value, std::string_view key) -> core::Result<void> {
    return sortWithCanonicalTieBreak(
        value, [key](const json::Value& left, const json::Value& right) {
            const auto* leftValue = left.find(key);
            const auto* rightValue = right.find(key);
            const auto* leftString = leftValue != nullptr ? leftValue->string() : nullptr;
            const auto* rightString = rightValue != nullptr ? rightValue->string() : nullptr;
            if (leftString == nullptr || rightString == nullptr) {
                return leftString != nullptr ? -1 : (rightString != nullptr ? 1 : 0);
            }
            return *leftString < *rightString ? -1 : (*rightString < *leftString ? 1 : 0);
        });
}

[[nodiscard]] auto sortByRationalKey(json::Value& value, std::string_view key)
    -> core::Result<void> {
    return sortWithCanonicalTieBreak(value, [key](const json::Value& left,
                                                  const json::Value& right) {
        const auto* leftValue = left.find(key);
        const auto* rightValue = right.find(key);
        const auto leftBeat = leftValue != nullptr ? rationalSortKey(*leftValue) : std::nullopt;
        const auto rightBeat = rightValue != nullptr ? rationalSortKey(*rightValue) : std::nullopt;
        if (!leftBeat || !rightBeat) {
            return leftBeat ? -1 : (rightBeat ? 1 : 0);
        }
        return *leftBeat < *rightBeat ? -1 : (*rightBeat < *leftBeat ? 1 : 0);
    });
}

[[nodiscard]] auto sortByStringAndRationalKey(json::Value& value, std::string_view stringKey,
                                              std::string_view rationalKey) -> core::Result<void> {
    return sortWithCanonicalTieBreak(value, [stringKey, rationalKey](const json::Value& left,
                                                                     const json::Value& right) {
        const auto* leftStringValue = left.find(stringKey);
        const auto* rightStringValue = right.find(stringKey);
        const auto* leftString = leftStringValue != nullptr ? leftStringValue->string() : nullptr;
        const auto* rightString =
            rightStringValue != nullptr ? rightStringValue->string() : nullptr;
        if (leftString == nullptr || rightString == nullptr) {
            return leftString != nullptr ? -1 : (rightString != nullptr ? 1 : 0);
        }
        if (*leftString != *rightString) {
            return *leftString < *rightString ? -1 : 1;
        }
        const auto* leftRationalValue = left.find(rationalKey);
        const auto* rightRationalValue = right.find(rationalKey);
        const auto leftRational =
            leftRationalValue != nullptr ? rationalSortKey(*leftRationalValue) : std::nullopt;
        const auto rightRational =
            rightRationalValue != nullptr ? rationalSortKey(*rightRationalValue) : std::nullopt;
        if (!leftRational || !rightRational) {
            return leftRational ? -1 : (rightRational ? 1 : 0);
        }
        return *leftRational < *rightRational ? -1 : (*rightRational < *leftRational ? 1 : 0);
    });
}

[[nodiscard]] auto sortStrings(json::Value& value) -> core::Result<void> {
    return sortWithCanonicalTieBreak(value, [](const json::Value& left, const json::Value& right) {
        const auto* leftString = left.string();
        const auto* rightString = right.string();
        if (leftString == nullptr || rightString == nullptr) {
            return leftString != nullptr ? -1 : (rightString != nullptr ? 1 : 0);
        }
        return *leftString < *rightString ? -1 : (*rightString < *leftString ? 1 : 0);
    });
}

[[nodiscard]] auto canonicalize(json::Value& value, bool sortBehaviorArrays, bool opaque = false)
    -> core::Result<void> {
    if (const auto* number = value.number()) {
        if (!std::isfinite(*number)) {
            return core::unexpected(core::Error{"chart.writer.number_non_finite",
                                                "Canonical JSON requires finite numbers"});
        }
        if (*number == 0.0) {
            value = json::Value{0.0};
        }
        return {};
    }
    if (auto* array = value.array()) {
        for (auto& item : *array) {
            auto normalized = canonicalize(item, sortBehaviorArrays, opaque);
            if (!normalized) {
                return normalized;
            }
        }
        return {};
    }
    auto* object = value.object();
    if (object == nullptr) {
        return {};
    }
    if (!opaque) {
        auto normalized = normalizeRational(value);
        if (!normalized) {
            return normalized;
        }
        object = value.object();
    }
    for (auto& [key, child] : *object) {
        const bool childOpaque = opaque || key == "extensions" || key == "metadata";
        auto normalized = canonicalize(child, sortBehaviorArrays, childOpaque);
        if (!normalized) {
            return normalized;
        }
    }
    if (opaque) {
        return {};
    }

    const auto sortField = [&](std::string_view field, std::string_view key) -> core::Result<void> {
        if (auto* child = value.find(field)) {
            return sortByStringKey(*child, key);
        }
        return {};
    };
    constexpr std::array stringSortFields{
        std::pair{std::string_view{"parameters"}, std::string_view{"id"}},
        std::pair{std::string_view{"templates"}, std::string_view{"id"}},
        std::pair{std::string_view{"behaviors"}, std::string_view{"id"}},
        std::pair{std::string_view{"animationTemplateImports"}, std::string_view{"id"}},
        std::pair{std::string_view{"animationClips"}, std::string_view{"id"}},
        std::pair{std::string_view{"objects"}, std::string_view{"id"}},
        std::pair{std::string_view{"requiredExtensions"}, std::string_view{"id"}},
        std::pair{std::string_view{"tracks"}, std::string_view{"property"}},
        std::pair{std::string_view{"stepTracks"}, std::string_view{"property"}},
        std::pair{std::string_view{"templateBindings"}, std::string_view{"bindingId"}},
        std::pair{std::string_view{"layers"}, std::string_view{"layerId"}},
        std::pair{std::string_view{"blendGroups"}, std::string_view{"groupId"}},
        std::pair{std::string_view{"instances"}, std::string_view{"instanceId"}},
    };
    for (const auto& [field, key] : stringSortFields) {
        if (auto sorted = sortField(field, key); !sorted) {
            return sorted;
        }
    }
    if (auto* segments = value.find("segments")) {
        if (auto sorted = sortByRationalKey(*segments, "startBeat"); !sorted) {
            return sorted;
        }
    }
    if (auto* steps = value.find("steps")) {
        if (auto sorted = sortByRationalKey(*steps, "beat"); !sorted) {
            return sorted;
        }
    }
    if (auto* tempoEvents = value.find("tempoEvents")) {
        if (auto sorted = sortByRationalKey(*tempoEvents, "startBeat"); !sorted) {
            return sorted;
        }
    }
    if (auto* stops = value.find("stops")) {
        if (auto sorted = sortByRationalKey(*stops, "beat"); !sorted) {
            return sorted;
        }
    }
    if (sortBehaviorArrays) {
        if (auto* events = value.find("events")) {
            if (auto sorted = sortByStringAndRationalKey(*events, "property", "startBeat");
                !sorted) {
                return sorted;
            }
        }
        if (auto* stepEvents = value.find("stepEvents")) {
            if (auto sorted = sortByStringAndRationalKey(*stepEvents, "property", "beat");
                !sorted) {
                return sorted;
            }
        }
    }
    if (auto* properties = value.find("properties")) {
        if (auto sorted = sortStrings(*properties); !sorted) {
            return sorted;
        }
    }
    if (auto* prefixes = value.find("prefixes")) {
        if (auto sorted = sortStrings(*prefixes); !sorted) {
            return sorted;
        }
    }
    return {};
}

[[nodiscard]] auto serializeCanonical(json::Value value, bool sortBehaviorArrays = false)
    -> core::Result<std::string> {
    auto normalized = canonicalize(value, sortBehaviorArrays);
    if (!normalized) {
        return core::unexpected(std::move(normalized.error()));
    }
    auto serialized = json::serialize(value, json::SerializeStyle::Pretty);
    if (!serialized) {
        return serialized;
    }
    while (!serialized->empty() && (serialized->back() == '\n' || serialized->back() == '\r')) {
        serialized->pop_back();
    }
    serialized->push_back('\n');
    return serialized;
}

[[nodiscard]] auto chartDocumentValue(const ChartDocument& document) -> core::Result<json::Value> {
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
            events.push_back(behaviorEventValue(event));
        }
        item.emplace("events", json::Value{std::move(events)});
        item.emplace("id", json::Value{behavior.id.value});
        Array stepEvents;
        for (const auto& event : behavior.stepEvents) {
            stepEvents.push_back(behaviorStepEventValue(event));
        }
        item.emplace("stepEvents", json::Value{std::move(stepEvents)});
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
            core::Error{"chart.writer.opaque_json_invalid",
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
    return json::Value{std::move(root)};
}

} // namespace

auto ChartWriter::write(const ChartDocument& document) -> core::Result<std::string> {
    auto value = chartDocumentValue(document);
    if (!value) {
        return core::unexpected(std::move(value.error()));
    }
    return serializeCanonical(std::move(*value));
}

auto ChartWriter::writeCanonicalJson(std::string_view jsonText, const ChartLimits& limits)
    -> core::Result<std::string> {
    auto parsed = json::parse(
        jsonText, {limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes});
    if (!parsed) {
        return core::unexpected(
            core::Error{"chart.writer.source_invalid", "Chart source JSON is invalid"}.withCause(
                std::move(parsed.error())));
    }
    return serializeCanonical(std::move(*parsed));
}

auto ChartWriter::writeV4(const ChartV4SourceDocument& document, const ChartLimits& limits)
    -> core::Result<std::string> {
    auto parsed =
        json::parse(document.canonicalSource.canonicalText,
                    {limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes});
    if (!parsed) {
        return core::unexpected(
            core::Error{"chart.writer.source_invalid", "Chart v4 source JSON is invalid"}.withCause(
                std::move(parsed.error())));
    }
    return serializeCanonical(std::move(*parsed), true);
}

auto ChartWriter::writeAnimationTemplate(const AnimationTemplateDocument& document,
                                         const ChartLimits& limits) -> core::Result<std::string> {
    Object root;
    Object application;
    application.emplace("blendMode",
                        json::Value{document.application.blendMode == AnimationBlendMode::Additive
                                        ? "additive"
                                        : "override"});
    application.emplace("coordinateSpace", json::Value{"local"});
    application.emplace(
        "fillMode",
        json::Value{document.application.fillMode == AnimationFillMode::Hold ? "hold" : "none"});
    if (document.application.iterations.infinite) {
        application.emplace("iterations", json::Value{"infinite"});
    } else {
        application.emplace("iterations",
                            json::Value{std::uint64_t{document.application.iterations.count}});
    }
    root.emplace("application", json::Value{std::move(application)});
    root.emplace("clip", animationClipValue(document.clip, false));

    auto extensions = opaqueValue(document.extensions);
    if (!extensions) {
        return core::unexpected(std::move(extensions.error()));
    }
    root.emplace("extensions", std::move(*extensions));
    root.emplace("format", json::Value{"cuexis.animation-template"});
    Object metadata;
    if (document.name) {
        metadata.emplace("name", json::Value{*document.name});
    }
    root.emplace("metadata", json::Value{std::move(metadata)});
    root.emplace("requiredExtensions", requiredExtensionsValue(document.requiredExtensions));
    root.emplace("templateId", json::Value{document.templateId});
    root.emplace("version", json::Value{std::uint64_t{1}});
    auto serialized = serializeCanonical(json::Value{std::move(root)});
    if (serialized && serialized->size() > limits.maxAnimationTemplateBytes) {
        return core::unexpected(core::Error{"cxt.budget.exceeded",
                                            "Canonical animation template exceeds the byte limit"});
    }
    return serialized;
}

} // namespace cuexis::chart
