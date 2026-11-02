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

} // namespace cuexis::chart
