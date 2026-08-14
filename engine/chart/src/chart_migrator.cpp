#include <cuexis/chart/chart_migrator.hpp>

#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_writer.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/value.hpp>

#include "sha256_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
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

[[nodiscard]] auto diagnosticSeverityName(core::DiagnosticSeverity severity) -> const char* {
    switch (severity) {
    case core::DiagnosticSeverity::Info:
        return "info";
    case core::DiagnosticSeverity::Warning:
        return "warning";
    case core::DiagnosticSeverity::Error:
        return "error";
    }
    return "error";
}

[[nodiscard]] auto diagnosticRecordValue(const ChartMigrationDiagnosticRecord& record)
    -> json::Value {
    Object item;
    item.emplace("code", json::Value{record.code});
    item.emplace("fieldPath", json::Value{record.fieldPath});
    item.emplace("message", json::Value{record.message});
    item.emplace("severity", json::Value{record.severity});
    return json::Value{std::move(item)};
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
    if (report.targetVersion == 4) {
        Array discarded;
        for (const auto& field : report.discardedFields) {
            discarded.emplace_back(field);
        }
        Array warnings;
        for (const auto& warning : report.warnings) {
            warnings.push_back(diagnosticRecordValue(warning));
        }
        Array diagnostics;
        for (const auto& diagnostic : report.diagnostics) {
            diagnostics.push_back(diagnosticRecordValue(diagnostic));
        }
        Object fieldCounts;
        const auto counts = report.fieldCounts.value_or(ChartMigrationFieldCounts{});
        fieldCounts.emplace("animationClips", json::Value{std::uint64_t{counts.animationClips}});
        fieldCounts.emplace("animationTemplateImports",
                            json::Value{std::uint64_t{counts.animationTemplateImports}});
        fieldCounts.emplace("behaviors", json::Value{std::uint64_t{counts.behaviors}});
        fieldCounts.emplace("objects", json::Value{std::uint64_t{counts.objects}});
        fieldCounts.emplace("parameters", json::Value{std::uint64_t{counts.parameters}});
        root.emplace("discardedFields", json::Value{std::move(discarded)});
        root.emplace("diagnostics", json::Value{std::move(diagnostics)});
        root.emplace("fieldCounts", json::Value{std::move(fieldCounts)});
        root.emplace("generatedBindings", json::Value{std::uint64_t{report.generatedBindings}});
        root.emplace("generatedClips", json::Value{std::uint64_t{report.generatedClips}});
        root.emplace("generatedParameters", json::Value{std::uint64_t{report.generatedParameters}});
        root.emplace("sourceCanonicalIdentity",
                     json::Value{report.sourceCanonicalIdentity.value_or(std::string{})});
        root.emplace("targetCanonicalIdentity",
                     json::Value{report.targetCanonicalIdentity.value_or(std::string{})});
        root.emplace("warnings", json::Value{std::move(warnings)});
    }
    auto serialized = json::serialize(json::Value{std::move(root)}, json::SerializeStyle::Pretty);
    if (serialized) {
        serialized->push_back('\n');
    }
    return serialized;
}

[[nodiscard]] auto parseLimits(const ChartLimits& limits) -> json::ParseLimits {
    return {limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes};
}

[[nodiscard]] auto canonicalIdentity(std::string_view bytes) -> std::string {
    return detail::sha256Hex(detail::sha256(bytes));
}

[[nodiscard]] auto arraySize(const json::Value& root, std::string_view key) -> std::size_t {
    const auto* field = root.find(key);
    if (field == nullptr || field->array() == nullptr) {
        return 0;
    }
    return field->array()->size();
}

[[nodiscard]] auto makeDiagnosticRecord(const core::Diagnostic& diagnostic)
    -> ChartMigrationDiagnosticRecord {
    return ChartMigrationDiagnosticRecord{std::string{diagnostic.code()},
                                          std::string{diagnostic.fieldPath()},
                                          diagnosticSeverityName(diagnostic.severity()),
                                          std::string{diagnostic.message()}};
}

