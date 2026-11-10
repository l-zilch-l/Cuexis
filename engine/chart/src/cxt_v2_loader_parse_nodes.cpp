#include <cuexis/chart/cxt_v2_loader.hpp>

#include "cxt_v2_loader_units_internal.hpp"

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/value.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart {

[[nodiscard]] auto parsePrototype(const Value& value, core::Diagnostics& diagnostics,
                                  std::string_view path, const ChartLimits& limits)
    -> std::optional<std::pair<std::string, Prototype>> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*values, {"id", "slots", "components", "requirements", "extensions"}, diagnostics,
                  path);
    const auto* idValue = field(*values, "id", diagnostics, path);
    const auto* slotsValue = field(*values, "slots", diagnostics, path);
    const auto* componentsValue = field(*values, "components", diagnostics, path);
    const auto* requirementsValue = field(*values, "requirements", diagnostics, path);
    if (!requireEmptyObject(*values, "extensions", diagnostics, path)) {
        return std::nullopt;
    }
    const auto id = idValue ? readId(*idValue, diagnostics, childPath(path, "id")) : std::nullopt;
    if (!id || slotsValue == nullptr || componentsValue == nullptr ||
        requirementsValue == nullptr) {
        return std::nullopt;
    }
    Prototype result;
    result.id = *id;
    const auto* slots = array(*slotsValue, diagnostics, childPath(path, "slots"));
    if (slots == nullptr) {
        return std::nullopt;
    }
    if (slots->size() > limits.maxCxtV2SlotsPerPrototype) {
        addCxtError(diagnostics, "cxt.v2.budget.slots", "Slot budget exceeded",
                    childPath(path, "slots"));
        return std::nullopt;
    }
    for (std::size_t i = 0; i < slots->size(); ++i) {
        auto slot = parseSlot((*slots)[i], diagnostics, indexPath(childPath(path, "slots"), i));
        if (!slot) {
            return std::nullopt;
        }
        if (!result.slots.emplace(slot->first, std::move(slot->second)).second) {
            addCxtError(diagnostics, "cxt.v2.id_duplicate", "Slot ID is duplicated",
                        indexPath(childPath(path, "slots"), i));
            return std::nullopt;
        }
    }
    const auto* components = array(*componentsValue, diagnostics, childPath(path, "components"));
    if (components == nullptr) {
        return std::nullopt;
    }
    std::set<std::string> componentTypes;
    for (std::size_t i = 0; i < components->size(); ++i) {
        const auto componentPath = indexPath(childPath(path, "components"), i);
        const auto* component = object((*components)[i], diagnostics, componentPath);
        if (component == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*component, {"type", "version", "fields", "extensions"}, diagnostics,
                      componentPath);
        const auto* typeValue = field(*component, "type", diagnostics, componentPath);
        const auto* versionValue = field(*component, "version", diagnostics, componentPath);
        const auto* fieldsValue = field(*component, "fields", diagnostics, componentPath);
        if (!requireEmptyObject(*component, "extensions", diagnostics, componentPath)) {
            return std::nullopt;
        }
        const auto type = typeValue ? string(*typeValue) : std::nullopt;
        const auto version = versionValue ? integer(*versionValue) : std::nullopt;
        if (!type || fieldsValue == nullptr) {
            addCxtError(diagnostics, "cxt.v2.component_invalid",
                        "Component requires type and fields", componentPath);
            return std::nullopt;
        }
        if (*type != "cuexis.transform" || !version || *version != 1) {
            addCxtError(diagnostics, "cxt.v2.component_unsupported",
                        "Foundation CXT v2 supports cuexis.transform version 1 only",
                        childPath(componentPath, "type"));
            return std::nullopt;
        }
        if (!componentTypes.insert(std::string{*type}).second) {
            addCxtError(diagnostics, "cxt.v2.id_duplicate", "Component type is duplicated",
                        childPath(componentPath, "type"));
            return std::nullopt;
        }
        result.hasTransform = true;
        const auto* fields = array(*fieldsValue, diagnostics, childPath(componentPath, "fields"));
        if (fields == nullptr) {
            return std::nullopt;
        }
        for (std::size_t j = 0; j < fields->size(); ++j) {
            const auto fieldPath = indexPath(childPath(componentPath, "fields"), j);
            const auto* entry = object((*fields)[j], diagnostics, fieldPath);
            if (entry == nullptr) {
                return std::nullopt;
            }
            rejectUnknown(*entry, {"path", "source"}, diagnostics, fieldPath);
            const auto* pathValue = field(*entry, "path", diagnostics, fieldPath);
            const auto* sourceValue = field(*entry, "source", diagnostics, fieldPath);
            const auto fieldName = pathValue ? string(*pathValue) : std::nullopt;
            if (!fieldName || sourceValue == nullptr) {
                addCxtError(diagnostics, "cxt.v2.component_invalid",
                            "Component field requires path and source", fieldPath);
                return std::nullopt;
            }
            if (*fieldName != "position[0]") {
                addCxtError(diagnostics, "cxt.v2.component_path_unsupported",
                            "Foundation CXT v2 supports transform position[0] only",
                            childPath(fieldPath, "path"));
                return std::nullopt;
            }
            if (result.positionX) {
                addCxtError(diagnostics, "cxt.v2.id_duplicate",
                            "Component field path is duplicated", childPath(fieldPath, "path"));
                return std::nullopt;
            }
            result.positionX = parseSource(*sourceValue, diagnostics,
                                           childPath(fieldPath, "source"), false, true, false);
            if (!result.positionX) {
                return std::nullopt;
            }
        }
    }
    const auto* requirements =
        array(*requirementsValue, diagnostics, childPath(path, "requirements"));
    if (requirements == nullptr) {
        return std::nullopt;
    }
    std::set<std::string> requirementIds;
    for (std::size_t i = 0; i < requirements->size(); ++i) {
        const auto requirementPath = indexPath(childPath(path, "requirements"), i);
        const auto* requirement = object((*requirements)[i], diagnostics, requirementPath);
        if (requirement == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*requirement,
                      {"id", "kind", "interval", "judgementDomain", "requiredAction", "constraints",
                       "effects", "extensions"},
                      diagnostics, requirementPath);
        const auto* idField = field(*requirement, "id", diagnostics, requirementPath);
        const auto* kindField = field(*requirement, "kind", diagnostics, requirementPath);
        const auto* intervalField = field(*requirement, "interval", diagnostics, requirementPath);
        const auto* domainField =
            field(*requirement, "judgementDomain", diagnostics, requirementPath);
        const auto* actionField =
            field(*requirement, "requiredAction", diagnostics, requirementPath);
        const auto* constraintsField =
            field(*requirement, "constraints", diagnostics, requirementPath);
        const auto* effectsField = field(*requirement, "effects", diagnostics, requirementPath);
        if (!requireEmptyObject(*requirement, "extensions", diagnostics, requirementPath)) {
            return std::nullopt;
        }
        if (idField == nullptr || kindField == nullptr || intervalField == nullptr ||
            domainField == nullptr || actionField == nullptr || constraintsField == nullptr ||
            effectsField == nullptr) {
            return std::nullopt;
        }
        const auto* effects =
            array(*effectsField, diagnostics, childPath(requirementPath, "effects"));
        if (effects == nullptr) {
            return std::nullopt;
        }
        if (!effects->empty()) {
            addCxtError(diagnostics, "cxt.v2.requirement_unsupported",
                        "Foundation requirements must not declare effects",
                        childPath(requirementPath, "effects"));
            return std::nullopt;
        }
        const auto requirementId = readId(*idField, diagnostics, childPath(requirementPath, "id"));
        const auto kind = string(*kindField);
        if (!requirementId || !kind || *kind != "tap" ||
            !isLiteralReference(*domainField, "judgement-domain", "candidate.lanes4", diagnostics,
                                childPath(requirementPath, "judgementDomain")) ||
            !isLiteralReference(*actionField, "action", "press", diagnostics,
                                childPath(requirementPath, "requiredAction"))) {
            addCxtError(
                diagnostics, "cxt.v2.requirement_unsupported",
                "Foundation CXT v2 supports tap/point candidate.lanes4 press requirements only",
                requirementPath);
            return std::nullopt;
        }
        if (!requirementIds.insert(*requirementId).second) {
            addCxtError(diagnostics, "cxt.v2.id_duplicate", "Requirement ID is duplicated",
                        childPath(requirementPath, "id"));
            return std::nullopt;
        }
        const auto* interval =
            object(*intervalField, diagnostics, childPath(requirementPath, "interval"));
        const auto* constraints =
            array(*constraintsField, diagnostics, childPath(requirementPath, "constraints"));
        if (interval == nullptr || constraints == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*interval, {"kind", "startBeat"}, diagnostics,
                      childPath(requirementPath, "interval"));
        const auto* intervalKind =
            field(*interval, "kind", diagnostics, childPath(requirementPath, "interval"));
        const auto* startBeat =
            field(*interval, "startBeat", diagnostics, childPath(requirementPath, "interval"));
        if (intervalKind == nullptr || startBeat == nullptr || !string(*intervalKind) ||
            *string(*intervalKind) != "point") {
            addCxtError(diagnostics, "cxt.v2.requirement_unsupported",
                        "Foundation CXT v2 supports point intervals only",
                        childPath(requirementPath, "interval"));
            return std::nullopt;
        }
        if (constraints->size() != 1) {
            addCxtError(diagnostics, "cxt.v2.requirement_unsupported",
                        "Foundation CXT v2 requires exactly one lane constraint",
                        childPath(requirementPath, "constraints"));
            return std::nullopt;
        }
        const auto constraintPath = indexPath(childPath(requirementPath, "constraints"), 0);
        const auto* constraint = object((*constraints)[0], diagnostics, constraintPath);
        if (constraint == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*constraint, {"kind", "value"}, diagnostics, constraintPath);
        const auto* constraintKind = field(*constraint, "kind", diagnostics, constraintPath);
        const auto* constraintValue = field(*constraint, "value", diagnostics, constraintPath);
        if (constraintKind == nullptr || constraintValue == nullptr || !string(*constraintKind) ||
            *string(*constraintKind) != "lane") {
            addCxtError(diagnostics, "cxt.v2.requirement_unsupported",
                        "Foundation CXT v2 supports lane constraints only", constraintPath);
            return std::nullopt;
        }
        auto beat = parseSource(*startBeat, diagnostics,
                                childPath(childPath(requirementPath, "interval"), "startBeat"),
                                false, true, false);
        auto lane = parseSource(*constraintValue, diagnostics, childPath(constraintPath, "value"),
                                false, true, false);
        if (!beat || !lane) {
            return std::nullopt;
        }
        result.requirements.push_back(
            Requirement{*requirementId, std::move(*beat), std::move(*lane)});
    }
    return std::pair{*id, std::move(result)};
}

