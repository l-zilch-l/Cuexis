#include "chart_v4_animation_reader_internal.hpp"

#include "chart_v4_animation_reader_values_internal.hpp"
#include "chart_v4_common_reader_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::detail {

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

} // namespace cuexis::chart::detail