void populateV4ReportRecords(ChartMigrationReport& report, const core::Diagnostics& diagnostics) {
    report.warnings.clear();
    report.diagnostics.clear();
    for (const auto& diagnostic : diagnostics.items()) {
        auto record = makeDiagnosticRecord(diagnostic);
        if (diagnostic.severity() == core::DiagnosticSeverity::Warning) {
            report.warnings.push_back(record);
        }
        report.diagnostics.push_back(std::move(record));
    }
}

[[nodiscard]] auto liftV3JsonToV4(std::string_view v3Json, const ChartLimits& limits)
    -> core::Result<std::string> {
    auto parsed = json::parse(v3Json, parseLimits(limits));
    if (!parsed) {
        return core::unexpected(
            core::Error{"chart.migration.serialize_failed", "Migrated Chart JSON is invalid"}
                .withCause(std::move(parsed.error())));
    }
    auto* root = parsed->object();
    if (root == nullptr) {
        return core::unexpected(
            core::Error{"chart.migration.serialize_failed", "Migrated Chart JSON must be an object"});
    }
    root->insert_or_assign("version", json::Value{std::uint64_t{4}});
    root->insert_or_assign("parameters", json::Value{Array{}});
    root->insert_or_assign("animationTemplateImports", json::Value{Array{}});
    root->insert_or_assign("animationClips", json::Value{Array{}});
    auto serialized = json::serialize(*parsed);
    if (!serialized) {
        return core::unexpected(
            core::Error{"chart.migration.serialize_failed",
                        "Lifted Chart v4 JSON could not be serialized"}
                .withCause(std::move(serialized.error())));
    }
    return serialized;
}

