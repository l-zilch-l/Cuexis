#include "canonical_chart_template_internal.hpp"

#include <cuexis/chart/uuid.hpp>

#include <array>
#include <utility>

namespace cuexis::chart::detail {
namespace {

using core::DiagnosticSeverity;

void addDiagnostic(core::Diagnostics& diagnostics, DiagnosticSeverity severity, std::string code,
                   std::string message, std::string path) {
    diagnostics.add(
        core::Diagnostic{severity, std::move(code), std::move(message), std::move(path)});
}

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Error, std::move(code), std::move(message),
                  std::move(path));
}

void addWarning(core::Diagnostics& diagnostics, std::string code, std::string message,
                std::string path) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Warning, std::move(code), std::move(message),
                  std::move(path));
}

[[nodiscard]] auto isChartEntityUuid(std::string_view value) noexcept -> bool {
    return isUuidV7(value) || isUuidV5(value);
}

[[nodiscard]] auto splitPatchPath(std::string_view path) -> std::vector<std::string> {
    std::vector<std::string> segments;
    if (path.empty() || path.front() != '/') {
        return segments;
    }
    std::size_t begin = 1;
    while (begin <= path.size()) {
        const auto end = path.find('/', begin);
        const auto encoded =
            path.substr(begin, end == std::string_view::npos ? path.size() - begin : end - begin);
        std::string decoded;
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
        segments.push_back(std::move(decoded));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return segments;
}

[[nodiscard]] auto isPatchableComponent(std::string_view component) noexcept -> bool {
    return component == "cuexis.transform" || component == "cuexis.renderable" ||
           component == "cuexis.behavior" || component == "cuexis.note" ||
           component == "cuexis.element" || component == "cuexis.camera";
}

[[nodiscard]] auto isPatchableField(std::string_view component, std::string_view field) noexcept
    -> bool {
    if (component == "cuexis.transform") {
        return field == "position" || field == "rotation" || field == "scale";
    }
    if (component == "cuexis.renderable") {
        return field == "mesh" || field == "material";
    }
    if (component == "cuexis.behavior") {
        return field == "behavior";
    }
    if (component == "cuexis.note") {
        return field == "beat";
    }
    if (component == "cuexis.camera") {
        return field == "fovY" || field == "near" || field == "far" || field == "type";
    }
    return false;
}

} // namespace

void applyPatches(json::Value& components, const json::Value::Array& patches,
                  std::string_view patchesPath, const ChartLimits& limits,
                  core::Diagnostics& diagnostics) {
    if (patches.size() > limits.maxPatchesPerTemplate) {
        addError(diagnostics, "chart.limit.patches", "Patch count exceeds the configured limit",
                 std::string{patchesPath});
        return;
    }
    auto* componentsObject = components.object();
    if (componentsObject == nullptr) {
        addError(diagnostics, "chart.patch.target_invalid", "Patch target components are invalid",
                 std::string{patchesPath});
        return;
    }

    for (std::size_t index = 0; index < patches.size(); ++index) {
        if (diagnostics.limitReached()) {
            break;
        }
        const std::string patchPath = json::appendIndexPath(patchesPath, index);
        json::Reader patchReader{patches[index], diagnostics, patchPath};
        constexpr std::array knownFields{std::string_view{"op"}, std::string_view{"path"},
                                         std::string_view{"value"}};
        if (patchReader.readObject() == nullptr) {
            continue;
        }
        patchReader.rejectUnknownFields(knownFields);
        const auto opReader = patchReader.requiredField("op");
        const auto pathReader = patchReader.requiredField("path");
        if (!opReader || !pathReader) {
            continue;
        }
        const auto operation = opReader->readString();
        const auto targetPath = pathReader->readString();
        if (!operation || !targetPath) {
            continue;
        }
        if (*operation != "add" && *operation != "remove" && *operation != "replace") {
            addError(diagnostics, "chart.patch.operation_unsupported",
                     "Only add, remove, and replace patch operations are supported",
                     std::string{opReader->fieldPath()});
            continue;
        }

        const auto segments = splitPatchPath(*targetPath);
        if ((segments.size() != 2 && segments.size() != 3) || segments[0] != "components" ||
            !isPatchableComponent(segments[1]) ||
            (segments.size() == 3 && !isPatchableField(segments[1], segments[2]))) {
            addError(diagnostics, "chart.patch.path_unsupported",
                     "Patch path is outside the fixed chart v1 component schema",
                     std::string{pathReader->fieldPath()});
            continue;
        }

        const auto valueReader = patchReader.optionalField("value");
        if (*operation == "remove" && valueReader) {
            addError(diagnostics, "chart.patch.remove_has_value",
                     "Remove patch must not contain a value",
                     std::string{valueReader->fieldPath()});
            continue;
        }
        if (*operation != "remove" && !valueReader) {
            addError(diagnostics, "chart.patch.value_missing", "Patch operation requires a value",
                     json::appendFieldPath(patchReader.fieldPath(), "value"));
            continue;
        }

        json::Value::Object* targetObject = componentsObject;
        std::string key = segments[1];
        if (segments.size() == 3) {
            const auto component = componentsObject->find(segments[1]);
            if (component == componentsObject->end() || component->second.object() == nullptr) {
                addError(diagnostics, "chart.patch.target_missing",
                         "Patch component target does not exist",
                         std::string{pathReader->fieldPath()});
                continue;
            }
            targetObject = component->second.object();
            key = segments[2];
        }

        const auto existing = targetObject->find(key);
        if (*operation == "remove") {
            if (existing == targetObject->end()) {
                addError(diagnostics, "chart.patch.target_missing",
                         "Remove patch target does not exist",
                         std::string{pathReader->fieldPath()});
                continue;
            }
            targetObject->erase(existing);
        } else if (*operation == "replace") {
            if (existing == targetObject->end()) {
                addError(diagnostics, "chart.patch.target_missing",
                         "Replace patch target does not exist",
                         std::string{pathReader->fieldPath()});
                continue;
            }
            existing->second = valueReader->value();
        } else {
            targetObject->insert_or_assign(std::move(key), valueReader->value());
        }
    }
}

