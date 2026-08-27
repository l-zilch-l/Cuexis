//  ChartCompiler 实现 — ChartDocument → ChartRuntime 编译
//  模板展开、引用解析、排序、定时编译；结果与 objects 数组顺序无关
//  RuntimeObject 使用索引引用父对象；Behavior 仅保留身份/type/version（opaque track）

#include <cuexis/chart/chart_runtime.hpp>

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/math.hpp>

#include "diagnostic_limit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart {
namespace {

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(path)});
}

void addWarning(core::Diagnostics& diagnostics, std::string code, std::string message,
                std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Warning, std::move(code),
                                     std::move(message), std::move(path)});
}

enum class HierarchyState {
    Unvisited,
    Visiting,
    Valid,
    Skipped,
    Cyclic,
};

struct HierarchyValidator final {
    const std::map<std::string, const ChartObject*>& objects;
    core::Diagnostics& diagnostics;
    std::map<std::string, HierarchyState> states;

    [[nodiscard]] auto visit(const std::string& id) -> HierarchyState {
        auto isTerminal = [](HierarchyState state) {
            return state == HierarchyState::Valid || state == HierarchyState::Skipped ||
                   state == HierarchyState::Cyclic;
        };
        if (isTerminal(states[id])) {
            return states[id];
        }

        std::vector<std::string> chain;
        std::string current = id;
        HierarchyState resolvedState = HierarchyState::Unvisited;
        while (true) {
            auto& state = states[current];
            if (isTerminal(state)) {
                resolvedState = state;
                break;
            }
            if (state == HierarchyState::Visiting) {
                addError(diagnostics, "chart.hierarchy.cycle", "Object hierarchy contains a cycle",
                         "$.objects[\"" + current + "\"].parent");
                resolvedState = HierarchyState::Cyclic;
                break;
            }

            state = HierarchyState::Visiting;
            chain.push_back(current);
            const ChartObject& object = *objects.at(current);
            if (!object.parent) {
                resolvedState = HierarchyState::Valid;
                break;
            }
            if (object.parent->value == current) {
                addError(diagnostics, "chart.hierarchy.cycle", "Object cannot be its own parent",
                         "$.objects[\"" + current + "\"].parent");
                resolvedState = HierarchyState::Cyclic;
                break;
            }

            const auto parent = objects.find(object.parent->value);
            if (parent == objects.end()) {
                addWarning(diagnostics, "chart.hierarchy.parent_missing",
                           "Object and its descendants are skipped because its parent is missing",
                           "$.objects[\"" + current + "\"].parent");
                resolvedState = HierarchyState::Skipped;
                break;
            }
            current = parent->first;
        }

        for (auto iterator = chain.rbegin(); iterator != chain.rend(); ++iterator) {
            states[*iterator] = resolvedState;
        }
        return states[id];
    }
};