[[nodiscard]] auto parseRepeatCount(const Value& value, core::Diagnostics& diagnostics,
                                    std::string_view path) -> std::optional<Source> {
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    const auto* kindValue = field(*values, "kind", diagnostics, path);
    if (kindValue == nullptr || !string(*kindValue)) {
        addCxtError(diagnostics, "cxt.v2.source_invalid", "Repeat count kind must be a string",
                    childPath(path, "kind"));
        return std::nullopt;
    }
    const auto kind = *string(*kindValue);
    if (kind == "literal") {
        rejectUnknown(*values, {"kind", "value"}, diagnostics, path);
        const auto* literal = field(*values, "value", diagnostics, path);
        if (literal == nullptr) {
            return std::nullopt;
        }
        const auto integerValue = integer(*literal);
        if (!integerValue) {
            addCxtError(diagnostics, "cxt.v2.repeat_count_invalid",
                        "Repeat count must be a literal integer or integer parameter",
                        childPath(path, "value"));
            return std::nullopt;
        }
        return Source{SourceKind::Literal, ValueKind::Integer, *integerValue, {}, {}, {}, {}};
    }
    if (kind == "parameter") {
        rejectUnknown(*values, {"kind", "id"}, diagnostics, path);
        const auto* idValue = field(*values, "id", diagnostics, path);
        const auto id =
            idValue ? readId(*idValue, diagnostics, childPath(path, "id")) : std::nullopt;
        if (!id) {
            return std::nullopt;
        }
        return Source{SourceKind::Parameter, ValueKind::Integer, std::int64_t{0}, *id, {}, {}, {}};
    }
    addCxtError(diagnostics, "cxt.v2.repeat_count_invalid",
                "Repeat count must be a literal integer or integer parameter", std::string{path});
    return std::nullopt;
}

