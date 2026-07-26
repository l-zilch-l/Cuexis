//  SimpleChartImporter 实现 — 方案 B → 方案 A 确定性转换
//  转换流程: 解析格式 → 校验 chartId/ID → 建立 ID 映射 → 转换 Template → 转换 Behavior
//  → 合并 Object 简写 → 生成方案 A ChartDocument → 执行 CanonicalChartLoader 结构/语义校验
//  Object UUID = UUIDv5(chartId, "object:" + readableId)，确定性生成，不依赖顺序/时区/随机数

#include <cuexis/chart/simple_chart_importer.hpp>

#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/rational_beat.hpp>
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

void addError(core::Diagnostics& diagnostics, const core::Error& error, std::string path) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(path)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto isSimpleId(std::string_view id) noexcept -> bool {
    if (id.empty() || id.size() > 128 || id.front() < 'a' || id.front() > 'z') {
        return false;
    }
    return std::all_of(id.begin() + 1, id.end(), [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
               character == '.' || character == '_' || character == '-';
    });
}

[[nodiscard]] auto isAssetId(std::string_view id, const ChartLimits& limits) noexcept -> bool {
    if (id.empty() || id.size() > limits.maxIdentifierBytes ||
        !((id.front() >= 'A' && id.front() <= 'Z') || (id.front() >= 'a' && id.front() <= 'z') ||
          (id.front() >= '0' && id.front() <= '9'))) {
        return false;
    }
    return std::all_of(id.begin() + 1, id.end(), [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '.' || character == '_' ||
               character == '-' || character == '/';
    });
}

[[nodiscard]] auto makeReference(std::string domain, std::string id) -> json::Value {
    json::Value::Object reference;
    reference.emplace("domain", json::Value{std::move(domain)});
    reference.emplace("id", json::Value{std::move(id)});
    return json::Value{std::move(reference)};
}

[[nodiscard]] auto makeBeat(const RationalBeat& beat) -> json::Value {
    json::Value::Object value;
    value.emplace("numerator", json::Value{beat.numerator()});
    value.emplace("denominator", json::Value{beat.denominator()});
    return json::Value{std::move(value)};
}