[[nodiscard]] auto finishV4Migration(std::string_view sourceJson, std::string_view v3Json,
                                     ChartMigrationReport report, const ChartLimits& limits,
                                     core::Diagnostics incoming = {}) -> ChartMigrationResult {
    ChartMigrationResult result;
    result.diagnostics = std::move(incoming);
    auto sourceCanonical = ChartWriter::writeCanonicalJson(sourceJson, limits);
    if (!sourceCanonical) {
        addError(result.diagnostics, "chart.migration.serialize_failed",
                 "Source Chart canonicalization failed");
        return result;
    }
    report.sourceCanonicalIdentity = canonicalIdentity(*sourceCanonical);

    auto lifted = liftV3JsonToV4(v3Json, limits);
    if (!lifted) {
        addError(result.diagnostics, std::string{lifted.error().code()},
                 std::string{lifted.error().message()});
        return result;
    }
    auto loaded = ChartV4Loader::load(*lifted, limits);
    result.diagnostics.append(std::move(loaded.diagnostics));
    if (!loaded.hasValue()) {
        populateV4ReportRecords(report, result.diagnostics);
        result.diagnostics.sortDeterministically();
        return result;
    }

    auto chartJson = ChartWriter::writeV4(*loaded.document, limits);
    if (!chartJson) {
        addError(result.diagnostics, "chart.migration.serialize_failed",
                 "Migrated Chart or report serialization failed");
        return result;
    }

    auto parsed = json::parse(*chartJson, parseLimits(limits));
    if (!parsed) {
        addError(result.diagnostics, "chart.migration.serialize_failed",
                 "Migrated Chart or report serialization failed");
        return result;
    }
    report.targetVersion = 4;
    report.targetCanonicalIdentity = canonicalIdentity(*chartJson);
    report.discardedFields = {};
    report.generatedClips = 0;
    report.generatedBindings = 0;
    report.generatedParameters = 0;
    report.fieldCounts = ChartMigrationFieldCounts{
        .animationClips = arraySize(*parsed, "animationClips"),
        .animationTemplateImports = arraySize(*parsed, "animationTemplateImports"),
        .behaviors = arraySize(*parsed, "behaviors"),
        .objects = arraySize(*parsed, "objects"),
        .parameters = arraySize(*parsed, "parameters"),
    };
    populateV4ReportRecords(report, result.diagnostics);
    auto reportJson = serializeReport(report);
    if (!reportJson) {
        addError(result.diagnostics, "chart.migration.serialize_failed",
                 "Migrated Chart or report serialization failed");
        return result;
    }
    result.artifact.emplace(ChartMigrationArtifact{.document = loaded.document->legacyProjection,
                                                   .report = std::move(report),
                                                   .chartJson = std::move(*chartJson),
                                                   .reportJson = std::move(*reportJson),
                                                   .v4Document = std::move(*loaded.document)});
    result.diagnostics.sortDeterministically();
    return result;
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
    ChartMigrationReport report{.sourceVersion = migrated.version,
                                .targetVersion = 3,
                                .convertedBehaviors = 0,
                                .generatedEvents = 0,
                                .rewrittenBindings = 0,
                                .expandedTemplateObjects = 0,
                                .unboundBehaviorIds = {}};
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
            if (bindings.empty() && track.keys.size() == 1) {
                addError(result.diagnostics, "chart.migration.unbound_single_key_unrepresentable",
                         "An unbound single-key Track cannot be represented in chart v3",
                         "$/behaviors/" + std::to_string(behaviorIndex) + "/tracks/" +
                             std::to_string(trackIndex));
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
    auto chartJson = ChartWriter::write(migrated);
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

auto ChartMigrator::migrateToV4(std::string_view sourceJson, const ChartLimits& limits)
    -> ChartMigrationResult {
    ChartMigrationResult result;
    auto parsed = json::parse(sourceJson, parseLimits(limits));
    if (!parsed) {
        addError(result.diagnostics, std::string{parsed.error().code()},
                 std::string{parsed.error().message()}, "$");
        result.diagnostics.sortDeterministically();
        return result;
    }
    const auto* versionValue = parsed->find("version");
    std::optional<std::uint32_t> sourceVersion;
    if (versionValue != nullptr) {
        if (const auto* signedVersion = versionValue->signedInteger();
            signedVersion != nullptr && *signedVersion >= 0) {
            sourceVersion = static_cast<std::uint32_t>(*signedVersion);
        } else if (const auto* unsignedVersion = versionValue->unsignedInteger();
                   unsignedVersion != nullptr && *unsignedVersion <= 4) {
            sourceVersion = static_cast<std::uint32_t>(*unsignedVersion);
        } else if (const auto* number = versionValue->number();
                   number != nullptr && *number >= 0.0 && *number <= 4.0 &&
                   *number == std::floor(*number)) {
            sourceVersion = static_cast<std::uint32_t>(*number);
        }
    }
    if (sourceVersion == 4 || (sourceVersion && *sourceVersion != 1 && *sourceVersion != 2 &&
                               *sourceVersion != 3)) {
        addError(result.diagnostics, "chart.migration.source_version_unsupported",
                 "Only canonical Chart v1, v2, and v3 can be migrated to v4", "$/version");
        result.diagnostics.sortDeterministically();
        return result;
    }
    if (sourceVersion == 3) {
        auto loaded = ChartLoader::load(sourceJson, limits);
        result.diagnostics.append(std::move(loaded.diagnostics));
        if (!loaded.hasValue()) {
            result.diagnostics.sortDeterministically();
            return result;
        }
        ChartMigrationReport report{.sourceVersion = 3,
                                    .targetVersion = 4,
                                    .convertedBehaviors = 0,
                                    .generatedEvents = 0,
                                    .rewrittenBindings = 0,
                                    .expandedTemplateObjects = 0,
                                    .unboundBehaviorIds = {}};
        return finishV4Migration(sourceJson, sourceJson, std::move(report), limits,
                                 std::move(result.diagnostics));
    }

    auto migrated = migrateToV3(sourceJson, limits);
    result.diagnostics.append(std::move(migrated.diagnostics));
    if (!migrated.hasValue()) {
        result.diagnostics.sortDeterministically();
        return result;
    }
    auto report = migrated.artifact->report;
    report.targetVersion = 4;
    return finishV4Migration(sourceJson, migrated.artifact->chartJson, std::move(report), limits,
                             std::move(result.diagnostics));
}

} // namespace cuexis::chart