[[nodiscard]] auto parseNode(const Value& value, core::Diagnostics& diagnostics,
                             std::string_view path, std::size_t& nodeCount,
                             const ChartLimits& limits, const std::set<std::string>& indexScope)
    -> std::optional<Node> {
    if (++nodeCount > limits.maxCxtV2Nodes) {
        addCxtError(diagnostics, "cxt.v2.budget.nodes", "CXT v2 node budget exceeded",
                    std::string{path});
        return std::nullopt;
    }
    const auto* values = object(value, diagnostics, path);
    if (values == nullptr) {
        return std::nullopt;
    }
    const auto* opValue = field(*values, "op", diagnostics, path);
    const auto* idValue = field(*values, "nodeId", diagnostics, path);
    if (opValue == nullptr || idValue == nullptr || !string(*opValue)) {
        if (opValue != nullptr && !string(*opValue)) {
            addCxtError(diagnostics, "cxt.v2.node_unsupported", "Pattern node op must be a string",
                        childPath(path, "op"));
        }
        return std::nullopt;
    }
    const auto id = readId(*idValue, diagnostics, childPath(path, "nodeId"));
    if (!id) {
        return std::nullopt;
    }
    Node node;
    node.nodeId = *id;
    if (*string(*opValue) == "emit") {
        rejectUnknown(*values, {"op", "nodeId", "prototype", "bindings", "parent"}, diagnostics,
                      path);
        const auto* protoValue = field(*values, "prototype", diagnostics, path);
        const auto* bindingsValue = field(*values, "bindings", diagnostics, path);
        const auto* parentValue = field(*values, "parent", diagnostics, path);
        const auto prototypeId = protoValue ? string(*protoValue) : std::nullopt;
        if (!prototypeId || !stableId(*prototypeId) || bindingsValue == nullptr ||
            parentValue == nullptr) {
            if (prototypeId && !stableId(*prototypeId)) {
                addCxtError(diagnostics, "cxt.v2.id_invalid", "Expected a portable stable ID",
                            childPath(path, "prototype"));
            } else if (protoValue != nullptr && !prototypeId) {
                addCxtError(diagnostics, "cxt.v2.prototype_missing",
                            "Emit prototype must be a string", childPath(path, "prototype"));
            }
            return std::nullopt;
        }
        node.op = Node::Op::Emit;
        node.prototype = std::string{*prototypeId};
        const auto* bindings = array(*bindingsValue, diagnostics, childPath(path, "bindings"));
        if (bindings == nullptr) {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < bindings->size(); ++i) {
            const auto bindingPath = indexPath(childPath(path, "bindings"), i);
            const auto* binding = object((*bindings)[i], diagnostics, bindingPath);
            if (binding == nullptr) {
                return std::nullopt;
            }
            rejectUnknown(*binding, {"slot", "source"}, diagnostics, bindingPath);
            const auto* slotValue = field(*binding, "slot", diagnostics, bindingPath);
            const auto* sourceValue = field(*binding, "source", diagnostics, bindingPath);
            if (slotValue == nullptr || sourceValue == nullptr) {
                return std::nullopt;
            }
            const auto slot = readId(*slotValue, diagnostics, childPath(bindingPath, "slot"));
            auto source = parseSource(*sourceValue, diagnostics, childPath(bindingPath, "source"),
                                      true, false, true);
            if (!slot || !source) {
                return std::nullopt;
            }
            if (!node.bindings.emplace(*slot, std::move(*source)).second) {
                addCxtError(diagnostics, "cxt.v2.id_duplicate", "Slot binding is duplicated",
                            bindingPath);
                return std::nullopt;
            }
        }
        const auto* parent = object(*parentValue, diagnostics, childPath(path, "parent"));
        if (parent == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*parent, {"kind", "nodeId"}, diagnostics, childPath(path, "parent"));
        const auto* kindValue = field(*parent, "kind", diagnostics, childPath(path, "parent"));
        if (kindValue == nullptr || !string(*kindValue)) {
            addCxtError(diagnostics, "cxt.v2.parent_invalid", "Parent kind must be a string",
                        childPath(path, "parent/kind"));
            return std::nullopt;
        }
        if (*string(*kindValue) == "root" || *string(*kindValue) == "invocation-parent") {
            if (parent->find("nodeId") != parent->end()) {
                addCxtError(diagnostics, "cxt.v2.parent_invalid",
                            "Only emission parents may declare nodeId", childPath(path, "parent"));
                return std::nullopt;
            }
            node.parentKind = *string(*kindValue) == "root" ? Node::ParentKind::Root
                                                            : Node::ParentKind::Invocation;
        } else if (*string(*kindValue) == "emission") {
            const auto* targetValue =
                field(*parent, "nodeId", diagnostics, childPath(path, "parent"));
            const auto target =
                targetValue ? readId(*targetValue, diagnostics, childPath(path, "parent/nodeId"))
                            : std::nullopt;
            if (!target) {
                return std::nullopt;
            }
            node.parentKind = Node::ParentKind::Emission;
            node.parentNodeId = *target;
        } else {
            addCxtError(diagnostics, "cxt.v2.parent_invalid", "Unsupported parent kind",
                        childPath(path, "parent/kind"));
            return std::nullopt;
        }
        return node;
    }
    if (*string(*opValue) != "repeat") {
        addCxtError(diagnostics, "cxt.v2.node_unsupported", "Unsupported pattern node operation",
                    childPath(path, "op"));
        return std::nullopt;
    }
    rejectUnknown(*values, {"op", "nodeId", "count", "index", "body"}, diagnostics, path);
    const auto* countValue = field(*values, "count", diagnostics, path);
    const auto* indexValue = field(*values, "index", diagnostics, path);
    const auto* bodyValue = field(*values, "body", diagnostics, path);
    if (countValue == nullptr || indexValue == nullptr || bodyValue == nullptr) {
        return std::nullopt;
    }
    const auto indexId = readId(*indexValue, diagnostics, childPath(path, "index"));
    if (!indexId) {
        return std::nullopt;
    }
    if (indexScope.contains(*indexId)) {
        addCxtError(diagnostics, "cxt.v2.index_shadowed",
                    "Repeat index ID shadows an enclosing index", childPath(path, "index"));
        return std::nullopt;
    }
    node.op = Node::Op::Repeat;
    node.indexId = *indexId;
    auto count = parseRepeatCount(*countValue, diagnostics, childPath(path, "count"));
    if (!count) {
        return std::nullopt;
    }
    node.count = std::move(*count);
    const auto* body = array(*bodyValue, diagnostics, childPath(path, "body"));
    if (body == nullptr || body->empty()) {
        addCxtError(diagnostics, "cxt.v2.repeat_body_empty", "Repeat body must not be empty",
                    childPath(path, "body"));
        return std::nullopt;
    }
    auto nestedScope = indexScope;
    nestedScope.insert(node.indexId);
    std::set<std::string> ids;
    for (std::size_t i = 0; i < body->size(); ++i) {
        auto child = parseNode((*body)[i], diagnostics, indexPath(childPath(path, "body"), i),
                               nodeCount, limits, nestedScope);
        if (!child) {
            return std::nullopt;
        }
        if (!ids.insert(child->nodeId).second) {
            addCxtError(diagnostics, "cxt.v2.id_duplicate", "Pattern node ID is duplicated",
                        indexPath(childPath(path, "body"), i));
            return std::nullopt;
        }
        node.body.push_back(std::move(*child));
    }
    return node;
}