namespace {

struct RawTemplate final {
    ChartTemplateId id;
    std::optional<std::string> name;
    std::optional<ChartTemplateId> extends;
    std::optional<json::Value> prototype;
    json::Value::Array patches;
    OpaqueJson extensions;
    std::string path;
    enum class State { Unvisited, Visiting, Resolved, Failed } state{State::Unvisited};
    std::optional<json::Value> expanded;
};

[[nodiscard]] auto resolveTemplate(const std::string& id,
                                   std::map<std::string, RawTemplate>& templates,
                                   const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<json::Value> {
    std::vector<std::string> chain;
    std::string current = id;
    std::optional<json::Value> components;
    bool failed = false;

    while (true) {
        auto& item = templates.at(current);
        if (item.state == RawTemplate::State::Resolved) {
            components = item.expanded;
            break;
        }
        if (item.state == RawTemplate::State::Failed) {
            failed = true;
            break;
        }
        if (item.state == RawTemplate::State::Visiting) {
            addError(diagnostics, "chart.template.inheritance_cycle",
                     "Template inheritance contains a cycle",
                     json::appendFieldPath(item.path, "extends"));
            failed = true;
            break;
        }

        item.state = RawTemplate::State::Visiting;
        chain.push_back(current);
        if (!item.extends) {
            if (item.prototype) {
                components = *item.prototype;
            } else {
                failed = true;
            }
            break;
        }

        const auto parent = templates.find(item.extends->value);
        if (parent == templates.end()) {
            addError(diagnostics, "chart.template.parent_missing",
                     "Template extends a missing template",
                     json::appendFieldPath(item.path, "extends"));
            failed = true;
            break;
        }
        current = parent->first;
    }

    if (failed || !components) {
        for (const auto& templateId : chain) {
            templates.at(templateId).state = RawTemplate::State::Failed;
        }
        return std::nullopt;
    }

    while (!chain.empty()) {
        auto& item = templates.at(chain.back());
        if (item.extends) {
            applyPatches(*components, item.patches, json::appendFieldPath(item.path, "patch"),
                         limits, diagnostics);
        }
        item.expanded = *components;
        item.state = RawTemplate::State::Resolved;
        chain.pop_back();
    }
    return templates.at(id).expanded;
}

} // namespace

auto parseTemplates(const json::Reader& reader, const ChartLimits& limits,
                    core::Diagnostics& diagnostics, const TemplateParserCallbacks& callbacks)
    -> std::pair<std::vector<ChartTemplate>, std::map<std::string, json::Value>> {
    std::vector<ChartTemplate> result;
    std::map<std::string, json::Value> expandedById;
    const auto* values = reader.readArray();
    if (values == nullptr) {
        return {std::move(result), std::move(expandedById)};
    }
    if (values->size() > limits.maxTemplates) {
        addError(diagnostics, "chart.limit.templates",
                 "Template count exceeds the configured limit", std::string{reader.fieldPath()});
        return {std::move(result), std::move(expandedById)};
    }

    std::map<std::string, RawTemplate> templates;
    for (std::size_t index = 0; index < values->size(); ++index) {
        if (diagnostics.limitReached()) {
            break;
        }
        const auto itemReader = reader.element(index);
        if (!itemReader || itemReader->readObject() == nullptr) {
            continue;
        }
        constexpr std::array knownFields{
            std::string_view{"id"},      std::string_view{"name"},
            std::string_view{"extends"}, std::string_view{"prototype"},
            std::string_view{"patch"},   std::string_view{"extensions"}};
        itemReader->rejectUnknownFields(knownFields);
        const auto idReader = itemReader->requiredField("id");
        const auto extendsReader = itemReader->requiredField("extends");
        const auto extensionsReader = itemReader->requiredField("extensions");
        if (!idReader || !extendsReader || !extensionsReader) {
            continue;
        }
        const auto id = callbacks.readIdentifier(*idReader, limits, diagnostics, "Template ID");
        if (!id) {
            continue;
        }
        if (!isChartEntityUuid(*id)) {
            addError(diagnostics, "chart.uuid.invalid_entity_id",
                     "Template ID must be a canonical UUIDv7 or imported UUIDv5",
                     std::string{idReader->fieldPath()});
            continue;
        }
        if (templates.contains(*id)) {
            addError(diagnostics, "chart.template.id_duplicate", "Template ID must be unique",
                     std::string{idReader->fieldPath()});
            continue;
        }

        RawTemplate raw{.id = ChartTemplateId{*id},
                        .name = callbacks.readNullableName(*itemReader, limits, diagnostics),
                        .extends = std::nullopt,
                        .prototype = std::nullopt,
                        .patches = {},
                        .extensions = {},
                        .path = std::string{itemReader->fieldPath()},
                        .state = RawTemplate::State::Unvisited,
                        .expanded = std::nullopt};
        if (extendsReader->value().isNull()) {
            const auto prototypeReader = itemReader->requiredField("prototype");
            if (itemReader->optionalField("patch")) {
                addError(diagnostics, "chart.template.shape_invalid",
                         "Root template must not contain patch",
                         json::appendFieldPath(itemReader->fieldPath(), "patch"));
            }
            if (prototypeReader) {
                constexpr std::array prototypeFields{std::string_view{"components"}};
                prototypeReader->rejectUnknownFields(prototypeFields);
                const auto componentsReader = prototypeReader->requiredField("components");
                if (componentsReader) {
                    raw.prototype = componentsReader->value();
                }
            }
        } else {
            const auto parent =
                callbacks.readReference(*extendsReader, "template", limits, diagnostics);
            if (parent) {
                raw.extends = ChartTemplateId{*parent};
            }
            if (itemReader->optionalField("prototype")) {
                addError(diagnostics, "chart.template.shape_invalid",
                         "Derived template must not contain prototype",
                         json::appendFieldPath(itemReader->fieldPath(), "prototype"));
            }
            const auto patchReader = itemReader->requiredField("patch");
            if (patchReader) {
                if (const auto* patches = patchReader->readArray()) {
                    raw.patches = *patches;
                }
            }
        }
        if (extensionsReader->readObject() != nullptr) {
            raw.extensions = callbacks.opaqueJson(extensionsReader->value(), diagnostics,
                                                  extensionsReader->fieldPath());
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
        templates.emplace(*id, std::move(raw));
    }

    for (auto& [id, item] : templates) {
        if (diagnostics.limitReached()) {
            break;
        }
        auto expanded = resolveTemplate(id, templates, limits, diagnostics);
        if (!expanded) {
            continue;
        }
        const auto typed = callbacks.parseComponents(
            *expanded, json::appendFieldPath(item.path, "prototype/components"), limits,
            diagnostics, false);
        if (!typed) {
            continue;
        }
        expandedById.emplace(id, *expanded);
        result.push_back(ChartTemplate{item.id, item.name, item.extends, *typed, item.extensions});
    }
    return {std::move(result), std::move(expandedById)};
}

} // namespace cuexis::chart::detail