void validateComponents(const ChartObject& object, const std::set<std::string>& behaviors,
                        core::Diagnostics& diagnostics) {
    const std::string path = "$.objects[\"" + object.id.value + "\"].components";
    if (object.components.transform) {
        const auto& transform = *object.components.transform;
        if (!core::isFinite(transform.position) || !core::isFinite(transform.rotation) ||
            !core::isFinite(transform.scale)) {
            addError(diagnostics, "chart.transform.non_finite",
                     "Transform components must contain only finite values",
                     path + ".cuexis.transform");
        } else if (!core::isNormalized(transform.rotation)) {
            addError(diagnostics, "chart.transform.rotation_not_normalized",
                     "Transform quaternion must be normalized",
                     path + ".cuexis.transform.rotation");
        }
    }
    if (object.components.renderable) {
        if (object.components.renderable->mesh.value.empty()) {
            addError(diagnostics, "chart.asset_id.empty", "Mesh AssetId cannot be empty",
                     path + ".cuexis.renderable.mesh.id");
        }
        if (object.components.renderable->material.value.empty()) {
            addError(diagnostics, "chart.asset_id.empty", "Material AssetId cannot be empty",
                     path + ".cuexis.renderable.material.id");
        }
    }
    if (object.components.behavior &&
        !behaviors.contains(object.components.behavior->behavior.value)) {
        addError(diagnostics, "chart.reference.behavior_missing",
                 "Behavior component refers to a missing behavior",
                 path + ".cuexis.behavior.behavior");
    }
    if (object.components.note && !object.components.note->beat) {
        addError(diagnostics, "chart.note.beat_missing", "Concrete note object requires a beat",
                 path + ".cuexis.note.beat");
    }
    if (object.components.camera) {
        const auto& camera = *object.components.camera;
        const std::string cameraPath = path + ".cuexis.camera";
        if (camera.type != "perspective") {
            addError(diagnostics, "chart.camera.unsupported_type",
                     "Unsupported camera projection type; expected 'perspective'",
                     cameraPath + ".type");
        }
        if (!std::isfinite(camera.fovY) || camera.fovY <= 0.0 || camera.fovY >= 179.0) {
            addError(diagnostics, "chart.camera.invalid_fov",
                     "Camera FOV must be finite and in (0, 179) degrees", cameraPath + ".fovY");
        }
        if (!std::isfinite(camera.nearPlane) || camera.nearPlane <= 0.0) {
            addError(diagnostics, "chart.camera.invalid_near",
                     "Camera near plane must be finite and positive", cameraPath + ".near");
        }
        if (!std::isfinite(camera.farPlane) || camera.farPlane <= 0.0) {
            addError(diagnostics, "chart.camera.invalid_far",
                     "Camera far plane must be finite and positive", cameraPath + ".far");
        }
        if (std::isfinite(camera.nearPlane) && std::isfinite(camera.farPlane) &&
            camera.nearPlane >= camera.farPlane) {
            addError(diagnostics, "chart.camera.near_exceeds_far",
                     "Camera near plane must be less than far plane", cameraPath);
        }
    }
}

void validateDefaultCamera(const CameraData& camera, core::Diagnostics& diagnostics) {
    constexpr std::string_view path = "$.camera";
    if (camera.type != "perspective") {
        addError(diagnostics, "chart.camera.unsupported_type",
                 "Unsupported camera projection type; expected 'perspective'",
                 std::string{path} + ".type");
    }
    if (!std::isfinite(camera.fovY) || camera.fovY <= 0.0 || camera.fovY >= 179.0) {
        addError(diagnostics, "chart.camera.invalid_fov",
                 "Camera FOV must be finite and in (0, 179) degrees", std::string{path} + ".fovY");
    }
    if (!std::isfinite(camera.nearPlane) || camera.nearPlane <= 0.0) {
        addError(diagnostics, "chart.camera.invalid_near",
                 "Camera near plane must be finite and positive", std::string{path} + ".near");
    }
    if (!std::isfinite(camera.farPlane) || camera.farPlane <= 0.0) {
        addError(diagnostics, "chart.camera.invalid_far",
                 "Camera far plane must be finite and positive", std::string{path} + ".far");
    }
    if (std::isfinite(camera.nearPlane) && std::isfinite(camera.farPlane) &&
        camera.nearPlane >= camera.farPlane) {
        addError(diagnostics, "chart.camera.near_exceeds_far",
                 "Camera near plane must be less than far plane", std::string{path});
    }
    if (!std::isfinite(camera.pitch) || !std::isfinite(camera.yaw) || !std::isfinite(camera.roll)) {
        addError(diagnostics, "chart.camera.rotation_non_finite",
                 "Camera rotation angles must be finite", std::string{path});
    }
    if (camera.defaultTransform.has_value()) {
        const auto& transform = *camera.defaultTransform;
        if (!core::isFinite(transform.position) || !core::isFinite(transform.rotation) ||
            !core::isFinite(transform.scale)) {
            addError(diagnostics, "chart.transform.non_finite",
                     "Transform components must contain only finite values",
                     std::string{path} + ".defaultTransform");
        } else if (!core::isNormalized(transform.rotation)) {
            addError(diagnostics, "chart.transform.rotation_not_normalized",
                     "Transform quaternion must be normalized",
                     std::string{path} + ".defaultTransform.rotation");
        }
    }
}

