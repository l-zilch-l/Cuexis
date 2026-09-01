#include <cuexis/chart/chart_v4_resolver.hpp>

#include <cuexis/chart/animation_template_loader.hpp>
#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/chart_writer.hpp>
#include <cuexis/chart/detail/chart_v4_resolver_internal.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>
#include <cuexis/json/value.hpp>

#include "canonical_chart_loader_internal.hpp"
#include "chart_parameter_resolver_internal.hpp"
#include "chart_project_path_internal.hpp"
#include "chart_v4_animation_reader_internal.hpp"
#include "chart_v4_common_reader_internal.hpp"
#include "chart_v4_loader_internal.hpp"
#include "chart_writer_internal.hpp"
#include "diagnostic_limit.hpp"
#include "sha256_internal.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart {
namespace {

using PropertySet = std::set<std::string, std::less<>>;

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
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

void appendTemplateDiagnostics(core::Diagnostics& destination, core::Diagnostics source,
                               std::string_view path, std::string_view importId) {
    for (const auto& item : source.items()) {
        auto diagnostic =
            core::Diagnostic{item.severity(), std::string{item.code()}, std::string{item.message()},
                             std::string{item.fieldPath()}};
        for (const auto& context : item.context()) {
            diagnostic.withContext(context.key, context.value);
        }
        diagnostic.withContext("source", std::string{path});
        diagnostic.withContext("template_id", std::string{importId});
        diagnostic.withContext("import_id", std::string{importId});
        destination.add(std::move(diagnostic));
    }
}

[[nodiscard]] auto parameterJsonValue(const ChartParameterValue& value) -> json::Value {
    if (const auto* number = std::get_if<double>(&value)) {
        return json::Value{*number == 0.0 ? 0.0 : *number};
    }
    const auto& rational = std::get<RationalBeat>(value);
    json::Value::Object object;
    object.emplace("denominator", json::Value{rational.denominator()});
    object.emplace("numerator", json::Value{rational.numerator()});
    return json::Value{std::move(object)};
}

[[nodiscard]] auto decodePointerSegment(std::string_view encoded) -> std::string {
    std::string decoded;
    decoded.reserve(encoded.size());
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
    return decoded;
}

[[nodiscard]] auto replaceAtFieldPath(json::Value& root, std::string_view fieldPath,
                                      const json::Value& replacement) -> bool {
    if (fieldPath.empty() || fieldPath.front() != '$') {
        return false;
    }
    json::Value* current = &root;
    std::size_t begin = 1;
    while (begin < fieldPath.size()) {
        if (fieldPath[begin] != '/') {
            return false;
        }
        ++begin;
        const auto end = fieldPath.find('/', begin);
        const auto segment = decodePointerSegment(fieldPath.substr(
            begin, end == std::string_view::npos ? fieldPath.size() - begin : end - begin));
        const bool final = end == std::string_view::npos;
        if (auto* object = current->object()) {
            const auto item = object->find(segment);
            if (item == object->end()) {
                return false;
            }
            if (final) {
                item->second = replacement;
                return true;
            }
            current = &item->second;
        } else if (auto* array = current->array()) {
            std::size_t index{};
            const auto [pointer, error] =
                std::from_chars(segment.data(), segment.data() + segment.size(), index);
            if (error != std::errc{} || pointer != segment.data() + segment.size() ||
                index >= array->size()) {
                return false;
            }
            if (final) {
                (*array)[index] = replacement;
                return true;
            }
            current = &(*array)[index];
        } else {
            return false;
        }
        begin = end;
    }
    return false;
}

void eraseAnimator(json::Value& components) {
    auto* object = components.object();
    if (object == nullptr) {
        return;
    }
    object->erase("cuexis.animator");
    if (object->empty()) {
        json::Value::Object element;
        element.emplace("version", json::Value{std::int64_t{1}});
        object->emplace("cuexis.element", json::Value{std::move(element)});
    }
}

void eraseAnimatorPatches(json::Value& patches) {
    auto* array = patches.array();
    if (array == nullptr) {
        return;
    }
    std::erase_if(*array, [](const json::Value& patch) {
        const auto* pathValue = patch.find("path");
        const auto* path = pathValue != nullptr ? pathValue->string() : nullptr;
        return path != nullptr && (*path == "/components/cuexis.animator" ||
                                   path->starts_with("/components/cuexis.animator/"));
    });
}

void eraseAnimators(json::Value& source, std::string_view ownerArray) {
    auto* ownersValue = source.find(ownerArray);
    auto* owners = ownersValue != nullptr ? ownersValue->array() : nullptr;
    if (owners == nullptr) {
        return;
    }
    for (auto& owner : *owners) {
        if (ownerArray == "templates") {
            if (auto* prototype = owner.find("prototype")) {
                if (auto* components = prototype->find("components")) {
                    eraseAnimator(*components);
                }
            }
            if (auto* patches = owner.find("patch")) {
                eraseAnimatorPatches(*patches);
            }
        } else {
            if (auto* components = owner.find("components")) {
                eraseAnimator(*components);
            }
            if (auto* overrides = owner.find("overrides")) {
                eraseAnimatorPatches(*overrides);
            }
        }
    }
}

void normalizeResolvedChart(ChartDocument& chart) {
    std::ranges::sort(chart.timing.tempoEvents, {}, &TempoEvent::startBeat);
    std::ranges::sort(chart.timing.stops, [](const TimingStop& left, const TimingStop& right) {
        if (left.beat != right.beat) {
            return left.beat < right.beat;
        }
        return left.durationMs < right.durationMs;
    });
    std::ranges::sort(chart.templates, {}, [](const ChartTemplate& item) { return item.id.value; });
    for (auto& behavior : chart.behaviors) {
        std::ranges::sort(behavior.tracks.items, {}, &BehaviorTrack::property);
        for (auto& track : behavior.tracks.items) {
            std::ranges::sort(track.keys, {}, &BehaviorKey::beat);
        }
        std::ranges::sort(behavior.events,
                          [](const BehaviorEvent& left, const BehaviorEvent& right) {
                              if (left.property != right.property) {
                                  return left.property < right.property;
                              }
                              if (left.startBeat != right.startBeat) {
                                  return left.startBeat < right.startBeat;
                              }
                              return left.groupId < right.groupId;
                          });
        std::ranges::sort(behavior.stepEvents,
                          [](const BehaviorStepEvent& left, const BehaviorStepEvent& right) {
                              if (left.property != right.property) {
                                  return left.property < right.property;
                              }
                              if (left.beat != right.beat) {
                                  return left.beat < right.beat;
                              }
                              return left.groupId < right.groupId;
                          });
    }
    std::ranges::sort(chart.behaviors, {}, [](const ChartBehavior& item) { return item.id.value; });
    std::ranges::sort(chart.objects, {}, [](const ChartObject& item) { return item.id.value; });
}

[[nodiscard]] auto makeConcreteChart(json::Value source, const ChartLimits& limits,
                                     core::Diagnostics& diagnostics)
    -> std::optional<ChartDocument> {
    auto* root = source.object();
    if (root == nullptr) {
        addError(diagnostics, "chart.resolve.source_invalid", "Chart source root is invalid", "$");
        return std::nullopt;
    }
    root->insert_or_assign("version", json::Value{std::int64_t{3}});
    root->erase("parameters");
    root->erase("animationTemplateImports");
    root->erase("animationClips");
    root->insert_or_assign("requiredExtensions", json::Value{json::Value::Array{}});
    eraseAnimators(source, "templates");
    eraseAnimators(source, "objects");
    auto loaded = detail::loadCanonicalValue(std::move(source), limits);
    diagnostics.append(std::move(loaded.diagnostics));
    if (!loaded.document) {
        return std::nullopt;
    }
    loaded.document->version = 4;
    normalizeResolvedChart(*loaded.document);
    return std::move(*loaded.document);
}

[[nodiscard]] auto stringField(const json::Value& value, std::string_view name)
    -> std::optional<std::string> {
    const auto* child = value.find(name);
    const auto* text = child != nullptr ? child->string() : nullptr;
    return text != nullptr ? std::optional<std::string>{*text} : std::nullopt;
}

[[nodiscard]] auto referenceId(const json::Value& value) -> std::optional<std::string> {
    return stringField(value, "id");
}

[[nodiscard]] auto readAnimator(const json::Value* components, std::string path,
                                const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<AnimatorComponent> {
    if (components == nullptr) {
        return std::nullopt;
    }
    const auto* animator = components->find("cuexis.animator");
    if (animator == nullptr) {
        return std::nullopt;
    }
    std::vector<ParameterUse> unexpectedUses;
    json::Reader reader{*animator, diagnostics, std::move(path)};
    auto result = detail::readAnimatorComponent(reader, limits, diagnostics, unexpectedUses);
    for (const auto& use : unexpectedUses) {
        addError(diagnostics, "chart.parameter.use_not_allowed",
                 "Resolved Animator still contains a parameter reference", use.fieldPath);
    }
    return result;
}

void applyAnimatorPatches(std::optional<AnimatorComponent>& animator,
                          const json::Value::Array& patches, std::string_view patchesPath,
                          const ChartLimits& limits, core::Diagnostics& diagnostics) {
    if (patches.size() > limits.maxPatchesPerTemplate) {
        addError(diagnostics, "chart.limit.patches", "Patch count exceeds the configured limit",
                 std::string{patchesPath});
        return;
    }
    for (std::size_t index = 0; index < patches.size(); ++index) {
        const auto& patch = patches[index];
        const auto path = json::appendIndexPath(patchesPath, index);
        const auto operation = stringField(patch, "op");
        const auto target = stringField(patch, "path");
        if (!operation || !target || *target != "/components/cuexis.animator") {
            continue;
        }
        const auto* value = patch.find("value");
        if (*operation == "remove") {
            if (value != nullptr) {
                addError(diagnostics, "chart.patch.remove_has_value",
                         "Remove patch must not contain a value", path + "/value");
                continue;
            }
            if (!animator) {
                addError(diagnostics, "chart.patch.target_missing",
                         "Remove patch target does not exist", path + "/path");
                continue;
            }
            animator.reset();
            continue;
        }
        if (*operation != "add" && *operation != "replace") {
            addError(diagnostics, "chart.patch.operation_unsupported",
                     "Only add, remove, and replace patch operations are supported", path + "/op");
            continue;
        }
        if (value == nullptr) {
            addError(diagnostics, "chart.patch.value_missing", "Patch operation requires a value",
                     path + "/value");
            continue;
        }
        if (*operation == "replace" && !animator) {
            addError(diagnostics, "chart.patch.target_missing",
                     "Replace patch target does not exist", path + "/path");
            continue;
        }
        std::vector<ParameterUse> unexpectedUses;
        json::Reader reader{*value, diagnostics, path + "/value"};
        auto replacement =
            detail::readAnimatorComponent(reader, limits, diagnostics, unexpectedUses);
        for (const auto& use : unexpectedUses) {
            addError(diagnostics, "chart.parameter.use_not_allowed",
                     "Resolved Animator patch still contains a parameter reference", use.fieldPath);
        }
        if (replacement) {
            animator = std::move(*replacement);
        }
    }
}

struct RawAnimatorTemplate final {
    std::string id;
    std::optional<std::string> parentId;
    std::optional<AnimatorComponent> prototype;
    json::Value::Array patches;
    std::string path;
    enum class State { Unvisited, Visiting, Resolved, Failed } state{State::Unvisited};
    std::optional<AnimatorComponent> resolved;
};

[[nodiscard]] auto resolveAnimatorTemplate(
    const std::string& id, std::map<std::string, RawAnimatorTemplate, std::less<>>& templates,
    const ChartLimits& limits, core::Diagnostics& diagnostics) -> std::optional<AnimatorComponent> {
    std::vector<std::string> chain;
    std::string current = id;
    std::optional<AnimatorComponent> animator;
    bool failed = false;
    while (true) {
        auto& item = templates.at(current);
        if (item.state == RawAnimatorTemplate::State::Resolved) {
            animator = item.resolved;
            break;
        }
        if (item.state == RawAnimatorTemplate::State::Failed) {
            failed = true;
            break;
        }
        if (item.state == RawAnimatorTemplate::State::Visiting) {
            addError(diagnostics, "chart.template.inheritance_cycle",
                     "Template inheritance contains a cycle", item.path + "/extends");
            failed = true;
            break;
        }
        item.state = RawAnimatorTemplate::State::Visiting;
        chain.push_back(current);
        if (!item.parentId) {
            animator = item.prototype;
            break;
        }
        const auto parent = templates.find(*item.parentId);
        if (parent == templates.end()) {
            addError(diagnostics, "chart.template.parent_missing",
                     "Template extends a missing template", item.path + "/extends");
            failed = true;
            break;
        }
        current = parent->first;
    }
    if (failed) {
        for (const auto& templateId : chain) {
            templates.at(templateId).state = RawAnimatorTemplate::State::Failed;
        }
        return std::nullopt;
    }
    while (!chain.empty()) {
        auto& item = templates.at(chain.back());
        if (item.parentId) {
            applyAnimatorPatches(animator, item.patches, item.path + "/patch", limits, diagnostics);
        }
        item.resolved = animator;
        item.state = RawAnimatorTemplate::State::Resolved;
        chain.pop_back();
    }
    return templates.at(id).resolved;
}

[[nodiscard]] auto concreteAnimators(const json::Value& source, const ChartLimits& limits,
                                     core::Diagnostics& diagnostics)
    -> std::map<std::string, AnimatorComponent, std::less<>> {
    std::map<std::string, RawAnimatorTemplate, std::less<>> templates;
    const auto* templatesValue = source.find("templates");
    const auto* templateArray = templatesValue != nullptr ? templatesValue->array() : nullptr;
    if (templateArray != nullptr) {
        for (std::size_t index = 0; index < templateArray->size(); ++index) {
            const auto& item = (*templateArray)[index];
            const auto id = stringField(item, "id");
            if (!id) {
                continue;
            }
            RawAnimatorTemplate raw;
            raw.id = *id;
            raw.path = json::appendIndexPath("$/templates", index);
            const auto* extends = item.find("extends");
            if (extends != nullptr && !extends->isNull()) {
                raw.parentId = referenceId(*extends);
                if (const auto* patches = item.find("patch");
                    patches != nullptr && patches->array()) {
                    raw.patches = *patches->array();
                }
            } else if (const auto* prototype = item.find("prototype")) {
                raw.prototype = readAnimator(prototype->find("components"),
                                             raw.path + "/prototype/components/cuexis.animator",
                                             limits, diagnostics);
            }
            templates.emplace(raw.id, std::move(raw));
        }
    }
    for (const auto& [id, ignored] : templates) {
        static_cast<void>(ignored);
        static_cast<void>(resolveAnimatorTemplate(id, templates, limits, diagnostics));
    }

    std::map<std::string, AnimatorComponent, std::less<>> result;
    const auto* objectsValue = source.find("objects");
    const auto* objects = objectsValue != nullptr ? objectsValue->array() : nullptr;
    if (objects == nullptr) {
        return result;
    }
    for (std::size_t index = 0; index < objects->size(); ++index) {
        const auto& item = (*objects)[index];
        const auto id = stringField(item, "id");
        if (!id) {
            continue;
        }
        const auto path = json::appendIndexPath("$/objects", index);
        std::optional<AnimatorComponent> animator;
        if (const auto* components = item.find("components")) {
            animator =
                readAnimator(components, path + "/components/cuexis.animator", limits, diagnostics);
        } else if (const auto* templateReference = item.find("template")) {
            const auto templateId = referenceId(*templateReference);
            if (templateId) {
                const auto sourceTemplate = templates.find(*templateId);
                if (sourceTemplate != templates.end()) {
                    animator = sourceTemplate->second.resolved;
                }
            }
            if (const auto* overrides = item.find("overrides");
                overrides != nullptr && overrides->array()) {
                applyAnimatorPatches(animator, *overrides->array(), path + "/overrides", limits,
                                     diagnostics);
            }
        }
        if (animator) {
            result.emplace(*id, std::move(*animator));
        }
    }
    return result;
}

[[nodiscard]] auto
extensionSatisfied(const RequiredExtension& required,
                   const std::map<std::string, std::uint32_t, std::less<>>& supported) -> bool {
    const auto item = supported.find(required.id);
    return item != supported.end() && item->second >= required.version;
}

void validateRequiredExtensions(const std::vector<RequiredExtension>& required,
                                const std::map<std::string, std::uint32_t, std::less<>>& supported,
                                core::Diagnostics& diagnostics, std::string path,
                                std::optional<std::string_view> source = {},
                                std::optional<std::string_view> templateId = {},
                                std::optional<std::string_view> importId = {}) {
    for (const auto& extension : required) {
        if (extensionSatisfied(extension, supported)) {
            continue;
        }
        auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error,
                                           "chart.extension.required_unsupported",
                                           "Required extension is not supported", path};
        diagnostic.withContext("extension_id", extension.id);
        diagnostic.withContext("required_version", std::to_string(extension.version));
        if (source) {
            diagnostic.withContext("source", std::string{*source});
        }
        if (templateId) {
            diagnostic.withContext("template_id", std::string{*templateId});
        }
        if (importId) {
            diagnostic.withContext("import_id", std::string{*importId});
        }
        diagnostics.add(std::move(diagnostic));
    }
}

struct ImportedTemplate final {
    AnimationTemplateDocument document;
    CanonicalContentIdentity identity;
    std::string source;
    std::string importFieldPath;
};

[[nodiscard]] auto
loadImports(const ChartV4SourceDocument& source, std::span<const ProjectDocument> projectDocuments,
            const std::map<std::string, std::uint32_t, std::less<>>& supportedExtensions,
            const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::map<std::string, ImportedTemplate, std::less<>> {
    std::map<std::string, const ProjectDocument*, std::less<>> documents;
    std::set<std::string, std::less<>> foldedPaths;
    for (std::size_t index = 0; index < projectDocuments.size(); ++index) {
        const auto& document = projectDocuments[index];
        if (!detail::isPortableProjectPath(document.path)) {
            addError(diagnostics, "cxt.template.invalid", "Project document path is not portable",
                     "$/projectDocuments/" + std::to_string(index) + "/path");
            continue;
        }
        if (!foldedPaths.emplace(detail::portableProjectPathCaseKey(document.path)).second ||
            !documents.emplace(document.path, &document).second) {
            addError(diagnostics, "cxt.import.duplicate", "Project document path is duplicated",
                     "$/projectDocuments/" + std::to_string(index) + "/path");
        }
    }

    std::map<std::string, ImportedTemplate, std::less<>> imports;
    if (source.animationTemplateImports.size() > limits.maxAnimationImports) {
        addError(diagnostics, "cxt.budget.exceeded",
                 "Animation template import count exceeds the configured limit",
                 "$/animationTemplateImports");
    }
    for (const auto& item : source.animationTemplateImports) {
        const auto document = documents.find(item.source);
        if (document == documents.end()) {
            auto diagnostic = core::Diagnostic{
                core::DiagnosticSeverity::Error, "cxt.import.missing",
                "Animation template source is missing", item.fieldPath + "/source"};
            diagnostic.withContext("source", item.source);
            diagnostic.withContext("template_id", item.id);
            diagnostic.withContext("import_id", item.id);
            diagnostics.add(std::move(diagnostic));
            continue;
        }
        if (document->second->utf8Text.size() > limits.maxAnimationTemplateBytes) {
            auto diagnostic = core::Diagnostic{
                core::DiagnosticSeverity::Error, "cxt.budget.exceeded",
                "Animation template exceeds the byte limit", item.fieldPath + "/source"};
            diagnostic.withContext("source", item.source);
            diagnostic.withContext("template_id", item.id);
            diagnostic.withContext("import_id", item.id);
            diagnostics.add(std::move(diagnostic));
            continue;
        }
        auto loaded = AnimationTemplateLoader::load(document->second->utf8Text, limits);
        const bool valid = loaded.hasValue();
        appendTemplateDiagnostics(diagnostics, std::move(loaded.diagnostics), item.source, item.id);
        if (!valid || !loaded.document) {
            continue;
        }
        if (loaded.document->templateId != item.id) {
            auto diagnostic = core::Diagnostic{
                core::DiagnosticSeverity::Error, "cxt.template.id_mismatch",
                "Animation template ID does not match the Chart import", item.fieldPath + "/id"};
            diagnostic.withContext("source", item.source);
            diagnostic.withContext("import_id", item.id);
            diagnostic.withContext("template_id", loaded.document->templateId);
            diagnostics.add(std::move(diagnostic));
            continue;
        }
        validateRequiredExtensions(loaded.document->requiredExtensions, supportedExtensions,
                                   diagnostics, "$/requiredExtensions", item.source,
                                   loaded.document->templateId, item.id);
        auto canonical = ChartWriter::writeAnimationTemplate(*loaded.document, limits);
        if (!canonical) {
            addError(diagnostics, canonical.error(), item.fieldPath + "/source");
            continue;
        }
        imports.emplace(item.id,
                        ImportedTemplate{std::move(*loaded.document),
                                         CanonicalContentIdentity{detail::sha256(*canonical)},
                                         item.source, item.fieldPath});
    }
    return imports;
}

[[nodiscard]] auto
resolvedWeight(const WeightSource& source,
               const std::map<std::string, ChartParameterValue, std::less<>>& parameters,
               core::Diagnostics& diagnostics, std::string path) -> std::optional<double> {
    if (const auto* literal = std::get_if<double>(&source)) {
        return *literal == 0.0 ? 0.0 : *literal;
    }
    const auto& reference = std::get<ParameterReference>(source);
    const auto item = parameters.find(reference.id);
    const auto* value = item != parameters.end() ? std::get_if<double>(&item->second) : nullptr;
    if (value == nullptr || !std::isfinite(*value) || *value < 0.0 || *value > 1.0) {
        addError(diagnostics, "chart.parameter.type_mismatch",
                 "Weight parameter did not resolve to a value within [0, 1]", std::move(path));
        return std::nullopt;
    }
    return *value == 0.0 ? 0.0 : *value;
}

[[nodiscard]] auto
resolvedRational(const RationalSource& source,
                 const std::map<std::string, ChartParameterValue, std::less<>>& parameters,
                 core::Diagnostics& diagnostics, std::string path) -> std::optional<RationalBeat> {
    if (const auto* literal = std::get_if<RationalBeat>(&source)) {
        return *literal;
    }
    const auto& reference = std::get<ParameterReference>(source);
    const auto item = parameters.find(reference.id);
    const auto* value =
        item != parameters.end() ? std::get_if<RationalBeat>(&item->second) : nullptr;
    if (value == nullptr || value->numerator() <= 0) {
        addError(diagnostics, "chart.parameter.type_mismatch",
                 "Duration-scale parameter did not resolve to a positive Rational",
                 std::move(path));
        return std::nullopt;
    }
    return *value;
}

[[nodiscard]] auto propertyName(AnimationProperty property) -> std::string_view {
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

[[nodiscard]] auto propertyName(AnimationStepProperty property) -> std::string_view {
    switch (property) {
    case AnimationStepProperty::RenderVisible:
        return "render.visible";
    case AnimationStepProperty::RenderMaterial:
        return "render.material";
    }
    return "";
}

[[nodiscard]] auto allProperties() -> const std::array<std::string_view, 9>& {
    static constexpr std::array values{
        std::string_view{"material.opacity"},     std::string_view{"material.tint"},
        std::string_view{"render.material"},      std::string_view{"render.visible"},
        std::string_view{"transform.position.x"}, std::string_view{"transform.position.y"},
        std::string_view{"transform.position.z"}, std::string_view{"transform.rotation"},
        std::string_view{"transform.scale"},
    };
    return values;
}

[[nodiscard]] auto expandMask(const PropertyMask& mask) -> PropertySet {
    PropertySet result(mask.properties.begin(), mask.properties.end());
    for (const auto& prefix : mask.prefixes) {
        for (const auto property : allProperties()) {
            if (property.starts_with(prefix)) {
                result.emplace(property);
            }
        }
    }
    return result;
}

[[nodiscard]] auto clipProperties(const AnimationClip& clip) -> PropertySet {
    PropertySet result;
    for (const auto& track : clip.tracks) {
        result.emplace(propertyName(track.property));
    }
    for (const auto& track : clip.stepTracks) {
        result.emplace(propertyName(track.property));
    }
    return result;
}

[[nodiscard]] auto propertyMask(const AnimationClip& clip) -> PropertyMask {
    const auto properties = clipProperties(clip);
    return PropertyMask{std::vector<std::string>{properties.begin(), properties.end()}, {}};
}

[[nodiscard]] auto intersects(const PropertySet& left, const PropertySet& right) -> bool {
    auto leftItem = left.begin();
    auto rightItem = right.begin();
    while (leftItem != left.end() && rightItem != right.end()) {
        if (*leftItem == *rightItem) {
            return true;
        }
        if (*leftItem < *rightItem) {
            ++leftItem;
        } else {
            ++rightItem;
        }
    }
    return false;
}

[[nodiscard]] auto intersection(const PropertySet& left, const PropertySet& right) -> PropertySet {
    PropertySet result;
    std::ranges::set_intersection(left, right, std::inserter(result, result.end()));
    return result;
}

[[nodiscard]] auto isSubset(const PropertySet& subset, const PropertySet& superset) -> bool {
    return std::ranges::includes(superset, subset);
}

[[nodiscard]] auto isDiscrete(const PropertySet& properties) -> bool {
    return properties.contains("render.visible") || properties.contains("render.material");
}

[[nodiscard]] auto additiveAllowed(const PropertySet& properties) -> bool {
    return std::ranges::all_of(
        properties, [](const std::string& property) { return property.starts_with("transform."); });
}

[[nodiscard]] auto scaleIsPositive(const AnimationClip& clip) -> bool {
    for (const auto& track : clip.tracks) {
        if (track.property != AnimationProperty::TransformScale) {
            continue;
        }
        for (const auto& segment : track.segments) {
            const auto* start = std::get_if<core::Vec3>(&segment.startValue);
            const auto* end = std::get_if<core::Vec3>(&segment.endValue);
            if (start == nullptr || end == nullptr || start->x <= 0.0F || start->y <= 0.0F ||
                start->z <= 0.0F || end->x <= 0.0F || end->y <= 0.0F || end->z <= 0.0F) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] auto identityLess(const AnimationRecordIdentity& left,
                                const AnimationRecordIdentity& right) -> bool {
    return left < right;
}

struct LayerPathKey final {
    std::string objectId;
    AnimationRecordIdentity layerIdentity;

    auto operator<=>(const LayerPathKey&) const = default;
};

struct GroupPathKey final {
    std::string objectId;
    AnimationRecordIdentity layerIdentity;
    AnimationRecordIdentity groupIdentity;

    auto operator<=>(const GroupPathKey&) const = default;
};

struct InstancePathKey final {
    std::string objectId;
    AnimationRecordIdentity layerIdentity;
    AnimationRecordIdentity groupIdentity;
    AnimationRecordIdentity instanceIdentity;

    auto operator<=>(const InstancePathKey&) const = default;
};

struct ProgramDiagnosticScope final {
    std::string fieldPath;
    std::optional<GeneratedAnimationIdentity> generatedIdentity;
};

struct ProgramPaths final {
    std::map<AnimationRecordIdentity, ProgramDiagnosticScope, std::less<>> clips;
    std::map<LayerPathKey, ProgramDiagnosticScope, std::less<>> layers;
    std::map<GroupPathKey, ProgramDiagnosticScope, std::less<>> groups;
    std::map<InstancePathKey, ProgramDiagnosticScope, std::less<>> instances;
};

struct ProgramBuild final {
    AnimationProgramInput program;
    ProgramPaths paths;
    std::size_t generatedRecords{};
};

[[nodiscard]] auto makeGeneratedIdentity(std::string_view objectId, const TemplateBinding& binding,
                                         GeneratedRecordKind kind) -> GeneratedAnimationIdentity {
    return GeneratedAnimationIdentity{std::string{objectId}, binding.bindingId, binding.templateId,
                                      kind};
}

[[nodiscard]] auto generatedRecordKindName(GeneratedRecordKind kind) -> std::string_view {
    switch (kind) {
    case GeneratedRecordKind::Clip:
        return "clip";
    case GeneratedRecordKind::Layer:
        return "layer";
    case GeneratedRecordKind::BlendGroup:
        return "blend_group";
    case GeneratedRecordKind::ClipInstance:
        return "clip_instance";
    }
    return "unknown";
}

[[nodiscard]] auto generatedScope(std::string fieldPath, const GeneratedAnimationIdentity& identity)
    -> ProgramDiagnosticScope {
    return ProgramDiagnosticScope{std::move(fieldPath), identity};
}

void addScopedError(core::Diagnostics& diagnostics, std::string code, std::string message,
                    const ProgramDiagnosticScope* scope, std::string fallbackPath) {
    auto diagnostic =
        core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code), std::move(message),
                         scope != nullptr ? scope->fieldPath : std::move(fallbackPath)};
    if (scope != nullptr && scope->generatedIdentity) {
        const auto& identity = *scope->generatedIdentity;
        diagnostic.withContext("object_id", identity.objectId);
        diagnostic.withContext("binding_id", identity.bindingId);
        diagnostic.withContext("template_id", identity.templateId);
        diagnostic.withContext("record_kind",
                               std::string{generatedRecordKindName(identity.recordKind)});
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto preferGeneratedScope(const ProgramDiagnosticScope* preferred,
                                        const ProgramDiagnosticScope* alternate)
    -> const ProgramDiagnosticScope* {
    if (preferred != nullptr && preferred->generatedIdentity) {
        return preferred;
    }
    if (alternate != nullptr && alternate->generatedIdentity) {
        return alternate;
    }
    return preferred != nullptr ? preferred : alternate;
}

[[nodiscard]] auto
buildProgram(const ChartV4SourceDocument& source,
             const std::map<std::string, AnimatorComponent, std::less<>>& animators,
             const std::map<std::string, ImportedTemplate, std::less<>>& imports,
             const std::map<std::string, ChartParameterValue, std::less<>>& parameters,
             const ChartLimits& limits, core::Diagnostics& diagnostics) -> ProgramBuild {
    ProgramBuild result;
    if (source.animationClips.size() > limits.maxAnimationClips) {
        addError(diagnostics, "chart.animation.generated_limit",
                 "Animation clip count exceeds the configured limit", "$/animationClips");
    }
    result.program.clips.reserve(source.animationClips.size());
    for (const auto& clip : source.animationClips) {
        result.program.clips.push_back(AnimationProgramClip{clip.id, clip});
        result.paths.clips.emplace(AnimationRecordIdentity{clip.id},
                                   ProgramDiagnosticScope{clip.fieldPath, std::nullopt});
    }
    const auto one = RationalBeat::one();

    for (const auto& [objectId, animator] : animators) {
        ObjectAnimationProgram object{ChartObjectId{objectId}, {}};
        for (const auto& layer : animator.layers) {
            const auto layerWeight =
                resolvedWeight(layer.weight, parameters, diagnostics, layer.fieldPath + "/weight");
            if (!layerWeight) {
                continue;
            }
            ResolvedAnimationLayer resolvedLayer{
                layer.layerId, layer.priority, *layerWeight, layer.propertyMask, {}};
            result.paths.layers.emplace(LayerPathKey{objectId, resolvedLayer.identity},
                                        ProgramDiagnosticScope{layer.fieldPath, std::nullopt});
            for (const auto& group : layer.blendGroups) {
                const auto groupWeight = resolvedWeight(group.weight, parameters, diagnostics,
                                                        group.fieldPath + "/weight");
                if (!groupWeight) {
                    continue;
                }
                ResolvedBlendGroup resolvedGroup{group.groupId, group.mode, *groupWeight, {}};
                result.paths.groups.emplace(
                    GroupPathKey{objectId, resolvedLayer.identity, resolvedGroup.identity},
                    ProgramDiagnosticScope{group.fieldPath, std::nullopt});
                for (const auto& instance : group.instances) {
                    const auto instanceWeight = resolvedWeight(
                        instance.weight, parameters, diagnostics, instance.fieldPath + "/weight");
                    if (!instanceWeight) {
                        continue;
                    }
                    const AnimationRecordIdentity instanceIdentity{instance.instanceId};
                    result.paths.instances.emplace(
                        InstancePathKey{objectId, resolvedLayer.identity, resolvedGroup.identity,
                                        instanceIdentity},
                        ProgramDiagnosticScope{instance.fieldPath, std::nullopt});
                    resolvedGroup.instances.push_back(
                        ResolvedClipInstance{instanceIdentity, instance.clipId, instance.startBeat,
                                             one, instance.iterations, instance.fillMode,
                                             *instanceWeight, instance.propertyMask});
                }
                std::ranges::sort(resolvedGroup.instances, [](const auto& left, const auto& right) {
                    return identityLess(left.identity, right.identity);
                });
                resolvedLayer.blendGroups.push_back(std::move(resolvedGroup));
            }
            std::ranges::sort(resolvedLayer.blendGroups, [](const auto& left, const auto& right) {
                return identityLess(left.identity, right.identity);
            });
            object.layers.push_back(std::move(resolvedLayer));
        }

        for (const auto& binding : animator.templateBindings) {
            const auto imported = imports.find(binding.templateId);
            if (imported == imports.end()) {
                addError(diagnostics, "chart.animation.template_reference_missing",
                         "Template binding refers to an unavailable CXT import",
                         binding.fieldPath + "/template");
                continue;
            }
            const auto durationScale =
                resolvedRational(binding.durationScale, parameters, diagnostics,
                                 binding.fieldPath + "/durationScale");
            const auto weight = resolvedWeight(binding.weight, parameters, diagnostics,
                                               binding.fieldPath + "/weight");
            if (!durationScale || !weight) {
                continue;
            }
            constexpr std::size_t recordsPerBinding = 4;
            if (recordsPerBinding > limits.maxGeneratedAnimationRecords ||
                result.generatedRecords > limits.maxGeneratedAnimationRecords - recordsPerBinding) {
                addError(diagnostics, "chart.animation.generated_limit",
                         "Generated animation record count exceeds the configured limit",
                         binding.fieldPath);
                continue;
            }
            result.generatedRecords += recordsPerBinding;
            const auto clipIdentity =
                makeGeneratedIdentity(objectId, binding, GeneratedRecordKind::Clip);
            const auto layerIdentity =
                makeGeneratedIdentity(objectId, binding, GeneratedRecordKind::Layer);
            const auto groupIdentity =
                makeGeneratedIdentity(objectId, binding, GeneratedRecordKind::BlendGroup);
            const auto instanceIdentity =
                makeGeneratedIdentity(objectId, binding, GeneratedRecordKind::ClipInstance);
            auto clip = imported->second.document.clip;
            clip.id = binding.templateId;
            result.program.clips.push_back(AnimationProgramClip{clipIdentity, std::move(clip)});
            result.paths.clips.emplace(AnimationRecordIdentity{clipIdentity},
                                       generatedScope(binding.fieldPath, clipIdentity));
            result.paths.layers.emplace(
                LayerPathKey{objectId, AnimationRecordIdentity{layerIdentity}},
                generatedScope(binding.fieldPath, layerIdentity));
            result.paths.groups.emplace(GroupPathKey{objectId,
                                                     AnimationRecordIdentity{layerIdentity},
                                                     AnimationRecordIdentity{groupIdentity}},
                                        generatedScope(binding.fieldPath, groupIdentity));
            result.paths.instances.emplace(
                InstancePathKey{objectId, AnimationRecordIdentity{layerIdentity},
                                AnimationRecordIdentity{groupIdentity},
                                AnimationRecordIdentity{instanceIdentity}},
                generatedScope(binding.fieldPath, instanceIdentity));

            const auto mask = propertyMask(imported->second.document.clip);
            ResolvedClipInstance instance{instanceIdentity,
                                          clipIdentity,
                                          binding.startBeat,
                                          *durationScale,
                                          imported->second.document.application.iterations,
                                          imported->second.document.application.fillMode,
                                          1.0,
                                          mask};
            ResolvedBlendGroup group{groupIdentity,
                                     imported->second.document.application.blendMode,
                                     *weight,
                                     {std::move(instance)}};
            object.layers.push_back(ResolvedAnimationLayer{
                layerIdentity, binding.priority, 1.0, mask, {std::move(group)}});
        }
        std::ranges::sort(object.layers, [](const auto& left, const auto& right) {
            if (left.priority != right.priority) {
                return left.priority < right.priority;
            }
            return identityLess(left.identity, right.identity);
        });
        result.program.objects.push_back(std::move(object));
    }
    std::ranges::sort(result.program.clips, [](const auto& left, const auto& right) {
        return identityLess(left.identity, right.identity);
    });
    std::ranges::sort(result.program.objects, {},
                      [](const auto& item) { return item.objectId.value; });
    return result;
}

[[nodiscard]] auto
findClip(const std::map<AnimationRecordIdentity, const AnimationClip*, std::less<>>& clips,
         const AnimationRecordIdentity& identity) -> const AnimationClip* {
    const auto item = clips.find(identity);
    return item != clips.end() ? item->second : nullptr;
}

[[nodiscard]] auto checkedSum(std::size_t left, std::size_t right) -> std::optional<std::size_t> {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

[[nodiscard]] auto checkedAccumulate(std::size_t& total, std::size_t count, std::size_t limit)
    -> bool {
    if (count > limit || total > limit - count) {
        return false;
    }
    total += count;
    return true;
}

void validateClipBudget(const AnimationClip& clip, std::string_view diagnosticPath,
                        const ProgramDiagnosticScope* scope, const ChartLimits& limits,
                        std::size_t& totalTracks, std::size_t& totalSegmentsAndSteps,
                        core::Diagnostics& diagnostics) {
    const auto trackCount = checkedSum(clip.tracks.size(), clip.stepTracks.size());
    if (!trackCount || *trackCount > limits.maxAnimationTracksPerClip) {
        addScopedError(diagnostics, "chart.animation.generated_limit",
                       "Animation track count exceeds the configured per-Clip limit", scope,
                       std::string{diagnosticPath});
    }
    if (!trackCount || !checkedAccumulate(totalTracks, *trackCount, limits.maxAnimationTracks)) {
        addScopedError(diagnostics, "chart.animation.generated_limit",
                       "Total animation track count exceeds the configured limit", scope,
                       std::string{diagnosticPath});
    }
    for (const auto& track : clip.tracks) {
        if (track.segments.size() > limits.maxAnimationSegmentsOrStepsPerTrack) {
            addScopedError(diagnostics, "chart.animation.generated_limit",
                           "Animation segment count exceeds the configured per-Track limit", scope,
                           track.fieldPath);
        }
        if (!checkedAccumulate(totalSegmentsAndSteps, track.segments.size(),
                               limits.maxAnimationSegmentsAndSteps)) {
            addScopedError(diagnostics, "chart.animation.generated_limit",
                           "Total animation segment and step count exceeds the configured limit",
                           scope, track.fieldPath);
        }
    }
    for (const auto& track : clip.stepTracks) {
        if (track.steps.size() > limits.maxAnimationSegmentsOrStepsPerTrack) {
            addScopedError(diagnostics, "chart.animation.generated_limit",
                           "Animation step count exceeds the configured per-Track limit", scope,
                           track.fieldPath);
        }
        if (!checkedAccumulate(totalSegmentsAndSteps, track.steps.size(),
                               limits.maxAnimationSegmentsAndSteps)) {
            addScopedError(diagnostics, "chart.animation.generated_limit",
                           "Total animation segment and step count exceeds the configured limit",
                           scope, track.fieldPath);
        }
    }
}

void validateProgram(const AnimationProgramInput& program, const ChartDocument& chart,
                     const ProgramPaths& paths,
                     const std::map<std::string, ImportedTemplate, std::less<>>& imports,
                     const ChartLimits& limits, core::Diagnostics& diagnostics) {
    std::map<AnimationRecordIdentity, const AnimationClip*, std::less<>> clips;
    std::size_t totalTracks{};
    std::size_t totalSegmentsAndSteps{};
    for (const auto& [id, imported] : imports) {
        static_cast<void>(id);
        validateClipBudget(imported.document.clip, imported.importFieldPath + "/source", nullptr,
                           limits, totalTracks, totalSegmentsAndSteps, diagnostics);
    }
    for (const auto& item : program.clips) {
        clips.emplace(item.identity, &item.clip);
        const auto clipPath = paths.clips.find(item.identity);
        const auto* clipScope = clipPath != paths.clips.end() ? &clipPath->second : nullptr;
        validateClipBudget(item.clip, item.clip.fieldPath, clipScope, limits, totalTracks,
                           totalSegmentsAndSteps, diagnostics);
    }

    std::map<std::string, const ObjectComponents*, std::less<>> componentsByObject;
    for (const auto& object : chart.objects) {
        componentsByObject.emplace(object.id.value, &object.components);
    }
    for (const auto& object : program.objects) {
        const auto components = componentsByObject.find(object.objectId.value);
        const ObjectComponents* objectComponents =
            components != componentsByObject.end() ? components->second : nullptr;
        for (std::size_t left = 0; left < object.layers.size(); ++left) {
            const auto leftMask = expandMask(object.layers[left].propertyMask);
            for (std::size_t right = left + 1; right < object.layers.size(); ++right) {
                if (object.layers[left].priority == object.layers[right].priority &&
                    intersects(leftMask, expandMask(object.layers[right].propertyMask))) {
                    const auto path = paths.layers.find(
                        LayerPathKey{object.objectId.value, object.layers[right].identity});
                    const auto alternatePath = paths.layers.find(
                        LayerPathKey{object.objectId.value, object.layers[left].identity});
                    const auto* scope = preferGeneratedScope(
                        path != paths.layers.end() ? &path->second : nullptr,
                        alternatePath != paths.layers.end() ? &alternatePath->second : nullptr);
                    addScopedError(diagnostics, "chart.animation.mask_conflict",
                                   "Layers with the same priority have overlapping masks", scope,
                                   "$/animationProgram");
                }
            }
            std::vector<PropertySet> groupProperties;
            for (const auto& group : object.layers[left].blendGroups) {
                PropertySet effectiveGroup;
                for (const auto& instance : group.instances) {
                    const auto instanceMask = expandMask(instance.propertyMask);
                    const auto instancePath = paths.instances.find(
                        InstancePathKey{object.objectId.value, object.layers[left].identity,
                                        group.identity, instance.identity});
                    if (!isSubset(instanceMask, leftMask)) {
                        addScopedError(diagnostics, "chart.animation.mask_conflict",
                                       "Clip instance mask is not a subset of its Layer mask",
                                       instancePath != paths.instances.end() ? &instancePath->second
                                                                             : nullptr,
                                       "$/animationProgram");
                    }
                    const auto* clip = findClip(clips, instance.clipIdentity);
                    if (clip == nullptr) {
                        addScopedError(diagnostics, "chart.animation.reference_missing",
                                       "Clip instance refers to a missing concrete Clip",
                                       instancePath != paths.instances.end() ? &instancePath->second
                                                                             : nullptr,
                                       "$/animationProgram");
                        continue;
                    }
                    auto effective = intersection(clipProperties(*clip), instanceMask);
                    effective = intersection(effective, leftMask);
                    effectiveGroup.insert(effective.begin(), effective.end());
                    if (group.mode == AnimationBlendMode::Additive &&
                        effective.contains("transform.scale") && !scaleIsPositive(*clip)) {
                        addScopedError(diagnostics, "chart.animation.additive_unsupported",
                                       "Additive transform.scale requires positive factors",
                                       instancePath != paths.instances.end() ? &instancePath->second
                                                                             : nullptr,
                                       "$/animationProgram");
                    }
                }
                const auto groupPath = paths.groups.find(GroupPathKey{
                    object.objectId.value, object.layers[left].identity, group.identity});
                if (group.mode == AnimationBlendMode::Additive &&
                    !additiveAllowed(effectiveGroup)) {
                    addScopedError(diagnostics, "chart.animation.additive_unsupported",
                                   "Additive BlendGroup targets an unsupported property",
                                   groupPath != paths.groups.end() ? &groupPath->second : nullptr,
                                   "$/animationProgram");
                }
                if (isDiscrete(effectiveGroup) &&
                    (object.layers[left].weight != 1.0 || group.weight != 1.0)) {
                    addScopedError(diagnostics, "chart.animation.discrete_weight_unsupported",
                                   "Discrete properties require resolved Layer and Group weight 1",
                                   groupPath != paths.groups.end() ? &groupPath->second : nullptr,
                                   "$/animationProgram");
                }
                for (const auto& prior : groupProperties) {
                    if (intersects(prior, effectiveGroup)) {
                        addScopedError(
                            diagnostics, "chart.animation.track_conflict",
                            "BlendGroups in one Layer have overlapping effective properties",
                            groupPath != paths.groups.end() ? &groupPath->second : nullptr,
                            "$/animationProgram");
                        break;
                    }
                }
                groupProperties.push_back(effectiveGroup);
                if (objectComponents != nullptr) {
                    for (const auto& property : effectiveGroup) {
                        if (property.starts_with("transform.") && !objectComponents->transform) {
                            addScopedError(diagnostics, "chart.animation.reference_missing",
                                           "Animation property requires cuexis.transform",
                                           groupPath != paths.groups.end() ? &groupPath->second
                                                                           : nullptr,
                                           "$/animationProgram");
                        }
                        if ((property.starts_with("material.") ||
                             property.starts_with("render.")) &&
                            !objectComponents->renderable) {
                            addScopedError(diagnostics, "chart.animation.reference_missing",
                                           "Animation property requires cuexis.renderable",
                                           groupPath != paths.groups.end() ? &groupPath->second
                                                                           : nullptr,
                                           "$/animationProgram");
                        }
                    }
                }
            }
        }
    }
}

void addResource(std::map<std::string, std::set<ChartResourceUse>, std::less<>>& resources,
                 const AssetId& assetId, ChartResourceUse use) {
    resources[assetId.value].insert(use);
}

void addRenderableResources(
    std::map<std::string, std::set<ChartResourceUse>, std::less<>>& resources,
    const ObjectComponents& components) {
    if (!components.renderable) {
        return;
    }
    addResource(resources, components.renderable->mesh, ChartResourceUse::RenderableMesh);
    addResource(resources, components.renderable->material, ChartResourceUse::RenderableMaterial);
}

void addClipResources(std::map<std::string, std::set<ChartResourceUse>, std::less<>>& resources,
                      const AnimationClip& clip) {
    for (const auto& track : clip.stepTracks) {
        if (track.property != AnimationStepProperty::RenderMaterial) {
            continue;
        }
        for (const auto& step : track.steps) {
            if (const auto* asset = std::get_if<AssetId>(&step.value)) {
                addResource(resources, *asset, ChartResourceUse::AnimationMaterial);
            }
        }
    }
}

[[nodiscard]] auto
resourceRequirements(const ChartDocument& chart, const std::vector<AnimationClip>& chartClips,
                     const std::map<std::string, ImportedTemplate, std::less<>>& imports)
    -> std::vector<ChartResourceRequirement> {
    std::map<std::string, std::set<ChartResourceUse>, std::less<>> resources;
    if (chart.audio) {
        addResource(resources, chart.audio->mainMusic, ChartResourceUse::MainMusic);
    }
    for (const auto& item : chart.templates) {
        addRenderableResources(resources, item.prototype);
    }
    for (const auto& item : chart.objects) {
        addRenderableResources(resources, item.components);
    }
    for (const auto& behavior : chart.behaviors) {
        for (const auto& event : behavior.stepEvents) {
            if (event.property == BehaviorStepProperty::RenderMaterial) {
                if (const auto* asset = std::get_if<AssetId>(&event.value)) {
                    addResource(resources, *asset, ChartResourceUse::BehaviorMaterial);
                }
            }
        }
    }
    for (const auto& clip : chartClips) {
        addClipResources(resources, clip);
    }
    for (const auto& [id, imported] : imports) {
        static_cast<void>(id);
        addClipResources(resources, imported.document.clip);
    }

    std::vector<ChartResourceRequirement> result;
    result.reserve(resources.size());
    for (const auto& [assetId, uses] : resources) {
        result.push_back(ChartResourceRequirement{
            AssetId{assetId}, std::vector<ChartResourceUse>{uses.begin(), uses.end()}});
    }
    return result;
}

[[nodiscard]] auto
capabilityRequirements(const ChartV4SourceDocument& source,
                       const std::map<std::string, AnimatorComponent, std::less<>>& animators)
    -> std::vector<std::string> {
    std::set<std::string, std::less<>> result{"cuexis.chart.v4"};
    if (!source.animationTemplateImports.empty()) {
        result.emplace("cuexis.source.cxt.v1");
    }
    bool hasAnimation = !source.animationTemplateImports.empty() || !source.animationClips.empty();
    for (const auto& animator : source.animators) {
        hasAnimation = hasAnimation || !animator.component.templateBindings.empty() ||
                       !animator.component.layers.empty();
    }
    for (const auto& [id, animator] : animators) {
        static_cast<void>(id);
        hasAnimation =
            hasAnimation || !animator.templateBindings.empty() || !animator.layers.empty();
    }
    if (hasAnimation) {
        result.emplace("cuexis.animation.clip.v1");
        result.emplace("cuexis.animation.layers.v1");
    }
    return {result.begin(), result.end()};
}

} // namespace

auto CanonicalContentIdentity::hex() const -> std::string {
    return detail::sha256Hex(sha256);
}

namespace {

[[nodiscard]] auto resolveParsedV4Source(const ChartV4SourceDocument& source, json::Value parsed,
                                         detail::ResolvedParameterSet resolvedParameters,
                                         std::span<const ProjectDocument> projectDocuments,
                                         std::span<const RequiredExtension> supportedExtensions,
                                         const ChartLimits& limits, core::Diagnostics diagnostics)
    -> ChartV4ResolveResult {
    auto canonicalChart = detail::writeV4Value(parsed, limits);
    if (!canonicalChart) {
        addError(diagnostics, canonicalChart.error(), "$");
    }

    for (const auto& use : source.parameterUses) {
        const auto value = resolvedParameters.byId.find(use.id);
        if (value == resolvedParameters.byId.end() ||
            !replaceAtFieldPath(parsed, use.fieldPath, parameterJsonValue(value->second))) {
            addError(diagnostics, "chart.parameter.use_not_allowed",
                     "Chart parameter use could not be resolved at its source path", use.fieldPath);
        }
    }

    std::map<std::string, std::uint32_t, std::less<>> supported;
    for (const auto& extension : supportedExtensions) {
        auto [item, inserted] = supported.emplace(extension.id, extension.version);
        if (!inserted) {
            item->second = std::max(item->second, extension.version);
        }
    }
    validateRequiredExtensions(source.requiredExtensions, supported, diagnostics,
                               "$/requiredExtensions");
    auto imports = loadImports(source, projectDocuments, supported, limits, diagnostics);
    auto chart = makeConcreteChart(parsed, limits, diagnostics);
    auto animators = concreteAnimators(parsed, limits, diagnostics);
    auto program =
        buildProgram(source, animators, imports, resolvedParameters.byId, limits, diagnostics);
    if (chart) {
        validateProgram(program.program, *chart, program.paths, imports, limits, diagnostics);
    }

    std::vector<CxtIdentityComponent> cxtIdentities;
    cxtIdentities.reserve(imports.size());
    for (const auto& [id, imported] : imports) {
        cxtIdentities.push_back(CxtIdentityComponent{id, imported.identity});
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors() || !chart || !canonicalChart ||
        imports.size() != source.animationTemplateImports.size()) {
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    auto resources = resourceRequirements(*chart, source.animationClips, imports);
    auto capabilities = capabilityRequirements(source, animators);
    return ChartV4ResolveResult{
        ChartV4ResolvedArtifact{
            ResolvedChartDocument{std::move(*chart), std::move(resolvedParameters.values)},
            std::move(program.program), CanonicalContentIdentity{detail::sha256(*canonicalChart)},
            std::move(cxtIdentities), resolvedParameters.identity, std::move(resources),
            std::move(capabilities)},
        std::move(diagnostics)};
}

} // namespace

auto ChartV4Resolver::resolve(const ChartV4SourceDocument& source,
                              std::span<const ChartParameterInput> parameters,
                              std::span<const ProjectDocument> projectDocuments,
                              std::span<const RequiredExtension> supportedExtensions,
                              const ChartLimits& limits) -> ChartV4ResolveResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    auto resolvedParameters = detail::resolveChartParameters(
        source.parameters, source.parameterUses, parameters, limits, diagnostics);
    auto parsed =
        json::parse(source.canonicalSource.canonicalText,
                    {limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes});
    if (!parsed) {
        addError(diagnostics, parsed.error(), "$");
    }
    if (!resolvedParameters || !parsed) {
        diagnostics.sortDeterministically();
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    return resolveParsedV4Source(source, std::move(*parsed), std::move(*resolvedParameters),
                                 projectDocuments, supportedExtensions, limits,
                                 std::move(diagnostics));
}

auto detail::resolveV4Parsed(const ChartV4SourceDocument& source,
                             const ParsedChartInput& parsedInput,
                             std::span<const ChartParameterInput> parameters,
                             std::span<const ProjectDocument> projectDocuments,
                             std::span<const RequiredExtension> supportedExtensions,
                             const ChartLimits& limits) -> ChartV4ResolveResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    auto resolvedParameters = detail::resolveChartParameters(
        source.parameters, source.parameterUses, parameters, limits, diagnostics);
    if (!resolvedParameters) {
        diagnostics.sortDeterministically();
        return ChartV4ResolveResult{std::nullopt, std::move(diagnostics)};
    }
    return resolveParsedV4Source(source, parsedInput.value, std::move(*resolvedParameters),
                                 projectDocuments, supportedExtensions, limits,
                                 std::move(diagnostics));
}

} // namespace cuexis::chart
