#include <cuexis/chart/animation_template_loader.hpp>
#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
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
#include "chart_v4_resolver_units_internal.hpp"
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

using PropertySet = std::set<std::string, std::less<>>;

void addResolverError(core::Diagnostics& diagnostics, std::string code, std::string message,
                      std::string path) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), std::move(path)});
}

void addResolverError(core::Diagnostics& diagnostics, const core::Error& error, std::string path) {
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
        addResolverError(diagnostics, "chart.resolve.source_invalid",
                         "Chart source root is invalid", "$");
        return std::nullopt;
    }
    root->insert_or_assign("version", json::Value{std::int64_t{3}});
    root->erase("parameters");
    root->erase("animationTemplateImports");
    root->erase("animationClips");
    root->insert_or_assign("requiredExtensions", json::Value{json::Value::Array{}});
    eraseAnimators(source, "templates");
    eraseAnimators(source, "objects");
    auto loaded = detail::loadCanonicalValue(source, limits);
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
        addResolverError(diagnostics, "chart.parameter.use_not_allowed",
                         "Resolved Animator still contains a parameter reference", use.fieldPath);
    }
    return result;
}

void applyAnimatorPatches(std::optional<AnimatorComponent>& animator,
                          const json::Value::Array& patches, std::string_view patchesPath,
                          const ChartLimits& limits, core::Diagnostics& diagnostics) {
    if (patches.size() > limits.maxPatchesPerTemplate) {
        addResolverError(diagnostics, "chart.limit.patches",
                         "Patch count exceeds the configured limit", std::string{patchesPath});
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
                addResolverError(diagnostics, "chart.patch.remove_has_value",
                                 "Remove patch must not contain a value", path + "/value");
                continue;
            }
            if (!animator) {
                addResolverError(diagnostics, "chart.patch.target_missing",
                                 "Remove patch target does not exist", path + "/path");
                continue;
            }
            animator.reset();
            continue;
        }
        if (*operation != "add" && *operation != "replace") {
            addResolverError(diagnostics, "chart.patch.operation_unsupported",
                             "Only add, remove, and replace patch operations are supported",
                             path + "/op");
            continue;
        }
        if (value == nullptr) {
            addResolverError(diagnostics, "chart.patch.value_missing",
                             "Patch operation requires a value", path + "/value");
            continue;
        }
        if (*operation == "replace" && !animator) {
            addResolverError(diagnostics, "chart.patch.target_missing",
                             "Replace patch target does not exist", path + "/path");
            continue;
        }
        std::vector<ParameterUse> unexpectedUses;
        json::Reader reader{*value, diagnostics, path + "/value"};
        auto replacement =
            detail::readAnimatorComponent(reader, limits, diagnostics, unexpectedUses);
        for (const auto& use : unexpectedUses) {
            addResolverError(diagnostics, "chart.parameter.use_not_allowed",
                             "Resolved Animator patch still contains a parameter reference",
                             use.fieldPath);
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
            addResolverError(diagnostics, "chart.template.inheritance_cycle",
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
            addResolverError(diagnostics, "chart.template.parent_missing",
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

} // namespace cuexis::chart