[[nodiscard]] auto valueMatchesProperty(BehaviorProperty property,
                                        const BehaviorValue& value) noexcept -> bool {
    switch (property) {
    case BehaviorProperty::TransformPositionX:
    case BehaviorProperty::TransformPositionY:
    case BehaviorProperty::TransformPositionZ: {
        const auto* scalar = std::get_if<double>(&value);
        constexpr auto floatMax = static_cast<double>(std::numeric_limits<float>::max());
        return scalar != nullptr && std::isfinite(*scalar) && *scalar >= -floatMax &&
               *scalar <= floatMax;
    }
    case BehaviorProperty::TransformRotation: {
        const auto* rotation = std::get_if<core::Quat>(&value);
        return rotation != nullptr && core::isNormalized(*rotation);
    }
    case BehaviorProperty::TransformScale: {
        const auto* scale = std::get_if<core::Vec3>(&value);
        return scale != nullptr && core::isFinite(*scale);
    }
    case BehaviorProperty::CameraFovY: {
        const auto* fov = std::get_if<double>(&value);
        return fov != nullptr && std::isfinite(*fov) && *fov > 0.0 && *fov < 179.0;
    }
    case BehaviorProperty::MaterialOpacity: {
        const auto* opacity = std::get_if<double>(&value);
        return opacity != nullptr && std::isfinite(*opacity) && *opacity >= 0.0 && *opacity <= 1.0;
    }
    case BehaviorProperty::MaterialTint: {
        const auto* tint = std::get_if<core::Vec3>(&value);
        return tint != nullptr && core::isFinite(*tint) && tint->x >= 0.0F && tint->x <= 1.0F &&
               tint->y >= 0.0F && tint->y <= 1.0F && tint->z >= 0.0F && tint->z <= 1.0F;
    }
    }
    return false;
}

[[nodiscard]] auto compileBehaviorTracks(const ChartBehavior& behavior, const TimingMap& timingMap,
                                         const ChartLimits& limits, std::size_t& totalKeyCount,
                                         core::Diagnostics& diagnostics)
    -> std::vector<RuntimeTrack> {
    std::vector<RuntimeTrack> runtimeTracks;
    if (behavior.tracks.items.size() > limits.maxTracksPerBehavior) {
        addError(diagnostics, "chart.limit.behavior_tracks",
                 "Chart behavior track count exceeds the limit",
                 "$.behaviors[\"" + behavior.id.value + "\"].tracks");
        return runtimeTracks;
    }
    if (behavior.tracks.items.empty() && behavior.tracks.canonicalText != "{}" &&
        behavior.tracks.canonicalText != "[]") {
        addError(diagnostics, "chart.behavior.opaque_tracks_unsupported",
                 "Opaque Behavior Track data must be loaded through the typed v1 reader",
                 "$.behaviors[\"" + behavior.id.value + "\"].tracks");
        return runtimeTracks;
    }

    std::set<BehaviorProperty> properties;
    runtimeTracks.reserve(behavior.tracks.items.size());
    for (std::size_t trackIndex = 0; trackIndex < behavior.tracks.items.size(); ++trackIndex) {
        const auto& track = behavior.tracks.items[trackIndex];
        const std::string path =
            "$.behaviors[\"" + behavior.id.value + "\"].tracks[" + std::to_string(trackIndex) + "]";
        if (!properties.insert(track.property).second) {
            addError(diagnostics, "chart.behavior.property_duplicate",
                     "A Behavior may write each v1 property at most once", path + ".property");
            continue;
        }
        if (track.keys.empty()) {
            addError(diagnostics, "chart.behavior.keys_empty",
                     "A Behavior Track must contain at least one key", path + ".keys");
            continue;
        }
        if (track.keys.size() > limits.maxKeysPerTrack ||
            track.keys.size() > limits.maxTotalBehaviorKeys ||
            totalKeyCount > limits.maxTotalBehaviorKeys - track.keys.size()) {
            addError(diagnostics, "chart.limit.behavior_keys",
                     "Behavior key count exceeds configured limit", path + ".keys");
            continue;
        }
        totalKeyCount += track.keys.size();

        std::vector<const BehaviorKey*> sortedKeys;
        sortedKeys.reserve(track.keys.size());
        for (const auto& key : track.keys) {
            sortedKeys.push_back(&key);
        }
        std::sort(sortedKeys.begin(), sortedKeys.end(),
                  [](const auto* left, const auto* right) { return left->beat < right->beat; });

        RuntimeTrack runtimeTrack{.property = track.property, .keys = {}};
        runtimeTrack.keys.reserve(sortedKeys.size());
        bool valid = true;
        for (std::size_t keyIndex = 0; keyIndex < sortedKeys.size(); ++keyIndex) {
            const auto& key = *sortedKeys[keyIndex];
            const std::string keyPath = path + ".keys[" + std::to_string(keyIndex) + "]";
            if (keyIndex != 0 && sortedKeys[keyIndex - 1]->beat == key.beat) {
                addError(diagnostics, "chart.behavior.beat_duplicate",
                         "Behavior Track keys must have unique beats", keyPath + ".beat");
                valid = false;
            }
            if (keyIndex == 0 && key.easing.has_value()) {
                addError(diagnostics, "chart.behavior.first_key_easing",
                         "The first key in beat order must omit easing", keyPath + ".easing");
                valid = false;
            }
            if (!valueMatchesProperty(track.property, key.value)) {
                addError(diagnostics, "chart.behavior.value_invalid",
                         "Behavior key value does not match its Property or range",
                         keyPath + ".value");
                valid = false;
            }
            const double chartTimeMs = timingMap.beatToChartTimeMs(key.beat);
            if (!std::isfinite(chartTimeMs)) {
                addError(diagnostics, "chart.behavior.time_out_of_range",
                         "Behavior key time conversion produced a non-finite value",
                         keyPath + ".beat");
                valid = false;
            }
            runtimeTrack.keys.push_back(RuntimeKey{
                .chartTimeMs = chartTimeMs,
                .value = key.value,
                .easing = key.easing.value_or(BehaviorEasing::Linear),
            });
        }
        if (valid) {
            runtimeTracks.push_back(std::move(runtimeTrack));
        }
    }
    std::sort(runtimeTracks.begin(), runtimeTracks.end(),
              [](const auto& left, const auto& right) { return left.property < right.property; });
    return runtimeTracks;
}