[[nodiscard]] auto parseReferenceText(const json::Reader& reader, std::string_view domain,
                                      const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<std::string> {
    const auto text = reader.readString();
    if (!text) {
        return std::nullopt;
    }
    const std::string prefix = std::string{domain} + ':';
    if (!text->starts_with(prefix)) {
        addError(diagnostics, "chart.simple.reference.domain_invalid",
                 "Simple reference has an invalid domain", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    const auto id = text->substr(prefix.size());
    const bool valid = domain == "asset" ? isAssetId(id, limits) : isSimpleId(id);
    if (!valid) {
        addError(diagnostics, "chart.simple.reference.id_invalid",
                 "Simple reference contains an invalid ID", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return std::string{id};
}

void preserveUnknown(const json::Value::Object& object, std::span<const std::string_view> known,
                     std::string_view path, json::Value::Object& unknown,
                     core::Diagnostics& diagnostics, std::string_view canonicalId = {}) {
    for (const auto& [name, value] : object) {
        if (diagnostics.limitReached()) {
            break;
        }
        if (std::find(known.begin(), known.end(), name) != known.end()) {
            continue;
        }
        const std::string fieldPath = json::appendFieldPath(path, name);
        auto diagnostic = core::Diagnostic{
            core::DiagnosticSeverity::Warning, "chart.simple.field.unknown",
            "Unknown simple-chart field is preserved without runtime behavior", fieldPath};
        if (!canonicalId.empty()) {
            diagnostic.withContext("canonical_id", std::string{canonicalId});
        }
        diagnostics.add(std::move(diagnostic));
        unknown.emplace(fieldPath, value);
    }
}

void rejectUnknown(const json::Value::Object& object, std::span<const std::string_view> known,
                   std::string_view path, core::Diagnostics& diagnostics) {
    for (const auto& [name, value] : object) {
        static_cast<void>(value);
        if (std::find(known.begin(), known.end(), name) != known.end()) {
            continue;
        }
        addError(diagnostics, "chart.simple.field.unknown",
                 "Unknown fields are not allowed in Simple Behavior Track data",
                 json::appendFieldPath(path, name));
    }
}

void preserveSimpleComponentUnknown(const json::Value::Object& fields, std::string_view path,
                                    json::Value::Object& unknown, core::Diagnostics& diagnostics,
                                    std::string_view canonicalId) {
    if (const auto transform = fields.find("transform");
        transform != fields.end() && transform->second.object() != nullptr) {
        constexpr std::array known{std::string_view{"position"}, std::string_view{"rotationDeg"},
                                   std::string_view{"scale"}};
        preserveUnknown(*transform->second.object(), known,
                        json::appendFieldPath(path, "transform"), unknown, diagnostics,
                        canonicalId);
    }
    if (const auto render = fields.find("render");
        render != fields.end() && render->second.object() != nullptr) {
        constexpr std::array known{std::string_view{"mesh"}, std::string_view{"material"}};
        preserveUnknown(*render->second.object(), known, json::appendFieldPath(path, "render"),
                        unknown, diagnostics, canonicalId);
    }
}

[[nodiscard]] auto makeExtensions(const json::Value* original, json::Value::Object unknown,
                                  std::string_view path, core::Diagnostics& diagnostics)
    -> json::Value {
    json::Value::Object extensions;
    if (original != nullptr) {
        if (const auto* object = original->object()) {
            extensions = *object;
        } else {
            addError(diagnostics, "json.type.mismatch", "Extensions must be an object",
                     std::string{path});
        }
    }
    if (!unknown.empty()) {
        if (extensions.contains("cuexis.simple.unknown")) {
            addError(diagnostics, "chart.simple.extension.reserved",
                     "cuexis.simple.unknown is reserved for importer preservation",
                     json::appendFieldPath(path, "cuexis.simple.unknown"));
        } else {
            extensions.emplace("cuexis.simple.unknown", json::Value{std::move(unknown)});
        }
    }
    return json::Value{std::move(extensions)};
}

[[nodiscard]] auto quaternionMultiply(const core::Quat& left, const core::Quat& right) noexcept
    -> core::Quat {
    return core::Quat{
        (left.w * right.x) + (left.x * right.w) + (left.y * right.z) - (left.z * right.y),
        (left.w * right.y) - (left.x * right.z) + (left.y * right.w) + (left.z * right.x),
        (left.w * right.z) + (left.x * right.y) - (left.y * right.x) + (left.z * right.w),
        (left.w * right.w) - (left.x * right.x) - (left.y * right.y) - (left.z * right.z),
    };
}

[[nodiscard]] auto eulerDegreesToQuaternion(const core::Vec3& degrees,
                                            core::Diagnostics& diagnostics, std::string_view path)
    -> std::optional<core::Quat> {
    if (!core::isFinite(degrees)) {
        addError(diagnostics, "chart.simple.transform.non_finite",
                 "Euler rotation must contain finite values", std::string{path});
        return std::nullopt;
    }
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double halfX = static_cast<double>(degrees.x) * pi / 360.0;
    const double halfY = static_cast<double>(degrees.y) * pi / 360.0;
    const double halfZ = static_cast<double>(degrees.z) * pi / 360.0;
    const core::Quat x{static_cast<float>(std::sin(halfX)), 0.0F, 0.0F,
                       static_cast<float>(std::cos(halfX))};
    const core::Quat y{0.0F, static_cast<float>(std::sin(halfY)), 0.0F,
                       static_cast<float>(std::cos(halfY))};
    const core::Quat z{0.0F, 0.0F, static_cast<float>(std::sin(halfZ)),
                       static_cast<float>(std::cos(halfZ))};
    auto normalized = core::normalize(quaternionMultiply(z, quaternionMultiply(y, x)));
    if (!normalized) {
        addError(diagnostics, normalized.error(), std::string{path});
        return std::nullopt;
    }
    return *normalized;
}

[[nodiscard]] auto readVec3(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<core::Vec3> {
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return std::nullopt;
    }
    if (values->size() != 3) {
        addError(diagnostics, "chart.simple.vector.size",
                 "Simple transform vector must contain exactly three values",
                 std::string{reader.fieldPath()});
        return std::nullopt;
    }
    std::array<float, 3> converted{};
    bool valid = true;
    for (std::size_t index = 0; index < converted.size(); ++index) {
        const auto element = reader.element(index);
        const auto value = element ? element->readNumber() : std::nullopt;
        if (!value || !std::isfinite(*value) ||
            std::abs(*value) > static_cast<double>(std::numeric_limits<float>::max())) {
            if (value) {
                addError(diagnostics, "chart.simple.number.out_of_range",
                         "Simple transform value must be finite and representable as float",
                         std::string{element->fieldPath()});
            }
            valid = false;
            continue;
        }
        converted[index] = static_cast<float>(*value);
    }
    if (!valid) {
        return std::nullopt;
    }
    return core::Vec3{converted[0], converted[1], converted[2]};
}

[[nodiscard]] auto vec3Value(const core::Vec3& vector) -> json::Value {
    json::Value::Array values;
    values.emplace_back(static_cast<double>(vector.x));
    values.emplace_back(static_cast<double>(vector.y));
    values.emplace_back(static_cast<double>(vector.z));
    return json::Value{std::move(values)};
}

[[nodiscard]] auto quatValue(const core::Quat& quaternion) -> json::Value {
    json::Value::Array values;
    values.emplace_back(static_cast<double>(quaternion.x));
    values.emplace_back(static_cast<double>(quaternion.y));
    values.emplace_back(static_cast<double>(quaternion.z));
    values.emplace_back(static_cast<double>(quaternion.w));
    return json::Value{std::move(values)};
}

void mergeNestedObject(json::Value::Object& destination, std::string_view name,
                       const json::Value& overrideValue, std::span<const std::string_view> fields) {
    if (overrideValue.isNull()) {
        destination.erase(std::string{name});
        return;
    }
    const auto* overrides = overrideValue.object();
    if (overrides == nullptr) {
        destination.insert_or_assign(std::string{name}, overrideValue);
        return;
    }
    json::Value::Object merged;
    if (const auto existing = destination.find(name);
        existing != destination.end() && existing->second.object() != nullptr) {
        merged = *existing->second.object();
    }
    for (const auto field : fields) {
        const auto item = overrides->find(field);
        if (item == overrides->end()) {
            continue;
        }
        if (item->second.isNull()) {
            merged.erase(std::string{field});
        } else {
            merged.insert_or_assign(std::string{field}, item->second);
        }
    }
    destination.insert_or_assign(std::string{name}, json::Value{std::move(merged)});
}

[[nodiscard]] auto mergeSimpleFields(const json::Value::Object* base,
                                     const json::Value::Object& overrides) -> json::Value::Object {
    json::Value::Object merged = base != nullptr ? *base : json::Value::Object{};
    constexpr std::array scalarFields{std::string_view{"kind"}, std::string_view{"parent"},
                                      std::string_view{"beat"}, std::string_view{"behavior"}};
    for (const auto field : scalarFields) {
        const auto value = overrides.find(field);
        if (value == overrides.end()) {
            continue;
        }
        if (value->second.isNull()) {
            merged.erase(std::string{field});
        } else {
            merged.insert_or_assign(std::string{field}, value->second);
        }
    }
    if (const auto transform = overrides.find("transform"); transform != overrides.end()) {
        constexpr std::array fields{std::string_view{"position"}, std::string_view{"rotationDeg"},
                                    std::string_view{"scale"}};
        mergeNestedObject(merged, "transform", transform->second, fields);
    }
    if (const auto render = overrides.find("render"); render != overrides.end()) {
        constexpr std::array fields{std::string_view{"mesh"}, std::string_view{"material"}};
        mergeNestedObject(merged, "render", render->second, fields);
    }
    return merged;
}

[[nodiscard]] auto
simpleFieldsToComponents(const json::Value::Object& fields, std::string_view path, bool concrete,
                         const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<json::Value> {
    json::Value::Object components;
    const auto kindValue = fields.find("kind");
    if (kindValue == fields.end() || kindValue->second.string() == nullptr) {
        addError(diagnostics, "chart.simple.kind_missing",
                 "Simple object requires a kind after template merge",
                 json::appendFieldPath(path, "kind"));
        return std::nullopt;
    }
    const std::string& kind = *kindValue->second.string();
    if (kind != "note" && kind != "element" && kind != "decoration") {
        addError(diagnostics, "chart.simple.kind_invalid", "Simple object kind is invalid",
                 json::appendFieldPath(path, "kind"));
        return std::nullopt;
    }

    if (const auto transformValue = fields.find("transform"); transformValue != fields.end()) {
        json::Reader transformReader{transformValue->second, diagnostics,
                                     json::appendFieldPath(path, "transform")};
        const auto* transform = transformReader.readObject();
        if (transform != nullptr) {
            core::Vec3 position{};
            core::Vec3 rotationDegrees{};
            core::Vec3 scale{1.0F, 1.0F, 1.0F};
            bool valid = true;
            if (const auto value = transformReader.optionalField("position")) {
                const auto parsed = readVec3(*value, diagnostics);
                if (parsed) {
                    position = *parsed;
                } else {
                    valid = false;
                }
            }
            if (const auto value = transformReader.optionalField("rotationDeg")) {
                const auto parsed = readVec3(*value, diagnostics);
                if (parsed) {
                    rotationDegrees = *parsed;
                } else {
                    valid = false;
                }
            }
            if (const auto value = transformReader.optionalField("scale")) {
                const auto parsed = readVec3(*value, diagnostics);
                if (parsed) {
                    scale = *parsed;
                } else {
                    valid = false;
                }
            }
            const auto rotation = eulerDegreesToQuaternion(
                rotationDegrees, diagnostics,
                json::appendFieldPath(transformReader.fieldPath(), "rotationDeg"));
            if (valid && rotation) {
                json::Value::Object component;
                component.emplace("version", json::Value{std::int64_t{1}});
                component.emplace("position", vec3Value(position));
                component.emplace("rotation", quatValue(*rotation));
                component.emplace("scale", vec3Value(scale));
                components.emplace("cuexis.transform", json::Value{std::move(component)});
            }
        }
    }

    if (const auto renderValue = fields.find("render"); renderValue != fields.end()) {
        json::Reader renderReader{renderValue->second, diagnostics,
                                  json::appendFieldPath(path, "render")};
        const auto* render = renderReader.readObject();
        if (render != nullptr) {
            const auto meshReader = renderReader.requiredField("mesh");
            const auto materialReader = renderReader.requiredField("material");
            const auto mesh = meshReader
                                  ? parseReferenceText(*meshReader, "asset", limits, diagnostics)
                                  : std::nullopt;
            const auto material =
                materialReader ? parseReferenceText(*materialReader, "asset", limits, diagnostics)
                               : std::nullopt;
            if (mesh && material) {
                json::Value::Object component;
                component.emplace("version", json::Value{std::int64_t{1}});
                component.emplace("mesh", makeReference("asset", *mesh));
                component.emplace("material", makeReference("asset", *material));
                components.emplace("cuexis.renderable", json::Value{std::move(component)});
            }
        }
    }

    if (const auto behaviorValue = fields.find("behavior"); behaviorValue != fields.end()) {
        json::Reader behaviorReader{behaviorValue->second, diagnostics,
                                    json::appendFieldPath(path, "behavior")};
        const auto behavior = parseReferenceText(behaviorReader, "behavior", limits, diagnostics);
        if (behavior) {
            json::Value::Object component;
            component.emplace("version", json::Value{std::int64_t{1}});
            component.emplace("behavior", makeReference("behavior", *behavior));
            components.emplace("cuexis.behavior", json::Value{std::move(component)});
        }
    }

    const auto beatValue = fields.find("beat");
    if (kind == "note") {
        json::Value::Object component;
        component.emplace("version", json::Value{std::int64_t{1}});
        if (beatValue != fields.end()) {
            json::Reader beatReader{beatValue->second, diagnostics,
                                    json::appendFieldPath(path, "beat")};
            const auto text = beatReader.readString();
            if (text) {
                auto beat = RationalBeat::parseSimple(*text, limits);
                if (beat) {
                    component.emplace("beat", makeBeat(*beat));
                } else {
                    addError(diagnostics, beat.error(), std::string{beatReader.fieldPath()});
                }
            }
        } else if (concrete) {
            addError(diagnostics, "chart.simple.note.beat_missing",
                     "Simple note requires a beat after template merge",
                     json::appendFieldPath(path, "beat"));
        }
        components.emplace("cuexis.note", json::Value{std::move(component)});
    } else {
        if (beatValue != fields.end()) {
            addError(diagnostics, "chart.simple.beat.not_allowed",
                     "Only simple notes may contain beat", json::appendFieldPath(path, "beat"));
        }
        if (kind == "element") {
            json::Value::Object component;
            component.emplace("version", json::Value{std::int64_t{1}});
            components.emplace("cuexis.element", json::Value{std::move(component)});
        }
    }

    return json::Value{std::move(components)};
}

[[nodiscard]] auto makeComponentPatches(const json::Value& base, const json::Value& merged)
    -> json::Value::Array {
    json::Value::Array patches;
    const auto* baseObject = base.object();
    const auto* mergedObject = merged.object();
    if (baseObject == nullptr || mergedObject == nullptr) {
        return patches;
    }
    constexpr std::array components{
        std::string_view{"cuexis.transform"}, std::string_view{"cuexis.renderable"},
        std::string_view{"cuexis.behavior"}, std::string_view{"cuexis.note"},
        std::string_view{"cuexis.element"}};
    for (const auto component : components) {
        const auto baseValue = baseObject->find(component);
        const auto mergedValue = mergedObject->find(component);
        if (baseValue == baseObject->end() && mergedValue == mergedObject->end()) {
            continue;
        }
        if (baseValue != baseObject->end() && mergedValue != mergedObject->end() &&
            baseValue->second == mergedValue->second) {
            continue;
        }
        json::Value::Object patch;
        patch.emplace("path", json::Value{"/components/" + std::string{component}});
        if (mergedValue == mergedObject->end()) {
            patch.emplace("op", json::Value{"remove"});
        } else if (baseValue == baseObject->end()) {
            patch.emplace("op", json::Value{"add"});
            patch.emplace("value", mergedValue->second);
        } else {
            patch.emplace("op", json::Value{"replace"});
            patch.emplace("value", mergedValue->second);
        }
        patches.emplace_back(json::Value{std::move(patch)});
    }
    return patches;
}

[[nodiscard]] auto convertBehaviorTracks(const json::Reader& tracksReader,
                                         const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> json::Value {
    json::Value::Array convertedTracks;
    const auto* tracks = tracksReader.readArray();
    if (tracks == nullptr) {
        return json::Value{std::move(convertedTracks)};
    }
    if (tracks->size() > limits.maxTracksPerBehavior) {
        addError(diagnostics, "chart.limit.behavior_tracks",
                 "Behavior track count exceeds configured limit",
                 std::string{tracksReader.fieldPath()});
        return json::Value{std::move(convertedTracks)};
    }
    for (std::size_t trackIndex = 0; trackIndex < tracks->size(); ++trackIndex) {
        if (diagnostics.limitReached()) {
            break;
        }
        const auto trackReader = tracksReader.element(trackIndex);
        const auto* track = trackReader ? trackReader->readObject() : nullptr;
        if (track == nullptr) {
            continue;
        }
        constexpr std::array known{std::string_view{"property"}, std::string_view{"keys"}};
        rejectUnknown(*track, known, trackReader->fieldPath(), diagnostics);
        const auto propertyReader = trackReader->requiredField("property");
        const auto keysReader = trackReader->requiredField("keys");
        if (!propertyReader || !keysReader) {
            continue;
        }
        const auto property = propertyReader->readString();
        const auto* keys = keysReader->readArray();
        if (!property || property->empty() || property->size() > limits.maxIdentifierBytes ||
            keys == nullptr) {
            continue;
        }
        if (keys->size() > limits.maxKeysPerTrack) {
            addError(diagnostics, "chart.limit.behavior_keys",
                     "Behavior key count exceeds configured limit",
                     std::string{keysReader->fieldPath()});
            continue;
        }
        json::Value::Array convertedKeys;
        for (std::size_t keyIndex = 0; keyIndex < keys->size(); ++keyIndex) {
            if (diagnostics.limitReached()) {
                break;
            }
            const auto keyReader = keysReader->element(keyIndex);
            const auto* key = keyReader ? keyReader->readObject() : nullptr;
            if (key == nullptr) {
                continue;
            }
            constexpr std::array keyFields{std::string_view{"beat"}, std::string_view{"value"},
                                           std::string_view{"easing"}};
            rejectUnknown(*key, keyFields, keyReader->fieldPath(), diagnostics);
            const auto beatReader = keyReader->requiredField("beat");
            const auto valueReader = keyReader->requiredField("value");
            if (!beatReader || !valueReader) {
                continue;
            }
            const auto beatText = beatReader->readString();
            const auto value = valueReader->readNumber();
            if (!beatText || !value || !std::isfinite(*value)) {
                if (value && !std::isfinite(*value)) {
                    addError(diagnostics, "chart.simple.behavior.value_non_finite",
                             "Behavior key value must be finite",
                             std::string{valueReader->fieldPath()});
                }
                continue;
            }
            auto beat = RationalBeat::parseSimple(*beatText, limits);
            if (!beat) {
                addError(diagnostics, beat.error(), std::string{beatReader->fieldPath()});
                continue;
            }
            json::Value::Object convertedKey;
            convertedKey.emplace("beat", makeBeat(*beat));
            convertedKey.emplace("value", json::Value{*value});
            if (const auto easingReader = keyReader->optionalField("easing")) {
                const auto easing = easingReader->readString();
                if (easing && (*easing == "linear" || *easing == "in_cubic" ||
                               *easing == "out_cubic" || *easing == "in_out_cubic")) {
                    convertedKey.emplace("easing", json::Value{std::string{*easing}});
                } else if (easing) {
                    addError(diagnostics, "chart.simple.behavior.easing_invalid",
                             "Behavior key easing is unsupported",
                             std::string{easingReader->fieldPath()});
                }
            }
            convertedKeys.emplace_back(json::Value{std::move(convertedKey)});
        }
        json::Value::Object convertedTrack;
        convertedTrack.emplace("property", json::Value{std::string{*property}});
        convertedTrack.emplace("keys", json::Value{std::move(convertedKeys)});
        convertedTracks.emplace_back(json::Value{std::move(convertedTrack)});
    }
    return json::Value{std::move(convertedTracks)};
}

} // namespace

auto SimpleChartImporter::import(std::string_view jsonText, const ChartLimits& limits)
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
    const auto* rootObject = root.readObject();
    if (rootObject == nullptr) {
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    constexpr std::array knownRoot{std::string_view{"format"},    std::string_view{"version"},
                                   std::string_view{"chartId"},   std::string_view{"metadata"},
                                   std::string_view{"timing"},    std::string_view{"camera"},
                                   std::string_view{"templates"}, std::string_view{"behaviors"},
                                   std::string_view{"objects"},   std::string_view{"extensions"}};
    json::Value::Object rootUnknown;
    preserveUnknown(*rootObject, knownRoot, root.fieldPath(), rootUnknown, diagnostics);

    const auto formatReader = root.requiredField("format");
    const auto versionReader = root.requiredField("version");
    const auto chartIdReader = root.requiredField("chartId");
    const auto metadataReader = root.requiredField("metadata");
    const auto timingReader = root.requiredField("timing");
    const auto cameraReader = root.optionalField("camera");
    const auto templatesReader = root.requiredField("templates");
    const auto behaviorsReader = root.requiredField("behaviors");
    const auto objectsReader = root.requiredField("objects");
    const auto extensionsReader = root.requiredField("extensions");

    std::optional<std::string> chartId;
    if (formatReader) {
        const auto format = formatReader->readString();
        if (format && *format != "cuexis.chart.simple") {
            addError(diagnostics, "chart.simple.format.invalid",
                     "Expected cuexis.chart.simple format", std::string{formatReader->fieldPath()});
        }
    }
    if (versionReader) {
        const auto version = versionReader->readInt64();
        if (version && *version != 1) {
            addError(diagnostics, "chart.simple.version.unsupported",
                     "Simple chart version is unsupported",
                     std::string{versionReader->fieldPath()});
        }
    }
    if (chartIdReader) {
        const auto id = chartIdReader->readString();
        if (id && isUuidV7(*id)) {
            chartId = std::string{*id};
        } else if (id) {
            addError(diagnostics, "chart.uuid.invalid_v7", "Chart ID must be a UUIDv7",
                     std::string{chartIdReader->fieldPath()});
        }
    }

    json::Value::Object canonical;
    canonical.emplace("format", json::Value{"cuexis.chart"});
    canonical.emplace("version", json::Value{std::int64_t{1}});
    if (chartId) {
        canonical.emplace("chartId", json::Value{*chartId});
    }
    if (metadataReader && metadataReader->readObject() != nullptr) {
        canonical.emplace("metadata", metadataReader->value());
    }

    const auto* timing = timingReader ? timingReader->readObject() : nullptr;
    if (timing != nullptr) {
        constexpr std::array timingFields{std::string_view{"offsetMs"}, std::string_view{"bpm"}};
        preserveUnknown(*timing, timingFields, timingReader->fieldPath(), rootUnknown, diagnostics);
        const auto offsetReader = timingReader->requiredField("offsetMs");
        const auto bpmReader = timingReader->requiredField("bpm");
        const auto offset = offsetReader ? offsetReader->readNumber() : std::nullopt;
        const auto bpm = bpmReader ? bpmReader->readNumber() : std::nullopt;
        const double offsetValue = offset.value_or(0.0);
        const double bpmValue = bpm.value_or(0.0);
        if (offset && bpm && std::isfinite(offsetValue) && std::isfinite(bpmValue) &&
            bpmValue > 0.0) {
            json::Value::Object convertedTiming;
            convertedTiming.emplace("offsetMs", json::Value{offsetValue});
            convertedTiming.emplace("defaultBpm", json::Value{bpmValue});
            convertedTiming.emplace("bpmChanges", json::Value{json::Value::Array{}});
            convertedTiming.emplace("stops", json::Value{json::Value::Array{}});
            canonical.emplace("timing", json::Value{std::move(convertedTiming)});
        } else if (offset && bpm) {
            addError(diagnostics, "chart.simple.timing.invalid",
                     "Simple timing values must be finite and BPM must be positive",
                     std::string{timingReader->fieldPath()});
        }
    }

    if (cameraReader && cameraReader->readObject() != nullptr) {
        canonical.emplace("camera", cameraReader->value());
    }

    std::map<std::string, std::string> templateIds;
    std::map<std::string, std::string> objectIds;
    const json::Value::Object* simpleTemplates = nullptr;
    const json::Value::Object* simpleObjects = nullptr;
    if (templatesReader) {
        simpleTemplates = templatesReader->readObject();
    }
    if (objectsReader) {
        simpleObjects = objectsReader->readObject();
    }
    if (simpleTemplates != nullptr && simpleTemplates->size() > limits.maxTemplates) {
        addError(diagnostics, "chart.limit.templates", "Template count exceeds configured limit",
                 std::string{templatesReader->fieldPath()});
    }
    if (simpleObjects != nullptr && simpleObjects->size() > limits.maxObjects) {
        addError(diagnostics, "chart.limit.objects", "Object count exceeds configured limit",
                 std::string{objectsReader->fieldPath()});
    }
    if (chartId && simpleTemplates != nullptr) {
        for (const auto& [id, ignored] : *simpleTemplates) {
            if (diagnostics.limitReached()) {
                break;
            }
            static_cast<void>(ignored);
            if (!isSimpleId(id)) {
                addError(diagnostics, "chart.simple.id.invalid", "Simple template ID is invalid",
                         json::appendFieldPath(templatesReader->fieldPath(), id));
                continue;
            }
            auto generated = uuidV5(*chartId, "template:" + id);
            if (generated) {
                templateIds.emplace(id, std::move(*generated));
            } else {
                addError(diagnostics, generated.error(),
                         json::appendFieldPath(templatesReader->fieldPath(), id));
            }
        }
    }
    if (chartId && simpleObjects != nullptr) {
        for (const auto& [id, ignored] : *simpleObjects) {
            if (diagnostics.limitReached()) {
                break;
            }
            static_cast<void>(ignored);
            if (!isSimpleId(id)) {
                addError(diagnostics, "chart.simple.id.invalid", "Simple object ID is invalid",
                         json::appendFieldPath(objectsReader->fieldPath(), id));
                continue;
            }
            auto generated = uuidV5(*chartId, "object:" + id);
            if (generated) {
                objectIds.emplace(id, std::move(*generated));
            } else {
                addError(diagnostics, generated.error(),
                         json::appendFieldPath(objectsReader->fieldPath(), id));
            }
        }
    }

    json::Value::Array canonicalTemplates;
    std::map<std::string, json::Value> templateComponents;
    if (simpleTemplates != nullptr) {
        for (const auto& [simpleId, value] : *simpleTemplates) {
            if (diagnostics.limitReached()) {
                break;
            }
            const auto generatedId = templateIds.find(simpleId);
            if (generatedId == templateIds.end()) {
                continue;
            }
            const std::string path = json::appendFieldPath(templatesReader->fieldPath(), simpleId);
            json::Reader templateReader{value, diagnostics, path};
            const auto* fields = templateReader.readObject();
            if (fields == nullptr) {
                continue;
            }
            constexpr std::array known{std::string_view{"kind"}, std::string_view{"transform"},
                                       std::string_view{"render"}, std::string_view{"behavior"}};
            json::Value::Object unknown;
            preserveUnknown(*fields, known, path, unknown, diagnostics, generatedId->second);
            preserveSimpleComponentUnknown(*fields, path, unknown, diagnostics,
                                           generatedId->second);
            if (fields->contains("parent") || fields->contains("beat") ||
                fields->contains("template")) {
                addError(diagnostics, "chart.simple.template.field_forbidden",
                         "Simple template cannot contain parent, beat, or template", path);
                continue;
            }
            auto components = simpleFieldsToComponents(*fields, path, false, limits, diagnostics);
            if (!components) {
                continue;
            }
            templateComponents.emplace(simpleId, *components);
            json::Value::Object prototype;
            prototype.emplace("components", *components);
            json::Value::Object converted;
            converted.emplace("id", json::Value{generatedId->second});
            converted.emplace("name", json::Value{simpleId});
            converted.emplace("extends", json::Value{});
            converted.emplace("prototype", json::Value{std::move(prototype)});
            converted.emplace("extensions",
                              makeExtensions(nullptr, std::move(unknown), path, diagnostics));
            canonicalTemplates.emplace_back(json::Value{std::move(converted)});
        }
    }
    canonical.emplace("templates", json::Value{std::move(canonicalTemplates)});

    json::Value::Array canonicalBehaviors;
    if (behaviorsReader) {
        const auto* behaviors = behaviorsReader->readObject();
        if (behaviors != nullptr) {
            if (behaviors->size() > limits.maxBehaviors) {
                addError(diagnostics, "chart.limit.behaviors",
                         "Behavior count exceeds configured limit",
                         std::string{behaviorsReader->fieldPath()});
            }
            for (const auto& [id, value] : *behaviors) {
                if (diagnostics.limitReached()) {
                    break;
                }
                const std::string path = json::appendFieldPath(behaviorsReader->fieldPath(), id);
                if (!isSimpleId(id)) {
                    addError(diagnostics, "chart.simple.id.invalid",
                             "Simple behavior ID is invalid", path);
                    continue;
                }
                json::Reader behaviorReader{value, diagnostics, path};
                const auto* behavior = behaviorReader.readObject();
                if (behavior == nullptr) {
                    continue;
                }
                constexpr std::array known{std::string_view{"tracks"}};
                rejectUnknown(*behavior, known, behaviorReader.fieldPath(), diagnostics);
                const auto tracksReader = behaviorReader.requiredField("tracks");
                if (!tracksReader) {
                    continue;
                }
                json::Value::Object converted;
                converted.emplace("id", json::Value{id});
                converted.emplace("type", json::Value{"behavior.transform.keyframe"});
                converted.emplace("version", json::Value{std::int64_t{1}});
                converted.emplace("tracks",
                                  convertBehaviorTracks(*tracksReader, limits, diagnostics));
                canonicalBehaviors.emplace_back(json::Value{std::move(converted)});
            }
        }
    }
    canonical.emplace("behaviors", json::Value{std::move(canonicalBehaviors)});

    json::Value::Array canonicalObjects;
    if (simpleObjects != nullptr) {
        for (const auto& [simpleId, value] : *simpleObjects) {
            if (diagnostics.limitReached()) {
                break;
            }
            const auto generatedId = objectIds.find(simpleId);
            if (generatedId == objectIds.end()) {
                continue;
            }
            const std::string path = json::appendFieldPath(objectsReader->fieldPath(), simpleId);
            json::Reader objectReader{value, diagnostics, path};
            const auto* fields = objectReader.readObject();
            if (fields == nullptr) {
                continue;
            }
            constexpr std::array known{std::string_view{"kind"},      std::string_view{"parent"},
                                       std::string_view{"template"},  std::string_view{"beat"},
                                       std::string_view{"transform"}, std::string_view{"render"},
                                       std::string_view{"behavior"}};
            json::Value::Object unknown;
            preserveUnknown(*fields, known, path, unknown, diagnostics, generatedId->second);
            preserveSimpleComponentUnknown(*fields, path, unknown, diagnostics,
                                           generatedId->second);

            const json::Value::Object* templateFields = nullptr;
            std::optional<std::string> templateSimpleId;
            if (const auto templateValue = fields->find("template");
                templateValue != fields->end()) {
                json::Reader referenceReader{templateValue->second, diagnostics,
                                             json::appendFieldPath(path, "template")};
                templateSimpleId =
                    parseReferenceText(referenceReader, "template", limits, diagnostics);
                if (templateSimpleId) {
                    const auto source = simpleTemplates != nullptr
                                            ? simpleTemplates->find(*templateSimpleId)
                                            : json::Value::Object::const_iterator{};
                    if (simpleTemplates == nullptr || source == simpleTemplates->end() ||
                        source->second.object() == nullptr) {
                        addError(diagnostics, "chart.simple.template.missing",
                                 "Simple object refers to a missing template",
                                 std::string{referenceReader.fieldPath()});
                        templateSimpleId.reset();
                    } else {
                        templateFields = source->second.object();
                    }
                }
            }
            const auto merged = mergeSimpleFields(templateFields, *fields);
            auto components = simpleFieldsToComponents(merged, path, true, limits, diagnostics);
            if (!components) {
                continue;
            }

            json::Value parent;
            if (const auto parentValue = merged.find("parent"); parentValue != merged.end()) {
                json::Reader parentReader{parentValue->second, diagnostics,
                                          json::appendFieldPath(path, "parent")};
                const auto parentSimpleId =
                    parseReferenceText(parentReader, "object", limits, diagnostics);
                if (parentSimpleId && chartId) {
                    auto parentId = uuidV5(*chartId, "object:" + *parentSimpleId);
                    if (parentId) {
                        parent = makeReference("object", *parentId);
                        if (!objectIds.contains(*parentSimpleId)) {
                            addWarning(
                                diagnostics, "chart.hierarchy.parent_missing",
                                "Object and descendants will be skipped because parent is missing",
                                std::string{parentReader.fieldPath()});
                        }
                    } else {
                        addError(diagnostics, parentId.error(),
                                 std::string{parentReader.fieldPath()});
                    }
                }
            }

            json::Value::Object converted;
            converted.emplace("id", json::Value{generatedId->second});
            converted.emplace("name", json::Value{simpleId});
            converted.emplace("parent", std::move(parent));
            if (templateSimpleId && templateComponents.contains(*templateSimpleId) &&
                templateIds.contains(*templateSimpleId)) {
                converted.emplace("template",
                                  makeReference("template", templateIds.at(*templateSimpleId)));
                converted.emplace("overrides",
                                  json::Value{makeComponentPatches(
                                      templateComponents.at(*templateSimpleId), *components)});
            } else {
                converted.emplace("components", std::move(*components));
            }
            converted.emplace("extensions",
                              makeExtensions(nullptr, std::move(unknown), path, diagnostics));
            canonicalObjects.emplace_back(json::Value{std::move(converted)});
        }
    }
    canonical.emplace("objects", json::Value{std::move(canonicalObjects)});
    canonical.emplace("requiredExtensions", json::Value{json::Value::Array{}});
    canonical.emplace("extensions",
                      makeExtensions(extensionsReader ? &extensionsReader->value() : nullptr,
                                     std::move(rootUnknown), "$/extensions", diagnostics));

    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    auto canonicalText = json::serialize(json::Value{std::move(canonical)});
    if (!canonicalText) {
        addError(diagnostics, canonicalText.error(), "$");
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    auto imported = CanonicalChartLoader::load(*canonicalText, limits);
    diagnostics.append(std::move(imported.diagnostics));
    diagnostics.sortDeterministically();
    if (!imported.document || diagnostics.hasErrors()) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    return ChartDocumentResult{std::move(imported.document), std::move(diagnostics)};
}

} // namespace cuexis::chart