[[nodiscard]] auto parseModule(const Value& root, core::Diagnostics& diagnostics,
                               const ChartLimits& limits) -> std::optional<Module> {
    const auto* values = object(root, diagnostics, "$");
    if (values == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*values,
                  {"format", "version", "moduleId", "moduleKind", "metadata", "parameters",
                   "prototypes", "patterns", "animations", "exports", "requiredExtensions",
                   "extensions"},
                  diagnostics, "$");
    const auto* format = field(*values, "format", diagnostics, "$");
    const auto* version = field(*values, "version", diagnostics, "$");
    if (format == nullptr || version == nullptr || !string(*format) ||
        *string(*format) != "cuexis.animation-template" || !integer(*version) ||
        *integer(*version) != 2) {
        addCxtError(diagnostics, "cxt.v2.version_unsupported", "Expected CXT v2 module",
                    "$/version");
        return std::nullopt;
    }
    if (limits.maxCxtV2Modules < 1) {
        addCxtError(diagnostics, "cxt.v2.budget.modules", "CXT v2 module budget exceeded", "$");
        return std::nullopt;
    }
    const auto* moduleId = field(*values, "moduleId", diagnostics, "$");
    const auto* kind = field(*values, "moduleKind", diagnostics, "$");
    const auto* metadata = field(*values, "metadata", diagnostics, "$");
    const auto* parameters = field(*values, "parameters", diagnostics, "$");
    const auto* prototypes = field(*values, "prototypes", diagnostics, "$");
    const auto* patterns = field(*values, "patterns", diagnostics, "$");
    const auto* animations = field(*values, "animations", diagnostics, "$");
    const auto* exports = field(*values, "exports", diagnostics, "$");
    if (!requireEmptyArray(*values, "requiredExtensions", diagnostics, "$") ||
        !requireEmptyObject(*values, "extensions", diagnostics, "$")) {
        return std::nullopt;
    }
    if (moduleId == nullptr || kind == nullptr || metadata == nullptr || parameters == nullptr ||
        prototypes == nullptr || patterns == nullptr || animations == nullptr ||
        exports == nullptr) {
        return std::nullopt;
    }
    if (object(*metadata, diagnostics, "$/metadata") == nullptr) {
        return std::nullopt;
    }
    auto id = readId(*moduleId, diagnostics, "$/moduleId");
    if (!id || !string(*kind)) {
        if (kind != nullptr && !string(*kind)) {
            addCxtError(diagnostics, "cxt.v2.module_kind_unsupported",
                        "Foundation supports prototype and pattern modules", "$/moduleKind");
        }
        return std::nullopt;
    }
    if (*string(*kind) != "prototype" && *string(*kind) != "pattern") {
        addCxtError(diagnostics, "cxt.v2.module_kind_unsupported",
                    "Foundation supports prototype and pattern modules", "$/moduleKind");
        return std::nullopt;
    }
    Module module;
    module.id = *id;
    module.moduleKind = std::string{*string(*kind)};
    const auto* parameterArray = array(*parameters, diagnostics, "$/parameters");
    if (parameterArray == nullptr) {
        return std::nullopt;
    }
    if (parameterArray->size() > limits.maxCxtV2Parameters) {
        addCxtError(diagnostics, "cxt.v2.budget.parameters", "Parameter budget exceeded",
                    "$/parameters");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < parameterArray->size(); ++i) {
        auto p = parseParameter((*parameterArray)[i], diagnostics, indexPath("$/parameters", i));
        if (!p) {
            return std::nullopt;
        }
        if (!module.parameters.emplace(p->first, std::move(p->second)).second) {
            addCxtError(diagnostics, "cxt.v2.id_duplicate", "Parameter ID is duplicated",
                        indexPath("$/parameters", i));
            return std::nullopt;
        }
    }
    const auto* prototypeArray = array(*prototypes, diagnostics, "$/prototypes");
    if (prototypeArray == nullptr) {
        return std::nullopt;
    }
    if (prototypeArray->size() > limits.maxCxtV2Prototypes) {
        addCxtError(diagnostics, "cxt.v2.budget.prototypes", "Prototype budget exceeded",
                    "$/prototypes");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < prototypeArray->size(); ++i) {
        auto p =
            parsePrototype((*prototypeArray)[i], diagnostics, indexPath("$/prototypes", i), limits);
        if (!p) {
            return std::nullopt;
        }
        if (!module.prototypes.emplace(p->first, std::move(p->second)).second) {
            addCxtError(diagnostics, "cxt.v2.id_duplicate", "Prototype ID is duplicated",
                        indexPath("$/prototypes", i));
            return std::nullopt;
        }
    }
    const auto* patternArray = array(*patterns, diagnostics, "$/patterns");
    if (patternArray == nullptr) {
        return std::nullopt;
    }
    std::size_t nodeCount = 0;
    if (patternArray->size() > limits.maxCxtV2Patterns) {
        addCxtError(diagnostics, "cxt.v2.budget.patterns", "Pattern budget exceeded", "$/patterns");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < patternArray->size(); ++i) {
        const auto path = indexPath("$/patterns", i);
        const auto* p = object((*patternArray)[i], diagnostics, path);
        if (p == nullptr) {
            return std::nullopt;
        }
        rejectUnknown(*p, {"id", "nodes", "extensions"}, diagnostics, path);
        const auto* idField = field(*p, "id", diagnostics, path);
        const auto* nodesField = field(*p, "nodes", diagnostics, path);
        if (!requireEmptyObject(*p, "extensions", diagnostics, path) || idField == nullptr ||
            nodesField == nullptr) {
            return std::nullopt;
        }
        auto pid = readId(*idField, diagnostics, childPath(path, "id"));
        const auto* nodes = array(*nodesField, diagnostics, childPath(path, "nodes"));
        if (!pid || nodes == nullptr) {
            return std::nullopt;
        }
        if (nodes->empty()) {
            addCxtError(diagnostics, "cxt.v2.pattern_empty", "Pattern nodes must not be empty",
                        childPath(path, "nodes"));
            return std::nullopt;
        }
        Pattern pattern;
        pattern.id = *pid;
        std::set<std::string> topLevelNodeIds;
        for (std::size_t j = 0; j < nodes->size(); ++j) {
            auto n = parseNode((*nodes)[j], diagnostics, indexPath(childPath(path, "nodes"), j),
                               nodeCount, limits, {});
            if (!n) {
                return std::nullopt;
            }
            if (!topLevelNodeIds.insert(n->nodeId).second) {
                addCxtError(diagnostics, "cxt.v2.id_duplicate", "Pattern node ID is duplicated",
                            indexPath(childPath(path, "nodes"), j));
                return std::nullopt;
            }
            pattern.nodes.push_back(std::move(*n));
        }
        if (!module.patterns.emplace(pattern.id, std::move(pattern)).second) {
            addCxtError(diagnostics, "cxt.v2.id_duplicate", "Pattern ID is duplicated", path);
            return std::nullopt;
        }
    }
    const auto* animationArray = array(*animations, diagnostics, "$/animations");
    if (animationArray == nullptr) {
        return std::nullopt;
    }
    if (!animationArray->empty()) {
        addCxtError(diagnostics, "cxt.v2.animation_unsupported",
                    "Foundation CXT v2 does not support animations", "$/animations");
        return std::nullopt;
    }
    const auto* exportArray = array(*exports, diagnostics, "$/exports");
    if (exportArray == nullptr || exportArray->size() != 1) {
        addCxtError(diagnostics, "cxt.v2.exports_invalid", "Exactly one export is required",
                    "$/exports");
        return std::nullopt;
    }
    const auto* ex = object((*exportArray)[0], diagnostics, "$/exports/0");
    if (ex == nullptr) {
        return std::nullopt;
    }
    rejectUnknown(*ex, {"kind", "id"}, diagnostics, "$/exports/0");
    const auto* exportKind = field(*ex, "kind", diagnostics, "$/exports/0");
    const auto* exportId = field(*ex, "id", diagnostics, "$/exports/0");
    const auto exportKindText = exportKind ? string(*exportKind) : std::nullopt;
    const auto exportIdText =
        exportId ? readId(*exportId, diagnostics, "$/exports/0/id") : std::nullopt;
    if (!exportKindText || !exportIdText || *exportKindText != module.moduleKind) {
        addCxtError(diagnostics, "cxt.v2.exports_invalid", "Export kind must match module kind",
                    "$/exports/0");
        return std::nullopt;
    }
    module.exportId = *exportIdText;
    if (module.moduleKind == "pattern" &&
        module.patterns.find(module.exportId) == module.patterns.end()) {
        addCxtError(diagnostics, "cxt.v2.exports_invalid", "Export ID is not declared",
                    "$/exports/0/id");
        return std::nullopt;
    }
    if (module.moduleKind == "prototype" &&
        module.prototypes.find(module.exportId) == module.prototypes.end()) {
        addCxtError(diagnostics, "cxt.v2.exports_invalid", "Export ID is not declared",
                    "$/exports/0/id");
        return std::nullopt;
    }
    if (module.moduleKind == "prototype" && !module.patterns.empty()) {
        addCxtError(diagnostics, "cxt.v2.module_shape_invalid",
                    "Prototype modules must not declare patterns", "$/patterns");
        return std::nullopt;
    }
    return module;
}

} // namespace cuexis::chart