struct GroupBoundary final {
    bool continuous{};
    RationalBeat startBeat;
    RationalBeat durationBeats;
};

[[nodiscard]] bool validGroupId(std::string_view value, const ChartLimits& limits) noexcept {
    if (value.empty() || value.size() > limits.maxIdentifierBytes || value.size() > 256) {
        return false;
    }
    const auto isAlphaNumeric = [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9');
    };
    if (!isAlphaNumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [&](char character) {
        return isAlphaNumeric(character) || character == '.' || character == '_' ||
               character == '-';
    });
}

[[nodiscard]] auto compileEventBehavior(const ChartBehavior& behavior, const ChartLimits& limits,
                                        std::size_t& totalEventCount,
                                        core::Diagnostics& diagnostics) -> RuntimeBehavior {
    RuntimeBehavior runtime{.id = behavior.id,
                            .type = behavior.type,
                            .version = behavior.version,
                            .tracks = {},
                            .eventTracks = {},
                            .stepTracks = {}};
    const auto eventCount = behavior.events.size() + behavior.stepEvents.size();
    const std::string basePath = "$.behaviors[\"" + behavior.id.value + "\"]";
    if (eventCount == 0) {
        addError(diagnostics, "chart.behavior.events_empty",
                 "behavior.event must contain at least one event", basePath);
        return runtime;
    }
    if (eventCount > limits.maxEventsPerBehavior || eventCount > limits.maxTotalBehaviorEvents ||
        totalEventCount > limits.maxTotalBehaviorEvents - eventCount) {
        addError(diagnostics, "chart.limit.behavior_events",
                 "Behavior Event count exceeds configured limit", basePath);
        return runtime;
    }
    totalEventCount += eventCount;

    const auto zero = RationalBeat::zero();
    std::map<std::string, GroupBoundary> groups;
    std::map<BehaviorProperty, std::vector<const BehaviorEvent*>> continuous;
    for (const auto& event : behavior.events) {
        continuous[event.property].push_back(&event);
        if (event.groupId) {
            if (!validGroupId(*event.groupId, limits)) {
                addError(diagnostics, "chart.behavior.group_id_invalid",
                         "Behavior group ID contains unsupported characters", basePath + ".events");
                continue;
            }
            const GroupBoundary boundary{true, event.startBeat, event.durationBeats};
            const auto [iterator, inserted] = groups.emplace(*event.groupId, boundary);
            if (!inserted &&
                (!iterator->second.continuous || iterator->second.startBeat != event.startBeat ||
                 iterator->second.durationBeats != event.durationBeats)) {
                addError(diagnostics, "chart.behavior.group_boundary_mismatch",
                         "Continuous Event group members must share start Beat and duration",
                         basePath + ".events");
            }
        }
    }
    for (auto& [property, sourceEvents] : continuous) {
        std::sort(
            sourceEvents.begin(), sourceEvents.end(),
            [](const auto* left, const auto* right) { return left->startBeat < right->startBeat; });
        RuntimeEventTrack track{.property = property, .events = {}};
        track.events.reserve(sourceEvents.size());
        std::optional<RationalBeat> previousEnd;
        std::optional<RationalBeat> previousStart;
        for (const auto* event : sourceEvents) {
            const std::string path = basePath + ".events";
            auto end = addRationalBeats(event->startBeat, event->durationBeats);
            bool valid = true;
            if (!end) {
                addError(diagnostics, "chart.behavior.beat_out_of_range",
                         "Behavior Event end Beat is outside the supported range", path);
                valid = false;
            }
            if (event->durationBeats < zero) {
                addError(diagnostics, "chart.behavior.duration_negative",
                         "Behavior Event duration must be non-negative", path);
                valid = false;
            }
            if (!std::isfinite(event->startSlope) || !std::isfinite(event->endSlope) ||
                event->startSlope < 0.0 || event->endSlope < 0.0 ||
                event->startSlope + event->endSlope > 3.0) {
                addError(diagnostics, "chart.behavior.slope_invalid",
                         "Behavior Event slopes are invalid", path);
                valid = false;
            }
            if (!valueMatchesProperty(property, event->startValue) ||
                !valueMatchesProperty(property, event->endValue)) {
                addError(diagnostics, "chart.behavior.value_invalid",
                         "Behavior Event value does not match its Property or range", path);
                valid = false;
            }
            if (event->durationBeats == zero &&
                (event->startValue != event->endValue || event->startSlope != 0.0 ||
                 event->endSlope != 0.0)) {
                addError(diagnostics, "chart.behavior.zero_duration_invalid",
                         "Zero-duration Behavior Events require equal values and zero slopes",
                         path);
                valid = false;
            }
            if (previousStart && *previousStart == event->startBeat) {
                addError(diagnostics, "chart.behavior.event_start_duplicate",
                         "Events for one Property must have unique start Beats", path);
                valid = false;
            } else if (previousEnd && event->startBeat < *previousEnd) {
                addError(diagnostics, "chart.behavior.event_overlap",
                         "Events for one Property must not overlap", path);
                valid = false;
            }
            previousStart = event->startBeat;
            if (end) {
                previousEnd = *end;
            }
            if (valid && end) {
                track.events.push_back(RuntimeEvent{event->startBeat.toDouble(), end->toDouble(),
                                                    event->startValue, event->endValue,
                                                    event->startSlope, event->endSlope,
                                                    event->durationBeats == zero, event->groupId});
            }
        }
        if (!track.events.empty()) {
            runtime.eventTracks.push_back(std::move(track));
        }
    }

    std::map<BehaviorStepProperty, std::vector<const BehaviorStepEvent*>> discrete;
    for (const auto& event : behavior.stepEvents) {
        discrete[event.property].push_back(&event);
        if (event.groupId) {
            if (!validGroupId(*event.groupId, limits)) {
                addError(diagnostics, "chart.behavior.group_id_invalid",
                         "Behavior group ID contains unsupported characters",
                         basePath + ".stepEvents");
                continue;
            }
            const GroupBoundary boundary{false, event.beat, zero};
            const auto [iterator, inserted] = groups.emplace(*event.groupId, boundary);
            if (!inserted &&
                (iterator->second.continuous || iterator->second.startBeat != event.beat)) {
                addError(diagnostics, "chart.behavior.group_boundary_mismatch",
                         "Step Event group members must share one Beat and cannot mix with "
                         "continuous Events",
                         basePath + ".stepEvents");
            }
        }
    }
    for (auto& [property, sourceEvents] : discrete) {
        std::sort(sourceEvents.begin(), sourceEvents.end(),
                  [](const auto* left, const auto* right) { return left->beat < right->beat; });
        RuntimeStepTrack track{.property = property, .events = {}};
        track.events.reserve(sourceEvents.size());
        for (std::size_t index = 0; index < sourceEvents.size(); ++index) {
            const auto& event = *sourceEvents[index];
            const std::string path = basePath + ".stepEvents";
            if (index != 0 && sourceEvents[index - 1]->beat == event.beat) {
                addError(diagnostics, "chart.behavior.step_beat_duplicate",
                         "Step Events for one Property must have unique Beats", path);
                continue;
            }
            const bool valueValid = (property == BehaviorStepProperty::RenderVisible &&
                                     std::holds_alternative<bool>(event.value)) ||
                                    (property == BehaviorStepProperty::RenderMaterial &&
                                     std::get_if<AssetId>(&event.value) != nullptr &&
                                     !std::get<AssetId>(event.value).value.empty());
            if (!valueValid) {
                addError(diagnostics, "chart.behavior.step_value_invalid",
                         "Step Event value does not match its Property", path);
                continue;
            }
            track.events.push_back(
                RuntimeStepEvent{event.beat.toDouble(), event.value, event.groupId});
        }
        if (!track.events.empty()) {
            runtime.stepTracks.push_back(std::move(track));
        }
    }
    return runtime;
}

} // namespace

auto ChartCompiler::compile(const ChartDocument& document, const ChartLimits& limits)
    -> ChartRuntimeResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartRuntimeResult{std::nullopt, std::move(diagnostics)};
    }
    if (document.objects.size() > limits.maxObjects) {
        addError(diagnostics, "chart.limit.objects", "Chart object count exceeds the limit",
                 "$.objects");
    }
    if (document.behaviors.size() > limits.maxBehaviors) {
        addError(diagnostics, "chart.limit.behaviors", "Chart behavior count exceeds the limit",
                 "$.behaviors");
    }
    if (document.version != 1 && document.version != 2 && document.version != 3 &&
        document.version != 4) {
        addError(diagnostics, "chart.version.unsupported", "Chart format version is unsupported",
                 "$.version");
    }
    if (document.version == 1 && document.audio.has_value()) {
        addError(diagnostics, "chart.audio.not_available_in_v1",
                 "Chart v1 must not declare an audio block", "$.audio");
    }
    if (document.audio) {
        if (document.audio->version != 1) {
            addError(diagnostics, "chart.audio.version_unsupported",
                     "Chart audio block version is unsupported", "$.audio.version");
        }
        if (document.audio->mainMusic.value.empty()) {
            addError(diagnostics, "chart.audio.main_music_empty",
                     "Chart main music AssetId must not be empty", "$.audio.mainMusic.id");
        }
    }
    validateDefaultCamera(document.camera, diagnostics);

    const bool usesTempoMap = document.version == 3 || document.version == 4;
    auto timingMap = usesTempoMap
                         ? TimingMap::create(document.timing.defaultBpm, document.timing.offsetMs,
                                             document.timing.tempoEvents, document.timing.stops)
                         : TimingMap::create(document.timing.defaultBpm, document.timing.offsetMs);
    if (!timingMap) {
        auto diagnostic =
            core::Diagnostic{core::DiagnosticSeverity::Error, std::string{timingMap.error().code()},
                             std::string{timingMap.error().message()}, "$.timing"};
        for (const auto& context : timingMap.error().context()) {
            diagnostic.withContext(context.key, context.value);
        }
        diagnostics.add(std::move(diagnostic));
    }

    std::vector<RuntimeBehavior> runtimeBehaviors;
    runtimeBehaviors.reserve(document.behaviors.size());
    std::set<std::string> behaviorIds;
    std::size_t totalBehaviorKeys = 0;
    std::size_t totalBehaviorEvents = 0;
    for (const auto& behavior : document.behaviors) {
        if (diagnostics.limitReached()) {
            break;
        }
        if (behavior.id.value.empty()) {
            addError(diagnostics, "chart.behavior.id_empty", "Behavior ID cannot be empty",
                     "$.behaviors");
            continue;
        }
        if (!behaviorIds.insert(behavior.id.value).second) {
            addError(diagnostics, "chart.behavior.id_duplicate", "Behavior ID must be unique",
                     "$.behaviors[\"" + behavior.id.value + "\"].id");
            continue;
        }
        const bool isV3Behavior = behavior.type == "behavior.event" && behavior.version == 1 &&
                                  (document.version == 3 || document.version == 4);
        const bool isLegacyBehavior = behavior.type == "behavior.transform.keyframe" &&
                                      behavior.version == 1 && document.version != 3 &&
                                      document.version != 4;
        if (!isV3Behavior && !isLegacyBehavior) {
            addError(diagnostics, "chart.behavior.version_unsupported",
                     "Behavior type and version are unsupported",
                     "$.behaviors[\"" + behavior.id.value + "\"]");
            continue;
        }
        if (isV3Behavior) {
            runtimeBehaviors.push_back(
                compileEventBehavior(behavior, limits, totalBehaviorEvents, diagnostics));
        } else {
            auto tracks = timingMap ? compileBehaviorTracks(behavior, *timingMap, limits,
                                                            totalBehaviorKeys, diagnostics)
                                    : std::vector<RuntimeTrack>{};
            runtimeBehaviors.push_back(RuntimeBehavior{.id = behavior.id,
                                                       .type = behavior.type,
                                                       .version = behavior.version,
                                                       .tracks = std::move(tracks),
                                                       .eventTracks = {},
                                                       .stepTracks = {}});
        }
    }
    std::sort(runtimeBehaviors.begin(), runtimeBehaviors.end(),
              [](const RuntimeBehavior& left, const RuntimeBehavior& right) {
                  return left.id.value < right.id.value;
              });

    std::map<std::string, const ChartObject*> objectsById;
    for (const auto& object : document.objects) {
        if (diagnostics.limitReached()) {
            break;
        }
        if (object.id.value.empty()) {
            addError(diagnostics, "chart.object.id_empty", "Object ID cannot be empty",
                     "$.objects");
            continue;
        }
        const auto [iterator, inserted] = objectsById.emplace(object.id.value, &object);
        static_cast<void>(iterator);
        if (!inserted) {
            addError(diagnostics, "chart.object.id_duplicate", "Object ID must be unique",
                     "$.objects[\"" + object.id.value + "\"].id");
        }
        validateComponents(object, behaviorIds, diagnostics);
    }

    HierarchyValidator hierarchy{objectsById, diagnostics, {}};
    for (const auto& [id, object] : objectsById) {
        if (diagnostics.limitReached()) {
            break;
        }
        static_cast<void>(object);
        static_cast<void>(hierarchy.visit(id));
    }

    if (diagnostics.hasErrors() || !timingMap) {
        diagnostics.sortDeterministically();
        return ChartRuntimeResult{std::nullopt, std::move(diagnostics)};
    }

    std::vector<const ChartObject*> includedObjects;
    includedObjects.reserve(objectsById.size());
    for (const auto& [id, object] : objectsById) {
        if (hierarchy.states.at(id) == HierarchyState::Valid) {
            includedObjects.push_back(object);
        }
    }

    std::map<std::string, std::size_t> runtimeIndices;
    for (std::size_t index = 0; index < includedObjects.size(); ++index) {
        runtimeIndices.emplace(includedObjects[index]->id.value, index);
    }

    std::vector<RuntimeObject> runtimeObjects;
    runtimeObjects.reserve(includedObjects.size());
    for (const ChartObject* object : includedObjects) {
        std::optional<std::size_t> parentIndex;
        if (object->parent) {
            const auto parent = runtimeIndices.find(object->parent->value);
            if (parent == runtimeIndices.end()) {
                addError(diagnostics, "chart.hierarchy.internal_parent_missing",
                         "Included object has no included parent",
                         "$.objects[\"" + object->id.value + "\"].parent");
                continue;
            }
            parentIndex = parent->second;
        }
        runtimeObjects.push_back(RuntimeObject{object->id, parentIndex, object->components});
    }

    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return ChartRuntimeResult{std::nullopt, std::move(diagnostics)};
    }

    diagnostics.sortDeterministically();
    const auto mainMusic =
        document.audio ? std::optional<AssetId>{document.audio->mainMusic} : std::nullopt;
    return ChartRuntimeResult{ChartRuntime{document.chartId, std::move(*timingMap), document.camera,
                                           std::move(runtimeBehaviors), std::move(runtimeObjects),
                                           document.version, mainMusic},
                              std::move(diagnostics)};
}

} // namespace cuexis::chart
